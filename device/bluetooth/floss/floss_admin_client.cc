// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_admin_client.h"

namespace floss {
PolicyEffect::PolicyEffect() = default;

PolicyEffect::PolicyEffect(const PolicyEffect&) = default;

PolicyEffect::~PolicyEffect() = default;

// Template specializations for dbus parsing
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    PolicyEffect* effect) {
  static FlossDBusClient::StructReader<PolicyEffect> struct_reader({
      {"service_blocked", CreateFieldReader(&PolicyEffect::service_blocked)},
      {"affected", CreateFieldReader(&PolicyEffect::affected)},
  });

  return struct_reader.ReadDBusParam(reader, effect);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const PolicyEffect*) {
  static DBusTypeInfo info{"a{sv}", "PolicyEffect"};
  return info;
}

std::unique_ptr<FlossAdminClient> FlossAdminClient::Create() {
  return std::make_unique<FlossAdminClient>();
}

constexpr char FlossAdminClient::kExportedCallbacksPath[] =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "/org/chromium/bluetooth/admin/callback/lacros";
#else
    "/org/chromium/bluetooth/admin/callback";
#endif

FlossAdminClient::FlossAdminClient() = default;
FlossAdminClient::~FlossAdminClient() {
  if (callback_id_) {
    CallAdminMethod<bool>(
        base::BindOnce(&FlossAdminClient::HandleCallbackUnregistered,
                       weak_ptr_factory_.GetWeakPtr()),
        admin::kUnregisterCallback, callback_id_.value());
  }
  if (bus_) {
    exported_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kExportedCallbacksPath));
    client_registered_ = false;
    initialized_callbacks_ = std::queue<base::OnceClosure>();
  }
}

void FlossAdminClient::OnMethodsExported() {
  CallAdminMethod<uint32_t>(
      base::BindOnce(&FlossAdminClient::HandleCallbackRegistered,
                     weak_ptr_factory_.GetWeakPtr()),
      admin::kRegisterCallback, dbus::ObjectPath(kExportedCallbacksPath));
}

void FlossAdminClient::HandleCallbackRegistered(DBusResult<uint32_t> result) {
  if (!result.has_value()) {
    LOG(WARNING) << "Failed to register admin client: " << result.error();
    return;
  }

  if (on_ready_) {
    std::move(on_ready_).Run();
  }

  client_registered_ = true;
  callback_id_ = *result;
  while (!initialized_callbacks_.empty()) {
    auto& cb = initialized_callbacks_.front();
    std::move(cb).Run();
    initialized_callbacks_.pop();
  }
}

void FlossAdminClient::HandleCallbackUnregistered(DBusResult<bool> result) {
  if (!result.has_value() || *result == false) {
    LOG(WARNING) << __func__ << ": Failed to unregister callback";
  }
}

void FlossAdminClient::HandleGetAllowedServices(
    ResponseCallback<std::vector<device::BluetoothUUID>> callback,
    DBusResult<std::vector<std::vector<uint8_t>>> result) {
  std::vector<device::BluetoothUUID> uuids;

  for (const auto& uuid_in_bytes : *result) {
    uuids.emplace_back(uuid_in_bytes);
  }

  std::move(callback).Run(uuids);
}

void FlossAdminClient::Init(dbus::Bus* bus,
                            const std::string& service_name,
                            const int adapter_index,
                            base::Version version,
                            base::OnceClosure on_ready) {
  bus_ = bus;
  admin_path_ = FlossDBusClient::GenerateAdminPath(adapter_index);
  service_name_ = service_name;
  version_ = version;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, admin_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossAdminClient couldn't init. Object proxy was null.";
    return;
  }

  dbus::ExportedObject* callbacks =
      bus_->GetExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  if (!callbacks) {
    LOG(ERROR) << "FlossAdminClient couldn't export client callbacks";
    return;
  }

  // Register callbacks for the admin.
  exported_callback_manager_.Init(bus_.get());
  exported_callback_manager_.AddMethod(
      admin::kOnServiceAllowlistChanged,
      &FlossAdminClient::OnServiceAllowlistChanged);
  exported_callback_manager_.AddMethod(
      admin::kOnDevicePolicyEffectChanged,
      &FlossAdminClient::OnDevicePolicyEffectChanged);

  if (!exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossAdminClient::OnMethodsExported,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Unable to successfully export FlossAdminClientObserver.";
    return;
  }

  on_ready_ = std::move(on_ready);
}

void FlossAdminClient::AddObserver(FlossAdminClientObserver* observer) {
  observers_.AddObserver(observer);
}

void FlossAdminClient::RemoveObserver(FlossAdminClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool FlossAdminClient::IsClientRegistered() {
  return client_registered_;
}

void FlossAdminClient::SetAllowedServices(
    ResponseCallback<Void> callback,
    const std::vector<device::BluetoothUUID>& UUIDs) {
  // Delay this function until we're initialized.
  if (!IsClientRegistered()) {
    // This shouldn't happen unless we have multiple policies to set.
    DCHECK(initialized_callbacks_.empty());

    initialized_callbacks_.push(BindOnce(&FlossAdminClient::SetAllowedServices,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback), UUIDs));
    return;
  }

  std::vector<std::vector<uint8_t>> uuids_in_bytes;
  for (const auto& uuid : UUIDs) {
    uuids_in_bytes.emplace_back(uuid.GetBytes());
  }

  CallAdminMethod<Void>(std::move(callback), admin::kSetAllowedServices,
                        uuids_in_bytes);
}

void FlossAdminClient::GetAllowedServices(
    ResponseCallback<std::vector<device::BluetoothUUID>> callback) {
  CallAdminMethod<std::vector<std::vector<uint8_t>>>(
      base::BindOnce(&FlossAdminClient::HandleGetAllowedServices,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      admin::kGetAllowedServices);
}

void FlossAdminClient::GetDevicePolicyEffect(
    ResponseCallback<PolicyEffect> callback,
    FlossDeviceId device) {
  CallAdminMethod<PolicyEffect>(std::move(callback),
                                admin::kGetDevicePolicyEffect, device);
}

void FlossAdminClient::OnServiceAllowlistChanged(
    const std::vector<std::vector<uint8_t>>& allowlist) {
  std::vector<device::BluetoothUUID> uuids;

  for (const auto& uuid_in_bytes : allowlist) {
    uuids.emplace_back(uuid_in_bytes);
  }

  for (auto& observer : observers_) {
    observer.ServiceAllowlistChanged(uuids);
  }
}

void FlossAdminClient::OnDevicePolicyEffectChanged(
    const FlossDeviceId& device_id,
    const std::optional<PolicyEffect>& effect) {
  for (auto& observer : observers_) {
    observer.DevicePolicyEffectChanged(device_id, effect);
  }
}
}  // namespace floss
