// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_

#include <ostream>
#include <string>

#include "base/callback.h"
#include "device/bluetooth/bluetooth_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class Bus;
class Response;
class ErrorResponse;
}  // namespace dbus

namespace floss {

extern int kDBusTimeoutMs;

// TODO(b/189499077) - Expose via floss package
extern DEVICE_BLUETOOTH_EXPORT const char kAdapterService[];
extern DEVICE_BLUETOOTH_EXPORT const char kManagerService[];
extern DEVICE_BLUETOOTH_EXPORT const char kAdapterInterface[];
extern DEVICE_BLUETOOTH_EXPORT const char kManagerInterface[];
extern DEVICE_BLUETOOTH_EXPORT const char kManagerObject[];
extern DEVICE_BLUETOOTH_EXPORT const char kAdapterObjectFormat[];

namespace adapter {
extern DEVICE_BLUETOOTH_EXPORT const char kGetAddress[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetName[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetName[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetDiscoverable[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetDiscoverable[];
extern DEVICE_BLUETOOTH_EXPORT const char kStartDiscovery[];
extern DEVICE_BLUETOOTH_EXPORT const char kCancelDiscovery[];
extern DEVICE_BLUETOOTH_EXPORT const char kCreateBond[];
extern DEVICE_BLUETOOTH_EXPORT const char kCancelBondProcess[];
extern DEVICE_BLUETOOTH_EXPORT const char kRemoveBond[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetConnectionState[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetBondState[];
extern DEVICE_BLUETOOTH_EXPORT const char kConnectAllEnabledProfiles[];
extern DEVICE_BLUETOOTH_EXPORT const char kRegisterCallback[];
extern DEVICE_BLUETOOTH_EXPORT const char kRegisterConnectionCallback[];
extern DEVICE_BLUETOOTH_EXPORT const char kCallbackInterface[];
extern DEVICE_BLUETOOTH_EXPORT const char kConnectionCallbackInterface[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetPairingConfirmation[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetPin[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetPasskey[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetBondedDevices[];

extern DEVICE_BLUETOOTH_EXPORT const char kOnAddressChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnNameChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDiscoverableChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDeviceFound[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDiscoveringChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnSspRequest[];

extern DEVICE_BLUETOOTH_EXPORT const char kOnBondStateChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDeviceConnected[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDeviceDisconnected[];
}  // namespace adapter

namespace manager {
extern DEVICE_BLUETOOTH_EXPORT const char kStart[];
extern DEVICE_BLUETOOTH_EXPORT const char kStop[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetFlossEnabled[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetFlossEnabled[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetState[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetAvailableAdapters[];
extern DEVICE_BLUETOOTH_EXPORT const char kRegisterCallback[];
extern DEVICE_BLUETOOTH_EXPORT const char kCallbackInterface[];

extern DEVICE_BLUETOOTH_EXPORT const char kOnHciDeviceChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnHciEnabledChanged[];
}  // namespace manager

// BluetoothDevice structure for DBus apis.
struct FlossDeviceId {
  std::string address;
  std::string name;

  inline bool operator==(const FlossDeviceId& rhs) const {
    return address == rhs.address && name == rhs.name;
  }

  friend std::ostream& operator<<(std::ostream& os, const FlossDeviceId& id) {
    return os << "FlossDeviceId(" << id.address << ", " << id.name << ")";
  }
};

// Represents an error sent through DBus.
struct Error {
  Error(const std::string& name, const std::string& message);

  std::string name;
  std::string message;
};

// Represents void return type of D-Bus (no return). Needed so that we can use
// "void" as a type in C++ templates.
// Needs to be exported because there are template instantiations using this.
struct DEVICE_BLUETOOTH_EXPORT Void {};

template <typename T>
using ResponseCallback =
    base::OnceCallback<void(const absl::optional<T>& ret,
                            const absl::optional<Error>& err)>;

// Restrict all access to DBus client initialization to FlossDBusManager so we
// can enforce the proper ordering of initialization and shutdowns.
class FlossDBusClient {
 public:
  // Error: No response from bus.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorNoResponse[];

  // Error: Invalid parameters.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorInvalidParameters[];

  // Error: Invalid return.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorInvalidReturn[];

  FlossDBusClient(const FlossDBusClient&) = delete;
  FlossDBusClient& operator=(const FlossDBusClient&) = delete;

  // Common init signature for all clients.
  virtual void Init(dbus::Bus* bus,
                    const std::string& bluetooth_service_name,
                    const std::string& bluetooth_adapter_path) = 0;

 protected:
  // Convert a dbus::ErrorResponse into a floss::Error struct.
  static Error ErrorResponseToError(const std::string& default_name,
                                    const std::string& default_message,
                                    dbus::ErrorResponse* error);

  FlossDBusClient();
  virtual ~FlossDBusClient();

  // Log a dbus::ErrorResponse.
  void LogErrorResponse(const std::string& message, dbus::ErrorResponse* error);

  // Default handler that runs |callback| with the callback with an optional
  // return and optional error.
  template <typename T>
  void DefaultResponseWithCallback(ResponseCallback<T> callback,
                                   dbus::Response* response,
                                   dbus::ErrorResponse* error_response);

  // Default handler for a response. It will either log the error response or
  // print |caller| to VLOG. |caller| should be the name of the DBus method that
  // is being called.
  void DefaultResponse(const std::string& caller,
                       dbus::Response* response,
                       dbus::ErrorResponse* error_response);
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
