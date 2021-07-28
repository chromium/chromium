// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class Bus;
class ErrorResponse;
}  // namespace dbus

namespace floss {

// TODO(b/189499077) - Expose via floss package
extern const char kAdapterService[];
extern const char kAdapterInterface[];
extern const char kManagerInterface[];
extern const char kManagerObject[];
extern const char kAdapterObjectFormat[];

namespace adapter {
extern const char kGetAddress[];
extern const char kStartDiscovery[];
extern const char kCancelDiscovery[];
extern const char kCreateBond[];
extern const char kRegisterCallback[];
extern const char kCallbackInterface[];

extern const char kOnAddressChanged[];
extern const char kOnDeviceFound[];
extern const char kOnDiscoveringChanged[];
extern const char kOnSspRequest[];
}  // namespace adapter

namespace manager {
extern const char kStart[];
extern const char kStop[];
extern const char kGetFlossEnabled[];
extern const char kSetFlossEnabled[];
extern const char kGetState[];
extern const char kListHciDevices[];
extern const char kRegisterCallback[];
extern const char kCallbackInterface[];

extern const char kOnHciDeviceChanged[];
extern const char kOnHciEnabledChanged[];
}  // namespace manager

// BluetoothDevice structure for DBus apis.
struct FlossDeviceId {
  std::string address;
  std::string name;
};

// Represents an error sent through DBus.
struct Error {
  Error(const std::string& name, const std::string& message);

  std::string name;
  std::string message;
};

using ResponseCallback = base::OnceCallback<void(const absl::optional<Error>&)>;

// Restrict all access to DBus client initialization to FlossDBusManager so we
// can enforce the proper ordering of initialization and shutdowns.
class FlossDBusClient {
 public:
  // Error: No response from bus.
  static const char kErrorNoResponse[];

  // Error: Invalid parameters.
  static const char kErrorInvalidParameters[];

  FlossDBusClient(const FlossDBusClient&) = delete;
  FlossDBusClient& operator=(const FlossDBusClient&) = delete;

 protected:
  // Convert a dbus::ErrorResponse into a floss::Error struct.
  static Error ErrorResponseToError(const std::string& default_name,
                                    const std::string& default_message,
                                    dbus::ErrorResponse* error);

  FlossDBusClient();
  virtual ~FlossDBusClient();

  virtual void Init(dbus::Bus* bus,
                    const std::string& bluetooth_service_name,
                    const std::string& bluetooth_adapter_path) = 0;

  // Log a dbus::ErrorResponse.
  void LogErrorResponse(const std::string& message, dbus::ErrorResponse* error);

 private:
  friend class FlossDBusManager;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
