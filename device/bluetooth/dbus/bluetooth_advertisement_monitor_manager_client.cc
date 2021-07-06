// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
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

// The BluetoothAdvertisementMonitorManagerClient implementation used in
// production.
class BluetoothAdvertisementMonitorManagerClientImpl
    : public BluetoothAdvertisementMonitorManagerClient {
 public:
  BluetoothAdvertisementMonitorManagerClientImpl() = default;

  ~BluetoothAdvertisementMonitorManagerClientImpl() override = default;
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

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
  }

 private:
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

  dbus::ObjectManager* object_manager_ = nullptr;

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
