// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_dbus_client.h"

#include <string>

#include "base/logging.h"
#include "dbus/message.h"
#include "device/bluetooth/floss/floss_adapter_client.h"

namespace floss {

// All Floss D-Bus methods return immediately, so the timeout can be very short.
int kDBusTimeoutMs = 2000;

namespace {

template <typename T>
bool ReadReturnFromResponse(dbus::MessageReader* reader, T* value);

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader, bool* value) {
  return reader->PopBool(value);
}

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader, uint8_t* value) {
  return reader->PopByte(value);
}

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader, uint32_t* value) {
  return reader->PopUint32(value);
}

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader, std::string* value) {
  return reader->PopString(value);
}

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader, FlossDeviceId* value) {
  return FlossAdapterClient::ParseFlossDeviceId(reader, value);
}

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader,
                            FlossAdapterClient::BluetoothDeviceType* value) {
  uint32_t val;
  bool success;

  success = reader->PopUint32(&val);
  *value = static_cast<FlossAdapterClient::BluetoothDeviceType>(val);

  return success;
}

template <>
bool ReadReturnFromResponse(dbus::MessageReader* reader,
                            device::BluetoothUUID* value) {
  return FlossAdapterClient::ParseUUID(reader, value);
}

// Specialization for vector of anything.
template <typename T>
bool ReadReturnFromResponse(dbus::MessageReader* reader,
                            std::vector<typename T::value_type>* value) {
  using ElemType = typename T::value_type;

  dbus::MessageReader subreader(nullptr);
  if (!reader->PopArray(&subreader))
    return false;

  while (subreader.HasMoreData()) {
    ElemType element;
    if (!ReadReturnFromResponse<ElemType>(&subreader, &element))
      return false;

    value->push_back(element);
  }

  return true;
}

}  // namespace

// TODO(b/189499077) - Expose via floss package
const char kAdapterService[] = "org.chromium.bluetooth";
const char kManagerService[] = "org.chromium.bluetooth.Manager";
const char kAdapterInterface[] = "org.chromium.bluetooth.Bluetooth";
const char kManagerInterface[] = "org.chromium.bluetooth.Manager";
const char kManagerObject[] = "/org/chromium/bluetooth/Manager";
const char kAdapterObjectFormat[] = "/org/chromium/bluetooth/hci%d/adapter";

namespace adapter {
const char kGetAddress[] = "GetAddress";
const char kGetName[] = "GetName";
const char kSetName[] = "SetName";
const char kGetDiscoverable[] = "GetDiscoverable";
const char kSetDiscoverable[] = "SetDiscoverable";
const char kStartDiscovery[] = "StartDiscovery";
const char kCancelDiscovery[] = "CancelDiscovery";
const char kCreateBond[] = "CreateBond";
const char kCancelBondProcess[] = "CancelBondProcess";
const char kRemoveBond[] = "RemoveBond";
const char kGetRemoteType[] = "GetRemoteType";
const char kGetRemoteClass[] = "GetRemoteClass";
const char kGetConnectionState[] = "GetConnectionState";
const char kGetRemoteUuids[] = "GetRemoteUuids";
const char kGetBondState[] = "GetBondState";
const char kConnectAllEnabledProfiles[] = "ConnectAllEnabledProfiles";
const char kDisconnectAllEnabledProfiles[] = "DisconnectAllEnabledProfiles";
const char kRegisterCallback[] = "RegisterCallback";
const char kRegisterConnectionCallback[] = "RegisterConnectionCallback";
const char kSetPairingConfirmation[] = "SetPairingConfirmation";
const char kSetPin[] = "SetPin";
const char kSetPasskey[] = "SetPasskey";
const char kGetBondedDevices[] = "GetBondedDevices";

// TODO(abps) - Rename this to AdapterCallback in platform and here
const char kCallbackInterface[] = "org.chromium.bluetooth.BluetoothCallback";
const char kConnectionCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothConnectionCallback";

const char kOnAddressChanged[] = "OnAddressChanged";
const char kOnNameChanged[] = "OnNameChanged";
const char kOnDiscoverableChanged[] = "OnDiscoverableChanged";
const char kOnDeviceFound[] = "OnDeviceFound";
const char kOnDeviceCleared[] = "OnDeviceCleared";
const char kOnDiscoveringChanged[] = "OnDiscoveringChanged";
const char kOnSspRequest[] = "OnSspRequest";

const char kOnBondStateChanged[] = "OnBondStateChanged";
const char kOnDeviceConnected[] = "OnDeviceConnected";
const char kOnDeviceDisconnected[] = "OnDeviceDisconnected";
}  // namespace adapter

namespace manager {
const char kStart[] = "Start";
const char kStop[] = "Stop";
const char kGetFlossEnabled[] = "GetFlossEnabled";
const char kSetFlossEnabled[] = "SetFlossEnabled";
const char kGetState[] = "GetState";
const char kGetAvailableAdapters[] = "GetAvailableAdapters";
const char kRegisterCallback[] = "RegisterCallback";
const char kCallbackInterface[] = "org.chromium.bluetooth.ManagerCallback";
const char kOnHciDeviceChanged[] = "OnHciDeviceChanged";
const char kOnHciEnabledChanged[] = "OnHciEnabledChanged";
}  // namespace manager

Error::Error(const std::string& name, const std::string& message)
    : name(name), message(message) {}

FlossDBusClient::FlossDBusClient() = default;
FlossDBusClient::~FlossDBusClient() = default;

const char FlossDBusClient::kErrorNoResponse[] =
    "org.chromium.Error.NoResponse";
const char FlossDBusClient::kErrorInvalidParameters[] =
    "org.chromium.Error.InvalidParameters";
const char FlossDBusClient::kErrorInvalidReturn[] =
    "org.chromium.Error.InvalidReturn";

// Default error handler for dbus clients is to just print the error right now.
// TODO(abps) - Deprecate this once error handling is implemented in the upper
//              layers.
void FlossDBusClient::LogErrorResponse(const std::string& message,
                                       dbus::ErrorResponse* error) {
  if (!error) {
    return;
  }

  dbus::MessageReader reader(error);
  auto error_name = error->GetErrorName();
  std::string error_message;
  reader.PopString(&error_message);

  LOG(ERROR) << message << ": " << error_name << ": " << error_message;
}

// static
Error FlossDBusClient::ErrorResponseToError(const std::string& default_name,
                                            const std::string& default_message,
                                            dbus::ErrorResponse* error) {
  Error result(default_name, default_message);

  if (error) {
    dbus::MessageReader reader(error);
    result.name = error->GetErrorName();
    reader.PopString(&result.message);
  }

  return result;
}

template <>
void FlossDBusClient::DefaultResponseWithCallback<Void>(
    ResponseCallback<Void> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (response) {
    std::move(callback).Run(/*ret=*/absl::nullopt, /*err=*/absl::nullopt);
    return;
  }

  std::move(callback).Run(
      /*ret=*/absl::nullopt,
      ErrorResponseToError(kErrorNoResponse, /*default_message=*/std::string(),
                           error_response));
}

template <typename T>
void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<T> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (response) {
    T ret;
    dbus::MessageReader reader(response);

    if (!ReadReturnFromResponse<T>(&reader, &ret)) {
      LOG(ERROR) << "Failed reading return from response";
      std::move(callback).Run(
          /*ret=*/absl::nullopt,
          Error(kErrorInvalidReturn, /*message=*/std::string()));
      return;
    }

    std::move(callback).Run(ret, /*err=*/absl::nullopt);
    return;
  }

  std::move(callback).Run(
      /*ret=*/absl::nullopt,
      ErrorResponseToError(kErrorNoResponse, /*default_message=*/std::string(),
                           error_response));
}

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<bool> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<uint8_t> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<uint32_t> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<std::string> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<std::vector<FlossDeviceId>> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<FlossAdapterClient::BluetoothDeviceType> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<device::BluetoothDevice::UUIDList> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

void FlossDBusClient::DefaultResponse(const std::string& caller,
                                      dbus::Response* response,
                                      dbus::ErrorResponse* error_response) {
  if (error_response) {
    FlossDBusClient::LogErrorResponse(caller, error_response);
  } else {
    DVLOG(1) << caller << "::OnResponse";
  }
}

}  // namespace floss
