// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbcharacteristic_mac.h"

#include "base/apple/foundation_util.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_cbdescriptor_mac.h"

using base::apple::ObjCCast;

namespace device {

namespace {

CBCharacteristicProperties AddCBCharacteristicProperties(
    CBCharacteristicProperties value1,
    CBCharacteristicProperties value2) {
  return static_cast<CBCharacteristicProperties>(value1 | value2);
}

CBCharacteristicProperties GattCharacteristicPropertyToCBCharacteristicProperty(
    BluetoothGattCharacteristic::Properties gatt_property) {
  CBCharacteristicProperties result =
      static_cast<CBCharacteristicProperties>(0);
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_BROADCAST) {
    result = AddCBCharacteristicProperties(result,
                                           CBCharacteristicPropertyBroadcast);
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_READ) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyRead);
  }
  if (gatt_property &
      BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE) {
    result = AddCBCharacteristicProperties(
        result, CBCharacteristicPropertyWriteWithoutResponse);
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_WRITE) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyWrite);
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_NOTIFY) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyNotify);
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_INDICATE) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyIndicate);
  }
  if (gatt_property &
      BluetoothGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES) {
    result = AddCBCharacteristicProperties(
        result, CBCharacteristicPropertyAuthenticatedSignedWrites);
  }
  if (gatt_property &
      BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES) {
    result = AddCBCharacteristicProperties(
        result, CBCharacteristicPropertyExtendedProperties);
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_RELIABLE_WRITE) {
  }
  if (gatt_property &
      BluetoothGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES) {
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyRead);
  }
  if (gatt_property & BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyWrite);
  }
  if (gatt_property &
      BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED_AUTHENTICATED) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyRead);
  }
  if (gatt_property &
      BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED) {
    result =
        AddCBCharacteristicProperties(result, CBCharacteristicPropertyWrite);
  }
  return result;
}
}  // namespace
}  // device

@interface MockCBCharacteristic () {
  // Owner of this instance.
  CBService* _service;
  CBUUID* __strong _UUID;
  CBCharacteristicProperties _cb_properties;
  NSMutableArray* __strong _descriptors;
  NSObject* __strong _value;
  BOOL _notifying;
}
@end

@implementation MockCBCharacteristic

- (instancetype)initWithService:(CBService*)service
                         CBUUID:(CBUUID*)uuid
                     properties:(int)properties {
  self = [super init];
  if (self) {
    _service = service;
    _UUID = uuid;
    _cb_properties =
        device::GattCharacteristicPropertyToCBCharacteristicProperty(
            properties);
    _descriptors = [[NSMutableArray alloc] init];
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBCharacteristic class] ||
      [aClass isSubclassOfClass:[CBCharacteristic class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBCharacteristic class] ||
      [aClass isSubclassOfClass:[CBCharacteristic class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (void)simulateReadWithValue:(id)value error:(NSError*)error {
  _value = [value copy];
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateValueForCharacteristic:self.characteristic
                                error:error];
}

- (void)simulateWriteWithError:(NSError*)error {
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didWriteValueForCharacteristic:self.characteristic
                               error:error];
}

- (void)simulateGattNotifySessionStarted {
  _notifying = YES;
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateNotificationStateForCharacteristic:self.characteristic
                                            error:nil];
}

- (void)simulateGattNotifySessionFailedWithError:(NSError*)error {
  _notifying = NO;
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateNotificationStateForCharacteristic:self.characteristic
                                            error:error];
}

- (void)simulateGattNotifySessionStopped {
  _notifying = NO;
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateNotificationStateForCharacteristic:self.characteristic
                                            error:nil];
}

- (void)simulateGattNotifySessionStoppedWithError:(NSError*)error {
  _notifying = NO;
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateNotificationStateForCharacteristic:self.characteristic
                                            error:error];
}

- (void)simulateGattCharacteristicChangedWithValue:(NSData*)value {
  _value = [value copy];
  CBPeripheral* peripheral = _service.peripheral;
  [peripheral.delegate peripheral:peripheral
      didUpdateValueForCharacteristic:self.characteristic
                                error:nil];
}

- (void)addDescriptorWithUUID:(CBUUID*)uuid {
  MockCBDescriptor* descriptor_mock =
      [[MockCBDescriptor alloc] initWithCharacteristic:self.characteristic
                                                CBUUID:uuid];
  [_descriptors addObject:descriptor_mock];
}

- (CBUUID*)UUID {
  return _UUID;
}

- (CBCharacteristic*)characteristic {
  return ObjCCast<CBCharacteristic>(self);
}

- (CBService*)service {
  return _service;
}

- (CBCharacteristicProperties)properties {
  return _cb_properties;
}

- (NSArray*)descriptors {
  return _descriptors;
}

- (id)value {
  return _value;
}

- (BOOL)isNotifying {
  return _notifying;
}

@end
