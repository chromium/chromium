// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothAdapter;
class BluetoothDevice;

// BluetoothGattConnection represents a GATT connection to a Bluetooth device
// that has GATT services. Instances are obtained from a BluetoothDevice,
// and the connection is kept alive as long as there is at least one
// active BluetoothGattConnection object. BluetoothGattConnection objects
// automatically update themselves, when the connection is terminated by the
// operating system (e.g. due to user action).
class DEVICE_BLUETOOTH_EXPORT BluetoothGattConnection {
 public:
  BluetoothGattConnection(scoped_refptr<device::BluetoothAdapter> adapter,
                          const std::string& device_address);

  BluetoothGattConnection(const BluetoothGattConnection&) = delete;
  BluetoothGattConnection& operator=(const BluetoothGattConnection&) = delete;

  // Destructor automatically closes this GATT connection. If this is the last
  // remaining GATT connection and this results in a call to the OS, that call
  // may not always succeed. Users can make an explicit call to
  // BluetoothGattConnection::Close to make sure that they are notified of
  // a possible error via the callback.
  virtual ~BluetoothGattConnection();

  // Returns the Bluetooth address of the device that this connection is open
  // to.
  const std::string& GetDeviceAddress() const;

  // Returns true if this GATT connection is open.
  virtual bool IsConnected();

  // Disconnects this GATT connection. The device may still remain connected due
  // to other GATT connections. When all BluetoothGattConnection objects are
  // disconnected the BluetoothDevice object will disconnect GATT.
  virtual void Disconnect();

 protected:
  friend BluetoothDevice;  // For InvalidateConnectionReference.

  // Sets this object to no longer have a reference maintaining the connection.
  // Only to be called by BluetoothDevice to avoid reentrant code to
  // RemoveGattConnection in that destructor after BluetoothDevice subclasses
  // have already been destroyed.
  void InvalidateConnectionReference();

  // The Bluetooth adapter that this connection is associated with. A reference
  // is held because BluetoothGattConnection keeps the connection alive.
  scoped_refptr<BluetoothAdapter> adapter_;

  // Bluetooth address of the underlying device.
  std::string device_address_;
  raw_ptr<BluetoothDevice, DanglingUntriaged> device_ = nullptr;

 private:
  bool owns_reference_for_connection_ = false;
};

}  // namespace device

#endif  //  DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_
