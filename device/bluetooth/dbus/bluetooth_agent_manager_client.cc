// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char BluetoothAgentManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

// The BluetoothAgentManagerClient implementation used in production.
class BluetoothAgentManagerClientImpl : public BluetoothAgentManagerClient,
                                        public dbus::ObjectManager::Interface {
 public:
  BluetoothAgentManagerClientImpl() {}

  ~BluetoothAgentManagerClientImpl() override = default;

  // BluetoothAgentManagerClient override.
  void AddObserver(BluetoothAgentManagerClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothAgentManagerClient override.
  void RemoveObserver(
      BluetoothAgentManagerClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothAgentManagerClient override.
  void RegisterAgent(const dbus::ObjectPath& agent_path,
                     const std::string& capability,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_agent_manager::kBluetoothAgentManagerInterface,
        bluetooth_agent_manager::kRegisterAgent);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(agent_path);
    writer.AppendString(capability);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAgentManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothAgentManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothAgentManagerClient override.
  void UnregisterAgent(const dbus::ObjectPath& agent_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_agent_manager::kBluetoothAgentManagerInterface,
        bluetooth_agent_manager::kUnregisterAgent);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(agent_path);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAgentManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothAgentManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothAgentManagerClient override.
  void RequestDefaultAgent(const dbus::ObjectPath& agent_path,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_agent_manager::kBluetoothAgentManagerInterface,
        bluetooth_agent_manager::kRequestDefaultAgent);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(agent_path);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothAgentManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothAgentManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);
    object_proxy_ = bus->GetObjectProxy(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_agent_manager::kBluetoothAgentManagerServicePath));

    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_agent_manager::kBluetoothAgentManagerInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the agent manager
  // interface is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.AgentManagerAdded(object_path);
  }

  // Called by dbus::ObjectManager when an object with the adapter interface
  // is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    for (auto& observer : observers_)
      observer.AgentManagerRemoved(object_path);
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new dbus::PropertySet(object_proxy, interface_name,
                                 base::DoNothing());
  }

  // Called when a response for successful method call is received.
  void OnSuccess(const base::Closure& callback, dbus::Response* response) {
    DCHECK(response);
    callback.Run();
  }

  // Called when a response for a failed method call is received.
  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response) {
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
    error_callback.Run(error_name, error_message);
  }

  dbus::ObjectProxy* object_proxy_;

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  base::ObserverList<BluetoothAgentManagerClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAgentManagerClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothAgentManagerClientImpl);
};

BluetoothAgentManagerClient::BluetoothAgentManagerClient() = default;

BluetoothAgentManagerClient::~BluetoothAgentManagerClient() = default;

BluetoothAgentManagerClient* BluetoothAgentManagerClient::Create() {
  return new BluetoothAgentManagerClientImpl();
}

}  // namespace bluez
