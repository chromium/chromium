// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluez_dbus_manager.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "device/base/features.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_battery_client.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

static BluezDBusManager* g_bluez_dbus_manager = nullptr;
static bool g_using_bluez_dbus_manager_for_testing = false;

BluezDBusManager::BluezDBusManager(dbus::Bus* bus,
                                   dbus::Bus* alternate_bus,
                                   bool use_dbus_fakes)
    : bus_(bus),
      alternate_bus_(alternate_bus),
      object_manager_support_known_(false),
      object_manager_supported_(false) {
  // On Chrome OS, Bluez might not be ready by the time we initialize the
  // BluezDBusManager so we initialize the clients anyway.
  bool should_check_object_manager = true;
#if BUILDFLAG(IS_CHROMEOS)
  should_check_object_manager = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (!use_dbus_fakes) {
    // Wait for the Floss Manager to be available
    GetSystemBus()
        ->GetObjectProxy(floss::kManagerService, dbus::ObjectPath("/"))
        ->WaitForServiceToBeAvailable(
            base::BindOnce(&BluezDBusManager::OnFlossManagerServiceAvailable,
                           weak_ptr_factory_.GetWeakPtr()));
  }

  if (!should_check_object_manager || use_dbus_fakes) {
    client_bundle_ =
        std::make_unique<BluetoothDBusClientBundle>(use_dbus_fakes);
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
          bluez_object_manager::kBluezObjectManagerServiceName,
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
    base::OnceClosure callback) {
  object_manager_support_known_callback_ = std::move(callback);
}

BluetoothAdapterClient* bluez::BluezDBusManager::GetBluetoothAdapterClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_adapter_client();
}

BluetoothAdminPolicyClient*
bluez::BluezDBusManager::GetBluetoothAdminPolicyClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_admin_policy_client();
}

BluetoothAdvertisementMonitorManagerClient*
bluez::BluezDBusManager::GetBluetoothAdvertisementMonitorManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_advertisement_monitor_manager_client();
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

BluetoothDebugManagerClient*
bluez::BluezDBusManager::GetBluetoothDebugManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_debug_manager_client();
}

BluetoothBatteryClient* bluez::BluezDBusManager::GetBluetoothBatteryClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_battery_client();
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

BluetoothProfileManagerClient*
bluez::BluezDBusManager::GetBluetoothProfileManagerClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->bluetooth_profile_manager_client();
}

BluetoothAdapterClient* BluezDBusManager::GetAlternateBluetoothAdapterClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->alternate_bluetooth_adapter_client();
}

BluetoothAdminPolicyClient*
BluezDBusManager::GetAlternateBluetoothAdminPolicyClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->alternate_bluetooth_admin_policy_client();
}

BluetoothDeviceClient* BluezDBusManager::GetAlternateBluetoothDeviceClient() {
  DCHECK(object_manager_support_known_);
  return client_bundle_->alternate_bluetooth_device_client();
}

void BluezDBusManager::OnObjectManagerSupported(dbus::Response* response) {
  DVLOG(1) << "Bluetooth supported. Initializing clients.";
  object_manager_supported_ = true;

  client_bundle_ =
      std::make_unique<BluetoothDBusClientBundle>(false /* use_fakes */);
  InitializeClients();

  object_manager_support_known_ = true;
  if (object_manager_support_known_callback_)
    std::move(object_manager_support_known_callback_).Run();
}

void BluezDBusManager::OnObjectManagerNotSupported(
    dbus::ErrorResponse* response) {
  DVLOG(1) << "Bluetooth not supported.";
  object_manager_supported_ = false;

  // We don't initialize clients since the clients need ObjectManager.

  object_manager_support_known_ = true;
  if (object_manager_support_known_callback_)
    std::move(object_manager_support_known_callback_).Run();
}

void BluezDBusManager::OnFlossManagerServiceAvailable(bool is_available) {
  if (!is_available) {
    LOG(WARNING) << "Floss manager service not available, cannot set Floss "
                    "enable/disable.";
    return;
  }

  // Make sure that Floss manager daemon is in agreement with Chrome about the
  // state of Floss enable/disable.
  dbus::MethodCall floss_method_call(dbus::kObjectManagerInterface,
                                     dbus::kObjectManagerGetManagedObjects);
  GetSystemBus()
      ->GetObjectProxy(floss::kManagerService, dbus::ObjectPath("/"))
      ->CallMethodWithErrorCallback(
          &floss_method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(&BluezDBusManager::OnFlossObjectManagerSupported,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&BluezDBusManager::OnFlossObjectManagerNotSupported,
                         weak_ptr_factory_.GetWeakPtr()));
}

void BluezDBusManager::OnFlossObjectManagerSupported(dbus::Response* response) {
  DVLOG(1) << "Floss manager present. Making sure Floss is enabled/disabled.";
  floss_manager_client_ = floss::FlossManagerClient::Create();
  floss_manager_client_->Init(GetSystemBus(), floss::kManagerInterface,
                              /*adapter_index=*/0, base::Version(),
                              base::DoNothing());
}

void BluezDBusManager::OnFlossObjectManagerNotSupported(
    dbus::ErrorResponse* response) {
  LOG(WARNING) << "Floss manager not present, cannot set Floss enable/disable.";
}

void BluezDBusManager::InitializeClients() {
  std::string bluetooth_service_name =
      bluez_object_manager::kBluezObjectManagerServiceName;
  client_bundle_->bluetooth_adapter_client()->Init(GetSystemBus(),
                                                   bluetooth_service_name);
  client_bundle_->bluetooth_admin_policy_client()->Init(GetSystemBus(),
                                                        bluetooth_service_name);
#if BUILDFLAG(IS_CHROMEOS)
  client_bundle_->bluetooth_advertisement_monitor_manager_client()->Init(
      GetSystemBus(), bluetooth_service_name);
#endif  // BUILDFLAG(IS_CHROMEOS)
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
  client_bundle_->bluetooth_profile_manager_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_debug_manager_client()->Init(
      GetSystemBus(), bluetooth_service_name);
  client_bundle_->bluetooth_battery_client()->Init(GetSystemBus(),
                                                   bluetooth_service_name);

  if (!alternate_bus_)
    return;

  client_bundle_->alternate_bluetooth_adapter_client()->Init(
      alternate_bus_, bluetooth_service_name);
  client_bundle_->alternate_bluetooth_admin_policy_client()->Init(
      alternate_bus_, bluetooth_service_name);
  client_bundle_->alternate_bluetooth_device_client()->Init(
      alternate_bus_, bluetooth_service_name);
}

// static
void BluezDBusManager::Initialize(dbus::Bus* system_bus) {
  // If we initialize BluezDBusManager twice we may also be shutting it down
  // early; do not allow that.
  if (g_using_bluez_dbus_manager_for_testing)
    return;

  CHECK(!g_bluez_dbus_manager);

  BluezDBusThreadManager::Initialize();

#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(system_bus);
  // On ChromeOS, BluetoothSystem needs a separate connection to Bluez, so we
  // use BluezDBusThreadManager to get two different connections to the same
  // services. This allows us to have two separate sets of clients in the same
  // process.
  CreateGlobalInstance(system_bus,
                       BluezDBusThreadManager::Get()->GetSystemBus(),
                       false /* use_dbus_stubs */);
#elif BUILDFLAG(IS_LINUX)
  // BluetoothSystem, the client that needs the extra connection, is not
  // implemented on Linux, so no need for an extra Bus.
  CreateGlobalInstance(BluezDBusThreadManager::Get()->GetSystemBus(), nullptr,
                       false /* use_dbus_stubs */);
#else
  NOTREACHED();
#endif
}

void BluezDBusManager::InitializeFake() {
  if (g_using_bluez_dbus_manager_for_testing)
    return;
  CHECK(!g_bluez_dbus_manager);
  BluezDBusThreadManager::Initialize();
  CreateGlobalInstance(nullptr, nullptr, true /* use_dbus_stubs */);
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

#if BUILDFLAG(IS_CHROMEOS)
  if (!g_using_bluez_dbus_manager_for_testing)
    BluezDBusThreadManager::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS)

  g_using_bluez_dbus_manager_for_testing = false;
  DVLOG(1) << "BluezDBusManager Shutdown completed";
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

void BluezDBusManagerSetter::SetBluetoothAdminPolicyClient(
    std::unique_ptr<BluetoothAdminPolicyClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_admin_policy_client_ = std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothAdvertisementMonitorManagerClient(
    std::unique_ptr<BluetoothAdvertisementMonitorManagerClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_advertisement_monitor_manager_client_ =
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

void BluezDBusManagerSetter::SetBluetoothBatteryClient(
    std::unique_ptr<BluetoothBatteryClient> client) {
  bluez::BluezDBusManager::Get()->client_bundle_->bluetooth_battery_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetBluetoothDebugManagerClient(
    std::unique_ptr<BluetoothDebugManagerClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->bluetooth_debug_manager_client_ = std::move(client);
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

void BluezDBusManagerSetter::SetAlternateBluetoothAdminPolicyClient(
    std::unique_ptr<BluetoothAdminPolicyClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->alternate_bluetooth_admin_policy_client_ =
      std::move(client);
}

void BluezDBusManagerSetter::SetAlternateBluetoothDeviceClient(
    std::unique_ptr<BluetoothDeviceClient> client) {
  bluez::BluezDBusManager::Get()
      ->client_bundle_->alternate_bluetooth_device_client_ = std::move(client);
}

}  // namespace bluez
