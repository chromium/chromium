// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_MANAGER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_MANAGER_MAC_H_

#include "base/memory/raw_ptr.h"

@class IOBluetoothDevice;

namespace device {

// Class used by BluetoothAdapterMac to manage classic and LE device discovery.
// For Bluetooth Classic, this class is responsible for keeping device inquiry
// running if device discovery is initiated.
class BluetoothDiscoveryManagerMac {
 public:
  // Interface for being notified of events during a device discovery session.
  class Observer {
   public:
    // Called when |this| manager has found a device through classic device
    // inquiry in the form of an IOBluetoothDevice.
    virtual void ClassicDeviceFound(IOBluetoothDevice* device) = 0;

    // Called when device discovery is no longer running, due to either a call
    // to BluetoothDiscoveryManagerMac::StopDiscovery or an unexpected reason,
    // such as when a user disables the controller, in which case the value of
    // |unexpected| will be true.
    virtual void ClassicDiscoveryStopped(bool unexpected) = 0;

   protected:
    virtual ~Observer() = default;
  };

  BluetoothDiscoveryManagerMac(const BluetoothDiscoveryManagerMac&) = delete;
  BluetoothDiscoveryManagerMac& operator=(const BluetoothDiscoveryManagerMac&) =
      delete;

  virtual ~BluetoothDiscoveryManagerMac();

  // Returns true, if discovery is currently being performed.
  virtual bool IsDiscovering() const = 0;

  // Initiates a discovery session. Returns true on success or if discovery
  // is already running. Returns false on failure.
  virtual bool StartDiscovery() = 0;

  // Stops a discovery session. Returns true on success or if discovery is
  // already not running. Returns false on failure.
  virtual bool StopDiscovery() = 0;

  // Creates a discovery manager for Bluetooth Classic device discovery with
  // observer |observer|. Note that the life-time of |observer| should not
  // end before that of the returned BluetoothDiscoveryManager, as that may
  // lead to use after free errors.
  static BluetoothDiscoveryManagerMac* CreateClassic(Observer* observer);

 protected:
  explicit BluetoothDiscoveryManagerMac(Observer* observer);

  // Observer interested in notifications from us.
  raw_ptr<Observer> observer_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_MANAGER_MAC_H_
