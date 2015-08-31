/* nttable.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvDataCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>

#define epicsExportSharedSymbols
#include <pv/nttable.h>
#include <pv/ntutils.h>

using namespace std;
using namespace epics::pvData;

namespace epics { namespace nt {

static NTFieldPtr ntField = NTField::get();

namespace detail {

NTTableBuilder::shared_pointer NTTableBuilder::addColumn(
        std::string const & name, epics::pvData::ScalarType scalarType
        )
{
    if (std::find(columnNames.begin(), columnNames.end(), name) != columnNames.end())
        throw std::runtime_error("duplicate column name");

    columnNames.push_back(name);
    types.push_back(scalarType);

    return shared_from_this();
}

StructureConstPtr NTTableBuilder::createStructure()
{
    FieldBuilderPtr builder = getFieldCreate()->createFieldBuilder();

    FieldBuilderPtr nestedBuilder =
            builder->
               setId(NTTable::URI)->
               addArray("labels", pvString)->
               addNestedStructure("value");

    vector<string>::size_type len = columnNames.size();
    for (vector<string>::size_type i = 0; i < len; i++)
        nestedBuilder->addArray(columnNames[i], types[i]);

    builder = nestedBuilder->endNested();

    if (descriptor)
        builder->add("descriptor", pvString);

    if (alarm)
        builder->add("alarm", ntField->createAlarm());

    if (timeStamp)
        builder->add("timeStamp", ntField->createTimeStamp());

    size_t extraCount = extraFieldNames.size();
    for (size_t i = 0; i< extraCount; i++)
        builder->add(extraFieldNames[i], extraFields[i]);

    StructureConstPtr s = builder->createStructure();

    reset();
    return s;
}

NTTableBuilder::shared_pointer NTTableBuilder::addDescriptor()
{
    descriptor = true;
    return shared_from_this();
}

NTTableBuilder::shared_pointer NTTableBuilder::addAlarm()
{
    alarm = true;
    return shared_from_this();
}

NTTableBuilder::shared_pointer NTTableBuilder::addTimeStamp()
{
    timeStamp = true;
    return shared_from_this();
}

PVStructurePtr NTTableBuilder::createPVStructure()
{
    // fill in labels with default values (the column names)
    size_t len = columnNames.size();
    shared_vector<string> l(len);
    for(size_t i=0; i<len; ++i) l[i] = columnNames[i];
    PVStructurePtr s = getPVDataCreate()->createPVStructure(createStructure());
    s->getSubField<PVStringArray>("labels")->replace(freeze(l));
    return s;
}

NTTablePtr NTTableBuilder::create()
{
    return NTTablePtr(new NTTable(createPVStructure()));
}

NTTableBuilder::NTTableBuilder()
{
    reset();
}

void NTTableBuilder::reset()
{
    columnNames.clear();
    types.clear();
    descriptor = false;
    alarm = false;
    timeStamp = false;
}

NTTableBuilder::shared_pointer NTTableBuilder::add(string const & name, FieldConstPtr const & field)
{
    extraFields.push_back(field); extraFieldNames.push_back(name);
    return shared_from_this();
}


}

const std::string NTTable::URI("epics:nt/NTTable:1.0");

NTTable::shared_pointer NTTable::wrap(PVStructurePtr const & structure)
{
    if(!isCompatible(structure)) return shared_pointer();
    return wrapUnsafe(structure);
}

NTTable::shared_pointer NTTable::wrapUnsafe(PVStructurePtr const & structure)
{
    return shared_pointer(new NTTable(structure));
}

bool NTTable::is_a(StructureConstPtr const & structure)
{
    return NTUtils::is_a(structure->getID(), URI);
}

bool NTTable::isCompatible(StructureConstPtr const & structure)
{
    if (!structure.get()) return false;

    StructureConstPtr valueField = structure->getField<Structure>("value");
    if (!valueField.get())
        return false;

    FieldConstPtrArray const & fields = valueField->getFields();
    for (FieldConstPtrArray::const_iterator it = fields.begin();
         it != fields.end(); ++it)
    {
        if ((*it)->getType() != scalarArray) return false;
    }

    ScalarArrayConstPtr labelsField = structure->getField<ScalarArray>("labels");
    if (!labelsField.get() || labelsField->getElementType() != pvString)
        return false;

    FieldConstPtr field = structure->getField("descriptor");
    if (field.get())
    {
        ScalarConstPtr descriptorField = structure->getField<Scalar>("descriptor");
        if (!descriptorField.get() || descriptorField->getScalarType() != pvString)
            return false;
    }

    NTFieldPtr ntField = NTField::get();

    field = structure->getField("alarm");
    if (field.get() && !ntField->isAlarm(field))
        return false;

    field = structure->getField("timeStamp");
    if (field.get() && !ntField->isTimeStamp(field))
        return false;

    return true;
}

bool NTTable::isCompatible(PVStructurePtr const & pvStructure)
{
    if(!pvStructure) return false;

    return isCompatible(pvStructure->getStructure());
}

bool NTTable::isValid()
{
    PVFieldPtrArray const & columns = pvValue->getPVFields();
        
    if (getLabels()->getLength() != columns.size()) return false;
    boolean first = true;
    int length = 0;
    for (PVFieldPtrArray::const_iterator it = columns.begin();
        it != columns.end();++it)
    {
        PVScalarArrayPtr column = std::tr1::dynamic_pointer_cast<PVScalarArray>(*it);
        if (!column.get()) return false;
        int colLength = column->getLength();
        if (first)
        {
            length = colLength;
            first = false;
        }
        else if (length != colLength)
            return false;
    }

    return true;
}


NTTableBuilderPtr NTTable::createBuilder()
{
    return NTTableBuilderPtr(new detail::NTTableBuilder());
}

bool NTTable::attachTimeStamp(PVTimeStamp &pvTimeStamp) const
{
    PVStructurePtr ts = getTimeStamp();
    if (ts)
        return pvTimeStamp.attach(ts);
    else
        return false;
}

bool NTTable::attachAlarm(PVAlarm &pvAlarm) const
{
    PVStructurePtr al = getAlarm();
    if (al)
        return pvAlarm.attach(al);
    else
        return false;
}

PVStructurePtr NTTable::getPVStructure() const
{
    return pvNTTable;
}

PVStringPtr NTTable::getDescriptor() const
{
    return pvNTTable->getSubField<PVString>("descriptor");
}

PVStructurePtr NTTable::getTimeStamp() const
{
    return pvNTTable->getSubField<PVStructure>("timeStamp");
}

PVStructurePtr NTTable::getAlarm() const
{
    return pvNTTable->getSubField<PVStructure>("alarm");
}

PVStringArrayPtr NTTable::getLabels() const
{
    return pvNTTable->getSubField<PVStringArray>("labels");
}

StringArray const & NTTable::getColumnNames() const
{
    return pvNTTable->getStructure()->getFieldNames();
}

PVFieldPtr NTTable::getColumn(std::string const & columnName) const
{
    return pvValue->getSubField(columnName);
}

NTTable::NTTable(PVStructurePtr const & pvStructure) :
    pvNTTable(pvStructure), pvValue(pvNTTable->getSubField<PVStructure>("value"))
{}


}}
