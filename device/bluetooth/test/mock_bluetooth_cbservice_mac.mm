// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbservice_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_cbcharacteristic_mac.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

@interface MockCBService () {
  // Owner of this instance.
  CBPeripheral* _peripheral;
  scoped_nsobject<CBUUID> _UUID;
  BOOL _primary;
  scoped_nsobject<NSMutableArray> _characteristics;
}

@end

@implementation MockCBService

@synthesize isPrimary = _primary;

- (instancetype)initWithPeripheral:(CBPeripheral*)peripheral
                            CBUUID:(CBUUID*)uuid
                           primary:(BOOL)isPrimary {
  self = [super init];
  if (self) {
    _UUID.reset([uuid retain]);
    _primary = isPrimary;
    _peripheral = peripheral;
    _characteristics.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBService class] ||
      [aClass isSubclassOfClass:[CBService class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBService class] ||
      [aClass isSubclassOfClass:[CBService class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (CBPeripheral*)peripheral {
  return _peripheral;
}

- (CBUUID*)UUID {
  return _UUID;
}

- (void)addCharacteristicWithUUID:(CBUUID*)cb_uuid properties:(int)properties {
  scoped_nsobject<MockCBCharacteristic> characteristic_mock(
      [[MockCBCharacteristic alloc] initWithService:self.service
                                             CBUUID:cb_uuid
                                         properties:properties]);
  [_characteristics addObject:characteristic_mock];
}

- (void)removeCharacteristicMock:(MockCBCharacteristic*)characteristic_mock {
  [_characteristics removeObject:characteristic_mock];
}

- (CBService*)service {
  return ObjCCast<CBService>(self);
}

- (NSArray*)characteristics {
  return _characteristics;
}

@end
