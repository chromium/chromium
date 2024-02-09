// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider_impl.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {
constexpr char kOrPatternsFilterType[] = "or_patterns";
}

namespace bluez {

BluetoothAdvertisementMonitorServiceProviderImpl::
    BluetoothAdvertisementMonitorServiceProviderImpl(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path,
        std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
        base::WeakPtr<BluetoothAdvertisementMonitorServiceProvider::Delegate>
            delegate)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      bus_(bus),
      object_path_(object_path),
      filter_(std::move(filter)),
      delegate_(delegate) {
  DVLOG(2)
      << "Created Bluetooth Bluetooth Advertisement Monitor Service Provider: "
      << object_path.value();

  DCHECK(bus_);
  DCHECK(object_path_.IsValid());

  exported_object_ = bus_->GetExportedObject(object_path_);
  if (!exported_object_) {
    LOG(WARNING) << "Invalid exported_object";
    return;
  }

  exported_object_->ExportMethod(
      bluetooth_advertisement_monitor::kBluetoothAdvertisementMonitorInterface,
      bluetooth_advertisement_monitor::kRelease,
      base::BindRepeating(
          &BluetoothAdvertisementMonitorServiceProviderImpl::Release,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothAdvertisementMonitorServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      bluetooth_advertisement_monitor::kBluetoothAdvertisementMonitorInterface,
      bluetooth_advertisement_monitor::kActivate,
      base::BindRepeating(
          &BluetoothAdvertisementMonitorServiceProviderImpl::Activate,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothAdvertisementMonitorServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      bluetooth_advertisement_monitor::kBluetoothAdvertisementMonitorInterface,
      bluetooth_advertisement_monitor::kDeviceFound,
      base::BindRepeating(
          &BluetoothAdvertisementMonitorServiceProviderImpl::DeviceFound,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothAdvertisementMonitorServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));

  exported_object_->ExportMethod(
      bluetooth_advertisement_monitor::kBluetoothAdvertisementMonitorInterface,
      bluetooth_advertisement_monitor::kDeviceLost,
      base::BindRepeating(
          &BluetoothAdvertisementMonitorServiceProviderImpl::DeviceLost,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothAdvertisementMonitorServiceProviderImpl::OnExported,
          weak_ptr_factory_.GetWeakPtr()));
}

BluetoothAdvertisementMonitorServiceProviderImpl::
    ~BluetoothAdvertisementMonitorServiceProviderImpl() {
  DVLOG(2) << "Cleaning up Bluetooth Advertisement Monitor Service: "
           << object_path_.value();
  DCHECK(bus_);
  bus_->UnregisterExportedObject(object_path_);
}

bool BluetoothAdvertisementMonitorServiceProviderImpl::OnOriginThread() const {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

void BluetoothAdvertisementMonitorServiceProviderImpl::Release(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!delegate_) {
    DVLOG(2) << "Could not forward D-Bus callback: Invalid delegate";
    return;
  }
  delegate_->OnRelease();
}

void BluetoothAdvertisementMonitorServiceProviderImpl::Activate(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!delegate_) {
    DVLOG(2) << "Could not forward D-Bus callback: Invalid delegate";
    return;
  }
  delegate_->OnActivate();
}

void BluetoothAdvertisementMonitorServiceProviderImpl::DeviceFound(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!delegate_) {
    DVLOG(2) << "Could not forward D-Bus callback: Invalid delegate";
    return;
  }

  dbus::MessageReader reader(method_call);
  dbus::ObjectPath device_path;
  if (!reader.PopObjectPath(&device_path)) {
    LOG(WARNING) << "DeviceFound called with incorrect parameters: "
                 << method_call->ToString();
    return;
  }
  delegate_->OnDeviceFound(device_path);
}

void BluetoothAdvertisementMonitorServiceProviderImpl::DeviceLost(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!delegate_) {
    DVLOG(2) << "Could not forward D-Bus callback: Invalid delegate";
    return;
  }
  dbus::MessageReader reader(method_call);
  dbus::ObjectPath device_path;
  if (!reader.PopObjectPath(&device_path)) {
    LOG(WARNING) << "DeviceLost called with incorrect paramters: "
                 << method_call->ToString();
    return;
  }
  delegate_->OnDeviceLost(device_path);
}

void BluetoothAdvertisementMonitorServiceProviderImpl::WriteProperties(
    dbus::MessageWriter* writer) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  dbus::MessageWriter dict_entry_writer(nullptr);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_advertisement_monitor::kType);
  // or_patterns is the only supported type.
  dict_entry_writer.AppendVariantOfString(kOrPatternsFilterType);
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(
      bluetooth_advertisement_monitor::kRSSIHighThreshold);
  dict_entry_writer.AppendVariantOfInt16(
      filter_->device_found_rssi_threshold());
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(
      bluetooth_advertisement_monitor::kRSSIHighTimeout);
  dict_entry_writer.AppendVariantOfUint16(
      filter_->device_found_timeout().InSeconds());
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(
      bluetooth_advertisement_monitor::kRSSILowThreshold);
  dict_entry_writer.AppendVariantOfInt16(filter_->device_lost_rssi_threshold());
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(
      bluetooth_advertisement_monitor::kRSSILowTimeout);
  dict_entry_writer.AppendVariantOfUint16(
      filter_->device_lost_timeout().InSeconds());
  array_writer.CloseContainer(&dict_entry_writer);

  if (filter_->rssi_sampling_period().has_value()) {
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement_monitor::kRSSISamplingPeriod);

    // Convert from ms to 100ms * N format.
    dict_entry_writer.AppendVariantOfUint16(
        filter_->rssi_sampling_period().value().InMilliseconds() / 100);
    array_writer.CloseContainer(&dict_entry_writer);
  }

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(bluetooth_advertisement_monitor::kPatterns);

  dbus::MessageWriter variant_writer(nullptr);
  dict_entry_writer.OpenVariant("a(yyay)", &variant_writer);

  dbus::MessageWriter pattern_array_writer(nullptr);
  variant_writer.OpenArray("(yyay)", &pattern_array_writer);
  for (auto& pattern : filter_->patterns()) {
    WritePattern(&pattern_array_writer, pattern.start_position(),
                 static_cast<uint8_t>(pattern.data_type()), pattern.value());
  }
  variant_writer.CloseContainer(&pattern_array_writer);

  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  writer->CloseContainer(&array_writer);
}

void BluetoothAdvertisementMonitorServiceProviderImpl::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                        << method_name;
}

void BluetoothAdvertisementMonitorServiceProviderImpl::WritePattern(
    dbus::MessageWriter* pattern_array_writer,
    uint8_t start_pos,
    uint8_t ad_data_type,
    base::span<const uint8_t> pattern) {
  dbus::MessageWriter struct_writer(nullptr);
  pattern_array_writer->OpenStruct(&struct_writer);

  struct_writer.AppendByte(start_pos);
  struct_writer.AppendByte(ad_data_type);
  struct_writer.AppendArrayOfBytes(pattern);
  pattern_array_writer->CloseContainer(&struct_writer);
}

const dbus::ObjectPath&
BluetoothAdvertisementMonitorServiceProviderImpl::object_path() const {
  return object_path_;
}

}  // namespace bluez
