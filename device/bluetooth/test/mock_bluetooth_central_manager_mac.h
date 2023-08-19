// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CENTRAL_MANAGER_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CENTRAL_MANAGER_MAC_H_

#import <CoreBluetooth/CoreBluetooth.h>

#include "build/build_config.h"
#import "device/bluetooth/bluetooth_adapter_mac.h"
#import "device/bluetooth/test/bluetooth_test_mac.h"

// Class to mock a CBCentralManager. Cannot use a OCMockObject because mocking
// the 'state' property gives a compiler warning when mock_central_manager is of
// type id (multiple methods named 'state' found), and a compiler warning when
// mock_central_manager is of type CBCentralManager (CBCentralManager may not
// respond to 'stub').
@interface MockCentralManager : NSObject

@property(nonatomic, assign) NSInteger scanForPeripheralsCallCount;
@property(nonatomic, assign) NSInteger stopScanCallCount;
@property(nonatomic, weak) id<CBCentralManagerDelegate> delegate;
@property(nonatomic, assign) CBManagerState state;
@property(nonatomic, assign) device::BluetoothTestMac* bluetoothTestMac;
@property(nonatomic, readonly) NSArray* retrieveConnectedPeripheralServiceUUIDs;

- (void)scanForPeripheralsWithServices:(NSArray*)serviceUUIDs
                               options:(NSDictionary*)options;

- (void)stopScan;

- (void)connectPeripheral:(CBPeripheral*)peripheral
                  options:(NSDictionary*)options;

// Simulates a peripheral being used by another application. This peripheral
// will be returned by -[MockCentralManager
// retrieveConnectedPeripheralsWithServices:].
- (void)setConnectedMockPeripheral:(CBPeripheral*)peripheral
                  withServiceUUIDs:(NSSet*)serviceUUIDs;

// Reset -[MockCentralManager retrieveConnectedPeripheralServiceUUIDs].
- (void)resetRetrieveConnectedPeripheralServiceUUIDs;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CENTRAL_MANAGER_MAC_H_
