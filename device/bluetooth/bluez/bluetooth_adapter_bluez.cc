// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluez/bluetooth_adapter_profile_bluez.h"
#include "device/bluetooth/bluez/bluetooth_advertisement_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"
#include "device/bluetooth/bluez/bluetooth_socket_bluez.h"
#include "device/bluetooth/bluez/bluez_features.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/chromeos_platform_features.h"
#endif // BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_battery_client.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/unguessable_token.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluez/bluetooth_low_energy_scan_session_bluez.h"
#include "device/bluetooth/chromeos/bluetooth_connection_logger.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/devicetype.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using device::BluetoothAdapter;
using device::BluetoothDevice;
using BatteryInfo = device::BluetoothDevice::BatteryInfo;
using BatteryType = device::BluetoothDevice::BatteryType;
using UUIDSet = device::BluetoothDevice::UUIDSet;
using device::BluetoothDiscoveryFilter;
using device::BluetoothSocket;
using device::BluetoothUUID;
using device::UMABluetoothDiscoverySessionOutcome;

namespace {

// The agent path is relatively meaningless since BlueZ only permits one to
// exist per D-Bus connection, it just has to be unique within Chromium.
const char kAgentPath[] = "/org/chromium/bluetooth_agent";
const char kGattApplicationObjectPath[] = "/gatt_application";

const char kDeviceNameArcTouch[] = "Arc Touch BT Mouse";

#if BUILDFLAG(IS_CHROMEOS)
// This root path identifies the application registering low energy scanners
// through D-Bus.
constexpr char kAdvertisementMonitorApplicationObjectPath[] =
    "/org/chromium/bluetooth_advertisement_monitor";
#endif  // BUILDFLAG(IS_CHROMEOS)

void OnUnregisterAgentError(const std::string& error_name,
                            const std::string& error_message) {
  // It's okay if the agent didn't exist, it means we never saw an adapter.
  if (error_name == bluetooth_agent_manager::kErrorDoesNotExist)
    return;

  BLUETOOTH_LOG(ERROR) << "Failed to unregister pairing agent: " << error_name
                       << ": " << error_message;
}

UMABluetoothDiscoverySessionOutcome TranslateDiscoveryErrorToUMA(
    const std::string& error_name) {
  if (error_name == bluez::BluetoothAdapterClient::kUnknownAdapterError) {
    return UMABluetoothDiscoverySessionOutcome::BLUEZ_DBUS_UNKNOWN_ADAPTER;
  } else if (error_name == bluez::BluetoothAdapterClient::kNoResponseError) {
    return UMABluetoothDiscoverySessionOutcome::BLUEZ_DBUS_NO_RESPONSE;
  } else if (error_name == bluetooth_device::kErrorInProgress) {
    return UMABluetoothDiscoverySessionOutcome::BLUEZ_DBUS_IN_PROGRESS;
  } else if (error_name == bluetooth_device::kErrorNotReady) {
    return UMABluetoothDiscoverySessionOutcome::BLUEZ_DBUS_NOT_READY;
  } else if (error_name == bluetooth_device::kErrorNotSupported) {
    return UMABluetoothDiscoverySessionOutcome::BLUEZ_DBUS_UNSUPPORTED_DEVICE;
  } else if (error_name == bluetooth_device::kErrorFailed) {
    return UMABluetoothDiscoverySessionOutcome::FAILED;
  } else {
    BLUETOOTH_LOG(ERROR) << "Unrecognized DBus error " << error_name;
    return UMABluetoothDiscoverySessionOutcome::UNKNOWN;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
device::BluetoothDevice::ServiceDataMap ConvertServiceDataMap(
    const base::flat_map<std::string, std::vector<uint8_t>>& input) {
  device::BluetoothDevice::ServiceDataMap output;
  for (auto& i : input) {
    output[BluetoothUUID(i.first)] = i.second;
  }

  return output;
}

device::BluetoothDevice::ManufacturerDataMap ConvertManufacturerDataMap(
    const base::flat_map<uint16_t, std::vector<uint8_t>>& input) {
  return device::BluetoothDevice::ManufacturerDataMap(input.begin(),
                                                      input.end());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsBatteryDisplayBlocklisted(const BluetoothDevice* device) {
  if (!device->GetName())
    return false;

  // b/191919291: Arc Touch BT Mouse battery values change back and forth.
  if (device->GetName()->find(kDeviceNameArcTouch) != std::string::npos)
    return true;

  return false;
}

}  // namespace

namespace bluez {

namespace {

void OnRegistrationErrorCallback(
    device::BluetoothGattService::ErrorCallback error_callback,
    bool is_register_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (is_register_callback) {
    BLUETOOTH_LOG(ERROR) << "Failed to Register service: " << error_name << ", "
                         << error_message;
  } else {
    BLUETOOTH_LOG(ERROR) << "Failed to Unregister service: " << error_name
                         << ", " << error_message;
  }
  std::move(error_callback)
      .Run(BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

void SetIntervalErrorCallbackConnector(
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << "Error while registering advertisement. error_name = "
                       << error_name << ", error_message = " << error_message;

  device::BluetoothAdvertisement::ErrorCode code = device::
      BluetoothAdvertisement::ErrorCode::INVALID_ADVERTISEMENT_ERROR_CODE;
  if (error_name == bluetooth_advertising_manager::kErrorInvalidArguments) {
    code = device::BluetoothAdvertisement::ErrorCode::
        ERROR_INVALID_ADVERTISEMENT_INTERVAL;
  }
  std::move(error_callback).Run(code);
}

void ResetAdvertisingErrorCallbackConnector(
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << "Error while resetting advertising. error_name = "
                       << error_name << ", error_message = " << error_message;

  std::move(error_callback)
      .Run(device::BluetoothAdvertisement::ErrorCode::ERROR_RESET_ADVERTISING);
}

#if BUILDFLAG(IS_CHROMEOS)
void SetServiceAllowListErrorCallback(
    BluetoothAdapterBlueZ::ErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << "Error while settting service allow list."
                          " error_name = "
                       << error_name << ", error_message = " << error_message;
  std::move(error_callback).Run();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

// static
scoped_refptr<BluetoothAdapterBlueZ> BluetoothAdapterBlueZ::CreateAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterBlueZ());
}

void BluetoothAdapterBlueZ::Initialize(base::OnceClosure callback) {
  init_callback_ = std::move(callback);

  // Can't initialize the adapter until DBus clients are ready.
  if (bluez::BluezDBusManager::Get()->IsObjectManagerSupportKnown()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothAdapterBlueZ::Init,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  bluez::BluezDBusManager::Get()->CallWhenObjectManagerSupportIsKnown(
      base::BindOnce(&BluetoothAdapterBlueZ::Init,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterBlueZ::Shutdown() {
  if (dbus_is_shutdown_)
    return;

  BLUETOOTH_LOG(EVENT) << "BluetoothAdapterBlueZ::Shutdown";

  DCHECK(bluez::BluezDBusManager::IsInitialized())
      << "Call BluetoothAdapterFactory::Shutdown() before "
         "BluezDBusManager::Shutdown().";

  // Since we don't initialize anything if Object Manager is not supported,
  // no need to do any clean up.
  if (!bluez::BluezDBusManager::Get()->IsObjectManagerSupported()) {
    dbus_is_shutdown_ = true;
    return;
  }

  if (IsPresent())
    RemoveAdapter();  // Also deletes devices_.
  DCHECK(devices_.empty());
#if BUILDFLAG(IS_CHROMEOS)
  is_advertisement_monitor_application_provider_registered_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // profiles_ must be empty because all BluetoothSockets have been notified
  // that this adapter is disappearing.
  DCHECK(profiles_.empty());

  // Some profiles may have been released but not yet removed; it is safe to
  // delete them.
  for (auto& it : released_profiles_)
    delete it.second;
  released_profiles_.clear();

  for (auto& it : profile_queues_)
    delete it.second;
  profile_queues_.clear();

  // This may call unregister on advertisements that have already been
  // unregistered but that's fine. The advertisement object keeps a track of
  // the fact that it has been already unregistered and will call our empty
  // error callback with an "Already unregistered" error, which we'll ignore.
  for (auto& it : advertisements_) {
    it->Unregister(base::DoNothing(), base::DoNothing());
  }
  advertisements_.clear();

  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->RemoveObserver(
      this);
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdminPolicyClient()
      ->RemoveObserver(this);
  bluez::BluezDBusManager::Get()->GetBluetoothBatteryClient()->RemoveObserver(
      this);
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(
      this);
  bluez::BluezDBusManager::Get()->GetBluetoothInputClient()->RemoveObserver(
      this);
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAgentManagerClient()
      ->RemoveObserver(this);
#if BUILDFLAG(IS_CHROMEOS)
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdvertisementMonitorManagerClient()
      ->RemoveObserver(this);
#endif  // BUILDFLAG(IS_CHROMEOS)

  BLUETOOTH_LOG(EVENT) << "Unregistering pairing agent";
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAgentManagerClient()
      ->UnregisterAgent(dbus::ObjectPath(kAgentPath), base::DoNothing(),
                        base::BindOnce(&OnUnregisterAgentError));

  agent_.reset();

  dbus_is_shutdown_ = true;
}

BluetoothAdapterBlueZ::BluetoothAdapterBlueZ()
    : initialized_(false), dbus_is_shutdown_(false) {
  ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  socket_thread_ = device::BluetoothSocketThread::Get();
}

void BluetoothAdapterBlueZ::Init() {
  // We may have been shutdown already, in which case do nothing. If the
  // platform doesn't support Object Manager then Bluez 5 is probably not
  // present. In this case we just return without initializing anything.
  if (dbus_is_shutdown_ ||
      !bluez::BluezDBusManager::Get()->IsObjectManagerSupported()) {
    initialized_ = true;
    std::move(init_callback_).Run();
    return;
  }

  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->AddObserver(
      this);
  bluez::BluezDBusManager::Get()->GetBluetoothAdminPolicyClient()->AddObserver(
      this);
  bluez::BluezDBusManager::Get()->GetBluetoothBatteryClient()->AddObserver(
      this);
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->AddObserver(this);
  bluez::BluezDBusManager::Get()->GetBluetoothInputClient()->AddObserver(this);
  bluez::BluezDBusManager::Get()->GetBluetoothAgentManagerClient()->AddObserver(
      this);
#if BUILDFLAG(IS_CHROMEOS)
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdvertisementMonitorManagerClient()
      ->AddObserver(this);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Register the pairing agent.
  dbus::Bus* system_bus = bluez::BluezDBusManager::Get()->GetSystemBus();
  agent_.reset(bluez::BluetoothAgentServiceProvider::Create(
      system_bus, dbus::ObjectPath(kAgentPath), this));
  DCHECK(agent_.get());

#if BUILDFLAG(IS_CHROMEOS)
  advertisement_monitor_application_provider_ =
      BluetoothAdvertisementMonitorApplicationServiceProvider::Create(
          system_bus,
          dbus::ObjectPath(kAdvertisementMonitorApplicationObjectPath));
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::vector<dbus::ObjectPath> object_paths = bluez::BluezDBusManager::Get()
                                                   ->GetBluetoothAdapterClient()
                                                   ->GetAdapters();

  BLUETOOTH_LOG(EVENT) << "BlueZ Adapter Initialized.";
  if (!object_paths.empty()) {
    BLUETOOTH_LOG(EVENT) << "BlueZ Adapters available: " << object_paths.size();
    SetAdapter(object_paths[0]);
#if BUILDFLAG(IS_CHROMEOS)
    RegisterAdvertisementMonitorApplicationServiceProvider();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  initialized_ = true;

#if BUILDFLAG(IS_CHROMEOS)
  bluez::BluezDBusManager::Get()
      ->GetBluetoothDebugManagerClient()
      ->SetDevCoredump(
          base::FeatureList::IsEnabled(
              chromeos::bluetooth::features::kBluetoothCoredump),
          base::BindOnce(&BluetoothAdapterBlueZ::OnSetDevCoredumpSuccess,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&BluetoothAdapterBlueZ::OnSetDevCoredumpError,
                         weak_ptr_factory_.GetWeakPtr()));
#endif // BUILDFLAG(IS_CHROMEOS)
  std::move(init_callback_).Run();
}

BluetoothAdapterBlueZ::~BluetoothAdapterBlueZ() {
  Shutdown();
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterBlueZ::OnSetDevCoredumpSuccess() {
  bool flag = base::FeatureList::IsEnabled(
      chromeos::bluetooth::features::kBluetoothCoredump);
  BLUETOOTH_LOG(DEBUG) << "Bluetooth devcoredump state set to " << flag;
}

void BluetoothAdapterBlueZ::OnSetDevCoredumpError(
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << "Failed to update bluetooth devcoredump state: "
                       << error_name << ": " << error_message;
}
#endif // BUILDFLAG(IS_CHROMEOS)

std::string BluetoothAdapterBlueZ::GetAddress() const {
  if (!IsPresent())
    return std::string();

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);
  DCHECK(properties);

  return device::CanonicalizeBluetoothAddress(properties->address.value());
}

std::string BluetoothAdapterBlueZ::GetName() const {
  if (!IsPresent())
    return std::string();

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

std::string BluetoothAdapterBlueZ::GetSystemName() const {
  if (!IsPresent())
    return std::string();

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);
  DCHECK(properties);

  return properties ? properties->name.value() : std::string();
}

void BluetoothAdapterBlueZ::SetName(const std::string& name,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  if (!IsPresent()) {
    std::move(error_callback).Run();
    return;
  }

  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->GetProperties(object_path_)
      ->alias.Set(name, base::BindOnce(
                            &BluetoothAdapterBlueZ::OnPropertyChangeCompleted,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            std::move(error_callback)));
}

bool BluetoothAdapterBlueZ::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterBlueZ::IsPresent() const {
  return !dbus_is_shutdown_ && !object_path_.value().empty();
}

bool BluetoothAdapterBlueZ::IsPowered() const {
  if (!IsPresent())
    return false;

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);

  return properties->powered.value();
}

void BluetoothAdapterBlueZ::SetPowered(bool powered,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  if (!IsPresent()) {
    BLUETOOTH_LOG(ERROR) << "SetPowered: " << powered << ". Not Present!";
    std::move(error_callback).Run();
    return;
  }

  BLUETOOTH_LOG(EVENT) << "SetPowered: " << powered;

  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->GetProperties(object_path_)
      ->powered.Set(
          powered,
          base::BindOnce(&BluetoothAdapterBlueZ::OnPropertyChangeCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(error_callback)));
}

bool BluetoothAdapterBlueZ::IsDiscoverable() const {
  if (!IsPresent())
    return false;

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);

  return properties->discoverable.value();
}

void BluetoothAdapterBlueZ::SetDiscoverable(bool discoverable,
                                            base::OnceClosure callback,
                                            ErrorCallback error_callback) {
  if (!IsPresent()) {
    std::move(error_callback).Run();
    return;
  }

  BLUETOOTH_LOG(EVENT) << "SetDiscoverable: " << discoverable;

  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->GetProperties(object_path_)
      ->discoverable.Set(
          discoverable,
          base::BindOnce(&BluetoothAdapterBlueZ::OnSetDiscoverable,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(error_callback)));
}

base::TimeDelta BluetoothAdapterBlueZ::GetDiscoverableTimeout() const {
  if (!IsPresent())
    return base::Seconds(0);

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);

  return base::Seconds(properties->discoverable_timeout.value());
}

bool BluetoothAdapterBlueZ::IsDiscovering() const {
  if (!IsPresent())
    return false;

  return NumScanningDiscoverySessions() > 0;
}

bool BluetoothAdapterBlueZ::IsDiscoveringForTesting() const {
  if (!IsPresent())
    return false;

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);
  return properties->discovering.value();
}

std::unordered_map<BluetoothDevice*, UUIDSet>
BluetoothAdapterBlueZ::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const BluetoothDiscoveryFilter& discovery_filter) {
  std::unordered_map<BluetoothDevice*, UUIDSet> connected_devices;

  std::set<BluetoothUUID> filter_uuids;
  discovery_filter.GetUUIDs(filter_uuids);

  for (BluetoothDevice* device : GetDevices()) {
    if (device->IsGattConnected() &&
        (device->GetType() & device::BLUETOOTH_TRANSPORT_LE)) {
      UUIDSet device_uuids = device->GetUUIDs();

      UUIDSet intersection;
      for (const BluetoothUUID& uuid : filter_uuids) {
        if (base::Contains(device_uuids, uuid)) {
          intersection.insert(uuid);
        }
      }

      if (filter_uuids.empty() || !intersection.empty()) {
        connected_devices[device] = std::move(intersection);
      }
    }
  }

  return connected_devices;
}

BluetoothAdapterBlueZ::UUIDList BluetoothAdapterBlueZ::GetUUIDs() const {
  if (!IsPresent())
    return UUIDList();

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);
  DCHECK(properties);

  std::vector<std::string> uuids = properties->uuids.value();

  return UUIDList(uuids.begin(), uuids.end());
}

void BluetoothAdapterBlueZ::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  DCHECK(!dbus_is_shutdown_);
  BLUETOOTH_LOG(DEBUG) << object_path_.value() << ": Creating RFCOMM service: "
                       << uuid.canonical_value();
  scoped_refptr<BluetoothSocketBlueZ> socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);
  socket->Listen(this, BluetoothSocketBlueZ::kRfcomm, uuid, options,
                 base::BindOnce(std::move(callback), socket),
                 std::move(error_callback));
}

void BluetoothAdapterBlueZ::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  DCHECK(!dbus_is_shutdown_);
  BLUETOOTH_LOG(DEBUG) << object_path_.value() << ": Creating L2CAP service: "
                       << uuid.canonical_value();
  scoped_refptr<BluetoothSocketBlueZ> socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);
  socket->Listen(this, BluetoothSocketBlueZ::kL2cap, uuid, options,
                 base::BindOnce(std::move(callback), socket),
                 std::move(error_callback));
}

void BluetoothAdapterBlueZ::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  scoped_refptr<BluetoothAdvertisementBlueZ> advertisement(
      new BluetoothAdvertisementBlueZ(std::move(advertisement_data), this));
  advertisement->Register(base::BindOnce(std::move(callback), advertisement),
                          std::move(error_callback));
  advertisements_.emplace_back(advertisement);
}

#if BUILDFLAG(IS_CHROMEOS)
bool BluetoothAdapterBlueZ::IsExtendedAdvertisementsAvailable() const {
  if (!IsPresent()) {
    return false;
  }

  BluetoothLEAdvertisingManagerClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothLEAdvertisingManagerClient()
          ->GetProperties(object_path_);

  if (!properties) {
    return false;
  }

  // Based on the implementation of kernel bluez, if the controller supports Ext
  // Advertisement, it must support HardwareOffload.
  // (net/bluetooth/mgmt.c:get_supported_adv_flags)
  return base::Contains(
      properties->supported_features.value(),
      bluetooth_advertising_manager::kSupportedFeaturesHardwareOffload);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothAdapterBlueZ::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());
  uint16_t min_ms = static_cast<uint16_t>(
      std::min(static_cast<int64_t>(std::numeric_limits<uint16_t>::max()),
               min.InMilliseconds()));
  uint16_t max_ms = static_cast<uint16_t>(
      std::min(static_cast<int64_t>(std::numeric_limits<uint16_t>::max()),
               max.InMilliseconds()));
  bluez::BluezDBusManager::Get()
      ->GetBluetoothLEAdvertisingManagerClient()
      ->SetAdvertisingInterval(
          object_path_, min_ms, max_ms, std::move(callback),
          base::BindOnce(&SetIntervalErrorCallbackConnector,
                         std::move(error_callback)));
}

void BluetoothAdapterBlueZ::ResetAdvertising(
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluezDBusManager::Get()
      ->GetBluetoothLEAdvertisingManagerClient()
      ->ResetAdvertising(object_path_, std::move(callback),
                         base::BindOnce(&ResetAdvertisingErrorCallbackConnector,
                                        std::move(error_callback)));
}

void BluetoothAdapterBlueZ::ConnectDevice(
    const std::string& address,
    const std::optional<device::BluetoothDevice::AddressType>& address_type,
    ConnectDeviceCallback callback,
    ConnectDeviceErrorCallback error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());

  std::optional<BluetoothAdapterClient::AddressType> client_address_type;
  if (address_type) {
    switch (*address_type) {
      case device::BluetoothDevice::AddressType::ADDR_TYPE_PUBLIC:
        client_address_type = BluetoothAdapterClient::AddressType::kPublic;
        break;
      case device::BluetoothDevice::AddressType::ADDR_TYPE_RANDOM:
        client_address_type = BluetoothAdapterClient::AddressType::kRandom;
        break;
      case device::BluetoothDevice::AddressType::ADDR_TYPE_UNKNOWN:
      default:
        // Keep |client_address_type| unset.
        break;
    };
  }

  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->ConnectDevice(
      object_path_, address, client_address_type,
      base::BindOnce(&BluetoothAdapterBlueZ::OnConnectDevice,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::BindOnce(&BluetoothAdapterBlueZ::OnConnectDeviceError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(error_callback)));
}

device::BluetoothLocalGattService* BluetoothAdapterBlueZ::GetGattService(
    const std::string& identifier) const {
  const auto& service = owned_gatt_services_.find(dbus::ObjectPath(identifier));
  return service == owned_gatt_services_.end() ? nullptr
                                               : service->second.get();
}

base::WeakPtr<device::BluetoothLocalGattService>
BluetoothAdapterBlueZ::CreateLocalGattService(
    const device::BluetoothUUID& uuid,
    bool is_primary,
    device::BluetoothLocalGattService::Delegate* delegate) {
  return bluez::BluetoothLocalGattServiceBlueZ::Create(this, uuid, is_primary,
                                                       delegate);
}

void BluetoothAdapterBlueZ::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  // Check if any device is using the pairing delegate.
  // If so, clear the pairing context which will make any responses no-ops.
  for (auto iter = devices_.begin(); iter != devices_.end(); ++iter) {
    BluetoothDeviceBlueZ* device_bluez =
        static_cast<BluetoothDeviceBlueZ*>(iter->second.get());

    BluetoothPairingBlueZ* pairing = device_bluez->GetPairing();
    if (pairing && pairing->GetPairingDelegate() == pairing_delegate)
      device_bluez->EndPairing();
  }
}

void BluetoothAdapterBlueZ::AdapterAdded(const dbus::ObjectPath& object_path) {
  // Set the adapter to the newly added adapter only if no adapter is present.
  if (!IsPresent())
    SetAdapter(object_path);

#if BUILDFLAG(IS_CHROMEOS)
  RegisterAdvertisementMonitorApplicationServiceProvider();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void BluetoothAdapterBlueZ::AdapterRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path == object_path_)
    RemoveAdapter();
}

void BluetoothAdapterBlueZ::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;
  DCHECK(IsPresent());

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);

  if (property_name == properties->powered.name()) {
    NotifyAdapterPoweredChanged(properties->powered.value());
  } else if (property_name == properties->discoverable.name()) {
    DiscoverableChanged(properties->discoverable.value());
  } else if (property_name == properties->discovering.name()) {
    DiscoveringChanged(properties->discovering.value());
  }
}

void BluetoothAdapterBlueZ::AdminPolicyAdded(
    const dbus::ObjectPath& object_path) {
  BLUETOOTH_LOG(DEBUG) << "Admin Policy added " << object_path.value();

  UpdateDeviceAdminPolicyFromAdminPolicyClient(object_path);
}

void BluetoothAdapterBlueZ::AdminPolicyRemoved(
    const dbus::ObjectPath& object_path) {
  BLUETOOTH_LOG(DEBUG) << "Admin Policy removed " << object_path.value();

  UpdateDeviceAdminPolicyFromAdminPolicyClient(object_path);
}

void BluetoothAdapterBlueZ::AdminPolicyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BLUETOOTH_LOG(DEBUG) << "Admin Policy property " << property_name
                       << " changed " << object_path.value();

  UpdateDeviceAdminPolicyFromAdminPolicyClient(object_path);
}

void BluetoothAdapterBlueZ::BatteryAdded(const dbus::ObjectPath& object_path) {
  BLUETOOTH_LOG(DEBUG) << "Battery added " << object_path.value();

  UpdateDeviceBatteryLevelFromBatteryClient(object_path);
}

void BluetoothAdapterBlueZ::BatteryRemoved(
    const dbus::ObjectPath& object_path) {
  BLUETOOTH_LOG(DEBUG) << "Battery removed " << object_path.value();

  UpdateDeviceBatteryLevelFromBatteryClient(object_path);
}

void BluetoothAdapterBlueZ::BatteryPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BLUETOOTH_LOG(DEBUG) << "Battery property changed " << object_path.value();

  if (property_name == bluetooth_battery::kPercentageProperty)
    UpdateDeviceBatteryLevelFromBatteryClient(object_path);
}

void BluetoothAdapterBlueZ::DeviceAdded(const dbus::ObjectPath& object_path) {
  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path);
  if (!properties || properties->adapter.value() != object_path_)
    return;
  DCHECK(IsPresent());

  BluetoothDeviceBlueZ* device_bluez = new BluetoothDeviceBlueZ(
      this, object_path, ui_task_runner_, socket_thread_);
  DCHECK(devices_.find(device_bluez->GetAddress()) == devices_.end());

  devices_[device_bluez->GetAddress()] = base::WrapUnique(device_bluez);

  if (properties->rssi.is_valid() && properties->eir.is_valid()) {
    NotifyDeviceAdvertisementReceived(device_bluez, properties->rssi.value(),
                                      properties->eir.value());
  }

  // There is no guarantee that BatteryAdded is called after DeviceAdded
  // (for the same device). So always update the battery value of this newly
  // detected device in case we ignored BatteryAdded calls for it before this
  // DeviceAdded call.
  UpdateDeviceBatteryLevelFromBatteryClient(object_path);
  UpdateDeviceAdminPolicyFromAdminPolicyClient(object_path);

  for (auto& observer : observers_)
    observer.DeviceAdded(this, device_bluez);
}

void BluetoothAdapterBlueZ::DeviceRemoved(const dbus::ObjectPath& object_path) {
  for (auto iter = devices_.begin(); iter != devices_.end(); ++iter) {
    BluetoothDeviceBlueZ* device_bluez =
        static_cast<BluetoothDeviceBlueZ*>(iter->second.get());
    if (device_bluez->object_path() == object_path) {
      std::unique_ptr<BluetoothDevice> scoped_device = std::move(iter->second);
      devices_.erase(iter);

      for (auto& observer : observers_)
        observer.DeviceRemoved(this, device_bluez);
      return;
    }
  }
}

void BluetoothAdapterBlueZ::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceBlueZ* device_bluez = GetDeviceWithPath(object_path);
  if (!device_bluez)
    return;

  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path);

  if (property_name == properties->address.name()) {
    for (auto iter = devices_.begin(); iter != devices_.end(); ++iter) {
      if (iter->second->GetAddress() == device_bluez->GetAddress()) {
        std::string old_address = iter->first;
        BLUETOOTH_LOG(EVENT) << "Device changed address, old: " << old_address
                             << " new: " << device_bluez->GetAddress();
        std::unique_ptr<BluetoothDevice> scoped_device =
            std::move(iter->second);
        devices_.erase(iter);

        DCHECK(devices_.find(device_bluez->GetAddress()) == devices_.end());
        devices_[device_bluez->GetAddress()] = std::move(scoped_device);
        NotifyDeviceAddressChanged(device_bluez, old_address);
        break;
      }
    }
  }

  if (property_name == properties->service_data.name())
    device_bluez->UpdateServiceData();
  else if (property_name == properties->manufacturer_data.name())
    device_bluez->UpdateManufacturerData();
  else if (property_name == properties->advertising_data_flags.name())
    device_bluez->UpdateAdvertisingDataFlags();
  else if (property_name == properties->uuids.name())
    device_bluez->UpdateServiceUUIDs();

  if (property_name == properties->bluetooth_class.name() ||
      property_name == properties->appearance.name() ||
      property_name == properties->address.name() ||
      property_name == properties->name.name() ||
      property_name == properties->paired.name() ||
#if BUILDFLAG(IS_CHROMEOS)
      property_name == properties->bonded.name() ||
#endif
      property_name == properties->trusted.name() ||
      property_name == properties->connected.name() ||
      property_name == properties->uuids.name() ||
      property_name == properties->rssi.name() ||
      property_name == properties->tx_power.name() ||
      property_name == properties->service_data.name() ||
      property_name == properties->manufacturer_data.name() ||
      property_name == properties->advertising_data_flags.name()) {
    NotifyDeviceChanged(device_bluez);
  }

  if (property_name == properties->mtu.name())
    NotifyDeviceMTUChanged(device_bluez, properties->mtu.value());

  // Bluez does not currently provide an explicit signal for an advertisement
  // packet being received. Currently, it implicitly does so by notifying of an
  // RSSI change. We also listen for whether the EIR packet data has changed.
  if ((property_name == properties->rssi.name() ||
       property_name == properties->eir.name()) &&
      properties->rssi.is_valid() && properties->eir.is_valid()) {
    NotifyDeviceAdvertisementReceived(device_bluez, properties->rssi.value(),
                                      properties->eir.value());
  }

  if (property_name == properties->connected.name())
    NotifyDeviceConnectedStateChanged(device_bluez,
                                      properties->connected.value());

  if (property_name == properties->services_resolved.name() &&
      properties->services_resolved.value()) {
    device_bluez->UpdateGattServices(object_path);
    NotifyGattServicesDiscovered(device_bluez);
  }

  if (property_name == properties->paired.name()) {
    NotifyDevicePairedChanged(device_bluez, properties->paired.value());
  }

// For CrOS, when a device becomes bonded, mark it as trusted so that the
// user does not need to approve every incoming connection
// This is not for other OS because,for non-CrOS, Chrome is not part of the OS.
// Leave the decision to the real OS
#if BUILDFLAG(IS_CHROMEOS)
  if (property_name == properties->bonded.name()) {
    if (properties->bonded.value() && !properties->trusted.value()) {
      device_bluez->SetTrusted();
    }
    NotifyDeviceBondedChanged(device_bluez, properties->bonded.value());
  }
#endif

  // UMA connection counting
  if (property_name == properties->connected.name()) {
    int count = 0;

    for (auto iter = devices_.begin(); iter != devices_.end(); ++iter) {
      if (iter->second->IsPaired() && iter->second->IsConnected())
        ++count;
    }

    UMA_HISTOGRAM_COUNTS_100("Bluetooth.ConnectedDeviceCount", count);
  }
}

void BluetoothAdapterBlueZ::InputPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceBlueZ* device_bluez = GetDeviceWithPath(object_path);
  if (!device_bluez)
    return;

  bluez::BluetoothInputClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothInputClient()->GetProperties(
          object_path);

  // Properties structure can be removed, which triggers a change in the
  // BluetoothDevice::IsConnectable() property, as does a change in the
  // actual reconnect_mode property.
  if (!properties || property_name == properties->reconnect_mode.name()) {
    NotifyDeviceChanged(device_bluez);
  }
}

void BluetoothAdapterBlueZ::AgentManagerAdded(
    const dbus::ObjectPath& object_path) {
  BLUETOOTH_LOG(DEBUG) << "Registering pairing agent";
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAgentManagerClient()
      ->RegisterAgent(
          dbus::ObjectPath(kAgentPath),
          bluetooth_agent_manager::kKeyboardDisplayCapability,
          base::BindOnce(&BluetoothAdapterBlueZ::OnRegisterAgent,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&BluetoothAdapterBlueZ::OnRegisterAgentError,
                         weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterBlueZ::AgentManagerRemoved(
    const dbus::ObjectPath& object_path) {}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterBlueZ::SupportedAdvertisementMonitorFeaturesChanged() {
  NotifyLowEnergyScanSessionHardwareOffloadingStatusChanged(
      GetLowEnergyScanSessionHardwareOffloadingStatus());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothAdapterBlueZ::Released() {
  BLUETOOTH_LOG(EVENT) << "Released";
  if (!IsPresent())
    return;
  DCHECK(agent_.get());

  // Called after we unregister the pairing agent, e.g. when changing I/O
  // capabilities. Nothing much to be done right now.
}

void BluetoothAdapterBlueZ::RequestPinCode(const dbus::ObjectPath& device_path,
                                           PinCodeCallback callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value() << ": RequestPinCode";

  BluetoothPairingBlueZ* pairing = GetPairing(device_path);
  if (!pairing) {
    std::move(callback).Run(REJECTED, "");
    return;
  }

  pairing->RequestPinCode(std::move(callback));
}

void BluetoothAdapterBlueZ::DisplayPinCode(const dbus::ObjectPath& device_path,
                                           const std::string& pincode) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value()
                       << ": DisplayPinCode: " << pincode;

  BluetoothPairingBlueZ* pairing = GetPairing(device_path);
  if (!pairing)
    return;

  pairing->DisplayPinCode(pincode);
}

void BluetoothAdapterBlueZ::RequestPasskey(const dbus::ObjectPath& device_path,
                                           PasskeyCallback callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value() << ": RequestPasskey";

  BluetoothPairingBlueZ* pairing = GetPairing(device_path);
  if (!pairing) {
    std::move(callback).Run(REJECTED, 0);
    return;
  }

  pairing->RequestPasskey(std::move(callback));
}

void BluetoothAdapterBlueZ::DisplayPasskey(const dbus::ObjectPath& device_path,
                                           uint32_t passkey,
                                           uint16_t entered) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value() << ": DisplayPasskey: " << passkey
                       << " (" << entered << " entered)";

  BluetoothPairingBlueZ* pairing = GetPairing(device_path);
  if (!pairing)
    return;

  if (entered == 0)
    pairing->DisplayPasskey(passkey);

  pairing->KeysEntered(entered);
}

void BluetoothAdapterBlueZ::RequestConfirmation(
    const dbus::ObjectPath& device_path,
    uint32_t passkey,
    ConfirmationCallback callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value()
                       << ": RequestConfirmation: " << passkey;

  BluetoothPairingBlueZ* pairing = GetPairing(device_path);
  if (!pairing) {
    std::move(callback).Run(REJECTED);
    return;
  }

  pairing->RequestConfirmation(passkey, std::move(callback));
}

void BluetoothAdapterBlueZ::RequestAuthorization(
    const dbus::ObjectPath& device_path,
    ConfirmationCallback callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value() << ": RequestAuthorization";

  BluetoothPairingBlueZ* pairing = GetPairing(device_path);
  if (!pairing) {
    std::move(callback).Run(REJECTED);
    return;
  }

  pairing->RequestAuthorization(std::move(callback));
}

void BluetoothAdapterBlueZ::AuthorizeService(
    const dbus::ObjectPath& device_path,
    const std::string& uuid,
    ConfirmationCallback callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << device_path.value() << ": AuthorizeService: " << uuid;

  BluetoothDeviceBlueZ* device_bluez = GetDeviceWithPath(device_path);
  if (!device_bluez) {
    std::move(callback).Run(CANCELLED);
    return;
  }

  // For CrOS, we always set trusted when a device becomes bonded, so the only
  // reason that this method call would ever be called is in the case of a
  // race condition where our "Set('Trusted', true)" method call is still
  // pending in the Bluetooth daemon because it's busy handling the incoming
  // connection.
#if BUILDFLAG(IS_CHROMEOS)
  if (device_bluez->IsBonded()) {
    std::move(callback).Run(SUCCESS);
    return;
  }
#endif

  // Allow nearby connection from unbonded devices.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::nearby::IsNearbyClientUuid(BluetoothUUID(uuid))) {
    std::move(callback).Run(SUCCESS);
    return;
  }
#endif

  // TODO(keybuk): reject service authorizations when not paired, determine
  // whether this is acceptable long-term.
  BLUETOOTH_LOG(ERROR) << "Rejecting service connection from unpaired device "
                       << device_bluez->GetAddress() << " for UUID " << uuid;
  std::move(callback).Run(REJECTED);
}

void BluetoothAdapterBlueZ::Cancel() {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  BLUETOOTH_LOG(EVENT) << "Cancel";
}

void BluetoothAdapterBlueZ::OnSetLLPrivacySuccess() {
  bool flag = base::FeatureList::IsEnabled(bluez::features::kLinkLayerPrivacy);
  BLUETOOTH_LOG(DEBUG) << "LL Privacy value set to " << flag;
}

void BluetoothAdapterBlueZ::OnSetLLPrivacyError(
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << "Failed to enable LL Privacy: " << error_name << ": "
                       << error_message;
}

void BluetoothAdapterBlueZ::OnRegisterAgent() {
  BLUETOOTH_LOG(EVENT)
      << "Pairing agent registered, requesting to be made default";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothAgentManagerClient()
      ->RequestDefaultAgent(
          dbus::ObjectPath(kAgentPath),
          base::BindOnce(&BluetoothAdapterBlueZ::OnRequestDefaultAgent,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&BluetoothAdapterBlueZ::OnRequestDefaultAgentError,
                         weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterBlueZ::OnRegisterAgentError(
    const std::string& error_name,
    const std::string& error_message) {
  // Our agent being already registered isn't an error.
  if (error_name == bluetooth_agent_manager::kErrorAlreadyExists)
    return;

  BLUETOOTH_LOG(ERROR) << "Failed to register pairing agent: " << error_name
                       << ": " << error_message;
}

void BluetoothAdapterBlueZ::OnRequestDefaultAgent() {
  BLUETOOTH_LOG(EVENT) << "Pairing agent now default";
}

void BluetoothAdapterBlueZ::OnRequestDefaultAgentError(
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << "Failed to make pairing agent default: " << error_name
                       << ": " << error_message;
}

void BluetoothAdapterBlueZ::CreateServiceRecord(
    const BluetoothServiceRecordBlueZ& record,
    ServiceRecordCallback callback,
    ServiceRecordErrorCallback error_callback) {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->CreateServiceRecord(
          object_path_, record, std::move(callback),
          base::BindOnce(&BluetoothAdapterBlueZ::ServiceRecordErrorConnector,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(error_callback)));
}

void BluetoothAdapterBlueZ::RemoveServiceRecord(
    uint32_t handle,
    base::OnceClosure callback,
    ServiceRecordErrorCallback error_callback) {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->RemoveServiceRecord(
          object_path_, handle, std::move(callback),
          base::BindOnce(&BluetoothAdapterBlueZ::ServiceRecordErrorConnector,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(error_callback)));
}

BluetoothDeviceBlueZ* BluetoothAdapterBlueZ::GetDeviceWithPath(
    const dbus::ObjectPath& object_path) {
  if (!IsPresent())
    return nullptr;

  for (auto iter = devices_.begin(); iter != devices_.end(); ++iter) {
    BluetoothDeviceBlueZ* device_bluez =
        static_cast<BluetoothDeviceBlueZ*>(iter->second.get());
    if (device_bluez->object_path() == object_path)
      return device_bluez;
  }

  return nullptr;
}

BluetoothPairingBlueZ* BluetoothAdapterBlueZ::GetPairing(
    const dbus::ObjectPath& object_path) {
  DCHECK(IsPresent());
  BluetoothDeviceBlueZ* device_bluez = GetDeviceWithPath(object_path);
  if (!device_bluez) {
    BLUETOOTH_LOG(ERROR) << "Pairing Agent request for unknown device: "
                         << object_path.value();
    return nullptr;
  }

  BluetoothPairingBlueZ* pairing = device_bluez->GetPairing();
  if (pairing)
    return pairing;

  // The device doesn't have its own pairing context, so this is an incoming
  // pairing request that should use our best default delegate (if we have one).
  BluetoothDevice::PairingDelegate* pairing_delegate = DefaultPairingDelegate();
  if (!pairing_delegate)
    return nullptr;

  return device_bluez->BeginPairing(pairing_delegate);
}

void BluetoothAdapterBlueZ::SetAdapter(const dbus::ObjectPath& object_path) {
  DCHECK(!IsPresent());
  DCHECK(!dbus_is_shutdown_);
  object_path_ = object_path;

  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": using adapter.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // No need to do this in Lacros because Ash would be around, and would have
  // done this already.
  SetStandardChromeOSAdapterName();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);

  PresentChanged(true);

  if (properties->powered.value())
    NotifyAdapterPoweredChanged(true);
  if (properties->discoverable.value())
    DiscoverableChanged(true);
  if (properties->discovering.value())
    DiscoveringChanged(true);

  std::vector<dbus::ObjectPath> device_paths =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothDeviceClient()
          ->GetDevicesForAdapter(object_path_);

  for (auto iter = device_paths.begin(); iter != device_paths.end(); ++iter) {
    DeviceAdded(*iter);
  }
}

void BluetoothAdapterBlueZ::RemoveAdapter() {
  DCHECK(IsPresent());
  BLUETOOTH_LOG(EVENT) << object_path_.value() << ": adapter removed.";

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);

  object_path_ = dbus::ObjectPath("");

  if (properties->powered.value())
    NotifyAdapterPoweredChanged(false);
  if (properties->discoverable.value())
    DiscoverableChanged(false);

  // The properties->discovering.value() may not be up to date with the real
  // discovering state (BlueZ bug: http://crbug.com/822104).
  // When the adapter is removed, make sure to clear all discovery sessions no
  // matter what the current properties->discovering.value() is.
  // DiscoveringChanged() properly handles the case where there is no discovery
  // sessions currently.
  DiscoveringChanged(false);

  ClearAllDevices();
  PresentChanged(false);

#if BUILDFLAG(IS_CHROMEOS)
  is_advertisement_monitor_application_provider_registered_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void BluetoothAdapterBlueZ::DiscoverableChanged(bool discoverable) {
  for (auto& observer : observers_)
    observer.AdapterDiscoverableChanged(this, discoverable);
}

void BluetoothAdapterBlueZ::DiscoveringChanged(bool discovering) {
  // If the adapter stopped discovery due to a reason other than a request by
  // us, reset the count to 0.
  BLUETOOTH_LOG(EVENT) << "Discovering changed: " << discovering;
  if (!discovering && NumScanningDiscoverySessions() > 0) {
    BLUETOOTH_LOG(DEBUG) << "Marking sessions as inactive.";
    MarkDiscoverySessionsAsInactive();
  }

  for (auto& observer : observers_)
    observer.AdapterDiscoveringChanged(this, discovering);
}

void BluetoothAdapterBlueZ::PresentChanged(bool present) {
  for (auto& observer : observers_)
    observer.AdapterPresentChanged(this, present);
}

void BluetoothAdapterBlueZ::NotifyDeviceAddressChanged(
    BluetoothDeviceBlueZ* device,
    const std::string& old_address) {
  DCHECK(device->adapter_ == this);

  for (auto& observer : observers_)
    observer.DeviceAddressChanged(this, device, old_address);
}

void BluetoothAdapterBlueZ::NotifyDeviceMTUChanged(BluetoothDeviceBlueZ* device,
                                                   uint16_t mtu) {
  DCHECK(device->adapter_ == this);

  for (auto& observer : observers_)
    observer.DeviceMTUChanged(this, device, mtu);
}

void BluetoothAdapterBlueZ::NotifyDeviceAdvertisementReceived(
    BluetoothDeviceBlueZ* device,
    int16_t rssi,
    const std::vector<uint8_t>& eir) {
  DCHECK(device->adapter_ == this);

  for (auto& observer : observers_)
    observer.DeviceAdvertisementReceived(this, device, rssi, eir);

#if BUILDFLAG(IS_CHROMEOS)
  if (ble_scan_parser_.is_bound()) {
    ScanRecordCallback callback =
        base::BindOnce(&BluetoothAdapterBlueZ::OnAdvertisementReceived,
                       weak_ptr_factory_.GetWeakPtr(), device->GetAddress(),
                       device->GetName() ? *(device->GetName()) : std::string(),
                       rssi, device->GetAppearance(), device->object_path());
    ble_scan_parser_->Parse(eir, std::move(callback));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterBlueZ::OnAdvertisementReceived(
    std::string device_address,
    std::string device_name,
    uint8_t rssi,
    uint16_t device_appearance,
    const dbus::ObjectPath& device_path,
    ScanRecordPtr scan_record) {
  // Ignore the packet if it could not be parsed successfully.
  if (!scan_record)
    return;

  auto service_data_map = ConvertServiceDataMap(scan_record->service_data_map);
  auto manufacturer_data_map =
      ConvertManufacturerDataMap(scan_record->manufacturer_data_map);
  for (auto& observer : observers_) {
    observer.DeviceAdvertisementReceived(
        device_address, device_name, scan_record->advertisement_name, rssi,
        scan_record->tx_power, device_appearance, scan_record->service_uuids,
        service_data_map, manufacturer_data_map);
  }

  BluetoothDeviceBlueZ* device = GetDeviceWithPath(device_path);
  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Device " << device_path.value() << " not found!";
    return;
  }

  device->SetAdvertisedUUIDs(scan_record->service_uuids);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothAdapterBlueZ::NotifyDeviceConnectedStateChanged(
    BluetoothDeviceBlueZ* device,
    bool is_now_connected) {
  DCHECK_EQ(device->adapter_, this);
  DCHECK_EQ(device->IsConnected(), is_now_connected);

#if BUILDFLAG(IS_CHROMEOS)
  if (is_now_connected) {
    device::BluetoothConnectionLogger::RecordDeviceConnected(
        device->GetIdentifier(), device->GetDeviceType());
  } else {
    device::RecordDeviceDisconnect(device->GetDeviceType());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  BluetoothAdapter::NotifyDeviceConnectedStateChanged(device, is_now_connected);
}

void BluetoothAdapterBlueZ::UseProfile(
    const BluetoothUUID& uuid,
    const dbus::ObjectPath& device_path,
    const bluez::BluetoothProfileManagerClient::Options& options,
    bluez::BluetoothProfileServiceProvider::Delegate* delegate,
    ProfileRegisteredCallback success_callback,
    ErrorCompletionCallback error_callback) {
  DCHECK(delegate);

  if (!IsPresent()) {
    BLUETOOTH_LOG(DEBUG) << "Adapter not present, erroring out";
    std::move(error_callback).Run("Adapter not present");
    return;
  }

  if (profiles_.find(uuid) != profiles_.end()) {
    // TODO(jamuraa) check that the options are the same and error when they are
    // not.
    SetProfileDelegate(uuid, device_path, delegate, std::move(success_callback),
                       std::move(error_callback));
    return;
  }

  if (profile_queues_.find(uuid) == profile_queues_.end()) {
    BluetoothAdapterProfileBlueZ::Register(
        uuid, options,
        base::BindOnce(&BluetoothAdapterBlueZ::OnRegisterProfile, this, uuid),
        base::BindOnce(&BluetoothAdapterBlueZ::OnRegisterProfileError, this,
                       uuid));

    profile_queues_[uuid] = new std::vector<RegisterProfileCompletionPair>();
  }

  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  profile_queues_[uuid]->push_back(std::make_pair(
      base::BindOnce(&BluetoothAdapterBlueZ::SetProfileDelegate, this, uuid,
                     device_path, delegate, std::move(success_callback),
                     std::move(split_error_callback.first)),
      std::move(split_error_callback.second)));
}

void BluetoothAdapterBlueZ::ReleaseProfile(
    const dbus::ObjectPath& device_path,
    BluetoothAdapterProfileBlueZ* profile) {
  BLUETOOTH_LOG(EVENT) << "Releasing Profile: "
                       << profile->uuid().canonical_value() << " from "
                       << device_path.value();
  BluetoothUUID uuid = profile->uuid();
  auto iter = profiles_.find(uuid);
  if (iter == profiles_.end()) {
    BLUETOOTH_LOG(ERROR) << "Profile not found for: " << uuid.canonical_value();
    return;
  }
  released_profiles_[uuid] = iter->second;
  profiles_.erase(iter);
  profile->RemoveDelegate(device_path,
                          base::BindOnce(&BluetoothAdapterBlueZ::RemoveProfile,
                                         weak_ptr_factory_.GetWeakPtr(), uuid));
}

void BluetoothAdapterBlueZ::RemoveProfile(const BluetoothUUID& uuid) {
  BLUETOOTH_LOG(EVENT) << "Remove Profile: " << uuid.canonical_value();

  auto iter = released_profiles_.find(uuid);
  if (iter == released_profiles_.end()) {
    BLUETOOTH_LOG(ERROR) << "Released Profile not found: "
                         << uuid.canonical_value();
    return;
  }
  delete iter->second;
  released_profiles_.erase(iter);
}

void BluetoothAdapterBlueZ::AddLocalGattService(
    std::unique_ptr<BluetoothLocalGattServiceBlueZ> service) {
  owned_gatt_services_[service->object_path()] = std::move(service);
}

void BluetoothAdapterBlueZ::RemoveLocalGattService(
    BluetoothLocalGattServiceBlueZ* service) {
  auto service_iter = owned_gatt_services_.find(service->object_path());
  if (service_iter == owned_gatt_services_.end()) {
    BLUETOOTH_LOG(ERROR) << "Trying to remove service: "
                         << service->object_path().value()
                         << " from adapter: " << object_path_.value()
                         << " that doesn't own it.";
    return;
  }

  if (registered_gatt_services_.count(service->object_path()) != 0) {
    registered_gatt_services_.erase(service->object_path());
    UpdateRegisteredApplication(true, base::DoNothing(), base::DoNothing());
  }

  owned_gatt_services_.erase(service_iter);
}

void BluetoothAdapterBlueZ::RegisterGattService(
    BluetoothLocalGattServiceBlueZ* service,
    base::OnceClosure callback,
    device::BluetoothGattService::ErrorCallback error_callback) {
  if (registered_gatt_services_.count(service->object_path()) > 0) {
    BLUETOOTH_LOG(ERROR)
        << "Re-registering a service that is already registered!";
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }

  registered_gatt_services_[service->object_path()] = service;

  // Always assume that we were already registered. If we weren't registered
  // we'll just get an error back which we can ignore. Any other approach will
  // introduce a race since we will always have a period when we may have been
  // registered with BlueZ, but not know that the registration succeeded
  // because the callback hasn't come back yet.
  UpdateRegisteredApplication(true, std::move(callback),
                              std::move(error_callback));
}

void BluetoothAdapterBlueZ::UnregisterGattService(
    BluetoothLocalGattServiceBlueZ* service,
    base::OnceClosure callback,
    device::BluetoothGattService::ErrorCallback error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());

  if (registered_gatt_services_.count(service->object_path()) == 0) {
    BLUETOOTH_LOG(ERROR)
        << "Unregistering a service that isn't registered! path: "
        << service->object_path().value();
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }

  registered_gatt_services_.erase(service->object_path());
  UpdateRegisteredApplication(false, std::move(callback),
                              std::move(error_callback));
}

bool BluetoothAdapterBlueZ::IsGattServiceRegistered(
    BluetoothLocalGattServiceBlueZ* service) {
  return registered_gatt_services_.count(service->object_path()) != 0;
}

bool BluetoothAdapterBlueZ::SendValueChanged(
    BluetoothLocalGattCharacteristicBlueZ* characteristic,
    const std::vector<uint8_t>& value) {
  if (registered_gatt_services_.count(
          static_cast<BluetoothLocalGattServiceBlueZ*>(
              characteristic->GetService())
              ->object_path()) == 0)
    return false;
  gatt_application_provider_->SendValueChanged(characteristic->object_path(),
                                               value);
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterBlueZ::SetServiceAllowList(const UUIDList& uuids,
                                                base::OnceClosure callback,
                                                ErrorCallback error_callback) {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdminPolicyClient()
      ->SetServiceAllowList(object_path_, uuids, std::move(callback),
                            base::BindOnce(&SetServiceAllowListErrorCallback,
                                           std::move(error_callback)));
}

std::unique_ptr<device::BluetoothLowEnergyScanSession>
BluetoothAdapterBlueZ::StartLowEnergyScanSession(
    std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
  DCHECK(filter);

  dbus::ObjectPath monitor_path = dbus::ObjectPath(
      static_cast<std::string>(kAdvertisementMonitorApplicationObjectPath) +
      "/" + base::UnguessableToken::Create().ToString());
  BLUETOOTH_LOG(EVENT) << __func__ << ": session_id = " << monitor_path.value();

  // Client will take ownership of |low_energy_scan_session|.
  // OnLowEnergyScanSessionDestroyed removes the session from the D-Bus
  // application. |low_energy_scan_session| forwards callbacks from D-Bus to the
  // client-owned |delegate|.
  auto low_energy_scan_session =
      std::make_unique<BluetoothLowEnergyScanSessionBlueZ>(
          monitor_path.value(), weak_ptr_factory_.GetWeakPtr(), delegate,
          base::BindOnce(
              &BluetoothAdapterBlueZ::OnLowEnergyScanSessionDestroyed,
              weak_ptr_factory_.GetWeakPtr()));

  // Implements the advertisement monitor interface and forwards dbus callbacks
  // to the |low_energy_scan_session|.
  auto advertisement_monitor =
      BluetoothAdvertisementMonitorServiceProvider::Create(
          bluez::BluezDBusManager::Get()->GetSystemBus(), monitor_path,
          std::move(filter), low_energy_scan_session->GetWeakPtr());

  if (advertisement_monitor_application_provider_ &&
      is_advertisement_monitor_application_provider_registered_) {
    // Signals D-Bus that a new advertisement monitor is added.
    advertisement_monitor_application_provider_->AddMonitor(
        std::move(advertisement_monitor));
  } else {
    BLUETOOTH_LOG(EVENT) << __func__
                         << ": Advertisement monitor application not yet "
                            "registered. Queuing low energy scan session.";

    pending_advertisement_monitors_.push(std::move(advertisement_monitor));
  }

  return low_energy_scan_session;
}

BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
BluetoothAdapterBlueZ::GetLowEnergyScanSessionHardwareOffloadingStatus() {
  if (!IsPresent())
    return LowEnergyScanSessionHardwareOffloadingStatus::kUndetermined;

  BluetoothAdvertisementMonitorManagerClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdvertisementMonitorManagerClient()
          ->GetProperties(object_path_);

  if (!properties) {
    return LowEnergyScanSessionHardwareOffloadingStatus::kUndetermined;
  }

  return base::Contains(properties->supported_features.value(),
                        bluetooth_advertisement_monitor_manager::
                            kSupportedFeaturesControllerPatterns)
             ? LowEnergyScanSessionHardwareOffloadingStatus::kSupported
             : LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported;
}

std::vector<BluetoothAdapter::BluetoothRole>
BluetoothAdapterBlueZ::GetSupportedRoles() {
  std::vector<BluetoothAdapter::BluetoothRole> roles;

  if (!IsPresent()) {
    return roles;
  }

  bluez::BluetoothAdapterClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdapterClient()
          ->GetProperties(object_path_);
  DCHECK(properties);

  for (auto role : properties->roles.value()) {
    if (role == "central") {
      roles.push_back(BluetoothAdapter::BluetoothRole::kCentral);
    } else if (role == "peripheral") {
      roles.push_back(BluetoothAdapter::BluetoothRole::kPeripheral);
    } else if (role == "central-peripheral") {
      roles.push_back(BluetoothAdapter::BluetoothRole::kCentralPeripheral);
    } else {
      BLUETOOTH_LOG(EVENT) << __func__ << ": Unknown role: " << role;
    }
  }

  return roles;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BluetoothAdapterBlueZ::SetStandardChromeOSAdapterName() {
  if (!IsPresent()) {
    return;
  }

  std::string alias = ash::GetDeviceBluetoothName(GetAddress());
  SetName(alias, base::DoNothing(), base::DoNothing());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

dbus::ObjectPath BluetoothAdapterBlueZ::GetApplicationObjectPath() const {
  return dbus::ObjectPath(object_path_.value() + kGattApplicationObjectPath);
}

void BluetoothAdapterBlueZ::OnRegisterProfile(
    const BluetoothUUID& uuid,
    std::unique_ptr<BluetoothAdapterProfileBlueZ> profile) {
  profiles_[uuid] = profile.release();

  if (profile_queues_.find(uuid) == profile_queues_.end())
    return;

  for (auto& it : *profile_queues_[uuid])
    std::move(it.first).Run();
  delete profile_queues_[uuid];
  profile_queues_.erase(uuid);
}

void BluetoothAdapterBlueZ::SetProfileDelegate(
    const BluetoothUUID& uuid,
    const dbus::ObjectPath& device_path,
    bluez::BluetoothProfileServiceProvider::Delegate* delegate,
    ProfileRegisteredCallback success_callback,
    ErrorCompletionCallback error_callback) {
  if (profiles_.find(uuid) == profiles_.end()) {
    std::move(error_callback).Run("Cannot find profile!");
    return;
  }

  if (profiles_[uuid]->SetDelegate(device_path, delegate)) {
    std::move(success_callback).Run(profiles_[uuid]);
    return;
  }
  // Already set
  std::move(error_callback).Run(bluetooth_agent_manager::kErrorAlreadyExists);
}

void BluetoothAdapterBlueZ::OnRegisterProfileError(
    const BluetoothUUID& uuid,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to register profile: " << error_name << ": "
                       << error_message;
  if (profile_queues_.find(uuid) == profile_queues_.end())
    return;

  for (auto& it : *profile_queues_[uuid])
    std::move(it.second).Run(error_message);

  delete profile_queues_[uuid];
  profile_queues_.erase(uuid);
}

void BluetoothAdapterBlueZ::OnSetDiscoverable(base::OnceClosure callback,
                                              ErrorCallback error_callback,
                                              bool success) {
  if (!success || !IsPresent()) {
    std::move(error_callback).Run();
    return;
  }

  // Set the discoverable_timeout property to zero so the adapter remains
  // discoverable forever.
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->GetProperties(object_path_)
      ->discoverable_timeout.Set(
          0, base::BindOnce(&BluetoothAdapterBlueZ::OnPropertyChangeCompleted,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            std::move(error_callback)));
}

void BluetoothAdapterBlueZ::OnPropertyChangeCompleted(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    bool success) {
  if (IsPresent() && success) {
    std::move(callback).Run();
  } else {
    std::move(error_callback).Run();
  }
}

base::WeakPtr<BluetoothAdapter> BluetoothAdapterBlueZ::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// BluetoothAdapterBlueZ should override SetPowered() instead.
bool BluetoothAdapterBlueZ::SetPoweredImpl(bool powered) {
  NOTREACHED();
}

void BluetoothAdapterBlueZ::UpdateFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  DCHECK_GT(NumDiscoverySessions(), 0);
  if (!IsPresent()) {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  // DCHECK(IsDiscovering()) is removed due to BlueZ bug
  // (https://crbug.com/822104).
  // TODO(sonnysasaka): Put it back here when BlueZ bug is fixed.
  SetDiscoveryFilter(
      std::move(discovery_filter),
      base::BindOnce(std::move(split_callback.first), /*is_error=*/false,
                     UMABluetoothDiscoverySessionOutcome::SUCCESS),
      base::BindOnce(std::move(split_callback.second), true));
  return;
}

void BluetoothAdapterBlueZ::StartScanWithFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  DCHECK(discovery_filter.get());

  if (!IsPresent()) {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  // Only one of these is going to be called.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  SetDiscoveryFilter(
      std::move(discovery_filter),
      base::BindOnce(&BluetoothAdapterBlueZ::OnPreSetDiscoveryFilter,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&BluetoothAdapterBlueZ::OnPreSetDiscoveryFilterError,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(std::move(split_callback.second), true)));
}

void BluetoothAdapterBlueZ::StopScan(DiscoverySessionResultCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  ble_scan_parser_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Not having an adapter qualifies as not scanning so we callback a success
  if (!IsPresent()) {
    std::move(callback).Run(
        /*is_error=*/false,
        UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  DCHECK_EQ(NumDiscoverySessions(), 0);

  // Confirm that there are no more discovery sessions left.
  DCHECK_EQ(NumDiscoverySessions(), 0);
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->StopDiscovery(
      object_path_,
      base::BindOnce(
          &BluetoothAdapterBlueZ::OnStopDiscovery,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(std::move(split_callback.first), /*is_error=*/false,
                         UMABluetoothDiscoverySessionOutcome::SUCCESS)),
      base::BindOnce(
          &BluetoothAdapterBlueZ::OnStopDiscoveryError,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(std::move(split_callback.second), /*is_error=*/true)));
}

void BluetoothAdapterBlueZ::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    base::OnceClosure callback,
    DiscoverySessionErrorCallback error_callback) {
  DCHECK(discovery_filter.get());

  if (!IsPresent()) {
    std::move(error_callback)
        .Run(UMABluetoothDiscoverySessionOutcome::ADAPTER_REMOVED);
    return;
  }

  bluez::BluetoothAdapterClient::DiscoveryFilter dbus_discovery_filter;
  uint16_t pathloss;
  int16_t rssi;
  uint8_t transport;
  std::set<device::BluetoothUUID> uuids;

  if (discovery_filter->GetPathloss(&pathloss))
    dbus_discovery_filter.pathloss = std::make_unique<uint16_t>(pathloss);

  if (discovery_filter->GetRSSI(&rssi))
    dbus_discovery_filter.rssi = std::make_unique<int16_t>(rssi);

  transport = discovery_filter->GetTransport();
  if (transport == device::BLUETOOTH_TRANSPORT_LE) {
    dbus_discovery_filter.transport = std::make_unique<std::string>("le");
  } else if (transport == device::BLUETOOTH_TRANSPORT_CLASSIC) {
    dbus_discovery_filter.transport = std::make_unique<std::string>("bredr");
  } else if (transport == device::BLUETOOTH_TRANSPORT_DUAL) {
    dbus_discovery_filter.transport = std::make_unique<std::string>("auto");
  }

  discovery_filter->GetUUIDs(uuids);
  if (uuids.size()) {
    dbus_discovery_filter.uuids = std::make_unique<std::vector<std::string>>();

    for (const auto& it : uuids)
      dbus_discovery_filter.uuids.get()->push_back(it.value());
  }

  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdapterClient()
      ->SetDiscoveryFilter(
          object_path_, dbus_discovery_filter,
          base::BindOnce(&BluetoothAdapterBlueZ::OnSetDiscoveryFilter,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(split_error_callback.first)),
          base::BindOnce(&BluetoothAdapterBlueZ::OnSetDiscoveryFilterError,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(split_error_callback.second)));
}

void BluetoothAdapterBlueZ::OnStartDiscovery(
    DiscoverySessionResultCallback callback) {
  // Report success on the original request and increment the count.
  BLUETOOTH_LOG(EVENT) << __func__;

#if BUILDFLAG(IS_CHROMEOS)
  device::BluetoothAdapterFactory::BleScanParserCallback
      ble_scan_parser_callback =
          device::BluetoothAdapterFactory::GetBleScanParserCallback();
  if (ble_scan_parser_callback) {
    // To avoid repeatedly restarting a crashed data decoder service,
    // don't add a connection error handler here. Wait to establish a
    // new connection after all discovery sessions are stopped.
    ble_scan_parser_.Bind(ble_scan_parser_callback.Run());
  } else {
#if DCHECK_IS_ON()
    static bool logged_once = false;
    DLOG_IF(ERROR, !logged_once)
        << "Attempted to connect to "
           "unconfigured BluetoothAdapterFactory::GetBleScanParserCallback()";
    logged_once = true;
#endif  // DCHECK_IS_ON()
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (IsPresent()) {
    std::move(callback).Run(false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
  } else {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_REMOVED);
  }
}

void BluetoothAdapterBlueZ::OnStartDiscoveryError(
    DiscoverySessionResultCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to start discovery: " << error_name << ": "
                       << error_message;

  std::move(callback).Run(true, TranslateDiscoveryErrorToUMA(error_name));
}

void BluetoothAdapterBlueZ::OnStopDiscovery(base::OnceClosure callback) {
  // Report success on the original request and decrement the count.
  BLUETOOTH_LOG(EVENT) << __func__;
  DCHECK_GE(NumDiscoverySessions(), 0);
  std::move(callback).Run();
}

void BluetoothAdapterBlueZ::OnStopDiscoveryError(
    DiscoverySessionErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to stop discovery: " << error_name << ": "
                       << error_message;

  std::move(error_callback).Run(TranslateDiscoveryErrorToUMA(error_name));
}

void BluetoothAdapterBlueZ::OnPreSetDiscoveryFilter(
    DiscoverySessionResultCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient()->StartDiscovery(
      object_path_,
      base::BindOnce(&BluetoothAdapterBlueZ::OnStartDiscovery,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&BluetoothAdapterBlueZ::OnStartDiscoveryError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void BluetoothAdapterBlueZ::OnPreSetDiscoveryFilterError(
    DiscoverySessionErrorCallback error_callback,
    UMABluetoothDiscoverySessionOutcome outcome) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to pre set discovery filter.";

  std::move(error_callback).Run(outcome);
}

void BluetoothAdapterBlueZ::OnSetDiscoveryFilter(
    base::OnceClosure callback,
    DiscoverySessionErrorCallback error_callback) {
  // Report success on the original request and increment the count.
  BLUETOOTH_LOG(EVENT) << __func__;
  if (IsPresent()) {
    std::move(callback).Run();
  } else {
    std::move(error_callback)
        .Run(UMABluetoothDiscoverySessionOutcome::ADAPTER_REMOVED);
  }
}

void BluetoothAdapterBlueZ::OnSetDiscoveryFilterError(
    DiscoverySessionErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << object_path_.value()
                       << ": Failed to set discovery filter: " << error_name
                       << ": " << error_message;

  UMABluetoothDiscoverySessionOutcome outcome =
      TranslateDiscoveryErrorToUMA(error_name);
  if (outcome == UMABluetoothDiscoverySessionOutcome::FAILED) {
    // bluez/doc/adapter-api.txt says "Failed" is returned from
    // SetDiscoveryFilter when the controller doesn't support the requested
    // transport.
    outcome = UMABluetoothDiscoverySessionOutcome::
        BLUEZ_DBUS_FAILED_MAYBE_UNSUPPORTED_TRANSPORT;
  }
  std::move(error_callback).Run(outcome);
}

void BluetoothAdapterBlueZ::UpdateRegisteredApplication(
    bool ignore_unregister_failure,
    base::OnceClosure callback,
    device::BluetoothGattService::ErrorCallback error_callback) {
  // If ignore_unregister_failure is set, we'll forward the error_callback to
  // the register call (to be called in case the register call fails). If not,
  // we'll call the error callback if this unregister itself fails.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattManagerClient()
      ->UnregisterApplication(
          object_path_, GetApplicationObjectPath(),
          base::BindOnce(&BluetoothAdapterBlueZ::RegisterApplication,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(split_callback.first),
                         std::move(split_error_callback.first)),
          ignore_unregister_failure
              ? base::BindOnce(
                    &BluetoothAdapterBlueZ::RegisterApplicationOnError,
                    weak_ptr_factory_.GetWeakPtr(),
                    std::move(split_callback.second),
                    std::move(split_error_callback.second))
              : base::BindOnce(&OnRegistrationErrorCallback,
                               std::move(split_error_callback.second), false));
}

void BluetoothAdapterBlueZ::RegisterApplication(
    base::OnceClosure callback,
    device::BluetoothGattService::ErrorCallback error_callback) {
  // Recreate our application service provider with the currently registered
  // GATT services before we register it.
  gatt_application_provider_.reset();
  // If we have no services registered, then leave the application unregistered
  // and no application provider.
  if (registered_gatt_services_.size() == 0) {
    std::move(callback).Run();
    return;
  }
  gatt_application_provider_ = BluetoothGattApplicationServiceProvider::Create(
      bluez::BluezDBusManager::Get()->GetSystemBus(),
      GetApplicationObjectPath(), registered_gatt_services_);

  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattManagerClient()
      ->RegisterApplication(object_path_, GetApplicationObjectPath(),
                            BluetoothGattManagerClient::Options(),
                            std::move(callback),
                            base::BindOnce(&OnRegistrationErrorCallback,
                                           std::move(error_callback),
                                           /*is_register_callback=*/true));
}

void BluetoothAdapterBlueZ::RegisterApplicationOnError(
    base::OnceClosure callback,
    device::BluetoothGattService::ErrorCallback error_callback,
    const std::string& /* error_name */,
    const std::string& /* error_message */) {
  RegisterApplication(std::move(callback), std::move(error_callback));
}

void BluetoothAdapterBlueZ::ServiceRecordErrorConnector(
    ServiceRecordErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  BLUETOOTH_LOG(EVENT) << "Creating service record failed: error: "
                       << error_name << " - " << error_message;

  BluetoothServiceRecordBlueZ::ErrorCode code =
      BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN;
  if (error_name == bluetooth_adapter::kErrorInvalidArguments) {
    code = BluetoothServiceRecordBlueZ::ErrorCode::ERROR_INVALID_ARGUMENTS;
  } else if (error_name == bluetooth_adapter::kErrorDoesNotExist) {
    code = BluetoothServiceRecordBlueZ::ErrorCode::ERROR_RECORD_DOES_NOT_EXIST;
  } else if (error_name == bluetooth_adapter::kErrorAlreadyExists) {
    code = BluetoothServiceRecordBlueZ::ErrorCode::ERROR_RECORD_ALREADY_EXISTS;
  } else if (error_name == bluetooth_adapter::kErrorNotReady) {
    code = BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY;
  }

  std::move(error_callback).Run(code);
}

void BluetoothAdapterBlueZ::OnConnectDevice(
    ConnectDeviceCallback callback,
    const dbus::ObjectPath& object_path) {
  std::move(callback).Run(GetDeviceWithPath(object_path));
}

void BluetoothAdapterBlueZ::OnConnectDeviceError(
    ConnectDeviceErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  std::move(error_callback).Run(error_message);
}

void BluetoothAdapterBlueZ::UpdateDeviceAdminPolicyFromAdminPolicyClient(
    const dbus::ObjectPath& object_path) {
#if BUILDFLAG(IS_CHROMEOS)
  BluetoothDevice* device = GetDeviceWithPath(object_path);

  if (!device) {
    BLUETOOTH_LOG(DEBUG)
        << "Trying to update admin policy for nonexistent device, object_path: "
        << object_path.value();
    return;
  }

  bluez::BluetoothAdminPolicyClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothAdminPolicyClient()
          ->GetProperties(object_path);

  if (properties && properties->is_blocked_by_policy.is_valid()) {
    device->SetIsBlockedByPolicy(properties->is_blocked_by_policy.value());
    return;
  }

  // |properties| is null or properties->is_blocked_by_policy is not valid, that
  // means BlueZ has removed the admin policy from the device and we should
  // clear our value as well.
  device->SetIsBlockedByPolicy(false);
#endif
}

void BluetoothAdapterBlueZ::UpdateDeviceBatteryLevelFromBatteryClient(
    const dbus::ObjectPath& object_path) {
  BluetoothDevice* device = GetDeviceWithPath(object_path);

  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Trying to update battery for non-existing device";
    return;
  }

  if (IsBatteryDisplayBlocklisted(device)) {
    // Some peripherals are known to send unreliable battery values. So don't
    // display the battery value if such device is detected (best effort).
    BLUETOOTH_LOG(DEBUG)
        << "Filtering out a device from having battery value set: "
        << (device->GetName() ? *(device->GetName()) : "<Unknown name>");
    return;
  }

  bluez::BluetoothBatteryClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothBatteryClient()
          ->GetProperties(object_path);

  if (properties && properties->percentage.is_valid()) {
    device->SetBatteryInfo(
        BatteryInfo(BatteryType::kDefault, properties->percentage.value()));
    return;
  }

  // |properties| is null or properties->percentage is not valid, that means
  // BlueZ has removed the battery info from the device and we should clear our
  // value as well.
  device->RemoveBatteryInfo(BatteryType::kDefault);
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterBlueZ::
    RegisterAdvertisementMonitorApplicationServiceProvider() {
  if (is_advertisement_monitor_application_provider_registered_ ||
      !IsPresent()) {
    return;
  }
  BLUETOOTH_LOG(EVENT) << __func__;

  auto err_callback = [](std::string error_name,
                         const std::string error_message) {
    LOG(ERROR) << "Error while registering advertisement monitor application "
                  "service provider. error_name = "
               << error_name << ", error_message = " << error_message;
  };

  // Registers root application path of advertisement monitors/low energy
  // scanners.
  bluez::BluezDBusManager::Get()
      ->GetBluetoothAdvertisementMonitorManagerClient()
      ->RegisterMonitor(
          dbus::ObjectPath(kAdvertisementMonitorApplicationObjectPath),
          object_path_,
          base::BindOnce(
              &BluetoothAdapterBlueZ::
                  OnRegisterAdvertisementMonitorApplicationServiceProvider,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(err_callback));
}

void BluetoothAdapterBlueZ::
    OnRegisterAdvertisementMonitorApplicationServiceProvider() {
  is_advertisement_monitor_application_provider_registered_ = true;
  BLUETOOTH_LOG(EVENT) << __func__;

  while (!pending_advertisement_monitors_.empty()) {
    // Signals D-Bus that a new advertisement monitor is added.
    advertisement_monitor_application_provider_->AddMonitor(
        std::move(pending_advertisement_monitors_.front()));
    pending_advertisement_monitors_.pop();
  }
}

void BluetoothAdapterBlueZ::OnLowEnergyScanSessionDestroyed(
    const std::string& session_id) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": session_id = " << session_id;
  if (!advertisement_monitor_application_provider_ ||
      !is_advertisement_monitor_application_provider_registered_) {
    return;
  }
  advertisement_monitor_application_provider_->RemoveMonitor(
      dbus::ObjectPath(session_id));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace bluez
