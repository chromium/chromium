// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_

#include <ostream>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace floss {

extern DEVICE_BLUETOOTH_EXPORT int kDBusTimeoutMs;

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
extern DEVICE_BLUETOOTH_EXPORT const char kGetRemoteType[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetRemoteClass[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetConnectionState[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetRemoteUuids[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetBondState[];
extern DEVICE_BLUETOOTH_EXPORT const char kConnectAllEnabledProfiles[];
extern DEVICE_BLUETOOTH_EXPORT const char kDisconnectAllEnabledProfiles[];
extern DEVICE_BLUETOOTH_EXPORT const char kRegisterCallback[];
extern DEVICE_BLUETOOTH_EXPORT const char kRegisterConnectionCallback[];
extern DEVICE_BLUETOOTH_EXPORT const char kCallbackInterface[];
extern DEVICE_BLUETOOTH_EXPORT const char kConnectionCallbackInterface[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetPairingConfirmation[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetPin[];
extern DEVICE_BLUETOOTH_EXPORT const char kSetPasskey[];
extern DEVICE_BLUETOOTH_EXPORT const char kGetBondedDevices[];

extern DEVICE_BLUETOOTH_EXPORT const char kOnAdapterPropertyChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnAddressChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnNameChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDiscoverableChanged[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDeviceFound[];
extern DEVICE_BLUETOOTH_EXPORT const char kOnDeviceCleared[];
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
struct DEVICE_BLUETOOTH_EXPORT FlossDeviceId {
  std::string address;
  std::string name;

  inline bool operator==(const FlossDeviceId& rhs) const {
    return address == rhs.address && name == rhs.name;
  }

  friend std::ostream& operator<<(std::ostream& os, const FlossDeviceId& id) {
    return os << "FlossDeviceId(" << id.address << ", " << id.name << ")";
  }

  static const char kDeviceIdNameKey[];
  static const char kDeviceIdAddressKey[];
};

// Represents an error sent through DBus.
struct DEVICE_BLUETOOTH_EXPORT Error {
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

// A Weakly Owned ResponseCallback<T>. The main usecase for this is to have
// a weak pointer available for |PostDelayedTask|, where deleting the main
// object will automatically cancel the posted task.
template <typename T>
class WeaklyOwnedCallback {
 public:
  explicit WeaklyOwnedCallback(ResponseCallback<T> cb) : cb_(std::move(cb)) {}
  ~WeaklyOwnedCallback() = default;

  static std::unique_ptr<WeaklyOwnedCallback> Create(ResponseCallback<T> cb) {
    return std::make_unique<WeaklyOwnedCallback>(std::move(cb));
  }

  // If the callback hasn't been executed, run it and return true. Otherwise
  // false.
  bool Run(const absl::optional<T>& ret, const absl::optional<Error>& err) {
    if (cb_) {
      std::move(cb_).Run(ret, err);
      return true;
    }

    return false;
  }

  base::WeakPtr<WeaklyOwnedCallback> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ResponseCallback<T> cb_;
  base::WeakPtrFactory<WeaklyOwnedCallback> weak_ptr_factory_{this};
};

// Restrict all access to DBus client initialization to FlossDBusManager so we
// can enforce the proper ordering of initialization and shutdowns.
class FlossDBusClient {
 public:
  // Error: DBus error.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorDBus[];

  // Error: No response from bus.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorNoResponse[];

  // Error: Invalid parameters.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorInvalidParameters[];

  // Error: Invalid return.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorInvalidReturn[];

  // Generalized DBus serialization (used for generalized method call
  // invocation).
  template <typename T>
  static void DEVICE_BLUETOOTH_EXPORT
  WriteDBusParam(dbus::MessageWriter* writer, const T& data);

  // Base case for variadic write.
  static void DEVICE_BLUETOOTH_EXPORT
  WriteAllDBusParams(dbus::MessageWriter* writer) {}

  // Variadic write method that expands to multiple WriteDBusParam calls.
  template <typename T, typename... Args>
  static void DEVICE_BLUETOOTH_EXPORT
  WriteAllDBusParams(dbus::MessageWriter* writer,
                     const T& first,
                     const Args&... args) {
    WriteDBusParam(writer, first);
    WriteAllDBusParams(writer, args...);
  }

  // Generalized DBus deserialization (used for generalized method call returns
  // and can be used for exported methods as well). Implement for each type that
  // you want deserialized.
  template <typename T>
  static bool DEVICE_BLUETOOTH_EXPORT ReadDBusParam(dbus::MessageReader* reader,
                                                    T* value);

  // Container type needs to be explicitly listed here.
  template <typename T>
  static bool DEVICE_BLUETOOTH_EXPORT ReadDBusParam(dbus::MessageReader* reader,
                                                    std::vector<T>* value);

  // Base case for variadic read.
  static bool DEVICE_BLUETOOTH_EXPORT
  ReadAllDBusParams(dbus::MessageReader* reader) {
    return true;
  }

  // Variadic read method that expands to multiple ReadDBusParam calls.
  // Individual calls to |ReadDBusParam| must succeed before the next call is
  // done.
  template <typename T, typename... Args>
  static bool DEVICE_BLUETOOTH_EXPORT
  ReadAllDBusParams(dbus::MessageReader* reader, T* first, Args*... args) {
    return ReadDBusParam(reader, first) && ReadAllDBusParams(reader, args...);
  }

  template <typename R, typename... Args>
  void CallMethod(ResponseCallback<R> callback,
                  dbus::Bus* bus,
                  const std::string& service_name,
                  const std::string& interface_name,
                  const dbus::ObjectPath& object_path,
                  const char* method_name,
                  Args... args) {
    if (bus == nullptr) {
      LOG(ERROR) << "D-Bus is not initialized, cannot call method "
                 << method_name << " on " << object_path.value();
      std::move(callback).Run(absl::nullopt,
                              Error(kErrorDBus, "DBus not initialized"));
      return;
    }

    dbus::ObjectProxy* object_proxy =
        bus->GetObjectProxy(service_name, object_path);
    if (!object_proxy) {
      LOG(ERROR) << "Object proxy does not exist when trying to call "
                 << method_name;
      std::move(callback).Run(absl::nullopt,
                              Error(kErrorDBus, "Invalid object proxy"));
      return;
    }

    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);

    FlossDBusClient::WriteAllDBusParams(&writer, args...);

    object_proxy->CallMethodWithErrorResponse(
        &method_call, kDBusTimeoutMs,
        base::BindOnce(&FlossDBusClient::DefaultResponseWithCallback<R>,
                       base::Unretained(this), std::move(callback)));
  }

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
  void DEVICE_BLUETOOTH_EXPORT
  DefaultResponseWithCallback(ResponseCallback<T> callback,
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
