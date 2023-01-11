// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_input_client.h"

#include <map>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

BluetoothInputClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_input::kReconnectModeProperty, &reconnect_mode);
}

BluetoothInputClient::Properties::~Properties() = default;

// The BluetoothInputClient implementation used in production.
class BluetoothInputClientImpl : public BluetoothInputClient,
                                 public dbus::ObjectManager::Interface {
 public:
  BluetoothInputClientImpl() : object_manager_(nullptr) {}

  BluetoothInputClientImpl(const BluetoothInputClientImpl&) = delete;
  BluetoothInputClientImpl& operator=(const BluetoothInputClientImpl&) = delete;

  ~BluetoothInputClientImpl() override {
    object_manager_->UnregisterInterface(
        bluetooth_input::kBluetoothInputInterface);
  }

  // BluetoothInputClient override.
  void AddObserver(BluetoothInputClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothInputClient override.
  void RemoveObserver(BluetoothInputClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new Properties(
        object_proxy, interface_name,
        base::BindRepeating(&BluetoothInputClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(), object_path));
  }

  // BluetoothInputClient override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path, bluetooth_input::kBluetoothInputInterface));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_input::kBluetoothInputInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the input interface
  // is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.InputAdded(object_path);
  }

  // Called by dbus::ObjectManager when an object with the input interface
  // is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.InputRemoved(object_path);
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_)
      observer.InputPropertyChanged(object_path, property_name);
  }

  raw_ptr<dbus::ObjectManager> object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothInputClient::Observer>::Unchecked observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothInputClientImpl> weak_ptr_factory_{this};
};

BluetoothInputClient::BluetoothInputClient() = default;

BluetoothInputClient::~BluetoothInputClient() = default;

BluetoothInputClient* BluetoothInputClient::Create() {
  return new BluetoothInputClientImpl();
}

}  // namespace bluez
