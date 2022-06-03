// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBSERVICE_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBSERVICE_MAC_H_

#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

@class MockCBCharacteristic;

// This class mocks the behavior of a CBService.
@interface MockCBService : NSObject

@property(readonly, nonatomic) CBUUID* UUID;
@property(readonly, nonatomic) BOOL isPrimary;
@property(readonly, nonatomic) CBService* service;

- (instancetype)initWithPeripheral:(CBPeripheral*)peripheral
                            CBUUID:(CBUUID*)uuid
                           primary:(BOOL)isPrimary;

// Creates and adds a mock characteristic.
- (void)addCharacteristicWithUUID:(CBUUID*)cb_uuid properties:(int)properties;
- (void)removeCharacteristicMock:(MockCBCharacteristic*)characteristic_mock;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBSERVICE_MAC_H_
