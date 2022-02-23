// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"

namespace floss {

namespace {
FlossDBusManager* g_floss_dbus_manager = nullptr;
FlossDBusThreadManager* g_floss_dbus_thread_manager = nullptr;
}  // namespace

const int FlossDBusManager::kInvalidAdapter = -1;

static bool g_using_floss_dbus_manager_for_testing = false;

FlossDBusManager::FlossDBusManager(dbus::Bus* bus, bool use_stubs) : bus_(bus) {
  if (use_stubs) {
    client_bundle_ = std::make_unique<FlossClientBundle>(use_stubs);
    active_adapter_ = 0;
    object_manager_supported_ = true;
    object_manager_support_known_ = true;
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
  FlossDBusThreadManager::Initialize();

  // |system_bus| is unused and we re-use the system bus connection created by
  // FlossDBusThreadManager.
  CreateGlobalInstance(FlossDBusThreadManager::Get()->GetSystemBus(),
                       /*use_stubs=*/false);
}

void FlossDBusManager::InitializeFake() {
  NOTIMPLEMENTED();
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
                                         std::string());

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
  if (object_manager_support_known_callback_)
    std::move(object_manager_support_known_callback_).Run();
}

void FlossDBusManager::SwitchAdapter(int adapter) {
  if (!object_manager_supported_) {
    DVLOG(1) << "Floss can't switch to adapter without object manager";
    return;
  }

  InitializeAdapterClients(adapter);
  return;
}

bool FlossDBusManager::HasActiveAdapter() const {
  return active_adapter_ != kInvalidAdapter;
}

FlossManagerClient* FlossDBusManager::GetManagerClient() {
  return client_bundle_->manager_client();
}

FlossAdapterClient* FlossDBusManager::GetAdapterClient() {
  return client_bundle_->adapter_client();
}

void FlossDBusManager::InitializeAdapterClients(int adapter) {
  // Clean up active adapter clients
  if (active_adapter_ != kInvalidAdapter) {
    client_bundle_->ResetAdapterClients();
  }

  // Set current adapter. If it's kInvalidAdapter, this doesn't need to do any
  // init.
  active_adapter_ = adapter;
  if (adapter == kInvalidAdapter) {
    return;
  }

  dbus::ObjectPath adapter_path =
      FlossManagerClient::GenerateAdapterPath(adapter);

  // Initialize any adapter clients.
  client_bundle_->adapter_client()->Init(GetSystemBus(), kAdapterService,
                                         adapter_path.value());
}

void FlossDBusManagerSetter::SetFlossManagerClient(
    std::unique_ptr<FlossManagerClient> client) {
  FlossDBusManager::Get()->client_bundle_->manager_client_ = std::move(client);
}

void FlossDBusManagerSetter::SetFlossAdapterClient(
    std::unique_ptr<FlossAdapterClient> client) {
  FlossDBusManager::Get()->client_bundle_->adapter_client_ = std::move(client);
}

FlossDBusThreadManager::FlossDBusThreadManager() {
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  dbus_thread_ = std::make_unique<base::Thread>("Floss D-Bus thread");
  dbus_thread_->StartWithOptions(std::move(thread_options));

  // Create the connection to the system bus.
  dbus::Bus::Options system_bus_options;
  system_bus_options.bus_type = dbus::Bus::SYSTEM;
  system_bus_options.connection_type = dbus::Bus::PRIVATE;
  system_bus_options.dbus_task_runner = dbus_thread_->task_runner();
  system_bus_ = base::MakeRefCounted<dbus::Bus>(system_bus_options);
}

FlossDBusThreadManager::~FlossDBusThreadManager() {
  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  dbus_thread_->Stop();

  if (!g_floss_dbus_thread_manager)
    return;  // Called from Shutdown() or local test instance.

  // There should never be both a global instance and a local instance.
  CHECK(this == g_floss_dbus_thread_manager);
}

dbus::Bus* FlossDBusThreadManager::GetSystemBus() {
  return system_bus_.get();
}

// static
void FlossDBusThreadManager::Initialize() {
  CHECK(!g_floss_dbus_thread_manager);
  g_floss_dbus_thread_manager = new FlossDBusThreadManager();
}

// static
void FlossDBusThreadManager::Shutdown() {
  // Ensure that we only shutdown FlossDBusThreadManager once.
  CHECK(g_floss_dbus_thread_manager);
  delete g_floss_dbus_thread_manager;
  g_floss_dbus_thread_manager = nullptr;
  DVLOG(1) << "FlossDBusThreadManager Shutdown completed";
}

// static
FlossDBusThreadManager* FlossDBusThreadManager::Get() {
  CHECK(g_floss_dbus_thread_manager)
      << "FlossDBusThreadManager::Get() called before Initialize()";
  return g_floss_dbus_thread_manager;
}

FlossClientBundle::FlossClientBundle(bool use_stubs) : use_stubs_(use_stubs) {
  if (use_stubs) {
    return;
  }

  manager_client_ = FlossManagerClient::Create();

  ResetAdapterClients();
}

FlossClientBundle::~FlossClientBundle() = default;

void FlossClientBundle::ResetAdapterClients() {
  if (use_stubs_) {
    return;
  }

  adapter_client_ = FlossAdapterClient::Create();
}

}  // namespace floss
