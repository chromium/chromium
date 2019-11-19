// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_media_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// TODO(mcchou): Add these service constants into dbus/service_constants.h
// later.
const char kBluetoothMediaInterface[] = "org.bluez.Media1";

// Method names supported by Media Interface.
const char kRegisterEndpoint[] = "RegisterEndpoint";
const char kUnregisterEndpoint[] = "UnregisterEndpoint";

// The set of properties which are used to register a media endpoint.
const char kUUIDEndpointProperty[] = "UUID";
const char kCodecEndpointProperty[] = "Codec";
const char kCapabilitiesEndpointProperty[] = "Capabilities";

}  // namespace

namespace bluez {

// static
const char BluetoothMediaClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

// static
const char BluetoothMediaClient::kBluetoothAudioSinkUUID[] =
    "0000110b-0000-1000-8000-00805f9b34fb";

BluetoothMediaClient::EndpointProperties::EndpointProperties() : codec(0x00) {}

BluetoothMediaClient::EndpointProperties::~EndpointProperties() = default;

class BluetoothMediaClientImpl : public BluetoothMediaClient,
                                 dbus::ObjectManager::Interface {
 public:
  BluetoothMediaClientImpl() : object_manager_(nullptr) {}

  ~BluetoothMediaClientImpl() override {
    object_manager_->UnregisterInterface(kBluetoothMediaInterface);
  }

  // dbus::ObjectManager::Interface overrides.

  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new dbus::PropertySet(object_proxy, interface_name,
                                 base::DoNothing());
  }

  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    VLOG(1) << "Remote Media added: " << object_path.value();
    for (auto& observer : observers_)
      observer.MediaAdded(object_path);
  }

  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    VLOG(1) << "Remote Media removed: " << object_path.value();
    for (auto& observer : observers_)
      observer.MediaRemoved(object_path);
  }

  // BluetoothMediaClient overrides.

  void AddObserver(BluetoothMediaClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BluetoothMediaClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  void RegisterEndpoint(const dbus::ObjectPath& object_path,
                        const dbus::ObjectPath& endpoint_path,
                        const EndpointProperties& properties,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override {
    VLOG(1) << "RegisterEndpoint - endpoint: " << endpoint_path.value();

    dbus::MethodCall method_call(kBluetoothMediaInterface, kRegisterEndpoint);

    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(nullptr);
    dbus::MessageWriter dict_entry_writer(nullptr);

    // Send the path to the endpoint.
    writer.AppendObjectPath(endpoint_path);

    writer.OpenArray("{sv}", &array_writer);

    // Send UUID.
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(kUUIDEndpointProperty);
    dict_entry_writer.AppendVariantOfString(properties.uuid);
    array_writer.CloseContainer(&dict_entry_writer);

    // Send Codec.
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(kCodecEndpointProperty);
    dict_entry_writer.AppendVariantOfByte(properties.codec);
    array_writer.CloseContainer(&dict_entry_writer);

    // Send Capabilities.
    dbus::MessageWriter variant_writer(nullptr);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(kCapabilitiesEndpointProperty);
    dict_entry_writer.OpenVariant("ay", &variant_writer);
    variant_writer.AppendArrayOfBytes(properties.capabilities.data(),
                                      properties.capabilities.size());
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer.CloseContainer(&dict_entry_writer);

    writer.CloseContainer(&array_writer);

    // Get Object Proxy based on the service name and the service path and call
    // RegisterEndpoint medthod.
    scoped_refptr<dbus::ObjectProxy> object_proxy(
        object_manager_->GetObjectProxy(object_path));
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothMediaClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothMediaClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  void UnregisterEndpoint(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& endpoint_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override {
    VLOG(1) << "UnregisterEndpoint - endpoint: " << endpoint_path.value();

    dbus::MethodCall method_call(kBluetoothMediaInterface, kUnregisterEndpoint);

    // Send the path to the endpoint.
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(endpoint_path);

    // Get Object Proxy based on the service name and the service path and call
    // RegisterEndpoint medthod.
    scoped_refptr<dbus::ObjectProxy> object_proxy(
        object_manager_->GetObjectProxy(object_path));
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothMediaClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothMediaClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(kBluetoothMediaInterface, this);
  }

 private:
  // Called when a response for successful method call is received.
  void OnSuccess(const base::Closure& callback, dbus::Response* response) {
    DCHECK(response);
    callback.Run();
  }

  // Called when a response for a failed method call is received.
  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response) {
    // Error response has an optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
    }
    error_callback.Run(error_name, error_message);
  }

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothMediaClient::Observer>::Unchecked observers_;

  base::WeakPtrFactory<BluetoothMediaClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothMediaClientImpl);
};

BluetoothMediaClient::BluetoothMediaClient() = default;

BluetoothMediaClient::~BluetoothMediaClient() = default;

BluetoothMediaClient* BluetoothMediaClient::Create() {
  return new BluetoothMediaClientImpl();
}

}  // namespace bluez
