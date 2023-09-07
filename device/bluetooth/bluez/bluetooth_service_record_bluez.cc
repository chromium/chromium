// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/values.h"

namespace bluez {

BluetoothServiceRecordBlueZ::BluetoothServiceRecordBlueZ() = default;

BluetoothServiceRecordBlueZ::BluetoothServiceRecordBlueZ(
    const BluetoothServiceRecordBlueZ& record) {
  this->attributes_ = record.attributes_;
}

BluetoothServiceRecordBlueZ::~BluetoothServiceRecordBlueZ() = default;

const std::vector<uint16_t> BluetoothServiceRecordBlueZ::GetAttributeIds()
    const {
  std::vector<uint16_t> ids;
  ids.reserve(attributes_.size());
  for (const auto& attribute : attributes_)
    ids.emplace_back(attribute.first);
  return ids;
}

const BluetoothServiceAttributeValueBlueZ&
BluetoothServiceRecordBlueZ::GetAttributeValue(uint16_t attribute_id) const {
  auto it = attributes_.find(attribute_id);
  CHECK(it != attributes_.end());
  return it->second;
}

void BluetoothServiceRecordBlueZ::AddRecordEntry(
    uint16_t id,
    const BluetoothServiceAttributeValueBlueZ& value) {
  auto it = attributes_.find(id);
  if (it != attributes_.end())
    attributes_.erase(it);
  attributes_.insert(
      std::pair<uint16_t, BluetoothServiceAttributeValueBlueZ>(id, value));
}

bool BluetoothServiceRecordBlueZ::IsAttributePresented(uint16_t id) const {
  return base::Contains(attributes_, id);
}

}  // namespace bluez
