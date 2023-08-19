// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_peripheral_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

namespace device {

// This class exists to bridge between the Objective-C CBPeripheralDelegate
// class and our BluetoothLowEnergyDiscoveryManagerMac and BluetoothAdapterMac
// classes.
class BluetoothLowEnergyPeripheralBridge {
 public:
  BluetoothLowEnergyPeripheralBridge(BluetoothLowEnergyDeviceMac* device_mac)
      : device_mac_(device_mac) {}

  ~BluetoothLowEnergyPeripheralBridge() = default;

  void DidModifyServices(NSArray* invalidatedServices) {
    device_mac_->DidModifyServices(invalidatedServices);
  }

  void DidDiscoverPrimaryServices(NSError* error) {
    device_mac_->DidDiscoverPrimaryServices(error);
  }

  void DidDiscoverCharacteristics(CBService* service, NSError* error) {
    device_mac_->DidDiscoverCharacteristics(service, error);
  }

  void DidUpdateValue(CBCharacteristic* characteristic, NSError* error) {
    device_mac_->DidUpdateValue(characteristic, error);
  }

  void DidWriteValue(CBCharacteristic* characteristic, NSError* error) {
    device_mac_->DidWriteValue(characteristic, error);
  }

  void DidUpdateNotificationState(CBCharacteristic* characteristic,
                                  NSError* error) {
    device_mac_->DidUpdateNotificationState(characteristic, error);
  }

  void DidDiscoverDescriptors(CBCharacteristic* characteristic,
                              NSError* error) {
    device_mac_->DidDiscoverDescriptors(characteristic, error);
  }

  void DidUpdateValueForDescriptor(CBDescriptor* descriptor, NSError* error) {
    device_mac_->DidUpdateValueForDescriptor(descriptor, error);
  }

  void DidWriteValueForDescriptor(CBDescriptor* descriptor, NSError* error) {
    device_mac_->DidWriteValueForDescriptor(descriptor, error);
  }

  CBPeripheral* GetPeripheral() { return device_mac_->GetPeripheral(); }

 private:
  raw_ptr<BluetoothLowEnergyDeviceMac> device_mac_;
};

}  // namespace device

@implementation BluetoothLowEnergyPeripheralDelegate

- (instancetype)initWithBluetoothLowEnergyDeviceMac:
    (device::BluetoothLowEnergyDeviceMac*)device_mac {
  if ((self = [super init])) {
    _bridge = std::make_unique<device::BluetoothLowEnergyPeripheralBridge>(
        device_mac);
  }
  return self;
}

- (void)peripheral:(CBPeripheral*)peripheral
    didModifyServices:(NSArray*)invalidatedServices {
  _bridge->DidModifyServices(invalidatedServices);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didDiscoverServices:(NSError*)error {
  _bridge->DidDiscoverPrimaryServices(error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didDiscoverCharacteristicsForService:(CBService*)service
                                   error:(NSError*)error {
  _bridge->DidDiscoverCharacteristics(service, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
                              error:(NSError*)error {
  _bridge->DidUpdateValue(characteristic, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didWriteValueForCharacteristic:(nonnull CBCharacteristic*)characteristic
                             error:(nullable NSError*)error {
  _bridge->DidWriteValue(characteristic, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didUpdateNotificationStateForCharacteristic:
        (nonnull CBCharacteristic*)characteristic
                                          error:(nullable NSError*)error {
  _bridge->DidUpdateNotificationState(characteristic, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didDiscoverDescriptorsForCharacteristic:(CBCharacteristic*)characteristic
                                      error:(nullable NSError*)error {
  _bridge->DidDiscoverDescriptors(characteristic, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didUpdateValueForDescriptor:(CBDescriptor*)descriptor
                          error:(nullable NSError*)error {
  _bridge->DidUpdateValueForDescriptor(descriptor, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didWriteValueForDescriptor:(CBDescriptor*)descriptor
                         error:(nullable NSError*)error {
  _bridge->DidWriteValueForDescriptor(descriptor, error);
}

@end
