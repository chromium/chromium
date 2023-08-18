// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbservice_mac.h"

#include "base/apple/foundation_util.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_cbcharacteristic_mac.h"

using base::apple::ObjCCast;

@interface MockCBService () {
  // Owner of this instance.
  CBPeripheral* __weak _peripheral;
  CBUUID* __strong _UUID;
  BOOL _primary;
  NSMutableArray* __strong _characteristics;
}

@end

@implementation MockCBService

@synthesize isPrimary = _primary;

- (instancetype)initWithPeripheral:(CBPeripheral*)peripheral
                            CBUUID:(CBUUID*)uuid
                           primary:(BOOL)isPrimary {
  self = [super init];
  if (self) {
    _UUID = uuid;
    _primary = isPrimary;
    _peripheral = peripheral;
    _characteristics = [[NSMutableArray alloc] init];
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
  MockCBCharacteristic* characteristicMock =
      [[MockCBCharacteristic alloc] initWithService:self.service
                                             CBUUID:cb_uuid
                                         properties:properties];
  [_characteristics addObject:characteristicMock];
}

- (void)removeCharacteristicMock:(MockCBCharacteristic*)characteristicMock {
  [_characteristics removeObject:characteristicMock];
}

- (CBService*)service {
  return ObjCCast<CBService>(self);
}

- (NSArray*)characteristics {
  return _characteristics;
}

@end
