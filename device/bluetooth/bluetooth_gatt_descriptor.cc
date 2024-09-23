// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/bluetooth_gatt_descriptor.h"

#include <stddef.h>

#include <vector>

#include "base/lazy_instance.h"

namespace device {
namespace {

struct UUIDs {
  UUIDs() : uuids_(MakeUUIDVector()) {}

  const std::vector<BluetoothUUID> uuids_;

 private:
  static std::vector<BluetoothUUID> MakeUUIDVector() {
    std::vector<BluetoothUUID> uuids;
    static const char* const strings[] = {
        "0x2900", "0x2901", "0x2902", "0x2903", "0x2904", "0x2905"
    };

    for (size_t i = 0; i < std::size(strings); ++i)
      uuids.push_back(BluetoothUUID(strings[i]));

    return uuids;
  }
};

base::LazyInstance<const UUIDs>::Leaky g_uuids = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const BluetoothUUID&
BluetoothGattDescriptor::CharacteristicExtendedPropertiesUuid() {
  return g_uuids.Get().uuids_[0];
}

// static
const BluetoothUUID&
BluetoothGattDescriptor::CharacteristicUserDescriptionUuid() {
  return g_uuids.Get().uuids_[1];
}

// static
const BluetoothUUID&
BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid() {
  return g_uuids.Get().uuids_[2];
}

// static
const BluetoothUUID&
BluetoothGattDescriptor::ServerCharacteristicConfigurationUuid() {
  return g_uuids.Get().uuids_[3];
}

// static
const BluetoothUUID&
BluetoothGattDescriptor::CharacteristicPresentationFormatUuid() {
  return g_uuids.Get().uuids_[4];
}

// static
const BluetoothUUID&
BluetoothGattDescriptor::CharacteristicAggregateFormatUuid() {
  return g_uuids.Get().uuids_[5];
}

BluetoothGattDescriptor::BluetoothGattDescriptor() = default;

BluetoothGattDescriptor::~BluetoothGattDescriptor() = default;

}  // namespace device
