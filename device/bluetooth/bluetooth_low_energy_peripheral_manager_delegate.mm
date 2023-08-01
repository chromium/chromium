// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_peripheral_manager_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"

namespace device {

// This class exists to bridge between the Objective-C
// CBPeripheralManagerDelegate class and our BluetoothLowEnergyAdapterApple
// classes.
class BluetoothLowEnergyPeripheralManagerBridge {
 public:
  BluetoothLowEnergyPeripheralManagerBridge(
      BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager,
      BluetoothLowEnergyAdapterApple* adapter)
      : advertisement_manager_(advertisement_manager), adapter_(adapter) {}

  ~BluetoothLowEnergyPeripheralManagerBridge() = default;

  void UpdatedState() {
    advertisement_manager_->OnPeripheralManagerStateChanged();
  }

  void DidStartAdvertising(NSError* error) {
    advertisement_manager_->DidStartAdvertising(error);
  }

  CBPeripheralManager* GetPeripheralManager() {
    return adapter_->GetPeripheralManager();
  }

 private:
  raw_ptr<BluetoothLowEnergyAdvertisementManagerMac> advertisement_manager_;
  raw_ptr<BluetoothLowEnergyAdapterApple> adapter_;
};

}  // namespace device

// Delegate for CBPeripheralManager, which forwards CoreBluetooth callbacks to
// their appropriate handler.
@implementation BluetoothLowEnergyPeripheralManagerDelegate {
  std::unique_ptr<device::BluetoothLowEnergyPeripheralManagerBridge> _bridge;
}

- (instancetype)
    initWithAdvertisementManager:
        (device::BluetoothLowEnergyAdvertisementManagerMac*)advertisementManager
                      andAdapter:
                          (device::BluetoothLowEnergyAdapterApple*)adapter {
  if ((self = [super init])) {
    _bridge =
        std::make_unique<device::BluetoothLowEnergyPeripheralManagerBridge>(
            advertisementManager, adapter);
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
