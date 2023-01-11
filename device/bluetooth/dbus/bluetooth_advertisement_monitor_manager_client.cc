// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {
const char kNoResponseError[] = "org.chromium.Error.NoResponse";
const char kFailedError[] = "org.chromium.Error.Failed";
}  // namespace

namespace bluez {

BluetoothAdvertisementMonitorManagerClient::Observer::~Observer() = default;

BluetoothAdvertisementMonitorManagerClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(
      bluetooth_advertisement_monitor_manager::kSupportedMonitorTypes,
      &supported_monitor_types);
  RegisterProperty(bluetooth_advertisement_monitor_manager::kSupportedFeatures,
                   &supported_features);
}

BluetoothAdvertisementMonitorManagerClient::Properties::~Properties() = default;

// The BluetoothAdvertisementMonitorManagerClient implementation used in
// production.
class BluetoothAdvertisementMonitorManagerClientImpl final
    : public BluetoothAdvertisementMonitorManagerClient,
      public dbus::ObjectManager::Interface {
 public:
  BluetoothAdvertisementMonitorManagerClientImpl() = default;

  ~BluetoothAdvertisementMonitorManagerClientImpl() override {
    // There is an instance of this client that is created but not initialized
    // on Linux. See 'Alternate D-Bus Client' note in bluez_dbus_manager.h.
    if (object_manager_) {
      object_manager_->UnregisterInterface(
          bluetooth_advertisement_monitor_manager::
              kBluetoothAdvertisementMonitorManagerInterface);
    }
  }

  BluetoothAdvertisementMonitorManagerClientImpl(
      const BluetoothAdvertisementMonitorManagerClientImpl&) = delete;
  BluetoothAdvertisementMonitorManagerClientImpl& operator=(
      const BluetoothAdvertisementMonitorManagerClientImpl&) = delete;

  // BluetoothAdvertisementMonitorManagerClient override.
  void RegisterMonitor(const dbus::ObjectPath& application,
                       const dbus::ObjectPath& adapter,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_advertisement_monitor_manager::
            kBluetoothAdvertisementMonitorManagerInterface,
        bluetooth_advertisement_monitor_manager::kRegisterMonitor);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(application);
    CallObjectProxyMethod(adapter, &method_call, std::move(callback),
                          std::move(error_callback));
  }

  // BluetoothAdvertisementMonitorManagerClient override.
  void UnregisterMonitor(const dbus::ObjectPath& application,
                         const dbus::ObjectPath& adapter,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_advertisement_monitor_manager::
            kBluetoothAdvertisementMonitorManagerInterface,
        bluetooth_advertisement_monitor_manager::kUnregisterMonitor);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(application);

    CallObjectProxyMethod(adapter, &method_call, std::move(callback),
                          std::move(error_callback));
  }

  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new Properties(
        object_proxy, interface_name,
        base::BindRepeating(
            &BluetoothAdvertisementMonitorManagerClientImpl::OnPropertyChanged,
            weak_ptr_factory_.GetWeakPtr(), object_path));
    ;
  }

  // BluetoothAdvertisementMonitorManagerClient override.
  void AddObserver(
      BluetoothAdvertisementMonitorManagerClient::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(
      BluetoothAdvertisementMonitorManagerClient::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    DCHECK(object_manager_);
    return static_cast<Properties*>(object_manager_->GetProperties(
        object_path, bluetooth_advertisement_monitor_manager::
                         kBluetoothAdvertisementMonitorManagerInterface));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_advertisement_monitor_manager::
            kBluetoothAdvertisementMonitorManagerInterface,
        this);
  }

 private:
  // Called by dbus::PropertySet when a property value is changed. Informs
  // observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    DVLOG(2) << "Bluetooth Advertisement Monitor Client property changed: "
             << object_path.value() << ": " << property_name;

    if (property_name ==
        bluetooth_advertisement_monitor_manager::kSupportedFeatures) {
      for (auto& observer : observers_)
        observer.SupportedAdvertisementMonitorFeaturesChanged();
    }
  }

  void CallObjectProxyMethod(const dbus::ObjectPath& manager_object_path,
                             dbus::MethodCall* method_call,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) {
    DCHECK(object_manager_);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(manager_object_path);
    if (!object_proxy) {
      std::move(error_callback).Run(kFailedError, "Adapter does not exist.");
      return;
    }
    object_proxy->CallMethodWithErrorCallback(
        method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &BluetoothAdvertisementMonitorManagerClientImpl::OnSuccess,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothAdvertisementMonitorManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
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
      error_message = "D-Bus did not provide a response.";
    }
    std::move(error_callback).Run(error_name, error_message);
  }

  base::ObserverList<Observer> observers_;

  raw_ptr<dbus::ObjectManager> object_manager_ = nullptr;

  base::WeakPtrFactory<BluetoothAdvertisementMonitorManagerClientImpl>
      weak_ptr_factory_{this};
};

BluetoothAdvertisementMonitorManagerClient::
    BluetoothAdvertisementMonitorManagerClient() = default;

BluetoothAdvertisementMonitorManagerClient::
    ~BluetoothAdvertisementMonitorManagerClient() = default;

// static
std::unique_ptr<BluetoothAdvertisementMonitorManagerClient>
BluetoothAdvertisementMonitorManagerClient::Create() {
  return base::WrapUnique(new BluetoothAdvertisementMonitorManagerClientImpl());
}

}  // namespace bluez
