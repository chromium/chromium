// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBDESCRIPTOR_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBDESCRIPTOR_MAC_H_

#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

// This class mocks the behavior of a CBDescriptor.
@interface MockCBDescriptor : NSObject

@property(readonly, nonatomic) CBUUID* UUID;
@property(readonly, nonatomic) CBDescriptor* descriptor;

- (instancetype)initWithCharacteristic:(CBCharacteristic*)characteristic
                                CBUUID:(CBUUID*)uuid;

- (void)simulateReadWithValue:(id)value error:(NSError*)error;
- (void)simulateWriteWithError:(NSError*)error;
- (void)simulateUpdateWithError:(NSError*)error;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBDESCRIPTOR_MAC_H_
