// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char BluetoothGattManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char BluetoothGattManagerClient::kUnknownGattManager[] =
    "org.chromium.Error.UnknownGattManager";

namespace {

const char kNoGattManagerMessage[] = "No GATT Manager found: ";

}  // namespace

// The BluetoothGattManagerClient implementation used in production.
class BluetoothGattManagerClientImpl : public BluetoothGattManagerClient {
 public:
  BluetoothGattManagerClientImpl() : object_manager_(nullptr) {}

  BluetoothGattManagerClientImpl(const BluetoothGattManagerClientImpl&) =
      delete;
  BluetoothGattManagerClientImpl& operator=(
      const BluetoothGattManagerClientImpl&) = delete;

  ~BluetoothGattManagerClientImpl() override = default;

  // BluetoothGattManagerClient override.
  void RegisterApplication(const dbus::ObjectPath& adapter_object_path,
                           const dbus::ObjectPath& application_path,
                           const Options& options,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_gatt_manager::kBluetoothGattManagerInterface,
        bluetooth_gatt_manager::kRegisterApplication);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(application_path);

    // The parameters of the Options dictionary are undefined but the method
    // signature still requires a value dictionary. Pass an empty dictionary
    // and fill in the contents later if and when we add any options.
    dbus::MessageWriter array_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);
    writer.CloseContainer(&array_writer);

    DCHECK(object_manager_);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(adapter_object_path);
    if (!object_proxy) {
      RespondWhenNoProxyAvailable(adapter_object_path,
                                  std::move(error_callback));
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothGattManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothGattManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  // BluetoothGattManagerClient override.
  void UnregisterApplication(const dbus::ObjectPath& adapter_object_path,
                             const dbus::ObjectPath& application_path,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_gatt_manager::kBluetoothGattManagerInterface,
        bluetooth_gatt_manager::kUnregisterApplication);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(application_path);

    DCHECK(object_manager_);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(adapter_object_path);
    if (!object_proxy) {
      RespondWhenNoProxyAvailable(adapter_object_path,
                                  std::move(error_callback));
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothGattManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothGattManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

 protected:
  // bluez::DBusClient override.
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);
    DCHECK(bus);
    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
  }

 private:
  // Called when a response for a successful method call is received.
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
    }
    std::move(error_callback).Run(error_name, error_message);
  }

  void RespondWhenNoProxyAvailable(const dbus::ObjectPath& application_path,
                                   ErrorCallback error_callback) {
    LOG(WARNING) << "No ObjectProxy found for " << application_path.value();
    std::move(error_callback)
        .Run(kUnknownGattManager,
             base::StrCat({kNoGattManagerMessage, application_path.value()}));
  }

  // The proxy to the bluez object manager.
  raw_ptr<dbus::ObjectManager> object_manager_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattManagerClientImpl> weak_ptr_factory_{this};
};

BluetoothGattManagerClient::BluetoothGattManagerClient() = default;

BluetoothGattManagerClient::~BluetoothGattManagerClient() = default;

// static
BluetoothGattManagerClient* BluetoothGattManagerClient::Create() {
  return new BluetoothGattManagerClientImpl();
}

}  // namespace bluez
