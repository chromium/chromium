// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/object_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

BluetoothGattServiceClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_gatt_service::kUUIDProperty, &uuid);
  RegisterProperty(bluetooth_gatt_service::kIncludesProperty, &includes);
  RegisterProperty(bluetooth_gatt_service::kDeviceProperty, &device);
  RegisterProperty(bluetooth_gatt_service::kPrimaryProperty, &primary);
}

BluetoothGattServiceClient::Properties::~Properties() = default;

// The BluetoothGattServiceClient implementation used in production.
class BluetoothGattServiceClientImpl : public BluetoothGattServiceClient,
                                       public dbus::ObjectManager::Interface {
 public:
  BluetoothGattServiceClientImpl() : object_manager_(nullptr) {}

  ~BluetoothGattServiceClientImpl() override {
    object_manager_->UnregisterInterface(
        bluetooth_gatt_service::kBluetoothGattServiceInterface);
  }

  // BluetoothGattServiceClientImpl override.
  void AddObserver(BluetoothGattServiceClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothGattServiceClientImpl override.
  void RemoveObserver(BluetoothGattServiceClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothGattServiceClientImpl override.
  std::vector<dbus::ObjectPath> GetServices() override {
    DCHECK(object_manager_);
    return object_manager_->GetObjectsWithInterface(
        bluetooth_gatt_service::kBluetoothGattServiceInterface);
  }

  // BluetoothGattServiceClientImpl override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    DCHECK(object_manager_);
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path, bluetooth_gatt_service::kBluetoothGattServiceInterface));
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    Properties* properties = new Properties(
        object_proxy, interface_name,
        base::Bind(&BluetoothGattServiceClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // dbus::ObjectManager::Interface override.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    VLOG(2) << "Remote GATT service added: " << object_path.value();
    for (auto& observer : observers_)
      observer.GattServiceAdded(object_path);
  }

  // dbus::ObjectManager::Interface override.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    VLOG(2) << "Remote GATT service removed: " << object_path.value();
    for (auto& observer : observers_)
      observer.GattServiceRemoved(object_path);
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
        bluetooth_gatt_service::kBluetoothGattServiceInterface, this);
  }

 private:
  // Called by dbus::PropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call. Informs
  // observers.
  virtual void OnPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name) {
    VLOG(2) << "Remote GATT service property changed: " << object_path.value()
            << ": " << property_name;
    for (auto& observer : observers_)
      observer.GattServicePropertyChanged(object_path, property_name);
  }

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothGattServiceClient::Observer>::Unchecked
      observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattServiceClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattServiceClientImpl);
};

BluetoothGattServiceClient::BluetoothGattServiceClient() = default;

BluetoothGattServiceClient::~BluetoothGattServiceClient() = default;

// static
BluetoothGattServiceClient* BluetoothGattServiceClient::Create() {
  return new BluetoothGattServiceClientImpl();
}

}  // namespace bluez
