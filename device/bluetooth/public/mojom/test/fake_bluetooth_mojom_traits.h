// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_MOJOM_TEST_FAKE_BLUETOOTH_MOJOM_TRAITS_H_
#define DEVICE_BLUETOOTH_PUBLIC_MOJOM_TEST_FAKE_BLUETOOTH_MOJOM_TRAITS_H_

#include <map>
#include <string>
#include <tuple>

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// TODO(https://crbug.com/820627): This file will no longer be needed if Mojo
// allows the ability to specify a custom hasher for Mojo maps.
template <>
struct StructTraits<bluetooth::mojom::ServiceDataMapDataView,
                    device::BluetoothDevice::ServiceDataMap> {
  static const std::map<std::string, std::vector<uint8_t>> service_data(
      const device::BluetoothDevice::ServiceDataMap service_data) {
    std::map<std::string, std::vector<uint8_t>> val;
    for (const auto& uuid_to_data : service_data) {
      val.emplace(uuid_to_data.first.canonical_value(), uuid_to_data.second);
    }
    return val;
  }

  static bool Read(bluetooth::mojom::ServiceDataMapDataView input,
                   device::BluetoothDevice::ServiceDataMap* output) {
    std::map<std::string, std::vector<uint8_t>> result;
    if (!input.ReadServiceData(&result)) {
      return false;
    }
    for (const auto& uuid_str_to_data : result) {
      device::BluetoothDevice::ServiceDataMap::iterator entry;
      bool inserted;
      std::tie(entry, inserted) =
          output->emplace(device::BluetoothUUID(uuid_str_to_data.first),
                          uuid_str_to_data.second);

      if (!(entry->first.IsValid() &&
            entry->first.format() == device::BluetoothUUID::kFormat128Bit &&
            inserted)) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace mojo

#endif  // DEVICE_BLUETOOTH_PUBLIC_MOJOM_TEST_FAKE_BLUETOOTH_MOJOM_TRAITS_H_
