// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_dbus_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_admin_client.h"
#include "device/bluetooth/floss/fake_floss_advertiser_client.h"
#include "device/bluetooth/floss/fake_floss_battery_manager_client.h"
#include "device/bluetooth/floss/fake_floss_bluetooth_telephony_client.h"
#include "device/bluetooth/floss/fake_floss_gatt_manager_client.h"
#include "device/bluetooth/floss/fake_floss_lescan_client.h"
#include "device/bluetooth/floss/fake_floss_logging_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/fake_floss_socket_manager.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_advertiser_client.h"
#include "device/bluetooth/floss/floss_battery_manager_client.h"
#include "device/bluetooth/floss/floss_bluetooth_telephony_client.h"
#include "device/bluetooth/floss/floss_lescan_client.h"
#include "device/bluetooth/floss/floss_logging_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "device/bluetooth/floss/floss_socket_manager.h"
#include "floss_dbus_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/floss/floss_admin_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace floss {

namespace {
FlossDBusManager* g_floss_dbus_manager = nullptr;
}  // namespace

const int FlossDBusManager::kInvalidAdapter = -1;

static bool g_using_floss_dbus_manager_for_testing = false;

// Wait 2s for clients to become ready before timing out.
const int FlossDBusManager::kClientReadyTimeoutMs = 2000;
// Wait 10s for bluetooth service to become ready before timing out.
const int FlossDBusManager::kWaitServiceTimeoutMs = 10000;

FlossDBusManager::ClientInitializer::ClientInitializer(
    base::OnceClosure on_ready,
    int client_count)
    : expected_closure_count_(client_count),
      pending_client_ready_(client_count),
      on_ready_(std::move(on_ready)) {}

FlossDBusManager::ClientInitializer::~ClientInitializer() = default;

FlossDBusManager::FlossDBusManager(dbus::Bus* bus, bool use_stubs) : bus_(bus) {
  if (use_stubs) {
    client_bundle_ = std::make_unique<FlossClientBundle>(use_stubs);
    active_adapter_ = 0;
    object_manager_supported_ = true;
    object_manager_support_known_ = true;
    mgmt_client_present_ = true;
    InitializeAdapterClients(active_adapter_, base::DoNothing());
    return;
  }

  CHECK(GetSystemBus()) << "Can't initialize real clients without DBus.";

  BLUETOOTH_LOG(EVENT) << "FlossDBusManager checking for object manager";

#if BUILDFLAG(IS_CHROMEOS)
  // Floss is always available on ChromeOS but could not yet be available right
  // after boot. Always init the manager client here, which allows
  // |BluetoothAdapterFloss| to register its observers right now. The client
  // will be init-ed later by |WaitForServiceToBeAvailable|.
  object_manager_supported_ = true;
  object_manager_support_known_ = true;
  mgmt_client_present_ = true;
  client_bundle_ = std::make_unique<FlossClientBundle>(/*use_stubs=*/false);
  instance_created_time_ = base::Time::Now();
#endif

  // Wait for the Floss Manager to be available
  GetSystemBus()
      ->GetObjectProxy(kManagerService, dbus::ObjectPath("/"))
      ->WaitForServiceToBeAvailable(
          base::BindOnce(&FlossDBusManager::OnManagerServiceAvailable,
                         weak_ptr_factory_.GetWeakPtr()));
}

FlossDBusManager::~FlossDBusManager() = default;

dbus::PropertySet* FlossDBusManager::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  return new dbus::PropertySet(object_proxy, interface_name, base::DoNothing());
}

// Some interface is available.
void FlossDBusManager::ObjectAdded(const dbus::ObjectPath& object_path,
                                   const std::string& interface_name) {
  DVLOG(1) << __func__ << ": " << object_path.value() << ", " << interface_name;

  if (interface_name == kAdapterInterface) {
    if (adapter_interface_present_) {
      DVLOG(1) << kAdapterInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    adapter_interface_present_ = true;
    InitAdapterClientsIfReady();
  } else if (interface_name == kAdapterLoggingInterface) {
    if (adapter_logging_interface_present_) {
      DVLOG(1) << kAdapterLoggingInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    adapter_logging_interface_present_ = true;
    InitAdapterLoggingClientsIfReady();
#if BUILDFLAG(IS_CHROMEOS)
  } else if (interface_name == kAdminInterface) {
    if (admin_interface_present_) {
      DVLOG(1) << kAdminInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    admin_interface_present_ = true;
    InitAdminClientsIfReady();
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else if (interface_name == kBatteryManagerInterface) {
    if (battery_interface_present_) {
      DVLOG(1) << kBatteryManagerInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    battery_interface_present_ = true;
    InitBatteryClientsIfReady();
  } else if (interface_name == kBluetoothTelephonyInterface) {
    if (telephony_interface_present_) {
      DVLOG(1) << kBluetoothTelephonyInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    telephony_interface_present_ = true;
    InitTelephonyClientsIfReady();
  } else if (interface_name == kGattInterface) {
    if (gatt_interface_present_) {
      DVLOG(1) << kGattInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    gatt_interface_present_ = true;
    InitGattClientsIfReady();
  } else if (interface_name == kSocketManagerInterface) {
    if (socket_manager_interface_present_) {
      DVLOG(1) << kSocketManagerInterface
               << " has already been added. Not initializing the client!";
      return;
    }
    socket_manager_interface_present_ = true;
    InitSocketManagerClientsIfReady();
  } else {
    LOG(WARNING) << object_path.value() << ": Unknown interface "
                 << interface_name;
  }
}

// Some interface is gone (no longer present).
void FlossDBusManager::ObjectRemoved(const dbus::ObjectPath& object_path,
                                     const std::string& interface_name) {
  DVLOG(1) << __func__ << ": " << object_path.value() << ", " << interface_name;

  if (interface_name == kAdapterInterface) {
    adapter_interface_present_ = false;
  } else if (interface_name == kAdapterLoggingInterface) {
    adapter_logging_interface_present_ = false;
#if BUILDFLAG(IS_CHROMEOS)
  } else if (interface_name.compare(std::string(kAdminInterface)) == 0) {
    admin_interface_present_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else if (interface_name == kBatteryManagerInterface) {
    battery_interface_present_ = false;
  } else if (interface_name == kBluetoothTelephonyInterface) {
    telephony_interface_present_ = false;
  } else if (interface_name == kGattInterface) {
    gatt_interface_present_ = false;
  } else if (interface_name == kSocketManagerInterface) {
    socket_manager_interface_present_ = false;
  } else {
    LOG(WARNING) << object_path.value() << ": Unknown interface "
                 << interface_name;
  }
}

void FlossDBusManager::InitAdapterClientsIfReady() {
  if (adapter_interface_present_ && HasActiveAdapter() && client_on_ready_ &&
      !client_on_ready_->Finished()) {
    GetAdapterClient()->Init(GetSystemBus(), kAdapterService, active_adapter_,
                             version_, client_on_ready_->CreateReadyClosure());
  }
}

void FlossDBusManager::InitAdapterLoggingClientsIfReady() {
  if (adapter_logging_interface_present_ && HasActiveAdapter() &&
      client_on_ready_ && !client_on_ready_->Finished()) {
    GetLoggingClient()->Init(GetSystemBus(), kAdapterService, active_adapter_,
                             version_, client_on_ready_->CreateReadyClosure());
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void FlossDBusManager::InitAdminClientsIfReady() {
  if (admin_interface_present_ && HasActiveAdapter() && client_on_ready_ &&
      !client_on_ready_->Finished()) {
    GetAdminClient()->Init(GetSystemBus(), kAdapterService, active_adapter_,
                           version_, client_on_ready_->CreateReadyClosure());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FlossDBusManager::InitBatteryClientsIfReady() {
  if (battery_interface_present_ && HasActiveAdapter() && client_on_ready_) {
    GetBatteryManagerClient()->Init(GetSystemBus(), kAdapterService,
                                    active_adapter_, version_,
                                    client_on_ready_->CreateReadyClosure());
  }
}

void FlossDBusManager::InitTelephonyClientsIfReady() {
  if (telephony_interface_present_ && HasActiveAdapter() && client_on_ready_ &&
      !client_on_ready_->Finished()) {
    GetBluetoothTelephonyClient()->Init(GetSystemBus(), kAdapterService,
                                        active_adapter_, version_,
                                        client_on_ready_->CreateReadyClosure());
  }
}

void FlossDBusManager::InitGattClientsIfReady() {
  if (gatt_interface_present_ && HasActiveAdapter() && client_on_ready_ &&
      !client_on_ready_->Finished()) {
    GetGattManagerClient()->Init(GetSystemBus(), kAdapterService,
                                 active_adapter_, version_,
                                 client_on_ready_->CreateReadyClosure());
    GetLEScanClient()->Init(GetSystemBus(), kAdapterService, active_adapter_,
                            version_, client_on_ready_->CreateReadyClosure());
    GetAdvertiserClient()->Init(GetSystemBus(), kAdapterService,
                                active_adapter_, version_,
                                client_on_ready_->CreateReadyClosure());
  }
}

void FlossDBusManager::InitSocketManagerClientsIfReady() {
  if (socket_manager_interface_present_ && HasActiveAdapter() &&
      client_on_ready_ && !client_on_ready_->Finished()) {
    GetSocketManager()->Init(GetSystemBus(), kAdapterService, active_adapter_,
                             version_, client_on_ready_->CreateReadyClosure());
  }
}

// static
void FlossDBusManager::Initialize(dbus::Bus* system_bus) {
  // If we initialize FlossDBusManager twice we may also be shutting it down
  // early; do not allow that.
  CHECK(!g_floss_dbus_manager);

  VLOG(1) << "FlossDBusManager about to initialize thread manager";

  CreateGlobalInstance(system_bus, /*use_stubs=*/false);
}

void FlossDBusManager::InitializeFake() {
  CreateGlobalInstance(nullptr, /*use_stubs=*/true);
}

void FlossDBusManager::SetAllClientsPresentForTesting() {
  mgmt_client_present_ = true;
  adapter_interface_present_ = true;
  adapter_logging_interface_present_ = true;
#if BUILDFLAG(IS_CHROMEOS)
  admin_interface_present_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  battery_interface_present_ = true;
  telephony_interface_present_ = true;
  gatt_interface_present_ = true;
  socket_manager_interface_present_ = true;
}

// static
std::unique_ptr<FlossDBusManagerSetter>
floss::FlossDBusManager::GetSetterForTesting() {
  if (!g_using_floss_dbus_manager_for_testing) {
    g_using_floss_dbus_manager_for_testing = true;
    CreateGlobalInstance(nullptr, /*use_stubs=*/true);
  }

  FlossDBusManager::Get()->SetAllClientsPresentForTesting();

  return base::WrapUnique(new FlossDBusManagerSetter());
}

// static
bool FlossDBusManager::IsInitialized() {
  return g_floss_dbus_manager;
}

// static
void FlossDBusManager::Shutdown() {
  // Ensure that we only shutdown FlossDBusManager once.
  CHECK(g_floss_dbus_manager);
  delete g_floss_dbus_manager;
  g_floss_dbus_manager = nullptr;

  DVLOG(1) << "FlossDBusManager Shutdown completed";
}

// static
FlossDBusManager* FlossDBusManager::Get() {
  CHECK(g_floss_dbus_manager)
      << "FlossDBusManager::Get() called before Initialize()";
  return g_floss_dbus_manager;
}

// static
void FlossDBusManager::CreateGlobalInstance(dbus::Bus* bus, bool use_stubs) {
  CHECK(!g_floss_dbus_manager);
  g_floss_dbus_manager = new FlossDBusManager(bus, use_stubs);
  VLOG(1) << "FlossDBusManager CreateGlobalInstance";
}

bool FlossDBusManager::CallWhenObjectManagerSupportIsKnown(
    base::OnceClosure callback) {
  DCHECK(!object_manager_support_known_callback_);

  if (object_manager_support_known_) {
    std::move(callback).Run();
    return true;
  }

  object_manager_support_known_callback_ = std::move(callback);
  return false;
}

void FlossDBusManager::OnObjectManagerSupported(dbus::Response* response) {
  object_manager_supported_ = true;

  if (!client_bundle_) {
    client_bundle_ = std::make_unique<FlossClientBundle>(/*use_stubs=*/false);
  }

  // Initialize the manager client (which doesn't depend on any specific
  // adapter being present)
  client_bundle_->manager_client()->Init(
      GetSystemBus(), kManagerInterface, kInvalidAdapter, base::Version(),
      base::BindOnce(&FlossDBusManager::OnManagerClientInitComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  // Register object manager for Manager.
  object_manager_ =
      GetSystemBus()->GetObjectManager(kAdapterService, dbus::ObjectPath("/"));
  object_manager_->RegisterInterface(kAdapterInterface, this);
  object_manager_->RegisterInterface(kAdapterLoggingInterface, this);
#if BUILDFLAG(IS_CHROMEOS)
  object_manager_->RegisterInterface(kAdminInterface, this);
#endif  // BUILDFLAG(IS_CHROMEOS)
  object_manager_->RegisterInterface(kBatteryManagerInterface, this);
  object_manager_->RegisterInterface(kBluetoothTelephonyInterface, this);
  object_manager_->RegisterInterface(kGattInterface, this);
  object_manager_->RegisterInterface(kSocketManagerInterface, this);
}

void FlossDBusManager::OnManagerServiceAvailable(bool is_available) {
  BLUETOOTH_LOG(EVENT) << "Floss Manager is available: " << is_available;
  if (!is_available) {
#if BUILDFLAG(IS_CHROMEOS)
    device::RecordFlossManagerClientInit(
        false, base::Time::Now() - instance_created_time_);
#endif  // BUILDFLAG(IS_CHROMEOS)
    if (!object_manager_support_known_) {
      object_manager_support_known_ = true;
      if (object_manager_support_known_callback_) {
        std::move(object_manager_support_known_callback_).Run();
      }
    }
    return;
  }

  dbus::MethodCall method_call(dbus::kObjectManagerInterface,
                               dbus::kObjectManagerGetManagedObjects);

  // Sets up callbacks checking for object manager support. Object manager is
  // registered on the root object "/"
  GetSystemBus()
      ->GetObjectProxy(kManagerService, dbus::ObjectPath("/"))
      ->CallMethodWithErrorCallback(
          &method_call, kDBusTimeoutMs,
          base::BindOnce(&FlossDBusManager::OnObjectManagerSupported,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&FlossDBusManager::OnObjectManagerNotSupported,
                         weak_ptr_factory_.GetWeakPtr()));
}

void FlossDBusManager::OnObjectManagerNotSupported(
    dbus::ErrorResponse* response) {
  BLUETOOTH_LOG(ERROR) << "Floss Bluetooth not supported.";
#if BUILDFLAG(IS_CHROMEOS)
  device::RecordFlossManagerClientInit(
      false, base::Time::Now() - instance_created_time_);
#endif  // BUILDFLAG(IS_CHROMEOS)
  object_manager_supported_ = false;

  // Don't initialize any clients since they need ObjectManager.

  object_manager_support_known_ = true;
  if (object_manager_support_known_callback_) {
    std::move(object_manager_support_known_callback_).Run();
  }
}

void FlossDBusManager::OnManagerClientInitComplete() {
  mgmt_client_present_ = client_bundle_->manager_client()->IsInitialized();
  DVLOG(1) << "Floss manager client initialized: " << mgmt_client_present_;
#if BUILDFLAG(IS_CHROMEOS)
  device::RecordFlossManagerClientInit(
      mgmt_client_present_, base::Time::Now() - instance_created_time_);
#endif  // BUILDFLAG(IS_CHROMEOS)
  object_manager_support_known_ = true;
  if (object_manager_support_known_callback_) {
    std::move(object_manager_support_known_callback_).Run();
  }
}

void FlossDBusManager::SwitchAdapter(int adapter, base::OnceClosure on_ready) {
  if (!object_manager_supported_) {
    DVLOG(1) << "Floss can't switch to adapter without object manager";
    std::move(on_ready).Run();
    return;
  }

  InitializeAdapterClients(adapter, std::move(on_ready));
  return;
}

bool FlossDBusManager::HasActiveAdapter() const {
  return active_adapter_ != kInvalidAdapter;
}

int FlossDBusManager::GetActiveAdapter() const {
  return active_adapter_;
}

bool FlossDBusManager::AreClientsReady() const {
  return client_on_ready_ && client_on_ready_->Finished();
}

FlossAdapterClient* FlossDBusManager::GetAdapterClient() {
  return client_bundle_->adapter_client();
}

FlossGattManagerClient* FlossDBusManager::GetGattManagerClient() {
  return client_bundle_->gatt_manager_client();
}

FlossManagerClient* FlossDBusManager::GetManagerClient() {
  return client_bundle_->manager_client();
}

FlossSocketManager* FlossDBusManager::GetSocketManager() {
  return client_bundle_->socket_manager();
}

FlossLEScanClient* FlossDBusManager::GetLEScanClient() {
  return client_bundle_->lescan_client();
}

FlossAdvertiserClient* FlossDBusManager::GetAdvertiserClient() {
  return client_bundle_->advertiser_client();
}

FlossBatteryManagerClient* FlossDBusManager::GetBatteryManagerClient() {
  return client_bundle_->battery_manager_client();
}

FlossBluetoothTelephonyClient* FlossDBusManager::GetBluetoothTelephonyClient() {
  return client_bundle_->bluetooth_telephony_client();
}

FlossLoggingClient* FlossDBusManager::GetLoggingClient() {
  return client_bundle_->logging_client();
}

#if BUILDFLAG(IS_CHROMEOS)
FlossAdminClient* FlossDBusManager::GetAdminClient() {
  return client_bundle_->admin_client();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FlossDBusManager::InitializeAdapterClients(int adapter,
                                                base::OnceClosure on_ready) {
  // Initializing already current adapter.
  if (active_adapter_ == adapter) {
    std::move(on_ready).Run();
    return;
  }

  // Clean up active adapter clients
  if (active_adapter_ != kInvalidAdapter) {
    client_bundle_->ResetAdapterClients();
  }

  // Set current adapter. If it's kInvalidAdapter, this doesn't need to do any
  // init.
  active_adapter_ = adapter;
  if (adapter == kInvalidAdapter) {
    std::move(on_ready).Run();
    return;
  }

  // Set current Floss API version.
  version_ = client_bundle_->manager_client()->GetFlossApiVersion();

  // Initialize callback readiness. If clients aren't ready within a certain
  // period, we will time out and send the ready signal anyway.
  client_on_ready_ = ClientInitializer::CreateWithTimeout(
      std::move(on_ready),
#if BUILDFLAG(IS_CHROMEOS)
      /*client_count=*/9,
#else
      /*client_count=*/8,
#endif
      base::Milliseconds(kClientReadyTimeoutMs));

  // Adapter is set. Try to init clients if the interface present.
  InitAdapterClientsIfReady();
  InitAdapterLoggingClientsIfReady();
#if BUILDFLAG(IS_CHROMEOS)
  InitAdminClientsIfReady();
#endif  // BUILDFLAG(IS_CHROMEOS)
  InitBatteryClientsIfReady();
  InitTelephonyClientsIfReady();
  InitGattClientsIfReady();
  InitSocketManagerClientsIfReady();
}

void FlossDBusManagerSetter::SetFlossManagerClient(
    std::unique_ptr<FlossManagerClient> client) {
  FlossDBusManager::Get()->client_bundle_->manager_client_ = std::move(client);
}

void FlossDBusManagerSetter::SetFlossAdapterClient(
    std::unique_ptr<FlossAdapterClient> client) {
  FlossDBusManager::Get()->client_bundle_->adapter_client_ = std::move(client);
}

void FlossDBusManagerSetter::SetFlossGattManagerClient(
    std::unique_ptr<FlossGattManagerClient> client) {
  FlossDBusManager::Get()->client_bundle_->gatt_manager_client_ =
      std::move(client);
}

void FlossDBusManagerSetter::SetFlossSocketManager(
    std::unique_ptr<FlossSocketManager> mgr) {
  FlossDBusManager::Get()->client_bundle_->socket_manager_ = std::move(mgr);
}

void FlossDBusManagerSetter::SetFlossLEScanClient(
    std::unique_ptr<FlossLEScanClient> client) {
  FlossDBusManager::Get()->client_bundle_->lescan_client_ = std::move(client);
}

void FlossDBusManagerSetter::SetFlossAdvertiserClient(
    std::unique_ptr<FlossAdvertiserClient> client) {
  FlossDBusManager::Get()->client_bundle_->advertiser_client_ =
      std::move(client);
}

void FlossDBusManagerSetter::SetFlossBatteryManagerClient(
    std::unique_ptr<FlossBatteryManagerClient> client) {
  FlossDBusManager::Get()->client_bundle_->battery_manager_client_ =
      std::move(client);
}

void FlossDBusManagerSetter::SetFlossBluetoothTelephonyClient(
    std::unique_ptr<FlossBluetoothTelephonyClient> client) {
  FlossDBusManager::Get()->client_bundle_->bluetooth_telephony_client_ =
      std::move(client);
}

void FlossDBusManagerSetter::SetFlossLoggingClient(
    std::unique_ptr<FlossLoggingClient> client) {
  FlossDBusManager::Get()->client_bundle_->logging_client_ = std::move(client);
}

#if BUILDFLAG(IS_CHROMEOS)
void FlossDBusManagerSetter::SetFlossAdminClient(
    std::unique_ptr<FlossAdminClient> client) {
  FlossDBusManager::Get()->client_bundle_->admin_client_ = std::move(client);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

FlossClientBundle::FlossClientBundle(bool use_stubs) : use_stubs_(use_stubs) {
#if defined(USE_REAL_DBUS_CLIENTS)
  if (use_stubs) {
    LOG(FATAL) << "Fakes are unavailable if USE_REAL_DBUS_CLIENTS is defined.";
  }
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  if (!use_stubs) {
    manager_client_ = FlossManagerClient::Create();
  } else {
    manager_client_ = std::make_unique<FakeFlossManagerClient>();
  }

  ResetAdapterClients();
}

FlossClientBundle::~FlossClientBundle() = default;

void FlossClientBundle::ResetAdapterClients() {
#if defined(USE_REAL_DBUS_CLIENTS)
  if (use_stubs_) {
    LOG(FATAL) << "Fakes are unavailable if USE_REAL_DBUS_CLIENTS is defined.";
  }
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  if (!use_stubs_) {
    adapter_client_ = FlossAdapterClient::Create();
    gatt_manager_client_ = FlossGattManagerClient::Create();
    socket_manager_ = FlossSocketManager::Create();
    lescan_client_ = FlossLEScanClient::Create();
    advertiser_client_ = FlossAdvertiserClient::Create();
    battery_manager_client_ = FlossBatteryManagerClient::Create();
    bluetooth_telephony_client_ = FlossBluetoothTelephonyClient::Create();
    logging_client_ = FlossLoggingClient::Create();
#if BUILDFLAG(IS_CHROMEOS)
    admin_client_ = FlossAdminClient::Create();
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else {
    adapter_client_ = std::make_unique<FakeFlossAdapterClient>();
    gatt_manager_client_ = std::make_unique<FakeFlossGattManagerClient>();
    socket_manager_ = std::make_unique<FakeFlossSocketManager>();
    lescan_client_ = std::make_unique<FakeFlossLEScanClient>();
    advertiser_client_ = std::make_unique<FakeFlossAdvertiserClient>();
    battery_manager_client_ = std::make_unique<FakeFlossBatteryManagerClient>();
    bluetooth_telephony_client_ =
        std::make_unique<FakeFlossBluetoothTelephonyClient>();
    logging_client_ = std::make_unique<FakeFlossLoggingClient>();
#if BUILDFLAG(IS_CHROMEOS)
    admin_client_ = std::make_unique<FakeFlossAdminClient>();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
}

}  // namespace floss
