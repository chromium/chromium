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
constexpr char kDeviceIdNameKey[] = "name";
constexpr char kDeviceIdAddressKey[] = "address";

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

void FlossAdapterClient::StartDiscovery(ResponseCallback callback) {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, adapter_path_);
  if (!object_proxy) {
    std::move(callback).Run(Error(kErrorUnknownAdapter, std::string()));
    return;
  }

  dbus::MethodCall method_call(kAdapterInterface, adapter::kStartDiscovery);
  object_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&FlossAdapterClient::DefaultResponseWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FlossAdapterClient::CancelDiscovery(ResponseCallback callback) {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, adapter_path_);
  if (!object_proxy) {
    std::move(callback).Run(Error(kErrorUnknownAdapter, std::string()));
    return;
  }

  dbus::MethodCall method_call(kAdapterInterface, adapter::kCancelDiscovery);
  object_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&FlossAdapterClient::DefaultResponseWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FlossAdapterClient::CreateBond(ResponseCallback callback,
                                    FlossDeviceId device,
                                    BluetoothTransport transport) {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, adapter_path_);
  if (!object_proxy) {
    std::move(callback).Run(Error(kErrorUnknownAdapter, std::string()));
    return;
  }

  dbus::MethodCall method_call(kAdapterInterface, adapter::kCreateBond);
  dbus::MessageWriter writer(&method_call);
  SerializeFlossDeviceId(&writer, device);
  writer.AppendUint32(static_cast<uint32_t>(transport));

  object_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&FlossAdapterClient::DefaultResponseWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
      &mc_get_address, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&FlossAdapterClient::HandleGetAddress,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::ExportedObject* callbacks =
      bus_->GetExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  if (!callbacks) {
    LOG(ERROR) << "FlossAdapterClient couldn't export client callbacks";
    return;
  }

  // Register callbacks for the adapter.
  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnAddressChanged,
      base::BindRepeating(&FlossAdapterClient::OnAddressChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnAddressChanged));

  callbacks->ExportMethod(
      adapter::kCallbackInterface, adapter::kOnDeviceFound,
      base::BindRepeating(&FlossAdapterClient::OnDeviceFound,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, adapter::kOnDeviceFound));

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

  dbus::MethodCall register_callback(kAdapterInterface,
                                     adapter::kRegisterCallback);

  dbus::MessageWriter writer(&register_callback);
  writer.AppendObjectPath(dbus::ObjectPath(kExportedCallbacksPath));

  object_proxy->CallMethodWithErrorResponse(
      &register_callback, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&FlossAdapterClient::DefaultResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     adapter::kRegisterCallback));
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

void FlossAdapterClient::OnDiscoveringChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  bool state;

  if (!msg.PopBool(&state)) {
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
  dbus::MessageReader msg(method_call);
  FlossDeviceId device;

  DVLOG(1) << __func__;

  if (!ParseFlossDeviceId(&msg, &device)) {
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

void FlossAdapterClient::OnSspRequest(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  FlossDeviceId device;
  uint32_t cod, passkey, variant;

  if (!(ParseFlossDeviceId(&msg, &device) && msg.PopUint32(&cod) &&
        msg.PopUint32(&variant) && msg.PopUint32(&passkey))) {
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

// Parse a FlossDeviceId from a message.
//
// The format:
// array (
//  dict_entry (
//    key "name"
//    variant string("")
//  )
//  dict entry (
//    key "address"
//    variant string("")
//  )
// )
bool FlossAdapterClient::ParseFlossDeviceId(dbus::MessageReader* reader,
                                            FlossDeviceId* device) {
  dbus::MessageReader array(nullptr);
  dbus::MessageReader dict(nullptr);
  bool found_name = false;
  bool found_address = false;

  if (reader->PopArray(&array)) {
    while (array.PopDictEntry(&dict)) {
      std::string key;
      dict.PopString(&key);

      if (key == kDeviceIdNameKey) {
        found_name = dict.PopVariantOfString(&device->name);
      } else if (key == kDeviceIdAddressKey) {
        found_address = dict.PopVariantOfString(&device->address);
      }
    }
  }

  return found_name && found_address;
}

// See |ParseFlossDeviceId| for serialized format.
void FlossAdapterClient::SerializeFlossDeviceId(
    dbus::MessageWriter* writer,
    const FlossDeviceId& device_id) {
  dbus::MessageWriter array(nullptr);
  dbus::MessageWriter dict(nullptr);

  writer->OpenArray("{sv}", &array);

  // Serialize name
  array.OpenDictEntry(&dict);
  dict.AppendString(kDeviceIdNameKey);
  dict.AppendVariantOfString(device_id.name);
  array.CloseContainer(&dict);

  // Serialize address
  array.OpenDictEntry(&dict);
  dict.AppendString(kDeviceIdAddressKey);
  dict.AppendVariantOfString(device_id.address);
  array.CloseContainer(&dict);

  writer->CloseContainer(&array);
}

}  // namespace floss
