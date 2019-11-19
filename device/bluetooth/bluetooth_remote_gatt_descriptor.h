// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothRemoteGattCharacteristic;

// BluetoothRemoteGattDescriptor represents a remote GATT characteristic
// descriptor.
//
// Note: We use virtual inheritance on the GATT descriptor since it will be
// inherited by platform specific versions of the GATT descriptor classes also.
// The platform specific remote GATT descriptor classes will inherit both this
// class and their GATT descriptor class, hence causing an inheritance diamond.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptor
    : public virtual BluetoothGattDescriptor {
 public:
  ~BluetoothRemoteGattDescriptor() override;

  // The ValueCallback is used to return the value of a remote characteristic
  // descriptor upon a read request.
  using ValueCallback = base::OnceCallback<void(const std::vector<uint8_t>&)>;

  // Returns the value of the descriptor. For remote descriptors, this is the
  // most recently cached value of the remote descriptor. For local descriptors
  // this is the most recently updated value or the value retrieved from the
  // delegate.
  virtual const std::vector<uint8_t>& GetValue() const = 0;

  // Returns a pointer to the GATT characteristic that this characteristic
  // descriptor belongs to.
  virtual BluetoothRemoteGattCharacteristic* GetCharacteristic() const = 0;

  // Sends a read request to a remote characteristic descriptor to read its
  // value. |callback| is called to return the read value on success and
  // |error_callback| is called for failures.
  virtual void ReadRemoteDescriptor(ValueCallback callback,
                                    ErrorCallback error_callback) = 0;

  // Sends a write request to a remote characteristic descriptor, to modify the
  // value of the descriptor with the new value |new_value|. |callback| is
  // called to signal success and |error_callback| for failures. This method
  // only applies to remote descriptors and will fail for those that are locally
  // hosted.
  virtual void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) = 0;

 protected:
  BluetoothRemoteGattDescriptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptor);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_H_
