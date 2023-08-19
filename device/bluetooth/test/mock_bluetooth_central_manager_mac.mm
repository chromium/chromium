// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"

#import "base/apple/foundation_util.h"
#import "device/bluetooth/test/bluetooth_test_mac.h"
#import "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"

@implementation MockCentralManager {
  NSMutableDictionary* __strong _connectedMockPeripheralPerServiceUUID;
  NSMutableArray* __strong _retrieveConnectedPeripheralServiceUUIDs;
}

@synthesize scanForPeripheralsCallCount = _scanForPeripheralsCallCount;
@synthesize stopScanCallCount = _stopScanCallCount;
@synthesize delegate = _delegate;
@synthesize state = _state;
@synthesize bluetoothTestMac = _bluetoothTestMac;

- (instancetype)init {
  self = [super init];
  if (self) {
    _connectedMockPeripheralPerServiceUUID = [[NSMutableDictionary alloc] init];
    _retrieveConnectedPeripheralServiceUUIDs = [[NSMutableArray alloc] init];
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBCentralManager class] ||
      [aClass isSubclassOfClass:[CBCentralManager class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBCentralManager class] ||
      [aClass isSubclassOfClass:[CBCentralManager class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (void)scanForPeripheralsWithServices:(NSArray*)serviceUUIDs
                               options:(NSDictionary*)options {
  _scanForPeripheralsCallCount++;
}

- (void)stopScan {
  _stopScanCallCount++;
}

- (void)connectPeripheral:(CBPeripheral*)peripheral
                  options:(NSDictionary*)options {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothDeviceConnectGattCalled();
  }
}

- (void)cancelPeripheralConnection:(CBPeripheral*)peripheral {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothGattDisconnect();
  }

  // When cancelPeripheralConnection is called macOS marks the device as
  // disconnected.
  MockCBPeripheral* mock_peripheral =
      base::apple::ObjCCastStrict<MockCBPeripheral>(peripheral);
  [mock_peripheral setState:CBPeripheralStateDisconnected];
}

- (NSArray*)retrieveConnectedPeripheralServiceUUIDs {
  return _retrieveConnectedPeripheralServiceUUIDs;
}

- (NSArray*)retrieveConnectedPeripheralsWithServices:(NSArray*)services {
  [_retrieveConnectedPeripheralServiceUUIDs
      addObjectsFromArray:[services copy]];
  NSMutableArray* connectedPeripherals = [[NSMutableArray alloc] init];
  for (CBUUID* uuid in services) {
    NSSet* peripheralSet =
        [_connectedMockPeripheralPerServiceUUID objectForKey:uuid];
    [connectedPeripherals addObjectsFromArray:peripheralSet.allObjects];
  }
  return connectedPeripherals;
}

- (void)setConnectedMockPeripheral:(CBPeripheral*)peripheral
                  withServiceUUIDs:(NSSet*)serviceUUIDs {
  for (CBUUID* uuid in serviceUUIDs) {
    NSMutableSet* peripheralSet =
        [_connectedMockPeripheralPerServiceUUID objectForKey:uuid];
    if (!peripheralSet) {
      peripheralSet = [NSMutableSet set];
      [_connectedMockPeripheralPerServiceUUID setObject:peripheralSet
                                                 forKey:uuid];
    }
    [peripheralSet addObject:peripheral];
  }
}

- (void)resetRetrieveConnectedPeripheralServiceUUIDs {
  [_retrieveConnectedPeripheralServiceUUIDs removeAllObjects];
}

@end
