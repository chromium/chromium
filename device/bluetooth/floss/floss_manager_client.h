// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_version.h"

namespace dbus {
class PropertySet;
}  // namespace dbus

namespace floss {
namespace internal {

// Struct with an adapter and whether it is enabled.
struct AdapterWithEnabled {
  int adapter;
  bool enabled;
};

// Exported callbacks that are used by Floss daemon.
class DEVICE_BLUETOOTH_EXPORT FlossManagerClientCallbacks {
 public:
  FlossManagerClientCallbacks(const FlossManagerClientCallbacks&) = delete;
  FlossManagerClientCallbacks& operator=(const FlossManagerClientCallbacks&) =
      delete;

  FlossManagerClientCallbacks() = default;
  virtual ~FlossManagerClientCallbacks() = default;

  // Notification sent when an HCI interface appears or disappears.
  virtual void OnHciDeviceChanged(int32_t adapter, bool present) = 0;

  // Notification sent when an HCI interface is enabled or disabled.
  virtual void OnHciEnabledChanged(int32_t adapter, bool enabled) = 0;

  // Notification sent when the default adapter changes. This can happen because
  // a new default adapter was chosen or the desired default adapter has its
  // presence changed.
  virtual void OnDefaultAdapterChanged(int32_t adapter) = 0;
};

}  // namespace internal

// The adapter manager client observes the Manager interface which lists all
// available adapters and signals when adapters become available + their state
// changes.
//
// Only the manager interface implements ObjectManager so it should be the first
// point of interaction with the platform adapters. It will also be used to
// select the default adapter.
class DEVICE_BLUETOOTH_EXPORT FlossManagerClient
    : public FlossDBusClient,
      public internal::FlossManagerClientCallbacks,
      public dbus::ObjectManager::Interface {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    Observer() = default;
    ~Observer() override = default;

    // Presence of manager daemon has changed.
    virtual void ManagerPresent(bool present) {}

    // Presence of an adapter has changed.
    virtual void AdapterPresent(int adapter, bool present) {}

    // An adapter is enabled or disabled.
    virtual void AdapterEnabledChanged(int adapter, bool enabled) {}

    // The default adapter has changed.
    virtual void DefaultAdapterChanged(int previous, int adapter) {}
  };

  // Creates the instance.
  static std::unique_ptr<FlossManagerClient> Create();

  FlossManagerClient(const FlossManagerClient&) = delete;
  FlossManagerClient& operator=(const FlossManagerClient&) = delete;

  FlossManagerClient();
  ~FlossManagerClient() override;

  // Add observers on this client.
  void AddObserver(Observer* observer);

  // Remove observers on this client.
  void RemoveObserver(Observer* observer);

  // Get a list of adapters available on the system.
  virtual std::vector<int> GetAdapters() const;

  // Get the default adapter (index) to use.
  virtual int GetDefaultAdapter() const;

  // Check whether the given adapter is present on the system.
  virtual bool GetAdapterPresent(int adapter) const;

  // Check whether an adapter is enabled.
  virtual bool GetAdapterEnabled(int adapter) const;

  // Enable or disable an adapter.
  virtual void SetAdapterEnabled(int adapter,
                                 bool enabled,
                                 ResponseCallback<Void> callback);

  // Gets Floss API version.
  virtual base::Version GetFlossApiVersion() const;

  // Invoke D-Bus API to enable or disable LL privacy.
  virtual void SetLLPrivacy(ResponseCallback<bool> callback, const bool enable);

  // Invoke D-Bus API to enable or disable devcoredump.
  virtual void SetDevCoredump(ResponseCallback<Void> callback,
                              const bool enable);

  // Initializes the manager client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

  // Whether the manager client has been initialized successfully.
  bool IsInitialized() const { return init_; }

 protected:
  friend class FlossManagerClientTest;

  // Check whether Floss is currently enabled. If the response matches the
  // target, continue. If it doesn't, retry setting the target value (with
  // a short delay) until the number of retries is 0.
  void GetFlossEnabledWithTarget(bool target, int retry, int retry_wait_ms);

  // Enable or disable Floss at the platform level. This will only be used while
  // Floss is being developed behind a feature flag. This api will be called on
  // the manager to stop Bluez from running and let Floss manage the adapters
  // instead. If setting this value fails, it will be automatically retried
  // several times with delays. Once the value of |GetFlossEnabled| matches
  // |enable| (or an error occurs), the ResponseCallback will be called.
  void SetFlossEnabled(bool enable,
                       int retry,
                       int retry_wait_ms,
                       std::optional<ResponseCallback<bool>> cb);

  // Make actual D-Bus call to retrieve Floss API version from daemon.
  void DoGetFlossApiVersion();

  // Checks if it is safe to use the API exported by the Floss daemon.
  bool IsCompatibleFlossApi();

  // Handle response to |GetDefaultAdapter| DBus method call.
  void HandleGetDefaultAdapter(DBusResult<int32_t> response);

  // Handle response to |GetAvailableAdapters| DBus method call.
  void HandleGetAvailableAdapters(
      DBusResult<std::vector<internal::AdapterWithEnabled>> adapters);

  // Handle response to |GetAdapterEnabled| DBus method call.
  // Currently we only expect to handle |GetAdapterEnabled| calls when we get a
  // notification that an adapter is present.
  void HandleGetAdapterEnabledAfterPresent(int32_t adapter,
                                           DBusResult<bool> response);

  // Handle response to |RegisterCallback| DBus method call.
  void HandleRegisterCallback(DBusResult<Void> result);

  // internal::FlossManagerClientCallbacks overrides.
  void OnHciDeviceChanged(int32_t adapter, bool present) override;
  void OnHciEnabledChanged(int32_t adapter, bool enabled) override;
  void OnDefaultAdapterChanged(int32_t adapter) override;

  // Handle response to |SetFlossEnabled|.
  void HandleSetFlossEnabled(bool target,
                             int retry,
                             int retry_wait_ms,
                             DBusResult<Void> response);

  // Handle response to |GetFlossEnabled|.
  void HandleGetFlossEnabled(bool target,
                             int retry,
                             int retry_wait_ms,
                             DBusResult<bool> response);

  // Completion of |SetFlossEnabled|.
  void CompleteSetFlossEnabled(DBusResult<bool> ret);

  // Handles response to |DoGetFlossApiVersion|.
  void HandleGetFlossApiVersion(DBusResult<uint32_t> response);

  // Get active adapters and register for callbacks with manager object.
  void RegisterWithManager();

  // Remove active adapters after manager is no longer available.
  void RemoveManager();

  // dbus::ObjectManager::Interface overrides
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override;
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Keep track of the object manager so we can keep track of when the manager
  // disappears. Managed by the bus object (do not delete).
  raw_ptr<dbus::ObjectManager> object_manager_ = nullptr;

  // Is there a manager available?
  bool manager_available_ = false;

  // Default adapter to use.
  int default_adapter_ = 0;

  // Cached list of available adapters and their enabled state indexed by hci
  // index.
  base::flat_map<int, bool> adapter_to_enabled_;

  // List of adapters that the enabled state is unknown, pending querying.
  base::flat_set<int> adapter_present_pending_;

  // Name of service that implements manager interface.
  std::string service_name_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;

  // Whether the manager client has been initialized successfully.
  bool init_ = false;

 private:
  // Handle response to SetAdapterEnabled
  void OnSetAdapterEnabled(DBusResult<Void> response);

  // Call methods in floss experimental interface
  template <typename R, typename... Args>
  void CallExperimentalMethod(ResponseCallback<R> callback,
                              const char* member,
                              Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kExperimentalInterface,
               dbus::ObjectPath(kManagerObject), member, args...);
  }

  // Object path for exported callbacks registered against manager interface.
  static const char kExportedCallbacksPath[];

  // Floss Manager registers ObjectManager at this path.
  static const char kObjectManagerPath[];

  // Retry SetFlossEnabled until the value sticks.
  static const int kSetFlossRetryCount;

  // Amount of time to wait when retrying |SetFlossEnabled|.
  static const int kSetFlossRetryDelayMs;

  // Custom timeout on DBus for |SetFlossEnabled| call. Since this call does
  // multiple things synchronously, give it a little bit more time to complete
  // (especially because it's crucial to set this correctly for Floss to be
  // enabled).
  static const int kSetFlossEnabledDBusTimeoutMs;

  // Enabled callback called only when adapter becomes enabled.
  std::unique_ptr<WeaklyOwnedResponseCallback<Void>> adapter_enabled_callback_;

  // Callback sent for SetFlossEnabled completion.
  std::unique_ptr<WeaklyOwnedResponseCallback<bool>>
      set_floss_enabled_callback_;

  template <typename R, typename... Args>
  void CallManagerMethod(ResponseCallback<R> callback,
                         const char* member,
                         Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kManagerInterface,
               dbus::ObjectPath(kManagerObject), member, args...);
  }

  // Exported callbacks for interacting with daemon.
  ExportedCallbackManager<FlossManagerClientCallbacks>
      exported_callback_manager_{manager::kCallbackInterface};

  // Signal when the client is ready to be used.
  base::OnceClosure on_ready_;

  base::WeakPtrFactory<FlossManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
