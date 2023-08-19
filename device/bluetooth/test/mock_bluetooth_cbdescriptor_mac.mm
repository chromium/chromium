// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbdescriptor_mac.h"

#include "base/apple/foundation_util.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

using base::apple::ObjCCast;

@interface MockCBDescriptor () {
  // Owner of this instance.
  CBCharacteristic* _characteristic;
  CBUUID* __strong _UUID;
  NSData* __strong _value;
}
@end

@implementation MockCBDescriptor

- (instancetype)initWithCharacteristic:(CBCharacteristic*)characteristic
                                CBUUID:(CBUUID*)uuid {
  self = [super init];
  if (self) {
    _characteristic = characteristic;
    _UUID = uuid;
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBDescriptor class] ||
      [aClass isSubclassOfClass:[CBDescriptor class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBDescriptor class] ||
      [aClass isSubclassOfClass:[CBDescriptor class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (CBUUID*)UUID {
  return _UUID;
}

- (NSData*)value {
  return _value;
}

- (CBDescriptor*)descriptor {
  return ObjCCast<CBDescriptor>(self);
}

- (CBCharacteristic*)characteristic {
  return _characteristic;
}

- (void)simulateReadWithValue:(id)value error:(NSError*)error {
  _value = [value copy];
  CBPeripheral* peripheral = _characteristic.service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateValueForDescriptor:self.descriptor
                            error:error];
}

- (void)simulateWriteWithError:(NSError*)error {
  CBPeripheral* peripheral = _characteristic.service.peripheral;
  [peripheral.delegate peripheral:peripheral
       didWriteValueForDescriptor:self.descriptor
                            error:error];
}

- (void)simulateUpdateWithError:(NSError*)error {
  CBPeripheral* peripheral = _characteristic.service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateValueForDescriptor:self.descriptor
                            error:error];
}

@end
