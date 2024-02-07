// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_lescan_client.h"

#include <algorithm>
#include <map>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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

ScanFilterPattern::ScanFilterPattern() = default;
ScanFilterPattern::ScanFilterPattern(const ScanFilterPattern&) = default;
ScanFilterPattern::~ScanFilterPattern() = default;

ScanFilterCondition::ScanFilterCondition() = default;
ScanFilterCondition::ScanFilterCondition(const ScanFilterCondition&) = default;
ScanFilterCondition::~ScanFilterCondition() = default;

ScanFilter::ScanFilter() = default;
ScanFilter::ScanFilter(const ScanFilter&) = default;
ScanFilter::~ScanFilter() = default;

ScanResult::ScanResult() = default;
ScanResult::ScanResult(const ScanResult&) = default;
ScanResult::~ScanResult() = default;

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
  while (!pending_register_scanners_.empty()) {
    std::move(pending_register_scanners_.front())
        .Run(base::unexpected(Error(kNoCallbackRegistered, "")));
    pending_register_scanners_.pop();
  }
}

void FlossLEScanClient::Init(dbus::Bus* bus,
                             const std::string& service_name,
                             const int adapter_index,
                             base::Version version,
                             base::OnceClosure on_ready) {
  bus_ = bus;
  object_path_ = FlossDBusClient::GenerateGattPath(adapter_index);
  service_name_ = service_name;
  version_ = version;

  exported_scanner_callback_manager_.Init(bus);

  exported_scanner_callback_manager_.AddMethod(
      adapter::kOnScannerRegistered, &ScannerClientObserver::ScannerRegistered);
  exported_scanner_callback_manager_.AddMethod(
      adapter::kOnScanResult, &ScannerClientObserver::ScanResultReceived);
  exported_scanner_callback_manager_.AddMethod(
      adapter::kOnAdvertisementFound,
      &ScannerClientObserver::AdvertisementFound);
  exported_scanner_callback_manager_.AddMethod(
      adapter::kOnAdvertisementLost, &ScannerClientObserver::AdvertisementLost);

  dbus::ObjectPath callback_path(kScannerCallbackPath);

  if (!exported_scanner_callback_manager_.ExportCallback(
          callback_path, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossLEScanClient::RegisterScannerCallback,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Failed exporting callback " + callback_path.value();
    return;
  }

  on_ready_ = std::move(on_ready);
}

void FlossLEScanClient::AddObserver(ScannerClientObserver* observer) {
  observers_.AddObserver(observer);
}

void FlossLEScanClient::RemoveObserver(ScannerClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FlossLEScanClient::RegisterScannerCallback() {
  CallLEScanMethod<>(
      base::BindOnce(&FlossLEScanClient::OnRegisterScannerCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      adapter::kRegisterScannerCallback,
      dbus::ObjectPath(kScannerCallbackPath));
}

void FlossLEScanClient::OnRegisterScannerCallback(DBusResult<uint32_t> ret) {
  if (!ret.has_value() || *ret == 0) {
    LOG(ERROR) << "Failed RegisterScannerCallback";
    exported_scanner_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kScannerCallbackPath));
    return;
  }

  le_scan_callback_id_ = ret.value();

  // Mark client as ready to use.
  if (on_ready_) {
    std::move(on_ready_).Run();
  }

  while (!pending_register_scanners_.empty()) {
    CallLEScanMethod<>(std::move(pending_register_scanners_.front()),
                       adapter::kRegisterScanner, le_scan_callback_id_.value());
    pending_register_scanners_.pop();
  }
}

void FlossLEScanClient::OnUnregisterScannerCallback(DBusResult<bool> ret) {
  if (!ret.has_value() || *ret == false) {
    LOG(ERROR) << "Failed OnUnregisterScannerCallback";
  }
}

void FlossLEScanClient::RegisterScanner(
    ResponseCallback<device::BluetoothUUID> callback) {
  if (!le_scan_callback_id_) {
    LOG(WARNING) << "RegisterScanner called before callback ID was available. "
                    "Queueing to register when callback ID is available.";

    // Add to queue to register when callback ID is available
    pending_register_scanners_.push(std::move(callback));
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

void FlossLEScanClient::StartScan(
    ResponseCallback<BtifStatus> callback,
    uint8_t scanner_id,
    const std::optional<ScanSettings>& scan_settings,
    const std::optional<ScanFilter>& filter) {
  if (version_ >= base::Version("0.3")) {
    CallLEScanMethod<>(std::move(callback), adapter::kStartScan, scanner_id,
                       scan_settings, filter);
  } else {
    CallLEScanMethod<>(std::move(callback), adapter::kStartScan, scanner_id,
                       ScanSettings{}, filter);
  }
}

void FlossLEScanClient::StopScan(ResponseCallback<BtifStatus> callback,
                                 uint8_t scanner_id) {
  CallLEScanMethod<>(std::move(callback), adapter::kStopScan, scanner_id);
}

void FlossLEScanClient::ScannerRegistered(device::BluetoothUUID uuid,
                                          uint8_t scanner_id,
                                          GattStatus status) {
  for (auto& observer : observers_) {
    observer.ScannerRegistered(uuid, scanner_id, status);
  }
}

void FlossLEScanClient::ScanResultReceived(ScanResult scan_result) {
  for (auto& observer : observers_) {
    observer.ScanResultReceived(scan_result);
  }
}

void FlossLEScanClient::AdvertisementFound(uint8_t scanner_id,
                                           ScanResult scan_result) {
  for (auto& observer : observers_) {
    observer.AdvertisementFound(scanner_id, scan_result);
  }
}

void FlossLEScanClient::AdvertisementLost(uint8_t scanner_id,
                                          ScanResult scan_result) {
  for (auto& observer : observers_) {
    observer.AdvertisementLost(scanner_id, scan_result);
  }
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanSettings& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "interval", data.interval);
  WriteDictEntry(&array_writer, "window", data.window);
  WriteDictEntry(&array_writer, "scan_type", data.scan_type);

  writer->CloseContainer(&array_writer);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanType& type) {
  int32_t value = static_cast<uint32_t>(type);
  WriteDBusParam(writer, value);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanFilterPattern& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "start_position", data.start_position);
  WriteDictEntry(&array_writer, "ad_type", data.ad_type);
  WriteDictEntry(&array_writer, "content", data.content);

  writer->CloseContainer(&array_writer);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanFilterCondition& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "patterns", data.patterns);

  writer->CloseContainer(&array_writer);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const ScanFilter& data) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "rssi_high_threshold",
                 static_cast<uint8_t>(data.rssi_high_threshold));
  WriteDictEntry(&array_writer, "rssi_low_threshold",
                 static_cast<uint8_t>(data.rssi_low_threshold));
  WriteDictEntry(&array_writer, "rssi_low_timeout",
                 static_cast<uint8_t>(data.rssi_low_timeout));
  WriteDictEntry(&array_writer, "rssi_sampling_period",
                 static_cast<uint8_t>(data.rssi_sampling_period));
  WriteDictEntry(&array_writer, "condition", data.condition);

  writer->CloseContainer(&array_writer);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    ScanResult* scan_result) {
  static StructReader<ScanResult> struct_reader({
      {"name", CreateFieldReader(&ScanResult::name)},
      {"address", CreateFieldReader(&ScanResult::address)},
      {"addr_type", CreateFieldReader(&ScanResult::addr_type)},
      {"event_type", CreateFieldReader(&ScanResult::event_type)},
      {"primary_phy", CreateFieldReader(&ScanResult::primary_phy)},
      {"secondary_phy", CreateFieldReader(&ScanResult::secondary_phy)},
      {"advertising_sid", CreateFieldReader(&ScanResult::advertising_sid)},
      {"tx_power", CreateFieldReader(&ScanResult::tx_power)},
      {"rssi", CreateFieldReader(&ScanResult::rssi)},
      {"periodic_adv_int", CreateFieldReader(&ScanResult::periodic_adv_int)},
      {"flags", CreateFieldReader(&ScanResult::flags)},
      {"service_uuids", CreateFieldReader(&ScanResult::service_uuids)},
      {"service_data", CreateFieldReader(&ScanResult::service_data)},
      {"manufacturer_data", CreateFieldReader(&ScanResult::manufacturer_data)},
      {"adv_data", CreateFieldReader(&ScanResult::adv_data)},
  });

  return struct_reader.ReadDBusParam(reader, scan_result);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const ScanSettings*) {
  static DBusTypeInfo info{"a{sv}", "ScanSettings"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const ScanType*) {
  static DBusTypeInfo info{"u", "ScanType"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const ScanFilterPattern*) {
  static DBusTypeInfo info{"a{sv}", "ScanFilterPattern"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const ScanFilterCondition*) {
  static DBusTypeInfo info{"a{sv}", "ScanFilterCondition"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const ScanFilter*) {
  static DBusTypeInfo info{"a{sv}", "ScanFilter"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const ScanResult*) {
  static DBusTypeInfo info{"a{sv}", "ScanResult"};
  return info;
}

}  // namespace floss
