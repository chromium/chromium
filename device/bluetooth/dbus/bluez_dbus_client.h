// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_CLIENT_H_

namespace dbus {
class Bus;
}  // namespace dbus

namespace bluez {

// Interface for all Bluez DBus clients handled by BluezDBusManager. It
// restricts
// access to the Init function to BluezDBusManager only to prevent
// incorrect calls. Stub clients may lift that restriction however.
class BluezDBusClient {
 public:
  BluezDBusClient& operator=(const BluezDBusClient&) = delete;

 protected:
  virtual ~BluezDBusClient() {}

  // This function is called by DBusThreadManager. Only in unit tests, which
  // don't use DBusThreadManager, this function can be called through Stub
  // implementations (they change Init's member visibility to public).
  virtual void Init(dbus::Bus* bus,
                    const std::string& bluetooth_service_name) = 0;

 private:
  friend class BluezDBusManager;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_CLIENT_H_
