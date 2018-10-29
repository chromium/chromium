// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluez_dbus_manager.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/base/features.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_media_client.h"
#include "device/bluetooth/dbus/bluetooth_media_transport_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

namespace bluez {

static BluezDBusManager* g_bluez_dbus_manager = nullptr;
static bool g_using_bluez_dbus_manager_for_testing = false;

BluezDBusManager::BluezDBusManager(dbus::Bus* bus,
                                   dbus::Bus* alternate_bus,
                                   bool use_dbus_fakes)
    : bus_(bus),
      alternate_bus_(alternate_bus),
      object_manager_support_known_(false),
      object_manager_supported_(false),
      weak_ptr_factory_(this) {
  // On Chrome OS, Bluez might not be ready by the time we initialize the
  // BluezDBusManager so we initialize the clients anyway.
  bool should_check_object_manager = true;
#if defined(OS_CHROMEOS)
  should_check_object_manager = false;
#endif

  if (!should_check_object_manager || use_dbus_fakes) {
    client_bundle_.reset(new BluetoothDBusClientBundle(use_dbus_fakes));
    InitializeClients();
    object_manager_supported_ = true;
    object_manager_support_known_ = true;
    return;
  }

  CHECK(GetSystemBus()) << "Can't initialize real clients without DBus.";
  dbus::MethodCall method_call(dbus::kObjectManagerInterface,
                               dbus::kObjectManagerGetManagedObjects);
  GetSystemBus()
      ->GetObjectProxy(
          GetBluetoothServiceName(),
          dbus::ObjectPath(
              bluetooth_object_manager::kBluetoothObjectManagerServicePath))
      ->CallMethodWithErrorCallback(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(&BluezDBusManager::OnObjectManagerSupported,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&BluezDBusManager::OnObjectManagerNotSupported,
                         weak_ptr_factory_.GetWeakPtr()));
}

BluezDBusManager::~BluezDBusManager() {
  // Delete all D-Bus clients before shutting down the system bus.
  client_bundle_.reset();
}

dbus::Bus* bluez::BluezDBusManager::GetSystemBus() {
  return bus_;
}

void BluezDBusManager::CallWhenObjectManagerSupportIsKnown(
    base::Closure callback) {
  object_manager_support_known_callback_ = callback;
}

BluetoothAdapterClient* bluez::BluezDBusManager::GetBluetoothAdapterClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_adapter_client();
}

BluetoothLEAdvertisingManagerClient*
bluez::BluezDBusManager::GetBluetoothLEAdvertisingManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_le_advertising_manager_client();
}

BluetoothAgentManagerClient*
bluez::BluezDBusManager::GetBluetoothAgentManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_agent_manager_client();
}

BluetoothDeviceClient* bluez::BluezDBusManager::GetBluetoothDeviceClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_device_client();
}

BluetoothGattCharacteristicClient*
bluez::BluezDBusManager::GetBluetoothGattCharacteristicClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_gatt_characteristic_client();
}

BluetoothGattDescriptorClient*
bluez::BluezDBusManager::GetBluetoothGattDescriptorClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_gatt_descriptor_client();
}

BluetoothGattManagerClient*
bluez::BluezDBusManager::GetBluetoothGattManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_gatt_manager_client();
}

BluetoothGattServiceClient*
bluez::BluezDBusManager::GetBluetoothGattServiceClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_gatt_service_client();
}

BluetoothInputClient* bluez::BluezDBusManager::GetBluetoothInputClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_input_client();
}

BluetoothMediaClient* bluez::BluezDBusManager::GetBluetoothMediaClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_media_client();
}

BluetoothMediaTransportClient*
bluez::BluezDBusManager::GetBluetoothMediaTransportClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_media_transport_client();
}

BluetoothProfileManagerClient*
bluez::BluezDBusManager::GetBluetoothProfileManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_profile_manager_client();
}

BluetoothAdapterClient* BluezDBusManager::GetAlternateBluetoothAdapterClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->alternate_bluetooth_adapter_client();
}

void BluezDBusManager::OnObjectManagerSupported(dbus::Response* response) {
  VLOG(1) << "Bluetooth supported. Initializing clients.";
  object_manager_supported_ = true;

  client_bundle_.reset(new BluetoothDBusClientBundle(false /* use_fakes */));
  InitializeClients();

  object_manager_support_known_ = true;
  if (!object_manager_support_known_callback_.is_null()) {
    object_manager_support_known_callback_.Run();
    object_manager_support_known_callback_.Reset();
  }
}

void BluezDBusManager::OnObjectManagerNotSupported(
    dbus::ErrorResponse* response) {
  VLOG(1) << "Bluetooth not supported.";
  object_manager_supported_ = false;

  // We don't initialize clients since the clients need ObjectManager.

  object_manager_support_known_ = true;
  if (!object_manager_support_known_callback_.is_null()) {
    object_manager_support_known_callback_.Run();
    object_manager_support_known_callback_.Reset();
  }
}

void BluezDBusManager::InitializeClients() {
  std::string bluetooth_service_name = GetBluetoothServiceName();
  client_bundle_->bluetooth_adapter_client()->Init(GetSystemBus(),
                                                   bluetooth_service_name);
  client_bundle_->bluetooth_agent_manager_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_device_client()->Init(GetSystemBus(),
                                                  bluetooth_service_name);
  client_bundle_->bluetooth_gatt_characteristic_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_gatt_descriptor_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_gatt_manager_client()->Init(GetSystemBus(),
                                                        bluetooth_service_name);
  client_bundle_->bluetooth_gatt_service_client()->Init(GetSystemBus(),
                                                        bluetooth_service_name);
  client_bundle_->bluetooth_input_client()->Init(GetSystemBus(),
                                                 bluetooth_service_name);
  client_bundle_->bluetooth_le_advertising_manager_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_media_client()->Init(GetSystemBus(),
                                                 bluetooth_service_name);
  client_bundle_->bluetooth_media_transport_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_profile_manager_client()->Init(
      GetSystemBus(), bluetooth_service_name);

  if (!alternate_bus_)
    return;

  client_bundle_->alternate_bluetooth_adapter_client()->Init(
      alternate_bus_, bluetooth_service_name);
}

std::string BluezDBusManager::GetBluetoothServiceName() {
  bool use_newblue = false;
#if defined(OS_CHROMEOS)
  use_newblue = base::FeatureList::IsEnabled(device::kNewblueDaemon);
#endif  // defined(OS_CHROMEOS)

  return use_newblue
             ? bluetooth_object_manager::kBluetoothObjectManagerServiceName
             : bluez_object_manager::kBluezObjectManagerServiceName;
}

// static
void BluezDBusManager::Initialize() {
  // If we initialize BluezDBusManager twice we may also be shutting it down
  // early; do not allow that.
  if (g_using_bluez_dbus_manager_for_testing)
    return;

  CHECK(!g_bluez_dbus_manager);

#if defined(OS_CHROMEOS)
  // On ChromeOS, BluetoothSystem needs a separate connection to Bluez, so we
  // use BluezDBusThreadManager to get two different connections to the same
  // services. This allows us to have two separate sets of clients in the same
  // process.
  BluezDBusThreadManager::Initialize();

  CreateGlobalInstance(chromeos::DBusThreadManager::Get()->GetSystemBus(),
                       BluezDBusThreadManager::Get()->GetSystemBus(),
                       chromeos::DBusThreadManager::Get()->IsUsingFakes());
#elif defined(OS_LINUX)
  // BluetoothSystem, the client that needs the extra connection, is not
  // implemented on Linux, so no need for an extra Bus.
  CreateGlobalInstance(BluezDBusThreadManager::Get()->GetSystemBus(), nullptr,
                       false /* use_dbus_stubs */);
#endif
}

// static
std::unique_ptr<BluezDBusManagerSetter>
bluez::BluezDBusManager::GetSetterForTesting() {
  if (!g_using_bluez_dbus_manager_for_testing) {
    g_using_bluez_dbus_manager_for_testing = true;
    CreateGlobalInstance(nullptr, nullptr, true);
  }

  return base::WrapUnique(new BluezDBusManagerSetter());
}

// static
void BluezDBusManager::CreateGlobalInstance(dbus::Bus* bus,
                                            dbus::Bus* alternate_bus,
                                            bool use_stubs) {
  CHECK(!g_bluez_dbus_manager);
  g_bluez_dbus_manager = new BluezDBusManager(bus, alternate_bus, use_stubs);
}

// static
bool BluezDBusManager::IsInitialized() {
  return g_bluez_dbus_manager != nullptr;
}

// static
void BluezDBusManager::Shutdown() {
  // Ensure that we only shutdown BluezDBusManager once.
  CHECK(g_bluez_dbus_manager);
  BluezDBusManager* dbus_manager = g_bluez_dbus_manager;
  g_bluez_dbus_manager = nullptr;
  delete dbus_manager;

#if defined(OS_CHROMEOS)
  if (!g_using_bluez_dbus_manager_for_testing)
    BluezDBusThreadManager::Shutdown();
#endif

  g_using_bluez_dbus_manager_for_testing = false;
  VLOG(1) << "BluezDBusManager Shutdown completed";
}

// static
BluezDBusManager* bluez::BluezDBusManager::Get() {
  CHECK(g_bluez_dbus_manager)
      << "bluez::BluezDBusManager::Get() called before Initialize()";
  return g_bluez_dbus_manager;
}

BluezDBusManagerSetter::BluezDBusManagerSetter() = default;

BluezDBusManagerSetter::~BluezDBusManagerSetter() = default;

void BluezDBusManagerSetter::SetBluetoothAdapterClient(
    std::unique_ptr<BluetoothAdapterClient> client) {
  bluez::BluezDBusManager::Get()->client_bundle_->bluetooth_adapter_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothLEAdvertisingManagerClient(
    std::unique_ptr<BluetoothLEAdvertisingManagerClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_le_advertising_manager_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothAgentManagerClient(
    std::unique_ptr<BluetoothAgentManagerClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_agent_manager_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothDeviceClient(
    std::unique_ptr<BluetoothDeviceClient> client) {
  bluez::BluezDBusManager::Get()->client_bundle_->bluetooth_device_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothGattCharacteristicClient(
    std::unique_ptr<BluetoothGattCharacteristicClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_gatt_characteristic_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothGattDescriptorClient(
    std::unique_ptr<BluetoothGattDescriptorClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_gatt_descriptor_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothGattManagerClient(
    std::unique_ptr<BluetoothGattManagerClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_gatt_manager_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothGattServiceClient(
    std::unique_ptr<BluetoothGattServiceClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_gatt_service_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothInputClient(
    std::unique_ptr<BluetoothInputClient> client) {
  bluez::BluezDBusManager::Get()->client_bundle_->bluetooth_input_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothMediaClient(
    std::unique_ptr<BluetoothMediaClient> client) {
  bluez::BluezDBusManager::Get()->client_bundle_->bluetooth_media_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothMediaTransportClient(
    std::unique_ptr<BluetoothMediaTransportClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_media_transport_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothProfileManagerClient(
    std::unique_ptr<BluetoothProfileManagerClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_profile_manager_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetAlternateBluetoothAdapterClient(
    std::unique_ptr<BluetoothAdapterClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->alternate_bluetooth_adapter_client_ = std::move(client);
}

}  // namespace bluez
