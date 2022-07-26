// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_adapter_client.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

namespace {
void HandleExported(const std::string& method_name,
                    const std::string& interface_name,
                    const std::string& object_path,
                    bool success) {
  DVLOG(1) << (success ? "Successfully exported " : "Failed to export ")
           << method_name << " on interface = " << interface_name
           << ", object = " << object_path;
}

}  // namespace

constexpr char FlossAdapterClient::kErrorUnknownAdapter[] =
    "org.chromium.Error.UnknownAdapter";
constexpr char FlossAdapterClient::kExportedCallbacksPath[] =
    "/org/chromium/bluetooth/adapterclient";

void FlossAdapterClient::SetName(ResponseCallback<Void> callback,
                                 const std::string& name) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kSetName, name);
}

void FlossAdapterClient::SetDiscoverable(ResponseCallback<Void> callback,
                                         bool discoverable) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kSetDiscoverable,
                          discoverable);
}

void FlossAdapterClient::StartDiscovery(ResponseCallback<Void> callback) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kStartDiscovery);
}

void FlossAdapterClient::CancelDiscovery(ResponseCallback<Void> callback) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kCancelDiscovery);
}

void FlossAdapterClient::CreateBond(ResponseCallback<bool> callback,
                                    FlossDeviceId device,
                                    BluetoothTransport transport) {
  CallAdapterMethod<bool>(std::move(callback), adapter::kCreateBond, device,
                          transport);
}

void FlossAdapterClient::CancelBondProcess(ResponseCallback<bool> callback,
                                           FlossDeviceId device) {
  CallAdapterMethod<bool>(std::move(callback), adapter::kCancelBondProcess,
                          device);
}

void FlossAdapterClient::RemoveBond(ResponseCallback<bool> callback,
                                    FlossDeviceId device) {
  CallAdapterMethod<bool>(std::move(callback), adapter::kRemoveBond, device);
}

void FlossAdapterClient::GetRemoteType(
    ResponseCallback<BluetoothDeviceType> callback,
    FlossDeviceId device) {
  CallAdapterMethod<BluetoothDeviceType>(std::move(callback),
                                         adapter::kGetRemoteType, device);
}

void FlossAdapterClient::GetRemoteClass(ResponseCallback<uint32_t> callback,
                                        FlossDeviceId device) {
  CallAdapterMethod<uint32_t>(std::move(callback), adapter::kGetRemoteClass,
                              device);
}

void FlossAdapterClient::GetConnectionState(ResponseCallback<uint32_t> callback,
                                            const FlossDeviceId& device) {
  CallAdapterMethod<uint32_t>(std::move(callback), adapter::kGetConnectionState,
                              device);
}

void FlossAdapterClient::GetRemoteUuids(
    ResponseCallback<device::BluetoothDevice::UUIDList> callback,
    FlossDeviceId device) {
  CallAdapterMethod<device::BluetoothDevice::UUIDList>(
      std::move(callback), adapter::kGetRemoteUuids, device);
}

void FlossAdapterClient::GetBondState(ResponseCallback<uint32_t> callback,
                                      const FlossDeviceId& device) {
  CallAdapterMethod<uint32_t>(std::move(callback), adapter::kGetBondState,
                              device);
}

void FlossAdapterClient::ConnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  CallAdapterMethod<Void>(std::move(callback),
                          adapter::kConnectAllEnabledProfiles, device);
}

void FlossAdapterClient::DisconnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  CallAdapterMethod<Void>(std::move(callback),
                          adapter::kDisconnectAllEnabledProfiles, device);
}

void FlossAdapterClient::SetPairingConfirmation(ResponseCallback<Void> callback,
                                                const FlossDeviceId& device,
                                                bool accept) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kSetPairingConfirmation,
                          device, accept);
}

void FlossAdapterClient::SetPin(ResponseCallback<Void> callback,
                                const FlossDeviceId& device,
                                bool accept,
                                const std::vector<uint8_t>& pin) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kSetPin, device, accept,
                          pin);
}

void FlossAdapterClient::SetPasskey(ResponseCallback<Void> callback,
                                    const FlossDeviceId& device,
                                    bool accept,
                                    const std::vector<uint8_t>& passkey) {
  CallAdapterMethod<Void>(std::move(callback), adapter::kSetPasskey, device,
                          accept, passkey);
}

void FlossAdapterClient::GetBondedDevices() {
  CallAdapterMethod<std::vector<FlossDeviceId>>(
      base::BindOnce(&FlossAdapterClient::OnGetBondedDevices,
                     weak_ptr_factory_.GetWeakPtr()),
      adapter::kGetBondedDevices);
}

void FlossAdapterClient::Init(dbus::Bus* bus,
                              const std::string& service_name,
                              const std::string& adapter_path) {
  bus_ = bus;
  adapter_path_ = dbus::ObjectPath(adapter_path);
  service_name_ = service_name;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, adapter_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossAdapterClient couldn't init. Object proxy was null.";
    return;
  }

  dbus::MethodCall mc_get_address(kAdapterInterface, adapter::kGetAddress);
  object_proxy->CallMethodWithErrorResponse(
      &mc_get_address, kDBusTimeoutMs,
      base::BindOnce(&FlossAdapterClient::HandleGetAddress,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::MethodCall mc_get_name(kAdapterInterface, adapter::kGetName);
  object_proxy->CallMethodWithErrorResponse(
      &mc_get_name, kDBusTimeoutMs,
      base::BindOnce(&FlossAdapterClient::HandleGetName,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::MethodCall mc_get_discoverable(kAdapterInterface,
                                       adapter::kGetDiscoverable);
  object_proxy->CallMethodWithErrorResponse(
      &mc_get_discoverable, kDBusTimeoutMs,
      base::BindOnce(&FlossAdapterClient::HandleGetDiscoverable,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::ExportedObject* callbacks =
      bus_->GetExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  if (!callbacks) {
    LOG(ERROR) << "FlossAdapterClient couldn't export client callbacks";
    return;
  }

  // Register callbacks for the adapter.
  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnAdapterPropertyChanged,
      base::BindRepeating(&FlossAdapterClient::OnAdapterPropertyChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnAdapterPropertyChanged));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnAddressChanged,
      base::BindRepeating(&FlossAdapterClient::OnAddressChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnAddressChanged));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnNameChanged,
      base::BindRepeating(&FlossAdapterClient::OnNameChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnNameChanged));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnDiscoverableChanged,
      base::BindRepeating(&FlossAdapterClient::OnDiscoverableChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDiscoverableChanged));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnDeviceFound,
      base::BindRepeating(&FlossAdapterClient::OnDeviceFound,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDeviceFound));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnDeviceCleared,
      base::BindRepeating(&FlossAdapterClient::OnDeviceCleared,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDeviceCleared));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnDiscoveringChanged,
      base::BindRepeating(&FlossAdapterClient::OnDiscoveringChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDiscoveringChanged));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnSspRequest,
      base::BindRepeating(&FlossAdapterClient::OnSspRequest,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnSspRequest));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnBondStateChanged,
      base::BindRepeating(&FlossAdapterClient::OnBondStateChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnBondStateChanged));

  callbacks->ExportMethod(
      adapter::kConnectionCallbackInterface, adapter::kOnDeviceConnected,
      base::BindRepeating(&FlossAdapterClient::OnDeviceConnected,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDeviceConnected));

  callbacks->ExportMethod(
      adapter::kConnectionCallbackInterface, adapter::kOnDeviceDisconnected,
      base::BindRepeating(&FlossAdapterClient::OnDeviceDisconnected,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDeviceDisconnected));

  dbus::MethodCall register_callback(kAdapterInterface,
                                     adapter::kRegisterCallback);

  dbus::MessageWriter writer(&register_callback);
  writer.AppendObjectPath(dbus::ObjectPath(kExportedCallbacksPath));

  object_proxy->CallMethodWithErrorResponse(
      &register_callback, kDBusTimeoutMs,
      base::BindOnce(&FlossAdapterClient::DefaultResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     adapter::kRegisterCallback));

  dbus::MethodCall register_connection_callback(
      kAdapterInterface, adapter::kRegisterConnectionCallback);

  dbus::MessageWriter writer2(&register_connection_callback);
  writer2.AppendObjectPath(dbus::ObjectPath(kExportedCallbacksPath));

  object_proxy->CallMethodWithErrorResponse(
      &register_connection_callback, kDBusTimeoutMs,
      base::BindOnce(&FlossAdapterClient::DefaultResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     adapter::kRegisterCallback));
}

void FlossAdapterClient::OnAdapterPropertyChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  uint32_t prop;

  if (!msg.PopUint32(&prop)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  BtPropertyType prop_type = static_cast<BtPropertyType>(prop);
  switch (prop_type) {
    case BtPropertyType::kAdapterBondedDevices:
      GetBondedDevices();
      break;
    default:;  // Do nothing for other property types
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::HandleGetAddress(dbus::Response* response,
                                          dbus::ErrorResponse* error_response) {
  if (!response) {
    LogErrorResponse("FlossAdapterClient::HandleGetAddress", error_response);
    return;
  }

  dbus::MessageReader msg(response);
  std::string address;

  if (msg.PopString(&address)) {
    adapter_address_ = address;
    for (auto& observer : observers_) {
      observer.AdapterAddressChanged(adapter_address_);
    }
  }
}

void FlossAdapterClient::OnAddressChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  std::string address;

  if (!msg.PopString(&address)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  adapter_address_ = address;
  for (auto& observer : observers_) {
    observer.AdapterAddressChanged(adapter_address_);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::HandleGetName(dbus::Response* response,
                                       dbus::ErrorResponse* error_response) {
  if (!response) {
    LogErrorResponse("FlossAdapterClient::HandleGetName", error_response);
    return;
  }

  dbus::MessageReader msg(response);
  std::string name;

  if (msg.PopString(&name)) {
    adapter_name_ = name;
  }
}

void FlossAdapterClient::OnNameChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  std::string name;

  if (!msg.PopString(&name)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  adapter_name_ = name;

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::HandleGetDiscoverable(
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (!response) {
    LogErrorResponse("FlossAdapterClient::HandleGetDiscoverable",
                     error_response);
    return;
  }

  dbus::MessageReader reader(response);
  bool discoverable;

  if (ReadAllDBusParams(&reader, &discoverable)) {
    adapter_discoverable_ = discoverable;
    for (auto& observer : observers_) {
      observer.DiscoverableChanged(adapter_discoverable_);
    }
  }
}

void FlossAdapterClient::OnDiscoverableChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  bool discoverable;

  if (!ReadAllDBusParams(&reader, &discoverable)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  adapter_discoverable_ = discoverable;
  for (auto& observer : observers_) {
    observer.DiscoverableChanged(adapter_discoverable_);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnDiscoveringChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  bool state;

  if (!ReadAllDBusParams(&reader, &state)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterDiscoveringChanged(state);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnDeviceFound(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  FlossDeviceId device;

  DVLOG(1) << __func__;

  if (!ReadAllDBusParams(&reader, &device)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterFoundDevice(device);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnDeviceCleared(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  FlossDeviceId device;

  DVLOG(1) << __func__;

  if (!ReadAllDBusParams(&reader, &device)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterClearedDevice(device);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnSspRequest(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  FlossDeviceId device;
  uint32_t cod, passkey, variant;

  if (!ReadAllDBusParams(&reader, &device, &cod, &variant, &passkey)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterSspRequest(
        device, cod, static_cast<BluetoothSspVariant>(variant), passkey);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnGetBondedDevices(
    DBusResult<std::vector<FlossDeviceId>> ret) {
  if (!ret.has_value()) {
    LOG(ERROR) << "Error on GetBondedDevices: " << ret.error();
    return;
  }

  for (const auto& device_id : *ret) {
    for (auto& observer : observers_) {
      observer.AdapterFoundDevice(device_id);
    }
  }
}

void FlossAdapterClient::OnBondStateChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  uint32_t status;
  std::string address;
  uint32_t bond_state;

  if (!ReadAllDBusParams(&reader, &status, &address, &bond_state)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters,
            /*error_message=*/std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(
        FlossDeviceId({address, ""}), status,
        static_cast<FlossAdapterClient::BondState>(bond_state));
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnDeviceConnected(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  FlossDeviceId device;

  if (!ReadAllDBusParams(&reader, &device)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterDeviceConnected(device);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossAdapterClient::OnDeviceDisconnected(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  FlossDeviceId device;

  if (!ReadAllDBusParams(&reader, &device)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterDeviceDisconnected(device);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

FlossAdapterClient::FlossAdapterClient() = default;
FlossAdapterClient::~FlossAdapterClient() {
  if (bus_) {
    bus_->UnregisterExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  }
}

void FlossAdapterClient::AddObserver(FlossAdapterClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FlossAdapterClient::RemoveObserver(
    FlossAdapterClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
std::unique_ptr<FlossAdapterClient> FlossAdapterClient::Create() {
  return std::make_unique<FlossAdapterClient>();
}

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const FlossAdapterClient::BluetoothTransport& data) {
  writer->AppendUint32(static_cast<uint32_t>(data));
}

// These methods are explicitly instantiated for FlossAdapterClientTest since
// now we don't have many methods that cover the various template use cases.
// TODO(b/202334519): Remove these and replace with real methods that implicitly
// instantiate the template once we have more coverage.
template void FlossAdapterClient::CallAdapterMethod(
    ResponseCallback<Void> callback,
    const char* member);
template void FlossAdapterClient::CallAdapterMethod(
    ResponseCallback<uint8_t> callback,
    const char* member);
template void FlossAdapterClient::CallAdapterMethod(
    ResponseCallback<std::string> callback,
    const char* member,
    const uint32_t& arg1);
template void FlossAdapterClient::CallAdapterMethod(
    ResponseCallback<Void> callback,
    const char* member,
    const uint32_t& arg1,
    const std::string& arg2);

}  // namespace floss
