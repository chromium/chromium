// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBCHARACTERISTIC_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBCHARACTERISTIC_MAC_H_

#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

// This class mocks the behavior of a CBCharacteristic.
@interface MockCBCharacteristic : NSObject

@property(nonatomic, readonly) CBUUID* UUID;
@property(nonatomic, readonly) CBCharacteristic* characteristic;
@property(nonatomic, readonly) NSArray* descriptors;

- (instancetype)initWithService:(CBService*)service
                         CBUUID:(CBUUID*)uuid
                     properties:(int)properties;

// Methods for faking events.
- (void)simulateReadWithValue:(id)value error:(NSError*)error;
- (void)simulateWriteWithError:(NSError*)error;
- (void)simulateGattNotifySessionStarted;
- (void)simulateGattNotifySessionFailedWithError:(NSError*)error;
- (void)simulateGattNotifySessionStopped;
- (void)simulateGattNotifySessionStoppedWithError:(NSError*)error;
- (void)simulateGattCharacteristicChangedWithValue:(NSData*)value;
- (void)addDescriptorWithUUID:(CBUUID*)uuid;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBCHARACTERISTIC_MAC_H_
