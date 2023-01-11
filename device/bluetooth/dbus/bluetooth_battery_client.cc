// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_battery_client.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

BluetoothBatteryClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_battery::kPercentageProperty, &percentage);
}

BluetoothBatteryClient::Properties::~Properties() = default;

// The BluetoothBatteryClient implementation used in production.
class BluetoothBatteryClientImpl : public BluetoothBatteryClient,
                                   public dbus::ObjectManager::Interface {
 public:
  BluetoothBatteryClientImpl() = default;

  BluetoothBatteryClientImpl(const BluetoothBatteryClientImpl&) = delete;
  BluetoothBatteryClientImpl& operator=(const BluetoothBatteryClientImpl&) =
      delete;

  ~BluetoothBatteryClientImpl() override {
    // There is an instance of this client that is created but not initialized
    // on Linux. See 'Alternate D-Bus Client' note in bluez_dbus_manager.h.
    if (object_manager_) {
      object_manager_->UnregisterInterface(
          bluetooth_adapter::kBluetoothAdapterInterface);
    }
  }

  // BluetoothBatteryClient override.
  void AddObserver(BluetoothBatteryClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothBatteryClient override.
  void RemoveObserver(BluetoothBatteryClient::Observer* observer) override {
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
        base::BindRepeating(&BluetoothBatteryClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(), object_path));
  }

  // BluetoothBatteryClient override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path, bluetooth_battery::kBluetoothBatteryInterface));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_battery::kBluetoothBatteryInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the battery interface
  // is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.BatteryAdded(object_path);
  }

  // Called by dbus::ObjectManager when an object with the battery interface
  // is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.BatteryRemoved(object_path);
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_)
      observer.BatteryPropertyChanged(object_path, property_name);
  }

  raw_ptr<dbus::ObjectManager> object_manager_ = nullptr;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothBatteryClient::Observer>::Unchecked observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothBatteryClientImpl> weak_ptr_factory_{this};
};

BluetoothBatteryClient::BluetoothBatteryClient() = default;

BluetoothBatteryClient::~BluetoothBatteryClient() = default;

BluetoothBatteryClient* BluetoothBatteryClient::Create() {
  return new BluetoothBatteryClientImpl();
}

}  // namespace bluez
