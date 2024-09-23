// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_central_manager_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

namespace device {

// This class exists to bridge between the Objective-C CBCentralManagerDelegate
// class and our BluetoothLowEnergyDiscoveryManagerMac and
// BluetoothLowEnergyAdapterApple classes.
class BluetoothLowEnergyCentralManagerBridge {
 public:
  BluetoothLowEnergyCentralManagerBridge(
      BluetoothLowEnergyDiscoveryManagerMac* discovery_manager,
      BluetoothLowEnergyAdapterApple* adapter)
      : discovery_manager_(discovery_manager), adapter_(adapter) {}

  ~BluetoothLowEnergyCentralManagerBridge() = default;

  void DiscoveredPeripheral(CBPeripheral* peripheral,
                            NSDictionary* advertisementData,
                            int rssi) {
    discovery_manager_->DiscoveredPeripheral(peripheral, advertisementData,
                                             rssi);
  }

  void UpdatedState(bool powered) {
#if BUILDFLAG(IS_IOS)
    // On Mac, the Bluetooth classic code notifies the power changed.
    adapter_->NotifyAdapterPoweredChanged(powered);
#endif
    discovery_manager_->TryStartDiscovery();
    adapter_->LowEnergyCentralManagerUpdatedState();
  }

  void DidConnectPeripheral(CBPeripheral* peripheral) {
    adapter_->DidConnectPeripheral(peripheral);
  }

  void DidFailToConnectPeripheral(CBPeripheral* peripheral, NSError* error) {
    adapter_->DidFailToConnectPeripheral(peripheral, error);
  }

  void DidDisconnectPeripheral(CBPeripheral* peripheral, NSError* error) {
    adapter_->DidDisconnectPeripheral(peripheral, error);
  }

  CBCentralManager* GetCentralManager() {
    return adapter_->low_energy_central_manager_;
  }

 private:
  // TODO(https://crbug.com/330009945): Fix those dangling dangling pointers.
  // They are dangling on mac_chromium_10.15_rel_ng during
  // ChromeDriverSecureContextTest.testRemoveAllCredentials test.
  raw_ptr<BluetoothLowEnergyDiscoveryManagerMac, AcrossTasksDanglingUntriaged>
      discovery_manager_;
  raw_ptr<BluetoothLowEnergyAdapterApple, AcrossTasksDanglingUntriaged>
      adapter_;
};

}  // namespace device

@implementation BluetoothLowEnergyCentralManagerDelegate

- (instancetype)
    initWithDiscoveryManager:
        (device::BluetoothLowEnergyDiscoveryManagerMac*)discovery_manager
                  andAdapter:(device::BluetoothLowEnergyAdapterApple*)adapter {
  if ((self = [super init])) {
    _bridge = std::make_unique<device::BluetoothLowEnergyCentralManagerBridge>(
        discovery_manager, adapter);
  }
  return self;
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary*)advertisementData
                  RSSI:(NSNumber*)RSSI {
  // Notifies the discovery of a device.
  _bridge->DiscoveredPeripheral(peripheral, advertisementData, [RSSI intValue]);
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  // Notifies when the powered state of the central manager changed.
  _bridge->UpdatedState(central.state == CBManagerStatePoweredOn);
}

- (void)centralManager:(CBCentralManager*)central
    didConnectPeripheral:(CBPeripheral*)peripheral {
  _bridge->DidConnectPeripheral(peripheral);
}

- (void)centralManager:(CBCentralManager*)central
    didFailToConnectPeripheral:(CBPeripheral*)peripheral
                         error:(nullable NSError*)error {
  _bridge->DidFailToConnectPeripheral(peripheral, error);
}

- (void)centralManager:(CBCentralManager*)central
    didDisconnectPeripheral:(CBPeripheral*)peripheral
                      error:(nullable NSError*)error {
  _bridge->DidDisconnectPeripheral(peripheral, error);
}

@end
