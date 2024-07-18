// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider_impl.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_helpers.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorPropertyReadOnly[] =
    "org.freedesktop.DBus.Error.PropertyReadOnly";
const char kErrorFailed[] = "org.freedesktop.DBus.Error.Failed";

}  // namespace

// The BluetoothGattDescriptorServiceProvider implementation used in production.
BluetoothGattDescriptorServiceProviderImpl::
    BluetoothGattDescriptorServiceProviderImpl(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path,
        std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
        const std::string& uuid,
        const std::vector<std::string>& flags,
        const dbus::ObjectPath& characteristic_path)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      uuid_(uuid),
      flags_(flags),
      bus_(bus),
      delegate_(std::move(delegate)),
      object_path_(object_path),
      characteristic_path_(characteristic_path) {
  DVLOG(1) << "Created Bluetooth GATT characteristic descriptor: "
           << object_path.value() << " UUID: " << uuid;
  if (!bus_)
    return;

  DCHECK(delegate_);
  DCHECK(!uuid_.empty());
  DCHECK(object_path_.IsValid());
  DCHECK(characteristic_path_.IsValid());
  DCHECK(base::StartsWith(object_path_.value(),
                          characteristic_path_.value() + "/",
                          base::CompareCase::SENSITIVE));

  exported_object_ = bus_->GetExportedObject(object_path_);

  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGet,
      base::BindRepeating(&BluetoothGattDescriptorServiceProviderImpl::Get,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesSet,
      base::BindRepeating(&BluetoothGattDescriptorServiceProviderImpl::Set,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGetAll,
      base::BindRepeating(&BluetoothGattDescriptorServiceProviderImpl::GetAll,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));

  // org.bluez.GattDescriptor1 interface:
  exported_object_->ExportMethod(
      bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
      bluetooth_gatt_descriptor::kReadValue,
      base::BindRepeating(
          &BluetoothGattDescriptorServiceProviderImpl::ReadValue,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
      bluetooth_gatt_descriptor::kWriteValue,
      base::BindRepeating(
          &BluetoothGattDescriptorServiceProviderImpl::WriteValue,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

BluetoothGattDescriptorServiceProviderImpl::
    ~BluetoothGattDescriptorServiceProviderImpl() {
  DVLOG(1) << "Cleaning up Bluetooth GATT characteristic descriptor: "
           << object_path_.value();
  if (bus_)
    bus_->UnregisterExportedObject(object_path_);
}

void BluetoothGattDescriptorServiceProviderImpl::SendValueChanged(
    const std::vector<uint8_t>& value) {
  DVLOG(2) << "Emitting a PropertiesChanged signal for descriptor value.";
  dbus::Signal signal(dbus::kDBusPropertiesInterface,
                      dbus::kDBusPropertiesChangedSignal);
  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter array_writer(NULL);
  dbus::MessageWriter dict_entry_writer(NULL);
  dbus::MessageWriter variant_writer(NULL);

  // interface_name
  writer.AppendString(
      bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface);

  // changed_properties
  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_descriptor::kValueProperty);
  dict_entry_writer.OpenVariant("ay", &variant_writer);
  variant_writer.AppendArrayOfBytes(value);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);

  // invalidated_properties.
  writer.OpenArray("s", &array_writer);
  writer.CloseContainer(&array_writer);

  exported_object_->SendSignal(&signal);
}

bool BluetoothGattDescriptorServiceProviderImpl::OnOriginThread() {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

void BluetoothGattDescriptorServiceProviderImpl::Get(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattDescriptorServiceProvider::Get: "
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

  // Only the GATT descriptor interface is supported.
  if (interface_name !=
      bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface) {
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
  dbus::MessageWriter variant_writer(NULL);

  if (property_name == bluetooth_gatt_descriptor::kUUIDProperty) {
    writer.OpenVariant("s", &variant_writer);
    variant_writer.AppendString(uuid_);
    writer.CloseContainer(&variant_writer);
  } else if (property_name ==
             bluetooth_gatt_descriptor::kCharacteristicProperty) {
    writer.OpenVariant("o", &variant_writer);
    variant_writer.AppendObjectPath(characteristic_path_);
    writer.CloseContainer(&variant_writer);
  } else if (property_name == bluetooth_gatt_descriptor::kFlagsProperty) {
    writer.OpenVariant("as", &variant_writer);
    variant_writer.AppendArrayOfStrings(flags_);
    writer.CloseContainer(&variant_writer);
  } else {
    response = dbus::ErrorResponse::FromMethodCall(
        method_call, kErrorInvalidArgs,
        "No such property: '" + property_name + "'.");
  }

  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattDescriptorServiceProviderImpl::Set(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattDescriptorServiceProviderImpl::Set: "
           << object_path_.value();
  DCHECK(OnOriginThread());
  // All of the properties on this interface are read-only, so just return
  // error.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(method_call, kErrorPropertyReadOnly,
                                          "All properties are read-only.");
  std::move(response_sender).Run(std::move(error_response));
}

void BluetoothGattDescriptorServiceProviderImpl::GetAll(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattDescriptorServiceProvider::GetAll: "
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

  // Only the GATT descriptor interface is supported.
  if (interface_name !=
      bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface) {
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

void BluetoothGattDescriptorServiceProviderImpl::ReadValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattDescriptorServiceProvider::ReadValue: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  std::map<std::string, dbus::MessageReader> options;
  dbus::ObjectPath device_path;
  ReadOptions(&reader, &options);
  auto it = options.find(bluetooth_gatt_descriptor::kOptionDevice);
  if (it != options.end())
    it->second.PopObjectPath(&device_path);

  if (device_path.value().empty()) {
    LOG(WARNING) << "ReadValue called with incorrect parameters: "
                 << method_call->ToString();
    // Continue on with an empty device path. This will return a null device to
    // the delegate, which should know how to handle it.
  }

  // GetValue() promises to only call either the success or error callback.
  auto split_response_sender =
      base::SplitOnceCallback(std::move(response_sender));

  DCHECK(delegate_);
  delegate_->GetValue(
      device_path,
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnReadValue,
                     weak_ptr_factory_.GetWeakPtr(), method_call,
                     std::move(split_response_sender.first)));
}

void BluetoothGattDescriptorServiceProviderImpl::WriteValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattDescriptorServiceProvider::WriteValue: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  const uint8_t* bytes = NULL;
  size_t length = 0;

  std::vector<uint8_t> value;
  if (!reader.PopArrayOfBytes(&bytes, &length)) {
    LOG(WARNING) << "Error reading value parameter. WriteValue called with "
                    "incorrect parameters: "
                 << method_call->ToString();
  }
  if (bytes)
    value.assign(bytes, bytes + length);

  std::map<std::string, dbus::MessageReader> options;
  dbus::ObjectPath device_path;
  ReadOptions(&reader, &options);
  auto it = options.find(bluetooth_gatt_descriptor::kOptionDevice);
  if (it != options.end())
    it->second.PopObjectPath(&device_path);

  if (device_path.value().empty()) {
    LOG(WARNING) << "WriteValue called with incorrect parameters: "
                 << method_call->ToString();
    // Continue on with an empty device path. This will return a null device to
    // the delegate, which should know how to handle it.
  }

  // SetValue() promises to only call either the success or error callback.
  auto split_response_sender =
      base::SplitOnceCallback(std::move(response_sender));

  DCHECK(delegate_);
  delegate_->SetValue(
      device_path, value,
      base::BindOnce(&BluetoothGattDescriptorServiceProviderImpl::OnWriteValue,
                     weak_ptr_factory_.GetWeakPtr(), method_call,
                     std::move(split_response_sender.first)),
      base::BindOnce(
          &BluetoothGattDescriptorServiceProviderImpl::OnWriteFailure,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(split_response_sender.second)));
}

void BluetoothGattDescriptorServiceProviderImpl::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                        << method_name;
}

void BluetoothGattDescriptorServiceProviderImpl::OnReadValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  if (error_code.has_value()) {
    DVLOG(2) << "Failed to get descriptor value. Report error.";
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(method_call, kErrorFailed,
                                            "Failed to get descriptor value.");
    std::move(response_sender).Run(std::move(error_response));
    return;
  }

  DVLOG(3) << "Descriptor value obtained from delegate. Responding to "
              "ReadValue.";

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(value);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattDescriptorServiceProviderImpl::OnWriteValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "Responding to WriteValue.";

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattDescriptorServiceProviderImpl::WriteProperties(
    dbus::MessageWriter* writer) {
  dbus::MessageWriter array_writer(NULL);
  dbus::MessageWriter dict_entry_writer(NULL);
  dbus::MessageWriter variant_writer(NULL);

  writer->OpenArray("{sv}", &array_writer);

  // UUID:
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_descriptor::kUUIDProperty);
  dict_entry_writer.AppendVariantOfString(uuid_);
  array_writer.CloseContainer(&dict_entry_writer);

  // Characteristic:
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(
      bluetooth_gatt_descriptor::kCharacteristicProperty);
  dict_entry_writer.AppendVariantOfObjectPath(characteristic_path_);
  array_writer.CloseContainer(&dict_entry_writer);

  // Flags:
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_descriptor::kFlagsProperty);
  dict_entry_writer.OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(flags_);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  writer->CloseContainer(&array_writer);
}

void BluetoothGattDescriptorServiceProviderImpl::OnWriteFailure(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "Failed to set descriptor value. Report error.";
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(method_call, kErrorFailed,
                                          "Failed to set descriptor value.");
  std::move(response_sender).Run(std::move(error_response));
}

const dbus::ObjectPath&
BluetoothGattDescriptorServiceProviderImpl::object_path() const {
  return object_path_;
}

}  // namespace bluez
