// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_H_

#include <optional>

#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothDevice;

class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyScanSession {
 public:
  enum class ErrorCode {
    kFailed,
  };

  // Interface for reacting to BluetoothLowEnergyScanSession events.
  class Delegate {
   public:
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Notifies that a scanning session has started. If there is an |error_code|
    // the session failed to start.
    virtual void OnSessionStarted(
        BluetoothLowEnergyScanSession* scan_session,
        std::optional<BluetoothLowEnergyScanSession::ErrorCode> error_code) = 0;

    // Notifies that a device matching the filter criteria has been found.
    virtual void OnDeviceFound(BluetoothLowEnergyScanSession* scan_session,
                               BluetoothDevice* device) = 0;

    // Notifies that a previously found device has been lost.
    virtual void OnDeviceLost(BluetoothLowEnergyScanSession* scan_session,
                              BluetoothDevice* device) = 0;

    // Notifies that the scan session was unexpectedly invalidated. This could
    // be due to a firmware crash on the bluetooth chipset, etc.
    virtual void OnSessionInvalidated(
        BluetoothLowEnergyScanSession* scan_session) = 0;
  };

  BluetoothLowEnergyScanSession(const BluetoothLowEnergyScanSession&) = delete;
  BluetoothLowEnergyScanSession& operator=(
      const BluetoothLowEnergyScanSession&) = delete;

  virtual ~BluetoothLowEnergyScanSession();

 protected:
  BluetoothLowEnergyScanSession();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_SESSION_H_
