// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_lescan_client.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

const char kNoCallbackRegistered[] =
    "org.chromium.bluetooth.Error.NoCallbackRegistered";

std::unique_ptr<FlossLEScanClient> FlossLEScanClient::Create() {
  return std::make_unique<FlossLEScanClient>();
}

FlossLEScanClient::FlossLEScanClient() = default;
FlossLEScanClient::~FlossLEScanClient() {
  if (le_scan_callback_id_) {
    CallLEScanMethod<>(
        base::BindOnce(&FlossLEScanClient::OnUnregisterScannerCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        adapter::kUnregisterScannerCallback, le_scan_callback_id_.value());
  }
  if (bus_) {
    exported_scanner_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kScannerCallbackPath));
  }
}

void FlossLEScanClient::Init(dbus::Bus* bus,
                             const std::string& service_name,
                             const int adapter_index) {
  bus_ = bus;
  object_path_ = FlossDBusClient::GenerateGattPath(adapter_index);
  service_name_ = service_name;

  exported_scanner_callback_manager_.Init(bus);

  exported_scanner_callback_manager_.AddMethod(
      adapter::kOnScannerRegistered, &ScannerClientObserver::ScannerRegistered);
  exported_scanner_callback_manager_.AddMethod(
      adapter::kOnScanResult, &ScannerClientObserver::ScanResultReceived);

  RegisterScannerCallback();
}

void FlossLEScanClient::AddObserver(ScannerClientObserver* observer) {
  observers_.AddObserver(observer);
}

void FlossLEScanClient::RemoveObserver(ScannerClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FlossLEScanClient::RegisterScannerCallback() {
  dbus::ObjectPath callback_path(kScannerCallbackPath);

  if (!exported_scanner_callback_manager_.ExportCallback(
          callback_path, weak_ptr_factory_.GetWeakPtr())) {
    LOG(ERROR) << "Failed exporting callback " + callback_path.value();
    return;
  }

  CallLEScanMethod<>(
      base::BindOnce(&FlossLEScanClient::OnRegisterScannerCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      adapter::kRegisterScannerCallback, callback_path);

  dbus::ExportedObject* exported_callback =
      bus_->GetExportedObject(callback_path);
  if (!exported_callback) {
    LOG(ERROR) << "FlossLEScanClient couldn't export client callbacks";
    return;
  }
}

void FlossLEScanClient::OnRegisterScannerCallback(DBusResult<uint32_t> ret) {
  if (!ret.has_value() || *ret == 0) {
    LOG(ERROR) << "Failed RegisterScannerCallback";
    exported_scanner_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kScannerCallbackPath));
    return;
  }

  le_scan_callback_id_ = ret.value();
}

void FlossLEScanClient::OnUnregisterScannerCallback(DBusResult<bool> ret) {
  if (!ret.has_value() || *ret == false) {
    LOG(ERROR) << "Failed OnUnregisterScannerCallback";
  }
}

void FlossLEScanClient::RegisterScanner(
    ResponseCallback<device::BluetoothUUID> callback) {
  if (!le_scan_callback_id_) {
    // callback ID required before registering scanners
    std::move(callback).Run(base::unexpected(Error(kNoCallbackRegistered, "")));
    return;
  }

  CallLEScanMethod<>(std::move(callback), adapter::kRegisterScanner,
                     le_scan_callback_id_.value());
}

void FlossLEScanClient::UnregisterScanner(ResponseCallback<bool> callback,
                                          uint8_t scanner_id) {
  CallLEScanMethod<>(std::move(callback), adapter::kUnregisterScanner,
                     scanner_id);
}

void FlossLEScanClient::StartScan(ResponseCallback<Void> callback,
                                  uint8_t scanner_id,
                                  const ScanSettings& scan_settings,
                                  const std::vector<ScanFilter>& filters) {
  CallLEScanMethod<>(std::move(callback), adapter::kStartScan, scanner_id,
                     scan_settings, filters);
}

void FlossLEScanClient::StopScan(ResponseCallback<Void> callback,
                                 uint8_t scanner_id) {
  CallLEScanMethod<>(std::move(callback), adapter::kStopScan, scanner_id);
}

void FlossLEScanClient::ScannerRegistered(device::BluetoothUUID uuid,
                                          uint8_t scanner_id,
                                          uint8_t status) {
  for (auto& observer : observers_) {
    observer.ScannerRegistered(uuid, scanner_id, status);
  }
}

void FlossLEScanClient::ScanResultReceived(ScanResult scan_result) {
  for (auto& observer : observers_) {
    observer.ScanResultReceived(scan_result);
  }
}

// TODO(b/217274013): Update these templates when structs in place
template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const RSSISettings& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{si}", &array_writer);

  WriteDictEntry(&array_writer, "low_threshold", static_cast<int32_t>(3));
  WriteDictEntry(&array_writer, "high_threshold", static_cast<int32_t>(3));

  writer->CloseContainer(&array_writer);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanSettings& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "interval", static_cast<int32_t>(3));
  WriteDictEntry(&array_writer, "window", static_cast<int32_t>(3));
  WriteDictEntry(&array_writer, "scan_type", static_cast<uint32_t>(1));
  WriteDictEntry(&array_writer, "rssi_settings", RSSISettings());

  writer->CloseContainer(&array_writer);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanFilter& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  // TODO(b/217274013): Write fields here.

  writer->CloseContainer(&array_writer);
}

template <typename T>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const std::vector<T>& value) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("a{sv}", &array_writer);
  for (const auto& entry : value) {
    WriteDBusParam<T>(&array_writer, entry);
  }
  writer->CloseContainer(&array_writer);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    ScanResult* scan_result) {
  static StructReader<ScanResult> struct_reader({
      {"address", CreateFieldReader(&ScanResult::address)},
      {"addr_type", CreateFieldReader(&ScanResult::addr_type)},
  });

  return struct_reader.ReadDBusParam(reader, scan_result);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<RSSISettings>() {
  static DBusTypeInfo info{"a{sv}", "RSSISettings"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<ScanSettings>() {
  static DBusTypeInfo info{"a{sv}", "ScanSettings"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<ScanFilter>() {
  static DBusTypeInfo info{"a{sv}", "ScanFilter"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<ScanResult>() {
  static DBusTypeInfo info{"a{sv}", "ScanResult"};
  return info;
}

}  // namespace floss
