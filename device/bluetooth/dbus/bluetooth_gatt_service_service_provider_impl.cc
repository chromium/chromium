// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider_impl.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorPropertyReadOnly[] =
    "org.freedesktop.DBus.Error.PropertyReadOnly";

}  // namespace

BluetoothGattServiceServiceProviderImpl::
    BluetoothGattServiceServiceProviderImpl(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path,
        const std::string& uuid,
        bool is_primary,
        const std::vector<dbus::ObjectPath>& includes)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      uuid_(uuid),
      is_primary_(is_primary),
      includes_(includes),
      bus_(bus),
      object_path_(object_path) {
  DVLOG(1) << "Creating Bluetooth GATT service: " << object_path_.value()
           << " UUID: " << uuid;
  if (!bus_)
    return;

  DCHECK(!uuid_.empty());
  DCHECK(object_path_.IsValid());

  exported_object_ = bus_->GetExportedObject(object_path_);

  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGet,
      base::BindRepeating(&BluetoothGattServiceServiceProviderImpl::Get,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattServiceServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesSet,
      base::BindRepeating(&BluetoothGattServiceServiceProviderImpl::Set,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattServiceServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGetAll,
      base::BindRepeating(&BluetoothGattServiceServiceProviderImpl::GetAll,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattServiceServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

BluetoothGattServiceServiceProviderImpl::
    ~BluetoothGattServiceServiceProviderImpl() {
  DVLOG(1) << "Cleaning up Bluetooth GATT service: " << object_path_.value();
  if (bus_)
    bus_->UnregisterExportedObject(object_path_);
}

bool BluetoothGattServiceServiceProviderImpl::OnOriginThread() {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

void BluetoothGattServiceServiceProviderImpl::Get(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattServiceServiceProvider::Get: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);

  std::string interface_name;
  std::string property_name;
  if (!reader.PopString(&interface_name) || !reader.PopString(&property_name) ||
      reader.HasMoreData()) {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(method_call, kErrorInvalidArgs,
                                            "Expected 'ss'.");
    std::move(response_sender).Run(std::move(error_response));
    return;
  }

  // Only the GATT service interface is allowed.
  if (interface_name !=
      bluetooth_gatt_service::kBluetoothGattServiceInterface) {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidArgs,
            "No such interface: '" + interface_name + "'.");
    std::move(response_sender).Run(std::move(error_response));
    return;
  }

  // Return error if |property_name| is unknown.
  if (property_name != bluetooth_gatt_service::kUUIDProperty &&
      property_name != bluetooth_gatt_service::kIncludesProperty) {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidArgs,
            "No such property: '" + property_name + "'.");
    std::move(response_sender).Run(std::move(error_response));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter variant_writer(NULL);

  if (property_name == bluetooth_gatt_service::kUUIDProperty) {
    writer.OpenVariant("s", &variant_writer);
    variant_writer.AppendString(uuid_);
    writer.CloseContainer(&variant_writer);
  } else {
    writer.OpenVariant("ao", &variant_writer);
    variant_writer.AppendArrayOfObjectPaths(includes_);
    writer.CloseContainer(&variant_writer);
  }

  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattServiceServiceProviderImpl::Set(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattServiceServiceProviderImpl::Set: "
           << object_path_.value();
  DCHECK(OnOriginThread());
  // All of the properties on this interface are read-only, so just return
  // error.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(method_call, kErrorPropertyReadOnly,
                                          "All properties are read-only.");
  std::move(response_sender).Run(std::move(error_response));
}

void BluetoothGattServiceServiceProviderImpl::GetAll(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattServiceServiceProvider::GetAll: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);

  std::string interface_name;
  if (!reader.PopString(&interface_name) || reader.HasMoreData()) {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(method_call, kErrorInvalidArgs,
                                            "Expected 's'.");
    std::move(response_sender).Run(std::move(error_response));
    return;
  }

  // Only the GATT service interface is allowed.
  if (interface_name !=
      bluetooth_gatt_service::kBluetoothGattServiceInterface) {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidArgs,
            "No such interface: '" + interface_name + "'.");
    std::move(response_sender).Run(std::move(error_response));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  WriteProperties(&writer);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattServiceServiceProviderImpl::WriteProperties(
    dbus::MessageWriter* writer) {
  dbus::MessageWriter array_writer(NULL);
  dbus::MessageWriter dict_entry_writer(NULL);
  dbus::MessageWriter variant_writer(NULL);

  writer->OpenArray("{sv}", &array_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_service::kUUIDProperty);
  dict_entry_writer.AppendVariantOfString(uuid_);
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_service::kPrimaryProperty);
  dict_entry_writer.AppendVariantOfBool(is_primary_);
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_service::kIncludesProperty);
  dict_entry_writer.OpenVariant("ao", &variant_writer);
  variant_writer.AppendArrayOfObjectPaths(includes_);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  writer->CloseContainer(&array_writer);
}

void BluetoothGattServiceServiceProviderImpl::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                        << method_name;
}

const dbus::ObjectPath& BluetoothGattServiceServiceProviderImpl::object_path()
    const {
  return object_path_;
}

}  // namespace bluez
