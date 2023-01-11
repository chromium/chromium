// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DEBUG_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DEBUG_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothDebugManagerClient is used to communicate with the debug manager
// object of the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothDebugManagerClient
    : public BluezDBusClient {
 public:
  BluetoothDebugManagerClient(const BluetoothDebugManagerClient&) = delete;
  BluetoothDebugManagerClient& operator=(const BluetoothDebugManagerClient&) =
      delete;

  ~BluetoothDebugManagerClient() override;

  // The ErrorCallback is used by debug manager methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  typedef base::OnceCallback<void(const std::string& error_name,
                                  const std::string& error_message)>
      ErrorCallback;

  // Invoke D-Bus API to update bluetooth devcoredump state.
  virtual void SetDevCoredump(const bool enable,
                              base::OnceClosure callback,
                              ErrorCallback error_callback) = 0;

  // Invoke D-Bus API to enable or disable LL privacy.
  virtual void SetLLPrivacy(const bool enable,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) = 0;

  // Invoke D-Bus API to enable or disable the Bluetooth Quality Report.
  virtual void SetBluetoothQualityReport(const bool enable,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) = 0;

  // Invoke D-Bus API to set the levels of logging verbosity for each of
  // the bluetooth daemons and kernel.
  virtual void SetLogLevels(const uint8_t bluez_level,
                            const uint8_t kernel_level,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothDebugManagerClient* Create();

  // Constants used to indicate exceptional error conditions. These are
  // returned as the |error_name| in ErrorCallback.
  static const char kNoResponseError[];
  static const char kInvalidArgumentError[];

 protected:
  BluetoothDebugManagerClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DEBUG_MANAGER_CLIENT_H_
