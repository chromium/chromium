// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_CABLE_MOCK_BLUETOOTH_ADAPTER_H_
#define DEVICE_FIDO_CABLE_CABLE_MOCK_BLUETOOTH_ADAPTER_H_

#include <array>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/public/cable_discovery_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothDevice;

namespace cablev2 {

// Mock BLE adapter that abstracts out authenticator logic with the following
// logic:
//  - Responds to BluetoothAdapter::StartDiscoverySessionWithFilter() by
//    invoking BluetoothAdapter::Observer::DeviceAdded() on a test bluetooth
//    device that includes service data containing authenticator EID.
class CableMockBluetoothAdapter : public MockBluetoothAdapter {
 public:
  static scoped_refptr<CableMockBluetoothAdapter> MakePoweredOn();
  static scoped_refptr<CableMockBluetoothAdapter> MakePoweredOff();
  static scoped_refptr<CableMockBluetoothAdapter> MakeNotPresent();
  static scoped_refptr<CableMockBluetoothAdapter>
  MakeWithUndeterminedPermission();

  void ExpectDiscoveryWithScanCallback();

  void ExpectDiscoveryWithScanCallback(
      const std::array<uint8_t, kAdvertSize> v2_advert);

#if BUILDFLAG(IS_CHROMEOS)
  void ExpectLEScan(const std::array<uint8_t, kAdvertSize> v2_advert);
#endif  // BUILDFLAG(IS_CHROMEOS)

  void AddNewTestBluetoothDevice(
      base::span<const uint8_t, kAdvertSize> v2_advert);

 protected:
  CableMockBluetoothAdapter();
  ~CableMockBluetoothAdapter() override;

 private:
  BluetoothDevice* CreateNewTestBluetoothDevice(
      base::span<const uint8_t, kAdvertSize> v2_advert);
};

}  // namespace cablev2

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_CABLE_MOCK_BLUETOOTH_ADAPTER_H_
