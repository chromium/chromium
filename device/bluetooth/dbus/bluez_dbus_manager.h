// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_MANAGER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_dbus_client_bundle.h"

namespace dbus {
class Bus;
class Response;
class ErrorResponse;
}  // namespace dbus

namespace floss {
class FlossManagerClient;
}

namespace bluez {

// Style Note: Clients are sorted by names.
class BluetoothAdapterClient;
class BluetoothAdminPolicyClient;
class BluetoothAdvertisementMonitorManagerClient;
class BluetoothAgentManagerClient;
class BluetoothBatteryClient;
class BluetoothDebugManagerClient;
class BluetoothDeviceClient;
class BluetoothGattCharacteristicClient;
class BluetoothGattDescriptorClient;
class BluetoothGattManagerClient;
class BluetoothGattServiceClient;
class BluetoothInputClient;
class BluetoothLEAdvertisingManagerClient;
class BluetoothMediaClient;
class BluetoothMediaTransportClient;
class BluetoothProfileManagerClient;
class BluezDBusManagerSetter;

// BluezDBusManager manages manages D-Bus connections and D-Bus clients, which
// depend on the D-Bus thread to ensure the right order of shutdowns for
// the D-Bus thread, the D-Bus connections, and the D-Bus clients.
//
// CALLBACKS IN D-BUS CLIENTS:
//
// D-Bus clients managed by BluezDBusManagerSetter are guaranteed to be deleted
// after the D-Bus thread so the clients don't need to worry if new
// incoming messages arrive from the D-Bus thread during shutdown of the
// clients. The UI message loop is not running during the shutdown hence
// the UI message loop won't post tasks to D-BUS clients during the
// shutdown. However, to be extra cautious, clients should use
// WeakPtrFactory when creating callbacks that run on UI thread. See
// session_manager_client.cc for examples.
//
// Alternate D-Bus Client:
//
// BluezDBusManager is used by two separate clients. If both clients used the
// same DBus connection to talk to BlueZ, then they could override each others'
// state. For example, clients can start a scan with a set of filters; if
// client #1 sets filter A, and then client #2 sets filter B, BlueZ would only
// scan with filter B. BlueZ distinguishes between clients based on their D-Bus
// connection, so if two clients with different connections try to start a scan
// with two filters, BlueZ will merge these filters.
//
// For this reason, BluezDBusManager keeps two sets of the same client and uses
// two separate D-Bus connections: "Bluetooth*Client" and
// "AlternateBluetooth*Client".

class DEVICE_BLUETOOTH_EXPORT BluezDBusManager {
 public:
  // Initializes the global instance with a real client. Must be called before
  // any calls to Get(). We explicitly initialize and shutdown the global object
  // rather than making it a Singleton to ensure clean startup and shutdown.
  // * On Chrome OS, |system_bus| must not be null. It will be used as the
  //   primary bus and a secondary bus will be created.
  // * On Linux, |system_bus| is ignored and a primary bus only will be created
  //   and used.
  static void Initialize(dbus::Bus* system_bus);

  // Initializes the global instance with a fake client.
  static void InitializeFake();

  // Returns a BluezDBusManagerSetter instance that allows tests to
  // replace individual D-Bus clients with their own implementations.
  // Also initializes the main BluezDBusManager for testing if necessary.
  static std::unique_ptr<BluezDBusManagerSetter> GetSetterForTesting();

  // Returns true if BluezDBusManager has been initialized. Call this to
  // avoid initializing + shutting down BluezDBusManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static BluezDBusManager* Get();

  BluezDBusManager(const BluezDBusManager&) = delete;
  BluezDBusManager& operator=(const BluezDBusManager&) = delete;

  // Returns various D-Bus bus instances, owned by BluezDBusManager.
  dbus::Bus* GetSystemBus();

  // Returns true once we know whether Object Manager is supported or not.
  // Until this method returns true, no classes should try to use the
  // D-Bus Clients.
  bool IsObjectManagerSupportKnown() { return object_manager_support_known_; }

  // Calls |callback| once we know whether Object Manager is supported or not.
  void CallWhenObjectManagerSupportIsKnown(base::OnceClosure callback);

  // Returns true if Object Manager is supported.
  bool IsObjectManagerSupported() { return object_manager_supported_; }

  // Returns true if |client| is fake.
  bool IsUsingFakes() { return client_bundle_->IsUsingFakes(); }

  // All returned objects are owned by BluezDBusManager.  Do not use these
  // pointers after BluezDBusManager has been shut down.
  BluetoothAdapterClient* GetBluetoothAdapterClient();
  BluetoothAdminPolicyClient* GetBluetoothAdminPolicyClient();
  BluetoothAdvertisementMonitorManagerClient*
  GetBluetoothAdvertisementMonitorManagerClient();
  BluetoothLEAdvertisingManagerClient* GetBluetoothLEAdvertisingManagerClient();
  BluetoothAgentManagerClient* GetBluetoothAgentManagerClient();
  BluetoothBatteryClient* GetBluetoothBatteryClient();
  BluetoothDebugManagerClient* GetBluetoothDebugManagerClient();
  BluetoothDeviceClient* GetBluetoothDeviceClient();
  BluetoothGattCharacteristicClient* GetBluetoothGattCharacteristicClient();
  BluetoothGattDescriptorClient* GetBluetoothGattDescriptorClient();
  BluetoothGattManagerClient* GetBluetoothGattManagerClient();
  BluetoothGattServiceClient* GetBluetoothGattServiceClient();
  BluetoothInputClient* GetBluetoothInputClient();
  BluetoothMediaClient* GetBluetoothMediaClient();
  BluetoothMediaTransportClient* GetBluetoothMediaTransportClient();
  BluetoothProfileManagerClient* GetBluetoothProfileManagerClient();

  // See "Alternate D-Bus Client" note above.
  BluetoothAdapterClient* GetAlternateBluetoothAdapterClient();
  BluetoothAdminPolicyClient* GetAlternateBluetoothAdminPolicyClient();
  BluetoothDeviceClient* GetAlternateBluetoothDeviceClient();

 private:
  friend class BluezDBusManagerSetter;

  // Creates a new BluezDBusManager using the DBusClients set in
  // |client_bundle|. |alternate_bus| is used by a separate set of D-Bus
  // clients; see "Alternate D-Bus Client" note above.
  explicit BluezDBusManager(dbus::Bus* bus,
                            dbus::Bus* alternate_bus,
                            bool use_stubs);
  ~BluezDBusManager();

  // Creates a global instance of BluezDBusManager. Cannot be called more than
  // once.
  static void CreateGlobalInstance(dbus::Bus* bus,
                                   dbus::Bus* alternate_bus,
                                   bool use_stubs);

  void OnObjectManagerSupported(dbus::Response* response);
  void OnObjectManagerNotSupported(dbus::ErrorResponse* response);

  void OnFlossManagerServiceAvailable(bool is_available);
  void OnFlossObjectManagerSupported(dbus::Response* response);
  void OnFlossObjectManagerNotSupported(dbus::ErrorResponse* response);

  // Initializes all currently stored DBusClients with the system bus and
  // performs additional setup.
  void InitializeClients();

  raw_ptr<dbus::Bus> bus_;
  // Separate D-Bus connection used by the "Alternate" set of D-Bus clients. See
  // "Alternate D-Bus Client" note above.
  raw_ptr<dbus::Bus> alternate_bus_;

  std::unique_ptr<BluetoothDBusClientBundle> client_bundle_;

  // Needed to enable/disable Floss at D-Bus initialization. We treat this
  // D-Bus client specially and not include it in client bundle since we only
  // need to Init() it and nothing else.
  std::unique_ptr<floss::FlossManagerClient> floss_manager_client_;

  base::OnceClosure object_manager_support_known_callback_;

  bool object_manager_support_known_;
  bool object_manager_supported_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluezDBusManager> weak_ptr_factory_{this};
};

class DEVICE_BLUETOOTH_EXPORT BluezDBusManagerSetter {
 public:
  BluezDBusManagerSetter(const BluezDBusManagerSetter&) = delete;
  BluezDBusManagerSetter& operator=(const BluezDBusManagerSetter&) = delete;

  ~BluezDBusManagerSetter();

  void SetBluetoothAdapterClient(
      std::unique_ptr<BluetoothAdapterClient> client);
  void SetBluetoothAdminPolicyClient(
      std::unique_ptr<BluetoothAdminPolicyClient> client);
  void SetBluetoothAdvertisementMonitorManagerClient(
      std::unique_ptr<BluetoothAdvertisementMonitorManagerClient> client);
  void SetBluetoothLEAdvertisingManagerClient(
      std::unique_ptr<BluetoothLEAdvertisingManagerClient> client);
  void SetBluetoothAgentManagerClient(
      std::unique_ptr<BluetoothAgentManagerClient> client);
  void SetBluetoothBatteryClient(
      std::unique_ptr<BluetoothBatteryClient> client);
  void SetBluetoothDebugManagerClient(
      std::unique_ptr<BluetoothDebugManagerClient> client);
  void SetBluetoothDeviceClient(std::unique_ptr<BluetoothDeviceClient> client);
  void SetBluetoothGattCharacteristicClient(
      std::unique_ptr<BluetoothGattCharacteristicClient> client);
  void SetBluetoothGattDescriptorClient(
      std::unique_ptr<BluetoothGattDescriptorClient> client);
  void SetBluetoothGattManagerClient(
      std::unique_ptr<BluetoothGattManagerClient> client);
  void SetBluetoothGattServiceClient(
      std::unique_ptr<BluetoothGattServiceClient> client);
  void SetBluetoothInputClient(std::unique_ptr<BluetoothInputClient> client);
  void SetBluetoothMediaClient(std::unique_ptr<BluetoothMediaClient> client);
  void SetBluetoothMediaTransportClient(
      std::unique_ptr<BluetoothMediaTransportClient> client);
  void SetBluetoothProfileManagerClient(
      std::unique_ptr<BluetoothProfileManagerClient> client);

  void SetAlternateBluetoothAdapterClient(
      std::unique_ptr<BluetoothAdapterClient> client);
  void SetAlternateBluetoothAdminPolicyClient(
      std::unique_ptr<BluetoothAdminPolicyClient> client);
  void SetAlternateBluetoothDeviceClient(
      std::unique_ptr<BluetoothDeviceClient> client);

 private:
  friend class BluezDBusManager;

  BluezDBusManagerSetter();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_MANAGER_H_
