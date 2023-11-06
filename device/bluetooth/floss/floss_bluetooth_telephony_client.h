// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_BLUETOOTH_TELEPHONY_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_BLUETOOTH_TELEPHONY_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {
// FlossBluetoothTelephonyClient is a D-Bus client that talks to Floss daemon to
// perform Telephony operations, such like enable phone ops, incoming call and
// answer call. It is managed by FlossClientBundle and will be initialized with
// an adapter.
class DEVICE_BLUETOOTH_EXPORT FlossBluetoothTelephonyClient
    : public FlossDBusClient {
 public:
  static std::unique_ptr<FlossBluetoothTelephonyClient> Create();

  FlossBluetoothTelephonyClient(const FlossBluetoothTelephonyClient&) = delete;
  FlossBluetoothTelephonyClient& operator=(
      const FlossBluetoothTelephonyClient&) = delete;

  FlossBluetoothTelephonyClient();
  ~FlossBluetoothTelephonyClient() override;

  // Enable bluetooth telephony in floss.
  virtual void SetPhoneOpsEnabled(ResponseCallback<Void> callback,
                                  bool enabled);

  // Initialize FlossBluetoothTelephonyClient for the given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 protected:
  friend class FlossBluetoothTelephonyClientTest;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Path used for bluetooth telephony api calls by this class.
  dbus::ObjectPath bluetooth_telephony_adapter_path_;

  // Service which implements the FlossBluetoothTelephonyClient interface.
  std::string service_name_;

 private:
  friend class FlossBluetoothTelephonyClientTest;

  template <typename R, typename... Args>
  void CallBluetoothTelephonyMethod(ResponseCallback<R> callback,
                                    const char* member,
                                    Args... args) {
    CallMethod(std::move(callback), bus_, service_name_,
               kBluetoothTelephonyInterface, bluetooth_telephony_adapter_path_,
               member, args...);
  }

  base::WeakPtrFactory<FlossBluetoothTelephonyClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_BLUETOOTH_TELEPHONY_CLIENT_H_
