// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char BluetoothProfileManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

BluetoothProfileManagerClient::Options::Options() = default;

BluetoothProfileManagerClient::Options::~Options() = default;

// The BluetoothProfileManagerClient implementation used in production.
class BluetoothProfileManagerClientImpl : public BluetoothProfileManagerClient {
 public:
  BluetoothProfileManagerClientImpl() {}

  ~BluetoothProfileManagerClientImpl() override = default;

  // BluetoothProfileManagerClient override.
  void RegisterProfile(const dbus::ObjectPath& profile_path,
                       const std::string& uuid,
                       const Options& options,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_profile_manager::kBluetoothProfileManagerInterface,
        bluetooth_profile_manager::kRegisterProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);
    writer.AppendString(uuid);

    dbus::MessageWriter array_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);

    dbus::MessageWriter dict_writer(NULL);

    // Send Name if provided.
    if (options.name.get() != NULL) {
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kNameOption);
      dict_writer.AppendVariantOfString(*(options.name));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send Service if provided.
    if (options.service.get() != NULL) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kServiceOption);
      dict_writer.AppendVariantOfString(*(options.service));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send Role if not the default value.
    if (options.role != SYMMETRIC) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kRoleOption);
      if (options.role == CLIENT)
        dict_writer.AppendVariantOfString(
            bluetooth_profile_manager::kClientRoleOption);
      else if (options.role == SERVER)
        dict_writer.AppendVariantOfString(
            bluetooth_profile_manager::kServerRoleOption);
      else
        dict_writer.AppendVariantOfString("");
      array_writer.CloseContainer(&dict_writer);
    }

    // Send Channel if provided.
    if (options.channel.get() != NULL) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kChannelOption);
      dict_writer.AppendVariantOfUint16(*(options.channel));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send PSM if provided.
    if (options.psm.get() != NULL) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kPSMOption);
      dict_writer.AppendVariantOfUint16(*(options.psm));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send RequireAuthentication if provided.
    if (options.require_authentication.get() != NULL) {
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(
          bluetooth_profile_manager::kRequireAuthenticationOption);
      dict_writer.AppendVariantOfBool(*(options.require_authentication));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send RequireAuthorization if provided.
    if (options.require_authorization.get() != NULL) {
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(
          bluetooth_profile_manager::kRequireAuthorizationOption);
      dict_writer.AppendVariantOfBool(*(options.require_authorization));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send AutoConnect if provided.
    if (options.auto_connect.get() != NULL) {
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kAutoConnectOption);
      dict_writer.AppendVariantOfBool(*(options.auto_connect));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send ServiceRecord if provided.
    if (options.service_record.get() != NULL) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kServiceRecordOption);
      dict_writer.AppendVariantOfString(*(options.service_record));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send Version if provided.
    if (options.version.get() != NULL) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kVersionOption);
      dict_writer.AppendVariantOfUint16(*(options.version));
      array_writer.CloseContainer(&dict_writer);
    }

    // Send Features if provided.
    if (options.features.get() != NULL) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kFeaturesOption);
      dict_writer.AppendVariantOfUint16(*(options.features));
      array_writer.CloseContainer(&dict_writer);
    }

    writer.CloseContainer(&array_writer);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothProfileManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothProfileManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothProfileManagerClient override.
  void UnregisterProfile(const dbus::ObjectPath& profile_path,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        bluetooth_profile_manager::kBluetoothProfileManagerInterface,
        bluetooth_profile_manager::kUnregisterProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&BluetoothProfileManagerClientImpl::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr(), callback),
        base::BindOnce(&BluetoothProfileManagerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 protected:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {
    DCHECK(bus);
    object_proxy_ = bus->GetObjectProxy(
        bluetooth_service_name,
        dbus::ObjectPath(
            bluetooth_profile_manager::kBluetoothProfileManagerServicePath));
  }

 private:
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

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothProfileManagerClientImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothProfileManagerClientImpl);
};

BluetoothProfileManagerClient::BluetoothProfileManagerClient() = default;

BluetoothProfileManagerClient::~BluetoothProfileManagerClient() = default;

BluetoothProfileManagerClient* BluetoothProfileManagerClient::Create() {
  return new BluetoothProfileManagerClientImpl();
}

}  // namespace bluez
