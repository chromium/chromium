// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_peripheral_manager_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_low_energy_advertisement_manager_mac.h"

namespace device {

// This class exists to bridge between the Objective-C
// CBPeripheralManagerDelegate class and our BluetoothLowEnergyAdapterApple
// classes.
class BluetoothLowEnergyPeripheralManagerBridge {
 public:
  BluetoothLowEnergyPeripheralManagerBridge(
      BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager)
      : advertisement_manager_(advertisement_manager) {}

  ~BluetoothLowEnergyPeripheralManagerBridge() = default;

  void UpdatedState() {
    advertisement_manager_->OnPeripheralManagerStateChanged();
  }

  void DidStartAdvertising(NSError* error) {
    advertisement_manager_->DidStartAdvertising(error);
  }

 private:
  // TODO(https://crbug.com/330009945): Fix this dangling dangling pointer.
  // It is dangling on mac_chromium_10.15_rel_ng during
  // ChromeDriverSecureContextTest.testRemoveAllCredentials test.
  raw_ptr<BluetoothLowEnergyAdvertisementManagerMac,
          AcrossTasksDanglingUntriaged>
      advertisement_manager_;
};

}  // namespace device

// Delegate for CBPeripheralManager, which forwards CoreBluetooth callbacks to
// their appropriate handler.
@implementation BluetoothLowEnergyPeripheralManagerDelegate {
  std::unique_ptr<device::BluetoothLowEnergyPeripheralManagerBridge> _bridge;
}

- (instancetype)initWithAdvertisementManager:
    (device::BluetoothLowEnergyAdvertisementManagerMac*)advertisementManager {
  if ((self = [super init])) {
    _bridge =
        std::make_unique<device::BluetoothLowEnergyPeripheralManagerBridge>(
            advertisementManager);
  }
  return self;
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager*)peripheral {
  _bridge->UpdatedState();
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager*)peripheral
                                       error:(NSError*)error {
  _bridge->DidStartAdvertising(error);
}

@end
