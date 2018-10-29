// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_peripheral_manager_delegate.h"

#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

// This class exists to bridge between the Objective-C
// CBPeripheralManagerDelegate class and our BluetoothAdapterMac classes.
class BluetoothLowEnergyPeripheralManagerBridge {
 public:
  BluetoothLowEnergyPeripheralManagerBridge(
      BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager,
      BluetoothAdapterMac* adapter)
      : advertisement_manager_(advertisement_manager), adapter_(adapter) {}

  ~BluetoothLowEnergyPeripheralManagerBridge() {}

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
  BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager_;
  BluetoothAdapterMac* adapter_;
};

}  // namespace device

// Delegate for CBPeripheralManager, which forwards CoreBluetooth callbacks to
// their appropriate handler.
@implementation BluetoothLowEnergyPeripheralManagerDelegate {
  std::unique_ptr<device::BluetoothLowEnergyPeripheralManagerBridge> bridge_;
}

- (id)initWithAdvertisementManager:
          (device::BluetoothLowEnergyAdvertisementManagerMac*)
              advertisementManager
                        andAdapter:(device::BluetoothAdapterMac*)adapter {
  if ((self = [super init])) {
    bridge_.reset(new device::BluetoothLowEnergyPeripheralManagerBridge(
        advertisementManager, adapter));
  }
  return self;
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager*)peripheral {
  bridge_->UpdatedState();
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager*)peripheral
                                       error:(NSError*)error {
  bridge_->DidStartAdvertising(error);
}

@end
