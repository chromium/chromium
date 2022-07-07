// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_manager_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_features.h"

namespace floss {

namespace {
constexpr char kUnknownManagerError[] = "org.chromium.Error.UnknownManager";
constexpr char kHciInterfaceKey[] = "hci_interface";
constexpr char kEnabledKey[] = "enabled";

bool ParseAdapterWithEnabled(dbus::MessageReader& array,
                             int* adapter,
                             bool* enabled) {
  dbus::MessageReader dict(nullptr);
  bool found_adapter = false;
  bool found_enabled = false;

  while (array.PopDictEntry(&dict)) {
    std::string key;
    dict.PopString(&key);

    if (key == kHciInterfaceKey) {
      found_adapter = dict.PopVariantOfInt32(adapter);
    } else if (key == kEnabledKey) {
      found_enabled = dict.PopVariantOfBool(enabled);
    }
  }

  return found_adapter && found_enabled;
}

void HandleExported(const std::string& method_name,
                    const std::string& interface_name,
                    const std::string& object_path,
                    bool success) {
  DVLOG(1) << (success ? "Successfully exported " : "Failed to export ")
           << method_name << " on interface = " << interface_name
           << ", object = " << object_path;
}

}  // namespace

FlossManagerClient::PoweredCallback::PoweredCallback(ResponseCallback<Void> cb,
                                                     int timeout_ms) {
  cb_ = std::move(cb);
  timeout_ms_ = timeout_ms;
}

FlossManagerClient::PoweredCallback::~PoweredCallback() = default;

// static
std::unique_ptr<FlossManagerClient::PoweredCallback>
FlossManagerClient::PoweredCallback::CreateWithTimeout(
    ResponseCallback<Void> cb,
    int timeout_ms) {
  std::unique_ptr<FlossManagerClient::PoweredCallback> self =
      std::make_unique<FlossManagerClient::PoweredCallback>(std::move(cb),
                                                            timeout_ms);
  self->PostDelayedError();

  return self;
}

void FlossManagerClient::PoweredCallback::PostDelayedError() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PoweredCallback::RunError,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(timeout_ms_));
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
  for (auto kv : adapter_to_powered_) {
    adapters.push_back(kv.first);
  }

  return adapters;
}

int FlossManagerClient::GetDefaultAdapter() const {
  return default_adapter_;
}

bool FlossManagerClient::GetAdapterPresent(int adapter) const {
  return base::Contains(adapter_to_powered_, adapter);
}

bool FlossManagerClient::GetAdapterEnabled(int adapter) const {
  auto iter = adapter_to_powered_.find(adapter);
  if (iter != adapter_to_powered_.end()) {
    return iter->second;
  }

  return false;
}

void FlossManagerClient::GetFlossEnabledWithTarget(bool target,
                                                   int retry,
                                                   int retry_wait_ms) {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));
  if (!object_proxy) {
    if (set_floss_enabled_callback_) {
      set_floss_enabled_callback_->Run(absl::nullopt,
                                       Error(kUnknownManagerError, ""));
      set_floss_enabled_callback_.reset();
    }
    return;
  }

  DVLOG(2) << __func__;

  dbus::MethodCall method_call(kManagerInterface, manager::kGetFlossEnabled);
  dbus::MessageWriter writer(&method_call);

  object_proxy->CallMethodWithErrorResponse(
      &method_call, kDBusTimeoutMs,
      base::BindOnce(&FlossManagerClient::HandleGetFlossEnabled,
                     weak_ptr_factory_.GetWeakPtr(), target, retry,
                     retry_wait_ms));
}

void FlossManagerClient::SetFlossEnabled(
    bool enabled,
    int retry,
    int retry_wait_ms,
    absl::optional<ResponseCallback<bool>> cb) {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));
  if (!object_proxy) {
    if (cb) {
      std::move(*cb).Run(/*ret=*/absl::nullopt,
                         /*err=*/Error(kUnknownManagerError, ""));
    }
    return;
  }

  DVLOG(1) << __func__;

  if (cb) {
    set_floss_enabled_callback_ =
        WeaklyOwnedCallback<bool>::Create(std::move(*cb));
  }

  dbus::MethodCall method_call(kManagerInterface, manager::kSetFlossEnabled);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(enabled);

  object_proxy->CallMethodWithErrorResponse(
      &method_call, kSetFlossEnabledDBusTimeoutMs,
      base::BindOnce(&FlossManagerClient::HandleSetFlossEnabled,
                     weak_ptr_factory_.GetWeakPtr(), enabled, retry,
                     retry_wait_ms));
}

void FlossManagerClient::SetAdapterEnabled(int adapter,
                                           bool enabled,
                                           ResponseCallback<Void> callback) {
  if (adapter != GetDefaultAdapter()) {
    return;
  }

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));
  if (!object_proxy) {
    std::move(callback).Run(absl::nullopt,
                            Error(kUnknownManagerError, std::string()));
    return;
  }

  DVLOG(1) << __func__;

  auto* command = enabled ? manager::kStart : manager::kStop;
  dbus::MethodCall method_call(kManagerInterface, command);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(adapter);

  powered_callback_ =
      PoweredCallback::CreateWithTimeout(std::move(callback), kDBusTimeoutMs);

  object_proxy->CallMethodWithErrorResponse(
      &method_call, kDBusTimeoutMs,
      base::BindOnce(&FlossManagerClient::OnSetAdapterEnabled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FlossManagerClient::OnSetAdapterEnabled(
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  // Only handle error cases since non-error called in OnHciEnabledChange
  if (powered_callback_ && (!response || error_response)) {
    powered_callback_->RunError();
    powered_callback_.reset();
  }
}

// Register manager client against manager.
void FlossManagerClient::RegisterWithManager() {
  DCHECK(!manager_available_);

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));

  // Get all hci devices available.
  dbus::MethodCall method_call(kManagerInterface,
                               manager::kGetAvailableAdapters);
  object_proxy->CallMethodWithErrorResponse(
      &method_call, kDBusTimeoutMs,
      base::BindOnce(&FlossManagerClient::HandleGetAvailableAdapters,
                     weak_ptr_factory_.GetWeakPtr()));

  // Register for callbacks.
  dbus::MethodCall register_callback(kManagerInterface,
                                     manager::kRegisterCallback);
  dbus::MessageWriter writer(&register_callback);
  writer.AppendObjectPath(dbus::ObjectPath(kExportedCallbacksPath));

  object_proxy->CallMethodWithErrorResponse(
      &register_callback, kDBusTimeoutMs,
      base::BindOnce(&FlossManagerClient::DefaultResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     manager::kRegisterCallback));

  manager_available_ = true;
  for (auto& observer : observers_) {
    observer.ManagerPresent(manager_available_);
  }
}

// Remove manager client (no longer available).
void FlossManagerClient::RemoveManager() {
  // Make copy of old adapters and clear existing ones.
  auto previous_adapters = std::move(adapter_to_powered_);
  adapter_to_powered_.clear();

  // All old adapters need to be sent a `present = false` notification.
  for (auto& kv : previous_adapters) {
    for (auto& observer : observers_) {
      observer.AdapterPresent(kv.first, false);
    }
  }

  manager_available_ = false;
  for (auto& observer : observers_) {
    observer.ManagerPresent(manager_available_);
  }
}

// The manager can manage multiple adapters so ignore the adapter path given
// here. It is unused.
void FlossManagerClient::Init(dbus::Bus* bus,
                              const std::string& service_name,
                              const std::string& adapter_path) {
  bus_ = bus;
  service_name_ = service_name;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));

  // We should always have object proxy since the client initialization is
  // gated on ObjectManager marking the manager interface as available.
  if (!object_proxy) {
    LOG(ERROR) << "FlossManagerClient couldn't init. Object proxy was null.";
    return;
  }

  DVLOG(1) << __func__;

  // Register callback object.
  dbus::ExportedObject* callbacks =
      bus_->GetExportedObject(dbus::ObjectPath(kExportedCallbacksPath));

  if (!callbacks) {
    LOG(ERROR) << "FlossManagerClient couldn't export client callbacks.";
    return;
  }

  // Register callbacks for OnHciDeviceChanged and OnHciEnabledChanged.
  callbacks->ExportMethod(
      manager::kCallbackInterface, manager::kOnHciDeviceChanged,
      base::BindRepeating(&FlossManagerClient::OnHciDeviceChange,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, manager::kOnHciDeviceChanged));

  callbacks->ExportMethod(
      manager::kCallbackInterface, manager::kOnHciEnabledChanged,
      base::BindRepeating(&FlossManagerClient::OnHciEnabledChange,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, manager::kOnHciEnabledChanged));

  // Register object manager for Manager.
  object_manager_ = bus_->GetObjectManager(
      service_name, dbus::ObjectPath(kObjectManagerPath));
  object_manager_->RegisterInterface(kManagerInterface, this);

  // Get manager ready.
  RegisterWithManager();

  // Enable Floss and retry a few times until it is set.
  SetFlossEnabled(floss::features::IsFlossEnabled(), kSetFlossRetryCount,
                  kSetFlossRetryDelayMs,
                  base::BindOnce(&FlossManagerClient::CompleteSetFlossEnabled,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void FlossManagerClient::HandleGetAvailableAdapters(
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (!response) {
    FlossDBusClient::LogErrorResponse(
        "FlossManagerClient::HandleGetAvailableAdapters", error_response);
    return;
  }

  dbus::MessageReader msg(response);
  dbus::MessageReader arr(nullptr);

  if (msg.PopArray(&arr)) {
    auto previous_adapters = std::move(adapter_to_powered_);

    // Clear existing adapters.
    adapter_to_powered_.clear();

    dbus::MessageReader propmap(nullptr);
    while (arr.PopArray(&propmap)) {
      int adapter = -1;
      bool enabled = false;

      if (ParseAdapterWithEnabled(propmap, &adapter, &enabled)) {
        DCHECK(adapter >= 0);
        adapter_to_powered_.emplace(adapter, enabled);
      }
    }

    // Trigger the observers for adapter present on any new ones we listed.
    for (auto& observer : observers_) {
      // Emit present for new adapters that weren't in old list. Also emit the
      // powered changed for them.
      for (auto& kv : adapter_to_powered_) {
        if (!base::Contains(previous_adapters, kv.first)) {
          observer.AdapterPresent(kv.first, true);
          observer.AdapterEnabledChanged(kv.first, kv.second);
        }
      }

      // Emit not present for adapters that aren't in new list.
      // We don't need to emit AdapterEnabledChanged since we emit
      // AdapterPresent is false
      for (auto& kv : previous_adapters) {
        if (!base::Contains(adapter_to_powered_, kv.first))
          observer.AdapterPresent(kv.first, false);
      }
    }
  }
}

void FlossManagerClient::OnHciDeviceChange(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  int adapter;
  bool present;

  if (!msg.PopInt32(&adapter) || !msg.PopBool(&present)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterPresent(adapter, present);
  }

  // Update the cached list of available adapters.
  auto iter = adapter_to_powered_.find(adapter);
  if (present && iter == adapter_to_powered_.end()) {
    adapter_to_powered_.insert({adapter, false});
  } else if (!present && iter != adapter_to_powered_.end()) {
    adapter_to_powered_.erase(iter);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossManagerClient::OnHciEnabledChange(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader msg(method_call);
  int adapter;
  bool enabled;

  if (!msg.PopInt32(&adapter) || !msg.PopBool(&enabled)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  if (adapter == GetDefaultAdapter() && powered_callback_) {
    powered_callback_->RunNoError();
    powered_callback_.reset();
  }

  adapter_to_powered_[adapter] = enabled;

  for (auto& observer : observers_) {
    observer.AdapterEnabledChanged(adapter, enabled);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossManagerClient::HandleSetFlossEnabled(
    bool target,
    int retry,
    int retry_wait_ms,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  // Failed to call |SetFlossEnabled| so first log the error and post a delayed
  // set if there are retries left.
  if (!response) {
    FlossDBusClient::LogErrorResponse(
        "FlossManagerClient::HandleSetFlossEnabled", error_response);
    if (retry > 0) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FlossManagerClient::SetFlossEnabled,
                         weak_ptr_factory_.GetWeakPtr(), target, retry - 1,
                         retry_wait_ms, absl::nullopt),
          base::Milliseconds(retry_wait_ms));
    } else if (set_floss_enabled_callback_) {
      set_floss_enabled_callback_->Run(
          /*ret=*/absl::nullopt,
          ErrorResponseToError(kUnknownManagerError, "", error_response));
      set_floss_enabled_callback_.reset();
    }

    return;
  }

  GetFlossEnabledWithTarget(target, retry, retry_wait_ms);
}

void FlossManagerClient::HandleGetFlossEnabled(
    bool target,
    int retry,
    int retry_wait_ms,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (!response) {
    FlossDBusClient::LogErrorResponse(
        "FlossManagerClient::HandleGetFlossEnabled", error_response);
    if (retry > 0) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FlossManagerClient::GetFlossEnabledWithTarget,
                         weak_ptr_factory_.GetWeakPtr(), target, retry - 1,
                         retry_wait_ms),
          base::Milliseconds(retry_wait_ms));
    } else if (set_floss_enabled_callback_) {
      set_floss_enabled_callback_->Run(
          /*ret=*/absl::nullopt,
          ErrorResponseToError(kUnknownManagerError, "", error_response));
      set_floss_enabled_callback_.reset();
    }

    return;
  }

  bool floss_enabled = false;
  dbus::MessageReader msg(response);
  if (!msg.PopBool(&floss_enabled)) {
    LOG(ERROR) << "Response to GetFlossEnabled was not a bool";
    return;
  }

  // Target doesn't match reality. Retry |SetFlossEnabled|.
  if (floss_enabled != target && retry > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
      set_floss_enabled_callback_->Run(floss_enabled,
                                       /*err=*/absl::nullopt);
      set_floss_enabled_callback_.reset();
    }
  }
}

void FlossManagerClient::CompleteSetFlossEnabled(
    const absl::optional<bool>& ret,
    const absl::optional<Error>& err) {
  if (err) {
    LOG(ERROR) << "Floss couldn't be enabled. Error=" << err->name << ", "
               << err->message;
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
dbus::ObjectPath FlossManagerClient::GenerateAdapterPath(int adapter) {
  return dbus::ObjectPath(base::StringPrintf(kAdapterObjectFormat, adapter));
}

// static
std::unique_ptr<FlossManagerClient> FlossManagerClient::Create() {
  return std::make_unique<FlossManagerClient>();
}
}  // namespace floss
