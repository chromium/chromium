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
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

namespace {
constexpr char kExportedManagerPath[] = "/org/chromium/bluetooth/managerclient";
constexpr char kUnknownManagerError[] = "org.chromium.Error.UnknownManager";
constexpr char kObjectManagerPath[] = "/";
}  // namespace

class FlossManagerClientImpl : public FlossManagerClient,
                               public dbus::ObjectManager::Interface {
 public:
  FlossManagerClientImpl() = default;
  ~FlossManagerClientImpl() override {
    if (object_manager_) {
      object_manager_->UnregisterInterface(kManagerInterface);
    }

    if (bus_) {
      bus_->UnregisterExportedObject(dbus::ObjectPath(kExportedManagerPath));
    }
  }

  std::vector<int> GetAdapters() const override {
    std::vector<int> adapters;
    for (auto kv : adapter_to_powered_) {
      adapters.push_back(kv.first);
    }

    return adapters;
  }

  int GetDefaultAdapter() const override { return default_adapter_; }

  bool GetAdapterPresent(int adapter) const override {
    return base::Contains(adapter_to_powered_, adapter);
  }

  bool GetAdapterEnabled(int adapter) const override {
    auto iter = adapter_to_powered_.find(adapter);
    if (iter != adapter_to_powered_.end()) {
      return iter->second;
    }

    return false;
  }

  void SetFlossEnabled(bool enabled) override {
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));
    if (!object_proxy) {
      return;
    }

    DVLOG(1) << __func__;

    dbus::MethodCall method_call(kManagerInterface, manager::kSetFlossEnabled);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);

    object_proxy->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FlossManagerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       "FlossManagerClient::SetFlossEnabled"));
  }

  void SetAdapterEnabled(int adapter,
                         bool enabled,
                         ResponseCallback callback) override {
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));
    if (!object_proxy) {
      std::move(callback).Run(Error(kUnknownManagerError, std::string()));
      return;
    }

    DVLOG(1) << __func__;

    auto* command = enabled ? manager::kStart : manager::kStop;
    dbus::MethodCall method_call(kManagerInterface, command);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(adapter);

    object_proxy->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FlossManagerClientImpl::OnResponseWithCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  // Register manager client against manager.
  void RegisterWithManager() {
    DCHECK(!manager_available_);

    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(service_name_, dbus::ObjectPath(kManagerObject));

    // Get all hci devices available.
    dbus::MethodCall method_call(kManagerInterface, manager::kListHciDevices);
    object_proxy->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FlossManagerClientImpl::OnListHciDevices,
                       weak_ptr_factory_.GetWeakPtr()));

    // Register for callbacks.
    dbus::MethodCall register_callback(kManagerInterface,
                                       manager::kRegisterCallback);
    dbus::MessageWriter writer(&register_callback);
    writer.AppendObjectPath(dbus::ObjectPath(kExportedManagerPath));

    object_proxy->CallMethodWithErrorResponse(
        &register_callback, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FlossManagerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       manager::kRegisterCallback));

    manager_available_ = true;
  }

  // Remove manager client (no longer available).
  void RemoveManager() {
    manager_available_ = false;

    // Make copy of old adapters and clear existing ones.
    auto previous_adapters = std::move(adapter_to_powered_);

    // All old adapters need to be sent a `present = false` notification.
    for (auto& kv : previous_adapters) {
      for (auto& observer : observers_) {
        observer.AdapterPresent(kv.first, false);
      }
    }
  }

  // The manager can manage multiple adapters so ignore the adapter path given
  // here. It is unused.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const std::string& adapter_path) override {
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
        bus_->GetExportedObject(dbus::ObjectPath(kExportedManagerPath));

    if (!callbacks) {
      LOG(ERROR) << "FlossManagerClient couldn't export client callbacks.";
      return;
    }

    // Register callbacks for OnHciDeviceChanged and OnHciEnabledChanged.
    callbacks->ExportMethod(
        manager::kCallbackInterface, manager::kOnHciDeviceChanged,
        base::BindRepeating(&FlossManagerClientImpl::OnHciDeviceChange,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FlossManagerClientImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr(),
                       manager::kOnHciDeviceChanged));

    callbacks->ExportMethod(
        manager::kCallbackInterface, manager::kOnHciEnabledChanged,
        base::BindRepeating(&FlossManagerClientImpl::OnHciEnabledChange,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FlossManagerClientImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr(),
                       manager::kOnHciEnabledChanged));

    // Register object manager for Manager.
    object_manager_ = bus_->GetObjectManager(
        service_name, dbus::ObjectPath(kObjectManagerPath));
    object_manager_->RegisterInterface(kManagerInterface, this);

    // Get manager ready.
    RegisterWithManager();
  }

  void OnListHciDevices(dbus::Response* response,
                        dbus::ErrorResponse* error_response) {
    if (!response) {
      FlossDBusClient::LogErrorResponse(
          "FlossManagerClientImpl::OnListHciDevices", error_response);
      return;
    }

    dbus::MessageReader msg(response);
    dbus::MessageReader arr(nullptr);

    if (msg.PopArray(&arr)) {
      auto previous_adapters = std::move(adapter_to_powered_);

      // Clear existing adapters.
      adapter_to_powered_.clear();

      int adapter = 0;
      while (arr.PopInt32(&adapter)) {
        DCHECK(adapter >= 0);
        adapter_to_powered_.insert({adapter, false});
      }

      // Trigger the observers for adapter present on any new ones we listed.
      for (auto& observer : observers_) {
        // Emit present for new adapters that weren't in old list.
        for (auto& kv : adapter_to_powered_) {
          if (!base::Contains(previous_adapters, kv.first))
            observer.AdapterPresent(kv.first, true);
        }

        // Emit not present for adapters that aren't in new list.
        for (auto& kv : previous_adapters) {
          if (!base::Contains(adapter_to_powered_, kv.first))
            observer.AdapterPresent(kv.first, false);
        }
      }
    }
  }

  void OnHciDeviceChange(dbus::MethodCall* method_call,
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

  void OnHciEnabledChange(
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

    adapter_to_powered_[adapter] = enabled;

    for (auto& observer : observers_) {
      observer.AdapterEnabledChanged(adapter, enabled);
    }

    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  void OnExported(const std::string& method_name,
                  const std::string& interface_name,
                  const std::string& object_path,
                  bool success) {
    DVLOG(1) << (success ? "Successfully exported " : "Failed to export ")
             << method_name << " on interface = " << interface_name
             << ", object = " << object_path;
  }

  void OnResponseWithCallback(ResponseCallback callback,
                              dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (response) {
      std::move(callback).Run(absl::nullopt);
      return;
    }

    std::move(callback).Run(
        ErrorResponseToError(kErrorNoResponse, std::string(), error_response));
  }

  void OnResponse(const std::string& caller,
                  dbus::Response* response,
                  dbus::ErrorResponse* error_response) {
    if (error_response) {
      FlossDBusClient::LogErrorResponse(caller, error_response);
    } else {
      DVLOG(1) << caller << "::OnResponse";
    }
  }

  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    return new dbus::PropertySet(object_proxy, interface_name,
                                 base::DoNothing());
  }

  // Manager interface is available.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override {
    // TODO(b/193839304) - When manager exits, we're not getting the
    //                     ObjectRemoved notification. So remove the manager
    //                     before re-adding it here.
    if (manager_available_) {
      RemoveManager();
    }

    DVLOG(0) << __func__ << ": " << object_path.value() << ", "
             << interface_name;

    RegisterWithManager();
  }

  // Manager interface is gone (no longer present).
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override {
    if (!manager_available_)
      return;

    DVLOG(0) << __func__ << ": " << object_path.value() << ", "
             << interface_name;

    RemoveManager();
  }

 private:
  // Is there a manager available?
  bool manager_available_ = false;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  dbus::Bus* bus_ = nullptr;

  // Cached list of available adapters and their powered state indexed by hci
  // index.
  base::flat_map<int, bool> adapter_to_powered_;

  // Default adapter to use.
  // TODO(b/191906229) - Default adapter should be taken via manager api.
  int default_adapter_ = 0;

  // Name of service that implements manager interface.
  std::string service_name_;

  // Exported path. The actual exported object is managed by the bus connection
  // itself and will be cleared when the bus is unregistered.
  dbus::ObjectPath exported_path_;

  // Keep track of the object manager so we can keep track of when the manager
  // disappears. Managed by the bus object (do not delete).
  dbus::ObjectManager* object_manager_ = nullptr;

  base::WeakPtrFactory<FlossManagerClientImpl> weak_ptr_factory_{this};
};

FlossManagerClient::FlossManagerClient() = default;
FlossManagerClient::~FlossManagerClient() = default;

void FlossManagerClient::AddObserver(FlossManagerClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FlossManagerClient::RemoveObserver(
    FlossManagerClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
dbus::ObjectPath FlossManagerClient::GenerateAdapterPath(int adapter) {
  return dbus::ObjectPath(base::StringPrintf(kAdapterObjectFormat, adapter));
}

// static
std::unique_ptr<FlossManagerClient> FlossManagerClient::Create() {
  return std::make_unique<FlossManagerClientImpl>();
}
}  // namespace floss
