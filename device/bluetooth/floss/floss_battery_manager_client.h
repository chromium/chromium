// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_BATTERY_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_BATTERY_MANAGER_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

struct DEVICE_BLUETOOTH_EXPORT Battery {
  uint32_t percentage;
  std::string variant;

  Battery();
  ~Battery();
};

struct DEVICE_BLUETOOTH_EXPORT BatterySet {
  std::string address;
  std::string source_uuid;
  std::string source_info;
  std::vector<Battery> batteries;

  BatterySet();
  BatterySet(const BatterySet&);
  ~BatterySet();
};

// Callback functions expected to be imported by the BatteryManager client.
class DEVICE_BLUETOOTH_EXPORT FlossBatteryManagerClientObserver
    : public base::CheckedObserver {
 public:
  FlossBatteryManagerClientObserver(const FlossBatteryManagerClientObserver&) =
      delete;
  FlossBatteryManagerClientObserver& operator=(
      const FlossBatteryManagerClientObserver&) = delete;

  FlossBatteryManagerClientObserver() = default;
  ~FlossBatteryManagerClientObserver() override = default;

  // Result of a battery update.
  virtual void BatteryInfoUpdated(std::string remote_address,
                                  BatterySet battery_set) {}
};

class DEVICE_BLUETOOTH_EXPORT FlossBatteryManagerClient
    : public FlossDBusClient,
      public FlossBatteryManagerClientObserver {
 public:
  static const char kExportedCallbacksPath[];

  static std::unique_ptr<FlossBatteryManagerClient> Create();

  FlossBatteryManagerClient(const FlossBatteryManagerClient&) = delete;
  FlossBatteryManagerClient& operator=(const FlossBatteryManagerClient&) =
      delete;

  FlossBatteryManagerClient();
  ~FlossBatteryManagerClient() override;

  // Manage observers.
  virtual void AddObserver(FlossBatteryManagerClientObserver* observer);
  void RemoveObserver(FlossBatteryManagerClientObserver* observer);

  // Get the current cached battery info from BatteryManager.
  virtual void GetBatteryInformation(
      ResponseCallback<std::optional<BatterySet>> callback,
      const FlossDeviceId& device);

  // Initialize the BatteryManager client for the given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 protected:
  friend class FlossBatteryManagerClientTest;

  // Callback implementation for forwarding callback results.
  void BatteryInfoUpdated(std::string remote_address,
                          BatterySet battery_set) override;

  // Registers battery callback to daemon after callback methods are exported.
  void OnMethodsExported();

  // Handle BatteryManager RegisterCallback result.
  void BatteryCallbackRegistered(DBusResult<uint32_t> result);

  // Handle BatteryManager UnregisterCallback result.
  void BatteryCallbackUnregistered(DBusResult<bool> result);

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Path used for battery api calls by this class.
  dbus::ObjectPath battery_manager_adapter_path_;

  // Service which implements the BatteryManagerClient interface.
  std::string service_name_;

  // List of observers interested in battery event notifications.
  base::ObserverList<FlossBatteryManagerClientObserver> observers_;

 private:
  friend class FlossBatteryManagerClientTest;

  // Complete initialization once the client is ready to use.
  void CompleteInit();

  template <typename R, typename... Args>
  void CallBatteryManagerMethod(ResponseCallback<R> callback,
                                const char* member,
                                Args... args) {
    CallMethod(std::move(callback), bus_, service_name_,
               kBatteryManagerInterface, battery_manager_adapter_path_, member,
               args...);
  }

  // Exported callbacks for interacting with daemon.
  ExportedCallbackManager<FlossBatteryManagerClientObserver>
      exported_callback_manager_{battery_manager::kCallbackInterface};

  // Callback ID used for callbacks registered to this client.
  std::optional<uint32_t> battery_manager_callback_id_;

  // Signal when the client is ready to be used.
  base::OnceClosure on_ready_;

  base::WeakPtrFactory<FlossBatteryManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_BATTERY_MANAGER_CLIENT_H_
