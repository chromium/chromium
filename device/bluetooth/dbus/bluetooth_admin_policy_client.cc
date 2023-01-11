// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"

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

const char kNoResponseError[] = "org.chromium.Error.NoResponse";
const char kUnknownAdminPolicyError[] = "org.chromium.Error.UnknownAdminPolicy";

namespace bluez {

BluetoothAdminPolicyClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_admin_policy::kServiceAllowListProperty,
                   &service_allow_list);
  RegisterProperty(bluetooth_admin_policy::kIsBlockedByPolicyProperty,
                   &is_blocked_by_policy);
}

BluetoothAdminPolicyClient::Properties::~Properties() = default;

// The BluetoothAdminPolicyClient implementation used in production.
class BluetoothAdminPolicyClientImpl : public BluetoothAdminPolicyClient,
                                       public dbus::ObjectManager::Interface {
 public:
  BluetoothAdminPolicyClientImpl() = default;

  ~BluetoothAdminPolicyClientImpl() override {
    // There is an instance of this client that is created but not initialized
    // on Linux. See 'Alternate D-Bus Client' note in bluez_dbus_manager.h.
    if (object_manager_) {
      object_manager_->UnregisterInterface(
          bluetooth_adapter::kBluetoothAdapterInterface);
    }
  }

  // BluetoothAdminPolicyClient override.
  void AddObserver(BluetoothAdminPolicyClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothAdminPolicyClient override.
  void RemoveObserver(BluetoothAdminPolicyClient::Observer* observer) override {
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
        base::BindRepeating(&BluetoothAdminPolicyClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(), object_path));
  }

  // BluetoothAdminPolicyClient override.
  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path,
        bluetooth_admin_policy::kBluetoothAdminPolicyStatusInterface));
  }

  void SetServiceAllowList(const dbus::ObjectPath& object_path,
                           const UUIDList& service_uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override {
    std::vector<std::string> uuid_array;

    for (const auto& uuid : service_uuids)
      uuid_array.push_back(uuid.canonical_value());

    dbus::MethodCall method_call(
        bluetooth_admin_policy::kBluetoothAdminPolicySetInterface,
        bluetooth_admin_policy::kSetServiceAllowList);

    dbus::MessageWriter writer(&method_call);

    writer.AppendArrayOfStrings(uuid_array);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kUnknownAdminPolicyError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAdminPolicyClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothAdminPolicyClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_admin_policy::kBluetoothAdminPolicyStatusInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the Admin Policy
  // interface is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.AdminPolicyAdded(object_path);
  }

  // Called by dbus::ObjectManager when an object with the Admin Policy
  // interface is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.AdminPolicyRemoved(object_path);
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_)
      observer.AdminPolicyPropertyChanged(object_path, property_name);
  }

  // Called when a response for successful method call is received.
  void OnSuccess(base::OnceClosure callback, dbus::Response* response) {
    DCHECK(response);
    std::move(callback).Run();
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

  raw_ptr<dbus::ObjectManager> object_manager_ = nullptr;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothAdminPolicyClient::Observer>::Unchecked
      observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdminPolicyClientImpl> weak_ptr_factory_{this};
};

BluetoothAdminPolicyClient::BluetoothAdminPolicyClient() = default;

BluetoothAdminPolicyClient::~BluetoothAdminPolicyClient() = default;

std::unique_ptr<BluetoothAdminPolicyClient>
BluetoothAdminPolicyClient::Create() {
  return std::make_unique<BluetoothAdminPolicyClientImpl>();
}

}  // namespace bluez
