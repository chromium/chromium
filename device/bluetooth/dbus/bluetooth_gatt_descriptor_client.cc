// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/object_manager.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

// TODO(armansito): Move this constant to cros_system_api.
const char kValueProperty[] = "Value";

}  // namespace

// static
const char BluetoothGattDescriptorClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
// static
const char BluetoothGattDescriptorClient::kUnknownDescriptorError[] =
    "org.chromium.Error.UnknownDescriptor";

BluetoothGattDescriptorClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_gatt_descriptor::kUUIDProperty, &uuid);
  RegisterProperty(bluetooth_gatt_descriptor::kCharacteristicProperty,
                   &characteristic);
  RegisterProperty(kValueProperty, &value);
}

BluetoothGattDescriptorClient::Properties::~Properties() = default;

// The BluetoothGattDescriptorClient implementation used in production.
class BluetoothGattDescriptorClientImpl
    : public BluetoothGattDescriptorClient,
      public dbus::ObjectManager::Interface {
 public:
  BluetoothGattDescriptorClientImpl() : object_manager_(nullptr) {}

  ~BluetoothGattDescriptorClientImpl() override {
    object_manager_->UnregisterInterface(
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface);
  }

  // BluetoothGattDescriptorClientImpl override.
  void AddObserver(BluetoothGattDescriptorClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothGattDescriptorClientImpl override.
  void RemoveObserver(
      BluetoothGattDescriptorClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothGattDescriptorClientImpl override.
  std::vector<dbus::ObjectPath> GetDescriptors() override {
    DCHECK(object_manager_);
    return object_manager_->GetObjectsWithInterface(
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface);
  }

  // BluetoothGattDescriptorClientImpl override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    DCHECK(object_manager_);
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path,
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface));
  }

  // BluetoothGattDescriptorClientImpl override.
  void ReadValue(const dbus::ObjectPath& object_path,
                 ValueCallback callback,
                 ErrorCallback error_callback) override {
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDescriptorError, "");
      return;
    }

    dbus::MethodCall method_call(
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
        bluetooth_gatt_descriptor::kReadValue);

    // Append empty option dict
    dbus::MessageWriter writer(&method_call);
    base::DictionaryValue dict;
    dbus::AppendValueData(&writer, dict);

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothGattDescriptorClientImpl::OnValueSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothGattDescriptorClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothGattDescriptorClientImpl override.
  void WriteValue(const dbus::ObjectPath& object_path,
                  const std::vector<uint8_t>& value,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownDescriptorError, "");
      return;
    }

    dbus::MethodCall method_call(
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
        bluetooth_gatt_descriptor::kWriteValue);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(value.data(), value.size());

    // Append empty option dict
    base::DictionaryValue dict;
    dbus::AppendValueData(&writer, dict);

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothGattDescriptorClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothGattDescriptorClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    Properties* properties = new Properties(
        object_proxy, interface_name,
        base::Bind(&BluetoothGattDescriptorClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // dbus::ObjectManager::Interface override.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    VLOG(2) << "Remote GATT descriptor added: " << object_path.value();
    for (auto& observer : observers_)
      observer.GattDescriptorAdded(object_path);
  }

  // dbus::ObjectManager::Interface override.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    VLOG(2) << "Remote GATT descriptor removed: " << object_path.value();
    for (auto& observer : observers_)
      observer.GattDescriptorRemoved(object_path);
  }

 protected:
  // bluez::DBusClient override.
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface, this);
  }

 private:
  // Called by dbus::PropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call. Informs
  // observers.
  virtual void OnPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name) {
    VLOG(2) << "Remote GATT descriptor property changed: "
            << object_path.value() << ": " << property_name;
    for (auto& observer : observers_)
      observer.GattDescriptorPropertyChanged(object_path, property_name);
  }

  // Called when a response for a successful method call is received.
  void OnSuccess(base::OnceClosure callback, dbus::Response* response) {
    DCHECK(response);
    std::move(callback).Run();
  }

  // Called when a descriptor value response for a successful method call is
  // received.
  void OnValueSuccess(ValueCallback callback, dbus::Response* response) {
    DCHECK(response);
    dbus::MessageReader reader(response);

    const uint8_t* bytes = NULL;
    size_t length = 0;

    if (!reader.PopArrayOfBytes(&bytes, &length))
      VLOG(2) << "Error reading array of bytes in ValueCallback";

    std::vector<uint8_t> value;

    if (bytes)
      value.assign(bytes, bytes + length);

    std::move(callback).Run(value);
  }

  // Called when a response for a failed method call is received.
  void OnError(ErrorCallback error_callback, dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    std::move(error_callback).Run(error_name, error_message);
  }

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothGattDescriptorClient::Observer>::Unchecked
      observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattDescriptorClientImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattDescriptorClientImpl);
};

BluetoothGattDescriptorClient::BluetoothGattDescriptorClient() = default;

BluetoothGattDescriptorClient::~BluetoothGattDescriptorClient() = default;

// static
BluetoothGattDescriptorClient* BluetoothGattDescriptorClient::Create() {
  return new BluetoothGattDescriptorClientImpl();
}

}  // namespace bluez
