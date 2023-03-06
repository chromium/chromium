// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_CENTRAL_MANAGER_DELEGATE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_CENTRAL_MANAGER_DELEGATE_H_

#import <CoreBluetooth/CoreBluetooth.h>

#include <memory>

#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif

namespace device {

class BluetoothLowEnergyAdapterApple;
class BluetoothLowEnergyCentralManagerBridge;
class BluetoothLowEnergyDiscoveryManagerMac;

}  // namespace device

// This class will serve as the Objective-C delegate of CBCentralManager.
@interface BluetoothLowEnergyCentralManagerDelegate
    : NSObject<CBCentralManagerDelegate> {
  std::unique_ptr<device::BluetoothLowEnergyCentralManagerBridge> _bridge;
}

- (instancetype)
    initWithDiscoveryManager:
        (device::BluetoothLowEnergyDiscoveryManagerMac*)discovery_manager
                  andAdapter:(device::BluetoothLowEnergyAdapterApple*)adapter;

@end

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_CENTRAL_MANAGER_DELEGATE_H_
