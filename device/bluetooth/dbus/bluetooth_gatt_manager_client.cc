// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char BluetoothGattManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

// The BluetoothGattManagerClient implementation used in production.
class BluetoothGattManagerClientImpl : public BluetoothGattManagerClient {
 public:
  BluetoothGattManagerClientImpl() : object_manager_(nullptr) {}

  ~BluetoothGattManagerClientImpl() override = default;

  // BluetoothGattManagerClient override.
  void RegisterApplication(const dbus::ObjectPath& adapter_object_path,
                           const dbus::ObjectPath& application_path,
                           const Options& options,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) override {
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
    DCHECK(object_proxy);
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothGattManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothGattManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothGattManagerClient override.
  void UnregisterApplication(const dbus::ObjectPath& adapter_object_path,
                             const dbus::ObjectPath& application_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_gatt_manager::kBluetoothGattManagerInterface,
        bluetooth_gatt_manager::kUnregisterApplication);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(application_path);

    DCHECK(object_manager_);
    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(adapter_object_path);
    DCHECK(object_proxy);
    object_proxy->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothGattManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothGattManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
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
    }
    error_callback.Run(error_name, error_message);
  }

  // The proxy to the bluez object manager.
  dbus::ObjectManager* object_manager_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattManagerClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattManagerClientImpl);
};

BluetoothGattManagerClient::BluetoothGattManagerClient() = default;

BluetoothGattManagerClient::~BluetoothGattManagerClient() = default;

// static
BluetoothGattManagerClient* BluetoothGattManagerClient::Create() {
  return new BluetoothGattManagerClientImpl();
}

}  // namespace bluez
