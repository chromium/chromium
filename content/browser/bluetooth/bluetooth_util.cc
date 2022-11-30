// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_util.h"

namespace content {

bool AreScanFiltersSame(
    const blink::mojom::WebBluetoothLeScanFilter& filter_1,
    const blink::mojom::WebBluetoothLeScanFilter& filter_2) {
  if (filter_1.name.has_value() != filter_2.name.has_value())
    return false;

  if (filter_1.name.has_value() &&
      filter_1.name.value() != filter_2.name.value()) {
    return false;
  }

  if (filter_1.name_prefix.has_value() != filter_2.name_prefix.has_value())
    return false;

  if (filter_1.name_prefix.has_value() &&
      filter_1.name_prefix.value() != filter_2.name_prefix.value())
    return false;

  if (filter_1.services.has_value() != filter_2.services.has_value())
    return false;

  if (filter_1.services.has_value()) {
    std::vector<device::BluetoothUUID> services_1 = filter_1.services.value();
    std::vector<device::BluetoothUUID> services_2 = filter_2.services.value();
    if (services_1.size() != services_2.size())
      return false;

    std::sort(services_1.begin(), services_1.end());
    std::sort(services_2.begin(), services_2.end());
    if (services_1 != services_2)
      return false;
  }

  if (filter_1.manufacturer_data != filter_2.manufacturer_data)
    return false;

  return true;
}

bool MatchesBluetoothDataFilter(
    const std::vector<blink::mojom::WebBluetoothDataFilterPtr>& prefix,
    const device::BluetoothDevice::ManufacturerData& data) {
  if (prefix.size() > data.size())
    return false;
  // For each bit in mask, check the corresponding bit in device
  // manufacturer data is equal to the corresponding bit in expected data.
  size_t i = 0;
  for (const auto& byte : prefix) {
    if ((byte->mask & byte->data) != (byte->mask & data.at(i++))) {
      return false;
    }
  }
  return true;
}

}  // namespace content
