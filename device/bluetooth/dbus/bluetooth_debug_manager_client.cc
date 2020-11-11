// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/macros.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

// TODO(apusaka): move these consts to system_api/service_constants.h
namespace {
const char kBluetoothDebugObjectPath[] = "/org/chromium/Bluetooth";
const uint8_t kMinDispatcherLevel = 0;
const uint8_t kMinNewblueLevel = 0;
const uint8_t kMinBluezLevel = 0;
const uint8_t kMinKernelLevel = 0;
const uint8_t kMaxDispatcherLevel = 0xff;
const uint8_t kMaxNewblueLevel = 0xff;
const uint8_t kMaxBluezLevel = 2;
const uint8_t kMaxKernelLevel = 1;
}  // namespace

const char BluetoothDebugManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char BluetoothDebugManagerClient::kInvalidArgumentError[] =
    "org.chromium.Error.InvalidArgument";

// The BluetoothDebugManagerClient implementation used in production.
class BluetoothDebugManagerClientImpl : public BluetoothDebugManagerClient,
                                        public dbus::ObjectManager::Interface {
 public:
  BluetoothDebugManagerClientImpl() = default;

  ~BluetoothDebugManagerClientImpl() override = default;

  // BluetoothDebugManagerClient override.
  void SetLogLevels(const uint8_t dispatcher_level,
                    const uint8_t newblue_level,
                    const uint8_t bluez_level,
                    const uint8_t kernel_level,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
    if (kMinDispatcherLevel > dispatcher_level ||
        kMaxDispatcherLevel < dispatcher_level) {
      std::move(error_callback)
          .Run(kInvalidArgumentError, "dispatcher_level is out of range.");
      return;
    }
    if (kMinNewblueLevel > newblue_level || kMaxNewblueLevel < newblue_level) {
      std::move(error_callback)
          .Run(kInvalidArgumentError, "newblue_level is out of range.");
      return;
    }
    if (kMinBluezLevel > bluez_level || kMaxBluezLevel < bluez_level) {
      std::move(error_callback)
          .Run(kInvalidArgumentError, "bluez_level is out of range.");
      return;
    }
    if (kMinKernelLevel > kernel_level || kMaxKernelLevel < kernel_level) {
      std::move(error_callback)
          .Run(kInvalidArgumentError, "kernel_level is out of range.");
      return;
    }

    dbus::MethodCall method_call(bluetooth_debug::kBluetoothDebugInterface,
                                 bluetooth_debug::kSetLevels);

    dbus::MessageWriter writer(&method_call);
    writer.AppendByte(dispatcher_level);
    writer.AppendByte(newblue_level);
    writer.AppendByte(bluez_level);
    writer.AppendByte(kernel_level);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothDebugManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&BluetoothDebugManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);

    object_proxy_ = bus->GetObjectProxy(
        bluetooth_service_name, dbus::ObjectPath(kBluetoothDebugObjectPath));

    object_manager_ = bus->GetObjectManager(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_debug::kBluetoothDebugInterface, this);
  }

 private:
  // dbus::ObjectManager::Interface override.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new dbus::PropertySet(object_proxy, interface_name,
                                 base::DoNothing());
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
    }
    std::move(error_callback).Run(error_name, error_message);
  }

  dbus::ObjectProxy* object_proxy_;

  dbus::ObjectManager* object_manager_;

  base::WeakPtrFactory<BluetoothDebugManagerClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothDebugManagerClientImpl);
};

BluetoothDebugManagerClient::BluetoothDebugManagerClient() = default;

BluetoothDebugManagerClient::~BluetoothDebugManagerClient() = default;

BluetoothDebugManagerClient* BluetoothDebugManagerClient::Create() {
  return new BluetoothDebugManagerClientImpl();
}

}  // namespace bluez
