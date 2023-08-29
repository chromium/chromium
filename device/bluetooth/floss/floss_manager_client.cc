// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_manager_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/bluez/bluez_features.h"
#include "device/bluetooth/chromeos_platform_features.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_features.h"

namespace floss {

using internal::AdapterWithEnabled;

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    AdapterWithEnabled* adapter) {
  static FlossDBusClient::StructReader<AdapterWithEnabled> struct_reader({
      {"hci_interface", CreateFieldReader(&AdapterWithEnabled::adapter)},
      {"enabled", CreateFieldReader(&AdapterWithEnabled::enabled)},
  });

  return struct_reader.ReadDBusParam(reader, adapter);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<AdapterWithEnabled>(
    const AdapterWithEnabled* unused) {
  static DBusTypeInfo info{"a{sv}", "AdapterWithEnabled"};
  return info;
}

// static
const char FlossManagerClient::kExportedCallbacksPath[] =
    "/org/chromium/bluetooth/managerclient";

// static
const char FlossManagerClient::kObjectManagerPath[] = "/";

// static
const int FlossManagerClient::kSetFlossRetryCount = 3;

// static
const int FlossManagerClient::kSetFlossRetryDelayMs = 500;

// static
const int FlossManagerClient::kSetFlossEnabledDBusTimeoutMs = 10000;

FlossManagerClient::FlossManagerClient() = default;

FlossManagerClient::~FlossManagerClient() {
  if (object_manager_) {
    object_manager_->UnregisterInterface(kManagerInterface);
  }

  if (bus_) {
    bus_->UnregisterExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  }
}

void FlossManagerClient::AddObserver(FlossManagerClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FlossManagerClient::RemoveObserver(
    FlossManagerClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<int> FlossManagerClient::GetAdapters() const {
  std::vector<int> adapters;
  for (auto& [adapter, enabled] : adapter_to_enabled_) {
    adapters.push_back(adapter);
  }

  return adapters;
}

int FlossManagerClient::GetDefaultAdapter() const {
  return default_adapter_;
}

bool FlossManagerClient::GetAdapterPresent(int adapter) const {
  return base::Contains(adapter_to_enabled_, adapter);
}

bool FlossManagerClient::GetAdapterEnabled(int adapter) const {
  auto iter = adapter_to_enabled_.find(adapter);
  if (iter != adapter_to_enabled_.end()) {
    return iter->second;
  }

  return false;
}

void FlossManagerClient::GetFlossEnabledWithTarget(bool target,
                                                   int retry,
                                                   int retry_wait_ms) {
  CallManagerMethod<bool>(
      base::BindOnce(&FlossManagerClient::HandleGetFlossEnabled,
                     weak_ptr_factory_.GetWeakPtr(), target, retry,
                     retry_wait_ms),
      manager::kGetFlossEnabled);
}

void FlossManagerClient::SetFlossEnabled(
    bool enabled,
    int retry,
    int retry_wait_ms,
    absl::optional<ResponseCallback<bool>> cb) {
  if (cb) {
    set_floss_enabled_callback_ =
        WeaklyOwnedResponseCallback<bool>::Create(std::move(*cb));
  }

  CallManagerMethod<Void>(
      base::BindOnce(&FlossManagerClient::HandleSetFlossEnabled,
                     weak_ptr_factory_.GetWeakPtr(), enabled, retry,
                     retry_wait_ms),
      manager::kSetFlossEnabled, enabled);
}

void FlossManagerClient::SetAdapterEnabled(int adapter,
                                           bool enabled,
                                           ResponseCallback<Void> callback) {
  if (adapter != GetDefaultAdapter()) {
    return;
  }

  DVLOG(1) << __func__;

  adapter_enabled_callback_ =
      WeaklyOwnedResponseCallback<Void>::CreateWithTimeout(
          std::move(callback), kAdapterEnabledTimeoutMs,
          base::unexpected(Error(kErrorNoResponse, "")));

  const char* command = enabled ? manager::kStart : manager::kStop;
  CallManagerMethod<Void>(
      base::BindOnce(&FlossManagerClient::OnSetAdapterEnabled,
                     weak_ptr_factory_.GetWeakPtr()),
      command, adapter);
}

void FlossManagerClient::OnSetAdapterEnabled(DBusResult<Void> response) {
  // Only handle error cases since non-error called in OnHciEnabledChange
  if (adapter_enabled_callback_ && !response.has_value()) {
    adapter_enabled_callback_->Run(
        base::unexpected(Error(kErrorNoResponse, "")));
    adapter_enabled_callback_.reset();
  }
}

void FlossManagerClient::SetLLPrivacy(ResponseCallback<Void> callback,
                                      const bool enable) {
  CallExperimentalMethod<Void>(std::move(callback), experimental::kSetLLPrivacy,
                               enable);
}

void FlossManagerClient::SetDevCoredump(ResponseCallback<Void> callback,
                                        const bool enable) {
  CallExperimentalMethod<Void>(std::move(callback),
                               experimental::kSetDevCoredump, enable);
}

// Register manager client against manager.
void FlossManagerClient::RegisterWithManager() {
  DCHECK(!manager_available_);

  // Get the default adapter.
  CallManagerMethod<int>(
      base::BindOnce(&FlossManagerClient::HandleGetDefaultAdapter,
                     weak_ptr_factory_.GetWeakPtr()),
      manager::kGetDefaultAdapter);

  // Get the list of available adapters.
  CallManagerMethod<std::vector<AdapterWithEnabled>>(
      base::BindOnce(&FlossManagerClient::HandleGetAvailableAdapters,
                     weak_ptr_factory_.GetWeakPtr()),
      manager::kGetAvailableAdapters);

  // Register for callbacks.
  CallManagerMethod<Void>(
      base::BindOnce(&FlossManagerClient::HandleRegisterCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      manager::kRegisterCallback, dbus::ObjectPath(kExportedCallbacksPath));

  manager_available_ = true;
  for (auto& observer : observers_) {
    observer.ManagerPresent(manager_available_);
  }
}

// Remove manager client (no longer available).
void FlossManagerClient::RemoveManager() {
  // Make copy of old adapters and clear existing ones.
  auto previous_adapters = std::move(adapter_to_enabled_);
  adapter_to_enabled_.clear();

  // All old adapters need to be sent a `present = false` notification.
  for (auto& [adapter, enabled] : previous_adapters) {
    for (auto& observer : observers_) {
      observer.AdapterPresent(adapter, false);
    }
  }

  manager_available_ = false;
  for (auto& observer : observers_) {
    observer.ManagerPresent(manager_available_);
  }
}

// The manager can manage multiple adapters so ignore the adapter index given
// here. It is unused.
void FlossManagerClient::Init(dbus::Bus* bus,
                              const std::string& service_name,
                              const int adapter_index,
                              base::OnceClosure on_ready) {
  bus_ = bus;
  service_name_ = service_name;

  // We should always have object proxy since the client initialization is
  // gated on ObjectManager marking the manager interface as available.
  if (!bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject))) {
    LOG(ERROR) << "FlossManagerClient couldn't init. Object proxy was null.";
    return;
  }

  DVLOG(1) << __func__;

  exported_callback_manager_.Init(bus_.get());
  exported_callback_manager_.AddMethod(
      manager::kOnHciDeviceChanged,
      &FlossManagerClientCallbacks::OnHciDeviceChanged);
  exported_callback_manager_.AddMethod(
      manager::kOnHciEnabledChanged,
      &FlossManagerClientCallbacks::OnHciEnabledChanged);
  exported_callback_manager_.AddMethod(
      manager::kOnDefaultAdapterChanged,
      &FlossManagerClientCallbacks::OnDefaultAdapterChanged);
  if (!exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossManagerClient::RegisterWithManager,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Unable to successfully export FlossManagerClientCallbacks.";
    return;
  }

  // Register object manager for Manager.
  object_manager_ = bus_->GetObjectManager(
      service_name, dbus::ObjectPath(kObjectManagerPath));
  object_manager_->RegisterInterface(kManagerInterface, this);

  // Enable Floss and retry a few times until it is set.
  SetFlossEnabled(floss::features::IsFlossEnabled(), kSetFlossRetryCount,
                  kSetFlossRetryDelayMs,
                  base::BindOnce(&FlossManagerClient::CompleteSetFlossEnabled,
                                 weak_ptr_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_CHROMEOS)
  SetDevCoredump(base::BindOnce([](DBusResult<Void> ret) {
                   if (!ret.has_value()) {
                     LOG(ERROR) << "Fail to set devcoredump.\n";
                   }
                 }),
                 base::FeatureList::IsEnabled(
                     chromeos::bluetooth::features::kBluetoothFlossCoredump));
#endif  // BUILDFLAG(IS_CHROMEOS)

  SetLLPrivacy(
      base::BindOnce([](DBusResult<Void> ret) {
        if (!ret.has_value())
          LOG(ERROR) << "Fail to set LL privacy.\n";
      }),
      base::FeatureList::IsEnabled(bluez::features::kLinkLayerPrivacy));

  on_ready_ = std::move(on_ready);
}

void FlossManagerClient::HandleGetDefaultAdapter(DBusResult<int32_t> response) {
  if (!response.has_value()) {
    LOG(ERROR) << "GetDefaultAdapter responded with error: "
               << response.error();
    return;
  }

  OnDefaultAdapterChanged(response.value());
}

void FlossManagerClient::HandleGetAvailableAdapters(
    DBusResult<std::vector<AdapterWithEnabled>> adapters) {
  if (!adapters.has_value()) {
    LOG(WARNING) << "GetAvailableAdapters return error " << adapters.error();
    return;
  }

  auto previous_adapters = std::move(adapter_to_enabled_);

  // Clear existing adapters.
  adapter_to_enabled_.clear();
  for (auto v : adapters.value()) {
    adapter_to_enabled_.insert({v.adapter, v.enabled});
  }

  // Trigger the observers for adapter present on any new ones we listed.
  for (auto& observer : observers_) {
    // Emit present for new adapters that weren't in old list. Also emit the
    // enabled changed for them.
    for (auto& [adapter, enabled] : adapter_to_enabled_) {
      if (!base::Contains(previous_adapters, adapter)) {
        observer.AdapterEnabledChanged(adapter, enabled);
      }
    }

    // Emit not present for adapters that aren't in new list.
    // We don't need to emit AdapterEnabledChanged since we emit
    // AdapterPresent is false
    for (auto& [adapter, enabled] : previous_adapters) {
      if (!base::Contains(adapter_to_enabled_, adapter)) {
        observer.AdapterPresent(adapter, false);
      }
    }
  }
}

void FlossManagerClient::HandleRegisterCallback(DBusResult<Void> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Floss manager RegisterCallback returned error: "
               << result.error();
    return;
  }

  if (on_ready_) {
    std::move(on_ready_).Run();
  }
}

void FlossManagerClient::OnHciDeviceChanged(int32_t adapter, bool present) {
  for (auto& observer : observers_) {
    observer.AdapterPresent(adapter, present);
  }

  // Update the cached list of available adapters.
  auto iter = adapter_to_enabled_.find(adapter);
  if (present && iter == adapter_to_enabled_.end()) {
    adapter_to_enabled_.insert({adapter, false});
  } else if (!present && iter != adapter_to_enabled_.end()) {
    adapter_to_enabled_.erase(iter);
  }
}

void FlossManagerClient::OnHciEnabledChanged(int32_t adapter, bool enabled) {
  if (adapter == GetDefaultAdapter() && adapter_enabled_callback_) {
    adapter_enabled_callback_->Run(Void{});
    adapter_enabled_callback_.reset();
  }

  adapter_to_enabled_[adapter] = enabled;

  for (auto& observer : observers_) {
    observer.AdapterEnabledChanged(adapter, enabled);
  }
}

void FlossManagerClient::OnDefaultAdapterChanged(int32_t adapter) {
  int32_t previous_default = default_adapter_;
  default_adapter_ = adapter;

  for (auto& observer : observers_) {
    observer.DefaultAdapterChanged(previous_default, adapter);
  }
}

void FlossManagerClient::HandleSetFlossEnabled(bool target,
                                               int retry,
                                               int retry_wait_ms,
                                               DBusResult<Void> response) {
  // Failed to call |SetFlossEnabled| so first log the error and post a delayed
  // set if there are retries left.
  if (!response.has_value()) {
    LOG(ERROR) << response.error();
    if (retry > 0) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FlossManagerClient::SetFlossEnabled,
                         weak_ptr_factory_.GetWeakPtr(), target, retry - 1,
                         retry_wait_ms, absl::nullopt),
          base::Milliseconds(retry_wait_ms));
    } else if (set_floss_enabled_callback_) {
      set_floss_enabled_callback_->Run(base::unexpected(response.error()));
      set_floss_enabled_callback_.reset();
    }

    return;
  }

  GetFlossEnabledWithTarget(target, retry, retry_wait_ms);
}

void FlossManagerClient::HandleGetFlossEnabled(bool target,
                                               int retry,
                                               int retry_wait_ms,
                                               DBusResult<bool> response) {
  if (!response.has_value()) {
    LOG(ERROR) << response.error();
    if (retry > 0) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FlossManagerClient::GetFlossEnabledWithTarget,
                         weak_ptr_factory_.GetWeakPtr(), target, retry - 1,
                         retry_wait_ms),
          base::Milliseconds(retry_wait_ms));
    } else if (set_floss_enabled_callback_) {
      set_floss_enabled_callback_->Run(base::unexpected(response.error()));
      set_floss_enabled_callback_.reset();
    }

    return;
  }

  bool floss_enabled = response.value();

  // Target doesn't match reality. Retry |SetFlossEnabled|.
  if (floss_enabled != target && retry > 0) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FlossManagerClient::SetFlossEnabled,
                       weak_ptr_factory_.GetWeakPtr(), target, retry - 1,
                       retry_wait_ms, absl::nullopt),
        base::Milliseconds(kSetFlossRetryDelayMs));
  } else {
    DVLOG(1) << "Floss is currently "
             << (floss_enabled ? "enabled" : "disabled") << " and target was "
             << (target ? "enabled" : "disabled");
    if (set_floss_enabled_callback_) {
      set_floss_enabled_callback_->Run(floss_enabled);
      set_floss_enabled_callback_.reset();
    }
  }
}

void FlossManagerClient::CompleteSetFlossEnabled(DBusResult<bool> ret) {
  if (!ret.has_value()) {
    LOG(ERROR) << "Floss couldn't be enabled. Error=" << ret.error();
  } else {
    DVLOG(1) << "Completed SetFlossEnabled with value " << *ret;
  }
}

dbus::PropertySet* FlossManagerClient::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  return new dbus::PropertySet(object_proxy, interface_name, base::DoNothing());
}

// Manager interface is available.
void FlossManagerClient::ObjectAdded(const dbus::ObjectPath& object_path,
                                     const std::string& interface_name) {
  // TODO(b/193839304) - When manager exits, we're not getting the
  //                     ObjectRemoved notification. So remove the manager
  //                     before re-adding it here.
  if (manager_available_) {
    RemoveManager();
  }

  DVLOG(0) << __func__ << ": " << object_path.value() << ", " << interface_name;

  RegisterWithManager();
}

// Manager interface is gone (no longer present).
void FlossManagerClient::ObjectRemoved(const dbus::ObjectPath& object_path,
                                       const std::string& interface_name) {
  if (!manager_available_)
    return;

  DVLOG(0) << __func__ << ": " << object_path.value() << ", " << interface_name;

  RemoveManager();
}

// static
std::unique_ptr<FlossManagerClient> FlossManagerClient::Create() {
  return std::make_unique<FlossManagerClient>();
}
}  // namespace floss
