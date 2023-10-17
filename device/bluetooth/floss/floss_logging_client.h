// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_LOGGING_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_LOGGING_CLIENT_H_

#include <memory>

#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

// The logging client is an adapter-specific DBus client to set the debug
// logging capabilities of the Floss daemon.
class DEVICE_BLUETOOTH_EXPORT FlossLoggingClient : public FlossDBusClient {
 public:
  static std::unique_ptr<FlossLoggingClient> Create();

  FlossLoggingClient(const FlossLoggingClient&) = delete;
  FlossLoggingClient& operator=(const FlossLoggingClient&) = delete;

  FlossLoggingClient();
  ~FlossLoggingClient() override;

  // Check whether debug logging is currently enabled on this adapter.
  virtual void IsDebugEnabled(ResponseCallback<bool> callback);

  // Sets debug logging on this adapter. Changes will take effect immediately.
  virtual void SetDebugLogging(ResponseCallback<Void> callback, bool enabled);

  // Initializes the logging client with given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 protected:
  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // D-Bus path used for api calls by this class (unique to adapter).
  dbus::ObjectPath logging_path_;

  // Service which implements the Logging interface.
  std::string service_name_;

 private:
  template <typename R, typename... Args>
  void CallAdapterLoggingMethod(ResponseCallback<R> callback,
                                const char* member,
                                Args... args) {
    CallMethod(std::move(callback), bus_, service_name_,
               kAdapterLoggingInterface, logging_path_, member, args...);
  }

  base::WeakPtrFactory<FlossLoggingClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_LOGGING_CLIENT_H_
