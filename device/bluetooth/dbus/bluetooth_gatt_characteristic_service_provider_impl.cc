// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider_impl.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_helpers.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_delegate_wrapper.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorPropertyReadOnly[] =
    "org.freedesktop.DBus.Error.PropertyReadOnly";
const char kErrorFailed[] = "org.freedesktop.DBus.Error.Failed";

}  // namespace

BluetoothGattCharacteristicServiceProviderImpl::
    BluetoothGattCharacteristicServiceProviderImpl(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path,
        std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
        const std::string& uuid,
        const std::vector<std::string>& flags,
        const dbus::ObjectPath& service_path)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      uuid_(uuid),
      flags_(flags),
      bus_(bus),
      delegate_(std::move(delegate)),
      object_path_(object_path),
      service_path_(service_path) {
  DVLOG(1) << "Created Bluetooth GATT characteristic: " << object_path.value()
           << " UUID: " << uuid;
  if (!bus_)
    return;

  DCHECK(delegate_);
  DCHECK(!uuid_.empty());
  DCHECK(object_path_.IsValid());
  DCHECK(service_path_.IsValid());
  DCHECK(base::StartsWith(object_path_.value(), service_path_.value() + "/",
                          base::CompareCase::SENSITIVE));

  exported_object_ = bus_->GetExportedObject(object_path_);

  // org.freedesktop.DBus.Properties interface:
  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGet,
      base::BindRepeating(&BluetoothGattCharacteristicServiceProviderImpl::Get,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesSet,
      base::BindRepeating(&BluetoothGattCharacteristicServiceProviderImpl::Set,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGetAll,
      base::BindRepeating(
          &BluetoothGattCharacteristicServiceProviderImpl::GetAll,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));

  // org.bluez.GattCharacteristic1 interface:
  exported_object_->ExportMethod(
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
      bluetooth_gatt_characteristic::kReadValue,
      base::BindRepeating(
          &BluetoothGattCharacteristicServiceProviderImpl::ReadValue,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
      bluetooth_gatt_characteristic::kWriteValue,
      base::BindRepeating(
          &BluetoothGattCharacteristicServiceProviderImpl::WriteValue,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
      bluetooth_gatt_characteristic::kPrepareWriteValue,
      base::BindRepeating(
          &BluetoothGattCharacteristicServiceProviderImpl::PrepareWriteValue,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
      bluetooth_gatt_characteristic::kStartNotify,
      base::BindRepeating(
          &BluetoothGattCharacteristicServiceProviderImpl::StartNotify,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
  exported_object_->ExportMethod(
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
      bluetooth_gatt_characteristic::kStopNotify,
      base::BindRepeating(
          &BluetoothGattCharacteristicServiceProviderImpl::StopNotify,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
}

BluetoothGattCharacteristicServiceProviderImpl::
    ~BluetoothGattCharacteristicServiceProviderImpl() {
  DVLOG(1) << "Cleaning up Bluetooth GATT characteristic: "
           << object_path_.value();
  if (bus_)
    bus_->UnregisterExportedObject(object_path_);
}

void BluetoothGattCharacteristicServiceProviderImpl::SendValueChanged(
    const std::vector<uint8_t>& value) {
  // Running a test, don't actually try to write to use DBus.
  if (!bus_)
    return;

  DVLOG(2) << "Emitting a PropertiesChanged signal for characteristic value.";
  dbus::Signal signal(dbus::kDBusPropertiesInterface,
                      dbus::kDBusPropertiesChangedSignal);
  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_entry_writer(nullptr);
  dbus::MessageWriter variant_writer(nullptr);

  // interface_name
  writer.AppendString(
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);

  // changed_properties
  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_characteristic::kValueProperty);
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

// Returns true if the current thread is on the origin thread.
bool BluetoothGattCharacteristicServiceProviderImpl::OnOriginThread() {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

void BluetoothGattCharacteristicServiceProviderImpl::Get(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattCharacteristicServiceProvider::Get: "
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

  // Only the GATT characteristic interface is supported.
  if (interface_name !=
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface) {
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
  dbus::MessageWriter variant_writer(nullptr);

  if (property_name == bluetooth_gatt_characteristic::kUUIDProperty) {
    writer.OpenVariant("s", &variant_writer);
    variant_writer.AppendString(uuid_);
    writer.CloseContainer(&variant_writer);
  } else if (property_name == bluetooth_gatt_characteristic::kServiceProperty) {
    writer.OpenVariant("o", &variant_writer);
    variant_writer.AppendObjectPath(service_path_);
    writer.CloseContainer(&variant_writer);
  } else if (property_name == bluetooth_gatt_characteristic::kFlagsProperty) {
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

void BluetoothGattCharacteristicServiceProviderImpl::Set(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattCharacteristicServiceProviderImpl::Set: "
           << object_path_.value();
  DCHECK(OnOriginThread());
  // All of the properties on this interface are read-only, so just return
  // error.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(method_call, kErrorPropertyReadOnly,
                                          "All properties are read-only.");
  std::move(response_sender).Run(std::move(error_response));
}

void BluetoothGattCharacteristicServiceProviderImpl::GetAll(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "BluetoothGattCharacteristicServiceProvider::GetAll: "
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

  // Only the GATT characteristic interface is supported.
  if (interface_name !=
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface) {
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

void BluetoothGattCharacteristicServiceProviderImpl::ReadValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattCharacteristicServiceProvider::ReadValue: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  std::map<std::string, dbus::MessageReader> options;
  dbus::ObjectPath device_path;
  ReadOptions(&reader, &options);
  auto it = options.find(bluetooth_gatt_characteristic::kOptionDevice);
  if (it != options.end())
    it->second.PopObjectPath(&device_path);

  if (device_path.value().empty()) {
    LOG(WARNING) << "ReadValue called with incorrect parameters: "
                 << method_call->ToString();
    // Continue on with an empty device path. This will return a null device to
    // the delegate, which should know how to handle it.
  }

  DCHECK(delegate_);
  delegate_->GetValue(
      device_path,
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnReadValue,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(response_sender)));
}

void BluetoothGattCharacteristicServiceProviderImpl::WriteValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattCharacteristicServiceProvider::WriteValue: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  const uint8_t* bytes = nullptr;
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
  auto it = options.find(bluetooth_gatt_characteristic::kOptionDevice);
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
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnWriteValue,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(split_response_sender.first)),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnWriteValueFailure,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(split_response_sender.second)));
}

void BluetoothGattCharacteristicServiceProviderImpl::PrepareWriteValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattCharacteristicServiceProvider::PrepareWriteValue: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  const uint8_t* bytes = nullptr;
  size_t length = 0;

  std::vector<uint8_t> value;
  if (!reader.PopArrayOfBytes(&bytes, &length)) {
    LOG(WARNING) << "Error reading value parameter. PrepareWriteValue called "
                 << "with incorrect parameters: " << method_call->ToString();
  }
  if (bytes)
    value.assign(bytes, bytes + length);

  std::map<std::string, dbus::MessageReader> options;
  dbus::ObjectPath device_path;
  uint16_t offset = 0;
  bool has_subsequent_write = false;
  ReadOptions(&reader, &options);
  auto it = options.find(bluetooth_gatt_characteristic::kOptionDevice);
  if (it != options.end())
    it->second.PopObjectPath(&device_path);
  it = options.find(bluetooth_gatt_characteristic::kOptionOffset);
  if (it != options.end())
    it->second.PopUint16(&offset);
  it = options.find(bluetooth_gatt_characteristic::kOptionHasSubsequentWrite);
  if (it != options.end())
    it->second.PopBool(&has_subsequent_write);

  if (device_path.value().empty()) {
    LOG(WARNING) << "PrepareWriteValue called with incorrect parameters: "
                 << method_call->ToString();
    // Continue on with an empty device path. This will return a null device to
    // the delegate, which should know how to handle it.
  }

  // PrepareSetValue() promises to only call either the success or error
  // callback.
  auto split_response_sender =
      base::SplitOnceCallback(std::move(response_sender));

  DCHECK(delegate_);
  delegate_->PrepareSetValue(
      device_path, value, offset, has_subsequent_write,
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnWriteValue,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(split_response_sender.first)),
      base::BindOnce(
          &BluetoothGattCharacteristicServiceProviderImpl::OnWriteValueFailure,
          weak_ptr_factory_.GetWeakPtr(), method_call,
          std::move(split_response_sender.second)));
}

void BluetoothGattCharacteristicServiceProviderImpl::StartNotify(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattCharacteristicServiceProvider::StartNotify: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  uint8_t cccd_value = 0;
  if (!reader.PopByte(&cccd_value)) {
    LOG(WARNING) << "Error reading cccd_value parameter. StartNotify called "
                 << "with incorrect parameters: " << method_call->ToString();
  }

  std::map<std::string, dbus::MessageReader> options;
  dbus::ObjectPath device_path;
  ReadOptions(&reader, &options);
  auto it = options.find(bluetooth_gatt_characteristic::kOptionDevice);
  if (it != options.end())
    it->second.PopObjectPath(&device_path);

  DCHECK(delegate_);
  delegate_->StartNotifications(
      device_path,
      cccd_value == static_cast<uint8_t>(device::BluetoothGattCharacteristic::
                                             NotificationType::kIndication)
          ? device::BluetoothGattCharacteristic::NotificationType::kIndication
          : device::BluetoothGattCharacteristic::NotificationType::
                kNotification);

  // This is temporary fix to unblock b/191129417.
  // TODO(b/191129417); Plumb the callback up to ARC++ and add unit test.
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattCharacteristicServiceProviderImpl::StopNotify(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "BluetoothGattCharacteristicServiceProvider::StopNotify: "
           << object_path_.value();
  DCHECK(OnOriginThread());

  dbus::MessageReader reader(method_call);
  std::map<std::string, dbus::MessageReader> options;
  dbus::ObjectPath device_path;
  ReadOptions(&reader, &options);
  auto it = options.find(bluetooth_gatt_characteristic::kOptionDevice);
  if (it != options.end())
    it->second.PopObjectPath(&device_path);

  DCHECK(delegate_);
  delegate_->StopNotifications(device_path);

  // This is temporary fix to unblock b/191129417.
  // TODO(b/191129417); Plumb the callback up to ARC++ and add unit test.
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattCharacteristicServiceProviderImpl::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                        << method_name;
}

void BluetoothGattCharacteristicServiceProviderImpl::OnReadValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  if (error_code.has_value()) {
    DVLOG(2) << "Failed to read characteristic value. Report error.";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorFailed, "Failed to read characteristic value."));
    return;
  }

  DVLOG(3) << "Characteristic value obtained from delegate. Responding to "
              "ReadValue.";

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(value);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattCharacteristicServiceProviderImpl::OnWriteValue(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(3) << "Responding to WriteValue.";

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void BluetoothGattCharacteristicServiceProviderImpl::WriteProperties(
    dbus::MessageWriter* writer) {
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_entry_writer(nullptr);
  dbus::MessageWriter variant_writer(nullptr);

  writer->OpenArray("{sv}", &array_writer);

  // UUID:
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_characteristic::kUUIDProperty);
  dict_entry_writer.AppendVariantOfString(uuid_);
  array_writer.CloseContainer(&dict_entry_writer);

  // Service:
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(
      bluetooth_gatt_characteristic::kServiceProperty);
  dict_entry_writer.AppendVariantOfObjectPath(service_path_);
  array_writer.CloseContainer(&dict_entry_writer);

  // Flags:
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_gatt_characteristic::kFlagsProperty);
  dict_entry_writer.OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(flags_);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  writer->CloseContainer(&array_writer);
}

void BluetoothGattCharacteristicServiceProviderImpl::OnWriteValueFailure(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(2) << "Failed to set characteristic value. Report error.";
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(
          method_call, kErrorFailed, "Failed to set characteristic value.");
  std::move(response_sender).Run(std::move(error_response));
}

const dbus::ObjectPath&
BluetoothGattCharacteristicServiceProviderImpl::object_path() const {
  return object_path_;
}

}  // namespace bluez
