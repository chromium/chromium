// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "dbus/property.h"

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"

#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus {

//
// PropertyBase implementation.
//

PropertyBase::PropertyBase() : property_set_(nullptr), is_valid_(false) {}

PropertyBase::~PropertyBase() = default;

void PropertyBase::Init(PropertySet* property_set, const std::string& name) {
  DCHECK(!property_set_);
  property_set_ = property_set;
  is_valid_ = false;
  name_ = name;
}

//
// PropertySet implementation.
//

PropertySet::PropertySet(
    ObjectProxy* object_proxy,
    const std::string& interface,
    const PropertyChangedCallback& property_changed_callback)
    : object_proxy_(object_proxy),
      interface_(interface),
      property_changed_callback_(property_changed_callback) {}

PropertySet::~PropertySet() = default;

void PropertySet::RegisterProperty(const std::string& name,
                                   PropertyBase* property) {
  property->Init(this, name);
  properties_map_[name] = property;
}

void PropertySet::ConnectSignals() {
  DCHECK(object_proxy_);
  object_proxy_->ConnectToSignal(
      kPropertiesInterface, kPropertiesChanged,
      base::BindRepeating(&PropertySet::ChangedReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PropertySet::ChangedConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}


void PropertySet::ChangedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);

  std::string interface;
  if (!reader.PopString(&interface)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected interface name: " << signal->ToString();
    return;
  }

  if (interface != this->interface())
    return;

  if (!UpdatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected dictionary: " << signal->ToString();
  }

  if (!InvalidatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected array to invalidate: " << signal->ToString();
  }
}

void PropertySet::ChangedConnected(const std::string& interface_name,
                                   const std::string& signal_name,
                                   bool success) {
  LOG_IF(WARNING, !success) << "Failed to connect to " << signal_name
                            << "signal.";
}


void PropertySet::Get(PropertyBase* property, GetCallback callback) {
  MethodCall method_call(kPropertiesInterface, kPropertiesGet);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());
  writer.AppendString(property->name());

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::BindOnce(&PropertySet::OnGet, GetWeakPtr(),
                                           property, std::move(callback)));
}

void PropertySet::OnGet(PropertyBase* property, GetCallback callback,
                        Response* response) {
  if (!response) {
    LOG(WARNING) << property->name() << ": Get: failed.";
    return;
  }

  MessageReader reader(response);
  if (property->PopValueFromReader(&reader)) {
    property->set_valid(true);
    NotifyPropertyChanged(property->name());
  } else {
    if (property->is_valid()) {
      property->set_valid(false);
      NotifyPropertyChanged(property->name());
    }
  }

  if (!callback.is_null())
    std::move(callback).Run(response);
}

bool PropertySet::GetAndBlock(PropertyBase* property) {
  MethodCall method_call(kPropertiesInterface, kPropertiesGet);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());
  writer.AppendString(property->name());

  DCHECK(object_proxy_);
  auto result = object_proxy_->CallMethodAndBlock(
      &method_call, ObjectProxy::TIMEOUT_USE_DEFAULT);

  if (!result.has_value()) {
    LOG(WARNING) << property->name() << ": GetAndBlock: failed.";
    return false;
  }

  MessageReader reader(result->get());
  if (property->PopValueFromReader(&reader)) {
    property->set_valid(true);
    NotifyPropertyChanged(property->name());
  } else {
    if (property->is_valid()) {
      property->set_valid(false);
      NotifyPropertyChanged(property->name());
    }
  }
  return true;
}

void PropertySet::GetAll() {
  MethodCall method_call(kPropertiesInterface, kPropertiesGetAll);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(
      &method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&PropertySet::OnGetAll, weak_ptr_factory_.GetWeakPtr()));
}

void PropertySet::OnGetAll(Response* response) {
  if (!response) {
    LOG(WARNING) << "GetAll request failed for: " << interface_;
    return;
  }

  MessageReader reader(response);
  if (!UpdatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "GetAll response has wrong parameters: "
                 << "expected dictionary: " << response->ToString();
  }
}

void PropertySet::Set(PropertyBase* property, SetCallback callback) {
  MethodCall method_call(kPropertiesInterface, kPropertiesSet);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());
  writer.AppendString(property->name());
  property->AppendSetValueToWriter(&writer);

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::BindOnce(&PropertySet::OnSet, GetWeakPtr(),
                                           property, std::move(callback)));
}

bool PropertySet::SetAndBlock(PropertyBase* property) {
  MethodCall method_call(kPropertiesInterface, kPropertiesSet);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());
  writer.AppendString(property->name());
  property->AppendSetValueToWriter(&writer);

  DCHECK(object_proxy_);
  return object_proxy_
      ->CallMethodAndBlock(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT)
      .has_value();
}

void PropertySet::OnSet(PropertyBase* property,
                        SetCallback callback,
                        Response* response) {
  LOG_IF(WARNING, !response) << property->name() << ": Set: failed.";
  if (!callback.is_null())
    std::move(callback).Run(response);
}

bool PropertySet::UpdatePropertiesFromReader(MessageReader* reader) {
  DCHECK(reader);
  MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader))
    return false;

  while (array_reader.HasMoreData()) {
    MessageReader dict_entry_reader(nullptr);
    if (array_reader.PopDictEntry(&dict_entry_reader))
      UpdatePropertyFromReader(&dict_entry_reader);
  }

  return true;
}

bool PropertySet::UpdatePropertyFromReader(MessageReader* reader) {
  DCHECK(reader);

  std::string name;
  if (!reader->PopString(&name))
    return false;

  PropertiesMap::iterator it = properties_map_.find(name);
  if (it == properties_map_.end())
    return false;

  PropertyBase* property = it->second;
  if (property->PopValueFromReader(reader)) {
    property->set_valid(true);
    NotifyPropertyChanged(name);
    return true;
  } else {
    if (property->is_valid()) {
      property->set_valid(false);
      NotifyPropertyChanged(property->name());
    }
    return false;
  }
}

bool PropertySet::InvalidatePropertiesFromReader(MessageReader* reader) {
  DCHECK(reader);
  MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader))
    return false;

  while (array_reader.HasMoreData()) {
    std::string name;
    if (!array_reader.PopString(&name))
      return false;

    PropertiesMap::iterator it = properties_map_.find(name);
    if (it == properties_map_.end())
      continue;

    PropertyBase* property = it->second;
    if (property->is_valid()) {
      property->set_valid(false);
      NotifyPropertyChanged(property->name());
    }
  }

  return true;
}

void PropertySet::NotifyPropertyChanged(const std::string& name) {
  if (!property_changed_callback_.is_null())
    property_changed_callback_.Run(name);
}

//
// Property<Byte> specialization.
//

template <>
Property<uint8_t>::Property()
    : value_(0) {}

template <>
bool Property<uint8_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfByte(&value_);
}

template <>
void Property<uint8_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfByte(set_value_);
}

//
// Property<bool> specialization.
//

template <>
Property<bool>::Property() : value_(false) {
}

template <>
bool Property<bool>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfBool(&value_);
}

template <>
void Property<bool>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfBool(set_value_);
}

//
// Property<int16_t> specialization.
//

template <>
Property<int16_t>::Property()
    : value_(0) {}

template <>
bool Property<int16_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt16(&value_);
}

template <>
void Property<int16_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfInt16(set_value_);
}

//
// Property<uint16_t> specialization.
//

template <>
Property<uint16_t>::Property()
    : value_(0) {}

template <>
bool Property<uint16_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint16(&value_);
}

template <>
void Property<uint16_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfUint16(set_value_);
}

//
// Property<int32_t> specialization.
//

template <>
Property<int32_t>::Property()
    : value_(0) {}

template <>
bool Property<int32_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt32(&value_);
}

template <>
void Property<int32_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfInt32(set_value_);
}

//
// Property<uint32_t> specialization.
//

template <>
Property<uint32_t>::Property()
    : value_(0) {}

template <>
bool Property<uint32_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint32(&value_);
}

template <>
void Property<uint32_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfUint32(set_value_);
}

//
// Property<int64_t> specialization.
//

template <>
Property<int64_t>::Property()
    : value_(0), set_value_(0) {}

template <>
bool Property<int64_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt64(&value_);
}

template <>
void Property<int64_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfInt64(set_value_);
}

//
// Property<uint64_t> specialization.
//

template <>
Property<uint64_t>::Property()
    : value_(0) {}

template <>
bool Property<uint64_t>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint64(&value_);
}

template <>
void Property<uint64_t>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfUint64(set_value_);
}

//
// Property<double> specialization.
//

template <>
Property<double>::Property() : value_(0.0) {
}

template <>
bool Property<double>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfDouble(&value_);
}

template <>
void Property<double>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfDouble(set_value_);
}

//
// Property<std::string> specialization.
//

template <>
bool Property<std::string>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfString(&value_);
}

template <>
void Property<std::string>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfString(set_value_);
}

//
// Property<ObjectPath> specialization.
//

template <>
bool Property<ObjectPath>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfObjectPath(&value_);
}

template <>
void Property<ObjectPath>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfObjectPath(set_value_);
}

//
// Property<std::vector<std::string>> specialization.
//

template <>
bool Property<std::vector<std::string>>::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  if (!reader->PopVariant(&variant_reader))
    return false;

  value_.clear();
  return variant_reader.PopArrayOfStrings(&value_);
}

template <>
void Property<std::vector<std::string>>::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(set_value_);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::vector<ObjectPath>> specialization.
//

template <>
bool Property<std::vector<ObjectPath>>::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  if (!reader->PopVariant(&variant_reader))
    return false;

  value_.clear();
  return variant_reader.PopArrayOfObjectPaths(&value_);
}

template <>
void Property<std::vector<ObjectPath>>::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  writer->OpenVariant("ao", &variant_writer);
  variant_writer.AppendArrayOfObjectPaths(set_value_);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::vector<uint8_t>> specialization.
//

template <>
bool Property<std::vector<uint8_t>>::PopValueFromReader(MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  if (!reader->PopVariant(&variant_reader))
    return false;

  value_.clear();
  const uint8_t* bytes = nullptr;
  size_t length = 0;
  if (!variant_reader.PopArrayOfBytes(&bytes, &length))
    return false;
  value_.assign(bytes, bytes + length);
  return true;
}

template <>
void Property<std::vector<uint8_t>>::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  writer->OpenVariant("ay", &variant_writer);
  variant_writer.AppendArrayOfBytes(set_value_);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::map<std::string, std::string>> specialization.
//

template <>
bool Property<std::map<std::string, std::string>>::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  MessageReader array_reader(nullptr);
  if (!reader->PopVariant(&variant_reader) ||
      !variant_reader.PopArray(&array_reader))
    return false;
  value_.clear();
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader))
      return false;
    std::string key;
    std::string value;
    if (!dict_entry_reader.PopString(&key) ||
        !dict_entry_reader.PopString(&value))
      return false;
    value_[key] = value;
  }
  return true;
}

template <>
void Property<std::map<std::string, std::string>>::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);
  writer->OpenVariant("a{ss}", &variant_writer);
  variant_writer.OpenArray("{ss}", &dict_writer);
  for (const auto& pair : set_value_) {
    dbus::MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(pair.first);
    entry_writer.AppendString(pair.second);
    dict_writer.CloseContainer(&entry_writer);
  }
  variant_writer.CloseContainer(&dict_writer);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>
// specialization.
//

template <>
bool Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>::
    PopValueFromReader(MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  MessageReader array_reader(nullptr);
  if (!reader->PopVariant(&variant_reader) ||
      !variant_reader.PopArray(&array_reader))
    return false;

  value_.clear();
  while (array_reader.HasMoreData()) {
    dbus::MessageReader struct_reader(nullptr);
    if (!array_reader.PopStruct(&struct_reader))
      return false;

    std::pair<std::vector<uint8_t>, uint16_t> entry;
    const uint8_t* bytes = nullptr;
    size_t length = 0;
    if (!struct_reader.PopArrayOfBytes(&bytes, &length))
      return false;
    entry.first.assign(bytes, bytes + length);
    if (!struct_reader.PopUint16(&entry.second))
      return false;
    value_.push_back(entry);
  }
  return true;
}

template <>
void Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>::
    AppendSetValueToWriter(MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  MessageWriter array_writer(nullptr);
  writer->OpenVariant("a(ayq)", &variant_writer);
  variant_writer.OpenArray("(ayq)", &array_writer);
  for (const auto& pair : set_value_) {
    dbus::MessageWriter struct_writer(nullptr);
    array_writer.OpenStruct(&struct_writer);
    struct_writer.AppendArrayOfBytes(std::get<0>(pair));
    struct_writer.AppendUint16(std::get<1>(pair));
    array_writer.CloseContainer(&struct_writer);
  }
  variant_writer.CloseContainer(&array_writer);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::map<std::string, std::vector<uint8_t>>>
// specialization.
//

template <>
bool Property<std::map<std::string, std::vector<uint8_t>>>::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  MessageReader dict_reader(nullptr);
  if (!reader->PopVariant(&variant_reader) ||
      !variant_reader.PopArray(&dict_reader))
    return false;

  value_.clear();
  while (dict_reader.HasMoreData()) {
    MessageReader entry_reader(nullptr);
    if (!dict_reader.PopDictEntry(&entry_reader))
      return false;

    std::string key;
    if (!entry_reader.PopString(&key))
      return false;

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    if (entry_reader.GetDataType() == Message::VARIANT) {
      // Make BlueZ happy since it wraps the array of bytes with a variant.
      MessageReader value_variant_reader(nullptr);
      if (!entry_reader.PopVariant(&value_variant_reader))
        return false;
      if (!value_variant_reader.PopArrayOfBytes(&bytes, &length))
        return false;
    } else {
      if (!entry_reader.PopArrayOfBytes(&bytes, &length))
        return false;
    }

    value_[key].assign(bytes, bytes + length);
  }
  return true;
}

template <>
void Property<std::map<std::string, std::vector<uint8_t>>>::
    AppendSetValueToWriter(MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);

  writer->OpenVariant("a{sv}", &variant_writer);
  variant_writer.OpenArray("{sv}", &dict_writer);

  for (const auto& pair : set_value_) {
    MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendString(pair.first);

    MessageWriter value_varient_writer(nullptr);
    entry_writer.OpenVariant("ay", &value_varient_writer);
    value_varient_writer.AppendArrayOfBytes(pair.second);
    entry_writer.CloseContainer(&value_varient_writer);

    dict_writer.CloseContainer(&entry_writer);
  }

  variant_writer.CloseContainer(&dict_writer);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::map<uint16_t, std::vector<uint8_t>>>
// specialization.
//

template <>
bool Property<std::map<uint16_t, std::vector<uint8_t>>>::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(nullptr);
  MessageReader dict_reader(nullptr);
  if (!reader->PopVariant(&variant_reader) ||
      !variant_reader.PopArray(&dict_reader))
    return false;

  value_.clear();
  while (dict_reader.HasMoreData()) {
    MessageReader entry_reader(nullptr);
    if (!dict_reader.PopDictEntry(&entry_reader))
      return false;

    uint16_t key;
    if (!entry_reader.PopUint16(&key))
      return false;

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    if (entry_reader.GetDataType() == Message::VARIANT) {
      // Make BlueZ happy since it wraps the array of bytes with a variant.
      MessageReader value_variant_reader(nullptr);
      if (!entry_reader.PopVariant(&value_variant_reader))
        return false;
      if (!value_variant_reader.PopArrayOfBytes(&bytes, &length))
        return false;
    } else {
      if (!entry_reader.PopArrayOfBytes(&bytes, &length))
        return false;
    }

    value_[key].assign(bytes, bytes + length);
  }
  return true;
}

template <>
void Property<std::map<uint16_t, std::vector<uint8_t>>>::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);

  writer->OpenVariant("a{qv}", &variant_writer);
  variant_writer.OpenArray("{qv}", &dict_writer);

  for (const auto& pair : set_value_) {
    MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendUint16(pair.first);

    MessageWriter value_varient_writer(nullptr);
    entry_writer.OpenVariant("ay", &value_varient_writer);
    value_varient_writer.AppendArrayOfBytes(pair.second);
    entry_writer.CloseContainer(&value_varient_writer);

    dict_writer.CloseContainer(&entry_writer);
  }

  variant_writer.CloseContainer(&dict_writer);
  writer->CloseContainer(&variant_writer);
}

template class Property<uint8_t>;
template class Property<bool>;
template class Property<int16_t>;
template class Property<uint16_t>;
template class Property<int32_t>;
template class Property<uint32_t>;
template class Property<int64_t>;
template class Property<uint64_t>;
template class Property<double>;
template class Property<std::string>;
template class Property<ObjectPath>;
template class Property<std::vector<std::string>>;
template class Property<std::vector<ObjectPath>>;
template class Property<std::vector<uint8_t>>;
template class Property<std::map<std::string, std::string>>;
template class Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>;
template class Property<std::map<std::string, std::vector<uint8_t>>>;
template class Property<std::map<uint16_t, std::vector<uint8_t>>>;

}  // namespace dbus
