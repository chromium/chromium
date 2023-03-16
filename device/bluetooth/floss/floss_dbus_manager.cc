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
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_admin_client.h"
#include "device/bluetooth/floss/fake_floss_advertiser_client.h"
#include "device/bluetooth/floss/fake_floss_battery_manager_client.h"
#include "device/bluetooth/floss/fake_floss_gatt_manager_client.h"
#include "device/bluetooth/floss/fake_floss_lescan_client.h"
#include "device/bluetooth/floss/fake_floss_logging_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/fake_floss_socket_manager.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_advertiser_client.h"
#include "device/bluetooth/floss/floss_battery_manager_client.h"
#include "device/bluetooth/floss/floss_lescan_client.h"
#include "device/bluetooth/floss/floss_logging_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "device/bluetooth/floss/floss_socket_manager.h"

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
constexpr int kClientReadyTimeoutMs = 2000;

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
    InitializeAdapterClients(active_adapter_, base::DoNothing());
    return;
  }

  CHECK(GetSystemBus()) << "Can't initialize real clients without DBus.";

  dbus::MethodCall method_call(dbus::kObjectManagerInterface,
                               dbus::kObjectManagerGetManagedObjects);

  VLOG(1) << "FlossDBusManager checking for object manager";
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

FlossDBusManager::~FlossDBusManager() = default;

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

// static
std::unique_ptr<FlossDBusManagerSetter>
floss::FlossDBusManager::GetSetterForTesting() {
  if (!g_using_floss_dbus_manager_for_testing) {
    g_using_floss_dbus_manager_for_testing = true;
    CreateGlobalInstance(nullptr, /*use_stubs=*/true);
  }

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
  DVLOG(1) << "Floss Bluetooth supported. Initializing clients.";
  object_manager_supported_ = true;

  client_bundle_ = std::make_unique<FlossClientBundle>(/*use_stubs=*/false);

  // Initialize the manager client (which doesn't depend on any specific
  // adapter being present)
  client_bundle_->manager_client()->Init(GetSystemBus(), kManagerInterface,
                                         kInvalidAdapter, base::DoNothing());

  object_manager_support_known_ = true;
  if (object_manager_support_known_callback_) {
    std::move(object_manager_support_known_callback_).Run();
  }
}

void FlossDBusManager::OnObjectManagerNotSupported(
    dbus::ErrorResponse* response) {
  DVLOG(1) << "Floss Bluetooth not supported.";
  object_manager_supported_ = false;

  // Don't initialize any clients since they need ObjectManager.

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
  // Clean up active adapter clients
  if (active_adapter_ != kInvalidAdapter) {
    client_bundle_->ResetAdapterClients();
  }

  // Initializing already current adapter.
  if (active_adapter_ == adapter) {
    std::move(on_ready).Run();
    return;
  }

  // Set current adapter. If it's kInvalidAdapter, this doesn't need to do any
  // init.
  active_adapter_ = adapter;
  if (adapter == kInvalidAdapter) {
    std::move(on_ready).Run();
    return;
  }

  // Initialize callback readiness. If clients aren't ready within a certain
  // period, we will time out and send the ready signal anyway.
  client_on_ready_ = ClientInitializer::CreateWithTimeout(
      std::move(on_ready),
#if BUILDFLAG(IS_CHROMEOS)
      /*client_count=*/8,
#else
      /*client_count=*/7,
#endif
      base::Milliseconds(kClientReadyTimeoutMs));

  // Initialize any adapter clients.
  client_bundle_->adapter_client()->Init(
      GetSystemBus(), kAdapterService, active_adapter_,
      client_on_ready_->CreateReadyClosure());
  client_bundle_->gatt_manager_client()->Init(
      GetSystemBus(), kAdapterService, active_adapter_,
      client_on_ready_->CreateReadyClosure());
  client_bundle_->socket_manager()->Init(
      GetSystemBus(), kAdapterService, active_adapter_,
      client_on_ready_->CreateReadyClosure());
  client_bundle_->lescan_client()->Init(GetSystemBus(), kAdapterService,
                                        active_adapter_,
                                        client_on_ready_->CreateReadyClosure());
  client_bundle_->advertiser_client()->Init(
      GetSystemBus(), kAdapterService, active_adapter_,
      client_on_ready_->CreateReadyClosure());
  client_bundle_->battery_manager_client()->Init(
      GetSystemBus(), kAdapterService, active_adapter_,
      client_on_ready_->CreateReadyClosure());
  client_bundle_->logging_client()->Init(
      GetSystemBus(), kAdapterService, active_adapter_,
      client_on_ready_->CreateReadyClosure());
#if BUILDFLAG(IS_CHROMEOS)
  client_bundle_->admin_client()->Init(GetSystemBus(), kAdapterService,
                                       active_adapter_,
                                       client_on_ready_->CreateReadyClosure());
#endif  // BUILDFLAG(IS_CHROMEOS)
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
    return;
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
    return;
  }
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  if (!use_stubs_) {
    adapter_client_ = FlossAdapterClient::Create();
    gatt_manager_client_ = FlossGattManagerClient::Create();
    socket_manager_ = FlossSocketManager::Create();
    lescan_client_ = FlossLEScanClient::Create();
    advertiser_client_ = FlossAdvertiserClient::Create();
    battery_manager_client_ = FlossBatteryManagerClient::Create();
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
    logging_client_ = std::make_unique<FakeFlossLoggingClient>();
#if BUILDFLAG(IS_CHROMEOS)
    admin_client_ = std::make_unique<FakeFlossAdminClient>();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
}

}  // namespace floss
