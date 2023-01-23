// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/id_type.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothRemoteGattCharacteristic;

// A BluetoothGattNotifySession represents an active session for listening
// to value updates from GATT characteristics that support notifications and/or
// indications. Instances are obtained by calling
// BluetoothRemoteGattCharacteristic::StartNotifySession.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattNotifySession {
 public:
  using Id = base::IdTypeU64<BluetoothGattNotifySession>;

  explicit BluetoothGattNotifySession(
      base::WeakPtr<BluetoothRemoteGattCharacteristic> characteristic);

  BluetoothGattNotifySession(const BluetoothGattNotifySession&) = delete;
  BluetoothGattNotifySession& operator=(const BluetoothGattNotifySession&) =
      delete;

  Id unique_id() { return unique_id_; }

  // Destructor automatically stops this session.
  virtual ~BluetoothGattNotifySession();

  // Returns the identifier of the associated characteristic.
  virtual std::string GetCharacteristicIdentifier() const;

  // Returns the associated characteristic. This function will return nullptr
  // if the associated characteristic is deleted.
  virtual BluetoothRemoteGattCharacteristic* GetCharacteristic() const;

  // Returns true if this session is active. Notify sessions are active from
  // the time of creation, until they have been stopped, either explicitly, or
  // because the remote device disconnects.
  virtual bool IsActive();

  // Stops this session and calls |callback| upon completion. This won't
  // necessarily stop value updates from the characteristic -- since updates
  // are shared among BluetoothGattNotifySession instances -- but it will
  // terminate this session.
  virtual void Stop(base::OnceClosure callback);

 private:
  static Id GetNextId();

  // The associated characteristic.
  base::WeakPtr<BluetoothRemoteGattCharacteristic> characteristic_;
  std::string characteristic_id_;
  bool active_;
  const Id unique_id_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_H_
