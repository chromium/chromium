// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_MANAGER_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "dbus/object_manager.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_version.h"

namespace base {
class Thread;
}  // namespace base

namespace dbus {
class Bus;
class Response;
class ErrorResponse;
}  // namespace dbus

namespace floss {

class FlossAdapterClient;
class FlossAdvertiserClient;
class FlossBatteryManagerClient;
class FlossClientBundle;
class FlossDBusManagerSetter;
class FlossGattManagerClient;
class FlossLEScanClient;
class FlossLoggingClient;
class FlossManagerClient;
class FlossBluetoothTelephonyClient;
class FlossSocketManager;

#if BUILDFLAG(IS_CHROMEOS)
class FlossAdminClient;
#endif  // BUILDFLAG(IS_CHROMEOS)

// FlossDBusManager manages the lifetimes of D-Bus connections and clients. It
// ensures the proper ordering of shutdowns for the D-Bus thread, connections
// and clients.
//
// While similar to the BluezDBusManager, it requires totally different client
// implementations since the Floss dbus apis are drastically different. Thus, it
// doesn't make sense to share a common implementation between the two.
class DEVICE_BLUETOOTH_EXPORT FlossDBusManager
    : public dbus::ObjectManager::Interface {
 public:
  class ClientInitializer {
   public:
    ClientInitializer(base::OnceClosure on_ready, int client_count);
    ~ClientInitializer();

    static std::unique_ptr<ClientInitializer> CreateWithTimeout(
        base::OnceClosure on_ready,
        int client_count,
        base::TimeDelta timeout) {
      std::unique_ptr<ClientInitializer> self =
          std::make_unique<ClientInitializer>(std::move(on_ready),
                                              client_count);
      self->ScheduleTimeout(timeout);

      return self;
    }

    // Grab closure to indicate client is ready and decrement expected closure
    // count.
    base::OnceClosure CreateReadyClosure() {
      DCHECK(expected_closure_count_ > 0);

      --expected_closure_count_;
      return base::BindOnce(&ClientInitializer::OnReady,
                            weak_ptr_factory_.GetWeakPtr());
    }

    void OnReady() {
      DCHECK(pending_client_ready_ > 0);

      pending_client_ready_--;
      if (pending_client_ready_ == 0 && on_ready_) {
        DCHECK(expected_closure_count_ == 0);
        std::move(on_ready_).Run();
      }
    }

    void OnTimeout() {
      if (pending_client_ready_ > 0) {
        LOG(WARNING) << "ClientInitializer timed out with pending clients="
                     << pending_client_ready_;
      }

      if (on_ready_) {
        std::move(on_ready_).Run();
      }
    }

    bool Finished() { return on_ready_.is_null(); }

   private:
    void ScheduleTimeout(base::TimeDelta timeout) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ClientInitializer::OnTimeout,
                         weak_ptr_factory_.GetWeakPtr()),
          timeout);
    }

    int expected_closure_count_;
    int pending_client_ready_;
    base::OnceClosure on_ready_;

    base::WeakPtrFactory<ClientInitializer> weak_ptr_factory_{this};
  };

  // Initializes the global instance with a real client. Must be called before
  // any calls to Get(). We explicitly initialize and shutdown the global object
  // rather than making it a Singleton to ensure clean startup and shutdown.
  static void Initialize(dbus::Bus* system_bus);

  // Initializes the global instance with a fake client
  // TODO(b/193712484) - Implement this. Only added here to fix debug build.
  static void InitializeFake();

  // Returns a FlossDBusManagerSetter instance that allows tests to
  // replace individual D-Bus clients with their own implementations.
  // Also initializes the main FlossDBusManager for testing if necessary.
  static std::unique_ptr<FlossDBusManagerSetter> GetSetterForTesting();

  // Returns true if FlossDBusManager has been initialized. Call this to
  // avoid initializing + shutting down FlossDBusManager more than once.
  static bool IsInitialized();

  // Destroys the global instance. This will clean up all the clients managed by
  // the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static FlossDBusManager* Get();

  // Adapters are indexed from 0-N (corresponding to hci0..hciN). Use negative
  // value to represent invalid adapter.
  static const int kInvalidAdapter;

  // Timeout to wait for clients to become ready.
  static const int kClientReadyTimeoutMs;
  // Timeout to wait for bluetooth service to become ready.
  static const int kWaitServiceTimeoutMs;

  FlossDBusManager(const FlossDBusManager&) = delete;
  FlossDBusManager& operator=(const FlossDBusManager&) = delete;

  // Calls |callback| once we know whether Object Manager is supported or not.
  // If the object manager support is already known when this is called,
  // |callback| is run right away and this function returns true. Otherwise, it
  // will return false.
  bool CallWhenObjectManagerSupportIsKnown(base::OnceClosure callback);

  // Returns true if Object Manager is support is known.
  bool IsObjectManagerSupportKnown() const {
    return object_manager_support_known_;
  }

  // Returns true if Object Manager is supported.
  bool IsObjectManagerSupported() const {
    return object_manager_supported_ && mgmt_client_present_;
  }

  // Shuts down the existing adapter clients and initializes a new set for the
  // given adapter. When the new adapter clients are ready, calls the |on_ready|
  // callback.
  void SwitchAdapter(int adapter, base::OnceClosure on_ready);

  // Checks whether an adapter is currently enabled and being used.
  bool HasActiveAdapter() const;

  // Get the active adapter.
  int GetActiveAdapter() const;

  // Checks whether the necessary clients are ready. This will happen
  // asynchronously and may take multiple seconds during tests.
  bool AreClientsReady() const;

  // Returns system bus pointer (owned by FlossDBusThreadManager).
  dbus::Bus* GetSystemBus() const { return bus_; }

  // Gets Floss API version.
  base::Version GetFlossApiVersion() const { return version_; }

  // All returned objects are owned by FlossDBusManager. Do not use these
  // pointers after FlossDBusManager has been shut down.
  FlossAdapterClient* GetAdapterClient();
  FlossAdvertiserClient* GetAdvertiserClient();
  FlossBatteryManagerClient* GetBatteryManagerClient();
  FlossGattManagerClient* GetGattManagerClient();
  FlossLEScanClient* GetLEScanClient();
  FlossLoggingClient* GetLoggingClient();
  FlossManagerClient* GetManagerClient();
  FlossBluetoothTelephonyClient* GetBluetoothTelephonyClient();
  FlossSocketManager* GetSocketManager();

#if BUILDFLAG(IS_CHROMEOS)
  FlossAdminClient* GetAdminClient();
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  friend class FlossDBusManagerTest;
  // dbus::ObjectManager::Interface overrides
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override;
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override;

  // Keep track of the object manager so we can keep track of when the manager
  // disappears. Managed by the bus object (do not delete).
  raw_ptr<dbus::ObjectManager> object_manager_ = nullptr;

 private:
  friend class FlossDBusManagerSetter;

  // Creates a global instance of FlossDBusManager. Cannot be called more than
  // once.
  static void CreateGlobalInstance(dbus::Bus* bus, bool use_stubs);

  FlossDBusManager(dbus::Bus* bus, bool use_stubs);
  ~FlossDBusManager() override;

  void OnObjectManagerSupported(dbus::Response* response);
  void OnObjectManagerNotSupported(dbus::ErrorResponse* response);
  void OnManagerClientInitComplete();
  void OnManagerServiceAvailable(bool is_available);
  void OnWaitServiceAvailableTimeout();

  // Initializes the manager client
  void InitializeManagerClient();

  // Initializes all currently stored DBusClients with the system bus and
  // performs additional setup for a specific adapter. Once all clients are
  // ready, calls the |on_ready| callback.
  void InitializeAdapterClients(int adapter, base::OnceClosure on_ready);

  void InitAdapterClientsIfReady();
  void InitAdapterLoggingClientsIfReady();
#if BUILDFLAG(IS_CHROMEOS)
  void InitAdminClientsIfReady();
#endif  // BUILDFLAG(IS_CHROMEOS)
  void InitBatteryClientsIfReady();
  void InitTelephonyClientsIfReady();
  void InitGattClientsIfReady();
  void InitSocketManagerClientsIfReady();

  void SetAllClientsPresentForTesting();

  // System bus instance (owned by FlossDBusThreadManager).
  raw_ptr<dbus::Bus> bus_;

  // Bundle together all Floss clients to be initialized and shutdown in
  // a specified order.
  std::unique_ptr<FlossClientBundle> client_bundle_ = nullptr;

  // FlossDBusManager depends on object manager support to know when the manager
  // object has been added (via the InterfaceAdded signal) so it can detect if
  // an adapter is present. This callback gets called after GetManagedObjects
  // returns a response (an array or an error).
  base::OnceClosure object_manager_support_known_callback_;
  bool object_manager_support_known_ = false;
  bool object_manager_supported_ = false;

  // Whether the manager client has been initialized successfully.
  bool mgmt_client_present_ = false;

  bool adapter_interface_present_ = false;
  bool adapter_logging_interface_present_ = false;
#if BUILDFLAG(IS_CHROMEOS)
  bool admin_interface_present_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)
  bool battery_interface_present_ = false;
  bool telephony_interface_present_ = false;
  bool gatt_interface_present_ = false;
  bool socket_manager_interface_present_ = false;
  base::Time instance_created_time_;

  // Currently active Bluetooth adapter
  int active_adapter_ = kInvalidAdapter;

  // Floss API version exported by Floss daemon or
  // specified by a test stub for unit tests.
  base::Version version_;

  // Callback for when adapter clients are ready after init.
  std::unique_ptr<ClientInitializer> client_on_ready_;

  base::WeakPtrFactory<FlossDBusManager> weak_ptr_factory_{this};
};

class DEVICE_BLUETOOTH_EXPORT FlossDBusManagerSetter {
 public:
  void SetFlossAdapterClient(std::unique_ptr<FlossAdapterClient> client);
  void SetFlossAdvertiserClient(std::unique_ptr<FlossAdvertiserClient> client);
  void SetFlossBatteryManagerClient(
      std::unique_ptr<FlossBatteryManagerClient> client);
  void SetFlossGattManagerClient(
      std::unique_ptr<FlossGattManagerClient> client);
  void SetFlossLEScanClient(std::unique_ptr<FlossLEScanClient> client);
  void SetFlossLoggingClient(std::unique_ptr<FlossLoggingClient> client);
  void SetFlossManagerClient(std::unique_ptr<FlossManagerClient> client);
  void SetFlossBluetoothTelephonyClient(
      std::unique_ptr<FlossBluetoothTelephonyClient> client);
  void SetFlossSocketManager(std::unique_ptr<FlossSocketManager> manager);
#if BUILDFLAG(IS_CHROMEOS)
  void SetFlossAdminClient(std::unique_ptr<FlossAdminClient> client);
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// FlossDBusThreadManager manages the D-Bus thread, the thread dedicated to
// handling asynchronous D-Bus operations.
class DEVICE_BLUETOOTH_EXPORT FlossDBusThreadManager {
 public:
  FlossDBusThreadManager(const FlossDBusThreadManager&) = delete;
  FlossDBusThreadManager& operator=(const FlossDBusThreadManager&) = delete;

  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static FlossDBusThreadManager* Get();

  // Returns system bus pointer (owned by FlossDBusThreadManager).
  dbus::Bus* GetSystemBus();

 private:
  FlossDBusThreadManager();
  ~FlossDBusThreadManager();

  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
};

// The bundle of all DBus clients used by FlossDBusManager. The bundle is used
// to control the correct ordering of creation and deletion of clients before
// shutting down the system bus.
class DEVICE_BLUETOOTH_EXPORT FlossClientBundle {
 public:
  FlossClientBundle(const FlossClientBundle&) = delete;
  FlossClientBundle& operator=(const FlossClientBundle&) = delete;

  explicit FlossClientBundle(bool use_stubs);
  ~FlossClientBundle();

  FlossManagerClient* manager_client() { return manager_client_.get(); }

  FlossAdapterClient* adapter_client() { return adapter_client_.get(); }

  FlossGattManagerClient* gatt_manager_client() {
    return gatt_manager_client_.get();
  }

  FlossSocketManager* socket_manager() { return socket_manager_.get(); }

  FlossLEScanClient* lescan_client() { return lescan_client_.get(); }

  FlossLoggingClient* logging_client() { return logging_client_.get(); }

  FlossAdvertiserClient* advertiser_client() {
    return advertiser_client_.get();
  }
#if BUILDFLAG(IS_CHROMEOS)
  FlossAdminClient* admin_client() { return admin_client_.get(); }
#endif  // BUILDFLAG(IS_CHROMEOS)

  FlossBatteryManagerClient* battery_manager_client() {
    return battery_manager_client_.get();
  }

  FlossBluetoothTelephonyClient* bluetooth_telephony_client() {
    return bluetooth_telephony_client_.get();
  }

 private:
  friend FlossDBusManagerSetter;
  friend FlossDBusManager;

  void ResetAdapterClients();

  bool use_stubs_;
  std::unique_ptr<FlossManagerClient> manager_client_;
  std::unique_ptr<FlossAdapterClient> adapter_client_;
  std::unique_ptr<FlossGattManagerClient> gatt_manager_client_;
  std::unique_ptr<FlossSocketManager> socket_manager_;
  std::unique_ptr<FlossLEScanClient> lescan_client_;
  std::unique_ptr<FlossLoggingClient> logging_client_;
  std::unique_ptr<FlossAdvertiserClient> advertiser_client_;
  std::unique_ptr<FlossBatteryManagerClient> battery_manager_client_;
  std::unique_ptr<FlossBluetoothTelephonyClient> bluetooth_telephony_client_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<FlossAdminClient> admin_client_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_MANAGER_H_
