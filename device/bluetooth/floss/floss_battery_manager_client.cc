// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_battery_manager_client.h"

namespace floss {

// Template specializations for dbus parsing

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    Battery* battery) {
  static FlossDBusClient::StructReader<Battery> struct_reader(
      {{"percentage", CreateFieldReader(&Battery::percentage)},
       {"variant", CreateFieldReader(&Battery::variant)}});

  return struct_reader.ReadDBusParam(reader, battery);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const Battery*) {
  static DBusTypeInfo info{"a{sv}", "Battery"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const Battery& battery) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "percentage", battery.percentage);
  WriteDictEntry(&array_writer, "variant", battery.variant);

  writer->CloseContainer(&array_writer);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    BatterySet* battery_set) {
  static FlossDBusClient::StructReader<BatterySet> struct_reader(
      {{"address", CreateFieldReader(&BatterySet::address)},
       {"source_uuid", CreateFieldReader(&BatterySet::source_uuid)},
       {"source_info", CreateFieldReader(&BatterySet::source_info)},
       {"batteries", CreateFieldReader(&BatterySet::batteries)}});

  return struct_reader.ReadDBusParam(reader, battery_set);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const BatterySet*) {
  static DBusTypeInfo info{"a{sv}", "BatterySet"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const BatterySet& battery_set) {
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);

  WriteDictEntry(&array_writer, "address", battery_set.address);
  WriteDictEntry(&array_writer, "source_uuid", battery_set.source_uuid);
  WriteDictEntry(&array_writer, "source_info", battery_set.source_info);
  WriteDictEntry(&array_writer, "batteries", battery_set.batteries);

  writer->CloseContainer(&array_writer);
}

Battery::Battery() = default;
Battery::~Battery() = default;

BatterySet::BatterySet() = default;
BatterySet::BatterySet(const BatterySet&) = default;
BatterySet::~BatterySet() = default;

const char FlossBatteryManagerClient::kExportedCallbacksPath[] =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "/org/chromium/bluetooth/battery_manager/callback/lacros";
#else
    "/org/chromium/bluetooth/battery_manager/callback";
#endif

void FlossBatteryManagerClient::AddObserver(
    FlossBatteryManagerClient::FlossBatteryManagerClientObserver* observer) {
  observers_.AddObserver(observer);
}

void FlossBatteryManagerClient::RemoveObserver(
    FlossBatteryManagerClient::FlossBatteryManagerClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<FlossBatteryManagerClient> FlossBatteryManagerClient::Create() {
  return std::make_unique<FlossBatteryManagerClient>();
}

FlossBatteryManagerClient::FlossBatteryManagerClient() = default;
FlossBatteryManagerClient::~FlossBatteryManagerClient() {
  if (battery_manager_callback_id_) {
    CallBatteryManagerMethod<bool>(
        base::BindOnce(&FlossBatteryManagerClient::BatteryCallbackUnregistered,
                       weak_ptr_factory_.GetWeakPtr()),
        battery_manager::kUnregisterBatteryCallback,
        battery_manager_callback_id_.value());
  }
  if (bus_) {
    exported_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kExportedCallbacksPath));
  }
}

void FlossBatteryManagerClient::GetBatteryInformation(
    ResponseCallback<std::optional<BatterySet>> callback,
    const FlossDeviceId& device) {
  CallBatteryManagerMethod<std::optional<BatterySet>>(
      std::move(callback), battery_manager::kGetBatteryInformation,
      device.address);
}

void FlossBatteryManagerClient::Init(dbus::Bus* bus,
                                     const std::string& service_name,
                                     const int adapter_index,
                                     base::Version version,
                                     base::OnceClosure on_ready) {
  bus_ = bus;
  service_name_ = service_name;
  battery_manager_adapter_path_ = GenerateBatteryManagerPath(adapter_index);
  version_ = version;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, battery_manager_adapter_path_);
  if (!object_proxy) {
    LOG(ERROR)
        << "FlossBatteryManagerClient couldn't init. Object proxy was null.";
    return;
  }

  exported_callback_manager_.Init(bus_.get());
  exported_callback_manager_.AddMethod(
      battery_manager::kOnBatteryInfoUpdated,
      &FlossBatteryManagerClientObserver::BatteryInfoUpdated);

  if (!exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossBatteryManagerClient::OnMethodsExported,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR)
        << "Unable to successfully export FlossBatteryManagerClientObserver.";
    return;
  }

  on_ready_ = std::move(on_ready);
}

void FlossBatteryManagerClient::OnMethodsExported() {
  CallBatteryManagerMethod<uint32_t>(
      base::BindOnce(&FlossBatteryManagerClient::BatteryCallbackRegistered,
                     weak_ptr_factory_.GetWeakPtr()),
      battery_manager::kRegisterBatteryCallback,
      dbus::ObjectPath(kExportedCallbacksPath));
}

void FlossBatteryManagerClient::BatteryInfoUpdated(std::string remote_address,
                                                   BatterySet battery_set) {
  for (auto& observer : observers_) {
    observer.BatteryInfoUpdated(remote_address, battery_set);
  }
}

void FlossBatteryManagerClient::BatteryCallbackRegistered(
    DBusResult<uint32_t> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "RegisterBatteryCallback call failed: " << result.error();
    return;
  }

  battery_manager_callback_id_ = result.value();
  CompleteInit();
}

void FlossBatteryManagerClient::BatteryCallbackUnregistered(
    DBusResult<bool> result) {
  if (!result.has_value() || *result == false) {
    LOG(WARNING) << __func__ << ": Failed to unregister callback";
  }
}

void FlossBatteryManagerClient::CompleteInit() {
  if (on_ready_) {
    std::move(on_ready_).Run();
  }
}

}  // namespace floss
