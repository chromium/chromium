// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBPERIPHERAL_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBPERIPHERAL_MAC_H_

#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

namespace device {

class BluetoothTestMac;
}

@class MockCBCharacteristic;

// This class mocks the behavior of a CBPeripheral.
@interface MockCBPeripheral : NSObject

@property(nonatomic, readonly) CBPeripheralState state;
@property(nonatomic, strong, readonly) NSUUID* identifier;
@property(nonatomic, readonly) NSString* name;
@property(nonatomic, weak) id<CBPeripheralDelegate> delegate;
@property(nonatomic, readonly) CBPeripheral* peripheral;
@property(strong, readonly) NSArray* services;
@property(nonatomic, assign) device::BluetoothTestMac* bluetoothTestMac;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithUTF8StringIdentifier:(const char*)identifier;
- (instancetype)initWithUTF8StringIdentifier:(const char*)identifier
                                        name:(NSString*)name;
- (instancetype)initWithIdentifier:(NSUUID*)identifier
                              name:(NSString*)name NS_DESIGNATED_INITIALIZER;

// Methods for faking events.
- (void)setState:(CBPeripheralState)state;
- (void)removeAllServices;
- (void)addServices:(NSArray*)services;
- (void)mockDidDiscoverServicesWithError:(NSError*)error;
- (void)removeService:(CBService*)uuid;
- (void)mockDidDiscoverServices;
- (void)mockDidDiscoverCharacteristicsForService:(CBService*)service
                                       WithError:(NSError*)error;
- (void)mockDidDiscoverCharacteristicsForService:(CBService*)service;
- (void)mockDidDiscoverDescriptorsForCharacteristic:
    (CBCharacteristic*)characteristic;
- (void)mockDidDiscoverDescriptorsForCharacteristic:
            (CBCharacteristic*)characteristic
                                          WithError:(NSError*)error;
- (void)mockDidDiscoverEvents;
- (void)didModifyServices:(NSArray*)invalidatedServices;
- (void)didDiscoverDescriptorsWithCharacteristic:
    (MockCBCharacteristic*)characteristic_mock;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBPERIPHERAL_MAC_H_
