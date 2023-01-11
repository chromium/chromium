// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
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

  class PoweredCallback {
   public:
    explicit PoweredCallback(ResponseCallback<Void> cb, int timeout_ms);
    ~PoweredCallback();

    static std::unique_ptr<FlossManagerClient::PoweredCallback>
    CreateWithTimeout(ResponseCallback<Void> cb, int timeout_ms);
    void RunError() {
      if (cb_) {
        std::move(cb_).Run(base::unexpected(Error(kErrorNoResponse, "")));
      }
    }
    void RunNoError() {
      if (cb_) {
        std::move(cb_).Run(Void{});
      }
    }

   private:
    void PostDelayedError();

    ResponseCallback<Void> cb_;
    int timeout_ms_;
    base::WeakPtrFactory<PoweredCallback> weak_ptr_factory_{this};
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

  // Invoke D-Bus API to enable or disable LL privacy.
  virtual void SetLLPrivacy(ResponseCallback<Void> callback, const bool enable);

  // Initializes the manager client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index) override;

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
                       absl::optional<ResponseCallback<bool>> cb);

  // Handle response to |GetDefaultAdapter| DBus method call.
  void HandleGetDefaultAdapter(DBusResult<int32_t> response);

  // Handle response to |GetAvailableAdapters| DBus method call.
  void HandleGetAvailableAdapters(
      DBusResult<std::vector<internal::AdapterWithEnabled>> adapters);

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

  // Cached list of available adapters and their powered state indexed by hci
  // index.
  base::flat_map<int, bool> adapter_to_powered_;

  // Name of service that implements manager interface.
  std::string service_name_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;

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

  // Powered callback called only when adapter actually powers on
  std::unique_ptr<PoweredCallback> powered_callback_;

  // Callback sent for SetFlossEnabled completion.
  std::unique_ptr<WeaklyOwnedCallback<bool>> set_floss_enabled_callback_;

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

  base::WeakPtrFactory<FlossManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
