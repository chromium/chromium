// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_UTILS_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_UTILS_H_

#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/common/api/bluetooth.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif

namespace extensions {
namespace api {
namespace bluetooth {

// Fill in a Device object from a BluetoothDevice.
void BluetoothDeviceToApiDevice(
    const device::BluetoothDevice& device,
    Device* out);

// Fill in an AdapterState object from a BluetoothAdapter.
void PopulateAdapterState(const device::BluetoothAdapter& adapter,
                          AdapterState* out);

#if BUILDFLAG(IS_CHROMEOS)
device::BluetoothFilterType ToBluetoothDeviceFilterType(FilterType type);
#endif

}  // namespace bluetooth
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_UTILS_H_
