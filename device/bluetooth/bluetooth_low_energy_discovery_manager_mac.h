// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DISCOVERY_MANAGER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DISCOVERY_MANAGER_MAC_H_

#import <CoreBluetooth/CoreBluetooth.h>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_device.h"

#if !BUILDFLAG(IS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif

namespace device {

// This class will scan for Bluetooth LE device on Mac.
class BluetoothLowEnergyDiscoveryManagerMac {
 public:
  // Interface for being notified of events during a device discovery session.
  class Observer {
   public:
    // Called when |this| manager has found a device or an update on a device.
    virtual void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                                        NSDictionary* advertisementData,
                                        int rssi) = 0;

   protected:
    virtual ~Observer() = default;
  };

  BluetoothLowEnergyDiscoveryManagerMac(
      const BluetoothLowEnergyDiscoveryManagerMac&) = delete;
  BluetoothLowEnergyDiscoveryManagerMac& operator=(
      const BluetoothLowEnergyDiscoveryManagerMac&) = delete;

  virtual ~BluetoothLowEnergyDiscoveryManagerMac();

  // Returns true, if discovery is currently being performed.
  virtual bool IsDiscovering() const;

  // Initiates a discovery session.
  // BluetoothLowEnergyDeviceMac objects discovered within a previous
  // discovery session will be invalid.
  virtual void StartDiscovery(BluetoothDevice::UUIDList services_uuids);

  // Stops a discovery session.
  virtual void StopDiscovery();

  // Returns a new BluetoothLowEnergyDiscoveryManagerMac.
  static BluetoothLowEnergyDiscoveryManagerMac* Create(Observer* observer);

  virtual void SetCentralManager(CBCentralManager* central_manager);

 protected:
  // Called when a discovery or an update of a BLE device occurred.
  virtual void DiscoveredPeripheral(CBPeripheral* peripheral,
                                    NSDictionary* advertisementData,
                                    int rssi);

  // The device discovery can really be started when Bluetooth is powered on.
  // The method TryStartDiscovery() is called when it's a good time to try to
  // start the BLE device discovery. It will check if the discovery session has
  // been started and if the Bluetooth is powered and then really start the
  // CoreBluetooth BLE device discovery.
  virtual void TryStartDiscovery();

 private:
  explicit BluetoothLowEnergyDiscoveryManagerMac(Observer* observer);

  friend class BluetoothAdapterMacTest;
  friend class BluetoothLowEnergyCentralManagerBridge;

  // Observer interested in notifications from us.
  raw_ptr<Observer> observer_;

  // Underlying CoreBluetooth central manager, owned by |observer_|.
  CBCentralManager* central_manager_ = nil;

  // Discovery has been initiated by calling the API StartDiscovery().
  bool discovering_;

  // A discovery has been initiated but has not started yet because it's
  // waiting for Bluetooth to turn on.
  bool pending_;

  // List of service UUIDs to scan.
  BluetoothDevice::UUIDList services_uuids_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DISCOVERY_MANAGER_MAC_H_
