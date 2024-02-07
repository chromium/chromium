// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_ADMIN_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_ADMIN_CLIENT_H_

#include <queue>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace floss {

const char kAdminCallbackInterfaceName[] =
    "org.chromium.bluetooth.BluetoothAdmin";

struct DEVICE_BLUETOOTH_EXPORT PolicyEffect {
  std::vector<std::vector<uint8_t>> service_blocked;
  bool affected;

  PolicyEffect();
  PolicyEffect(const PolicyEffect&);
  ~PolicyEffect();
};

class FlossAdminClientObserver : public base::CheckedObserver {
 public:
  FlossAdminClientObserver(const FlossAdminClientObserver&) = delete;
  FlossAdminClientObserver& operator=(const FlossAdminClientObserver&) = delete;

  FlossAdminClientObserver() = default;
  ~FlossAdminClientObserver() override = default;

  // Notification sent when the policy effect to a device changed.
  virtual void DevicePolicyEffectChanged(
      const FlossDeviceId& device_id,
      const std::optional<PolicyEffect>& effect) {}

  // Notification sent when the service allowlist changed.
  virtual void ServiceAllowlistChanged(
      const std::vector<device::BluetoothUUID>& allowlist) {}
};

// Low-level interface to Floss's Admin API.
class DEVICE_BLUETOOTH_EXPORT FlossAdminClient : public FlossDBusClient {
  friend class FlossAdminClientTest;

 public:
  // Error: No such adapter.
  static const char kErrorUnknownAdapter[];

  // Creates the instance.
  static std::unique_ptr<FlossAdminClient> Create();

  FlossAdminClient(const FlossAdminClient&) = delete;
  FlossAdminClient& operator=(const FlossAdminClient&) = delete;

  FlossAdminClient();
  ~FlossAdminClient() override;

  // Manage observers.
  void AddObserver(FlossAdminClientObserver* observer);
  void RemoveObserver(FlossAdminClientObserver* observer);

  // Initialize the Admin client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

  virtual void SetAllowedServices(
      ResponseCallback<Void> callback,
      const std::vector<device::BluetoothUUID>& UUIDs);
  virtual void GetAllowedServices(
      ResponseCallback<std::vector<device::BluetoothUUID>> callback);
  virtual void GetDevicePolicyEffect(ResponseCallback<PolicyEffect> callback,
                                     FlossDeviceId device);

 protected:
  // Handle callback |OnDevicePolicyEffectChanged| on exported object path.
  void OnDevicePolicyEffectChanged(const FlossDeviceId& device_id,
                                   const std::optional<PolicyEffect>& effect);
  // Handle callback |OnServiceAllowlistChanged| on exported object path
  void OnServiceAllowlistChanged(
      const std::vector<std::vector<uint8_t>>& allowlist);

  // Handle the response of GetAllowedServices.
  void HandleGetAllowedServices(
      ResponseCallback<std::vector<device::BluetoothUUID>> callback,
      DBusResult<std::vector<std::vector<uint8_t>>> result);

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Admin managed by this client.
  dbus::ObjectPath admin_path_;

  // Service which implements the admin interface.
  std::string service_name_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<FlossAdminClientObserver> observers_;

 private:
  bool IsClientRegistered();
  std::queue<base::OnceClosure> initialized_callbacks_;
  bool client_registered_ = false;

  void OnMethodsExported();
  void HandleCallbackRegistered(DBusResult<uint32_t> result);
  void HandleCallbackUnregistered(DBusResult<bool> result);

  template <typename R, typename... Args>
  void CallAdminMethod(ResponseCallback<R> callback,
                       const char* member,
                       Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kAdminInterface,
               admin_path_, member, args...);
  }

  // Object path for exported callbacks registered against adapter interface.
  static const char kExportedCallbacksPath[];

  // Exported callbacks for interacting with daemon.
  ExportedCallbackManager<FlossAdminClient> exported_callback_manager_{
      admin::kCallbackInterface};

  // Callback ID used for callbacks registered to this client.
  std::optional<uint32_t> callback_id_;

  // Signal when the client is ready to be used.
  base::OnceClosure on_ready_;

  base::WeakPtrFactory<FlossAdminClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_ADMIN_CLIENT_H_
