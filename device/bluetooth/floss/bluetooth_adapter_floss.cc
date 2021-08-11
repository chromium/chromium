// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_adapter_floss.h"

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"

namespace floss {

namespace {
using device::UMABluetoothDiscoverySessionOutcome;

UMABluetoothDiscoverySessionOutcome TranslateDiscoveryErrorToUMA(
    const std::string& error_name) {
  // TODO(b/192289534) - Deal with UMA later
  return UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED;
}

// Helper function to gate init behind a check for Object Manager support.
void InitWhenObjectManagerKnown(base::OnceClosure callback) {
  FlossDBusManager::Get()->CallWhenObjectManagerSupportIsKnown(
      std::move(callback));
}

}  // namespace

// static
scoped_refptr<BluetoothAdapterFloss> BluetoothAdapterFloss::CreateAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterFloss());
}

BluetoothAdapterFloss::BluetoothAdapterFloss() = default;

BluetoothAdapterFloss::~BluetoothAdapterFloss() {
  Shutdown();
}

void BluetoothAdapterFloss::Initialize(base::OnceClosure callback) {
  BLUETOOTH_LOG(EVENT) << "BluetoothAdapterFloss::Initialize";
  init_callback_ = std::move(callback);

  // Queue a task to check for ObjectManager support and init once the support
  // is known.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&InitWhenObjectManagerKnown,
                     base::BindOnce(&BluetoothAdapterFloss::Init,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void BluetoothAdapterFloss::Shutdown() {
  BLUETOOTH_LOG(EVENT) << "BluetoothAdapterFloss::Shutdown";

  if (dbus_is_shutdown_)
    return;

  if (!FlossDBusManager::Get()->IsObjectManagerSupported()) {
    dbus_is_shutdown_ = true;
    return;
  }

  if (IsPresent())
    RemoveAdapter();  // Cleans up devices and adapter observers.
  DCHECK(devices_.empty());

  FlossDBusManager::Get()->GetManagerClient()->RemoveObserver(this);
  dbus_is_shutdown_ = true;
}

void BluetoothAdapterFloss::AddAdapterObservers() {
  DCHECK(FlossDBusManager::Get()->HasActiveAdapter());

  // Add any observers that depend on a specific observer.
  // FlossDBusManager::SwitchAdapter will control what the active adapter is
  // that we are controlling.
  FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
}

void BluetoothAdapterFloss::RemoveAdapter() {
  if (!FlossDBusManager::Get()->HasActiveAdapter())
    return;

  // Remove adapter by switching to an invalid adapter (cleans up DBus clients)
  // and then emitting |AdapterPresentChanged| to observers.
  FlossDBusManager::Get()->SwitchAdapter(FlossDBusManager::kInvalidAdapter);
  PresentChanged(false);
}

void BluetoothAdapterFloss::Init() {
  // If dbus is shutdown or ObjectManager isn't supported, we just return
  // without initializing anything.
  if (dbus_is_shutdown_ ||
      !FlossDBusManager::Get()->IsObjectManagerSupported()) {
    BLUETOOTH_LOG(ERROR) << "Floss Adapter initialized without object manager";
    initialized_ = true;
    std::move(init_callback_).Run();
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Floss Adapter Initialized";

  // Tie rest of init behind feature flag
  if (base::FeatureList::IsEnabled(floss::features::kFlossEnabled)) {
    FlossDBusManager::Get()->GetManagerClient()->SetFlossEnabled(true);
  }

  // Register for manager callbacks
  FlossDBusManager::Get()->GetManagerClient()->AddObserver(this);

  // Switch to adapter if the default adapter is present and enabled. If it is
  // not enabled, wait for upper layers to power it on.
  if (IsPresent()) {
    FlossManagerClient* manager = FlossDBusManager::Get()->GetManagerClient();
    int default_adapter = manager->GetDefaultAdapter();

    if (manager->GetAdapterEnabled(default_adapter)) {
      FlossDBusManager::Get()->SwitchAdapter(default_adapter);
      AddAdapterObservers();
    }
  }

  VLOG(1) << "BluetoothAdapterFloss::Init completed. Calling init callback.";
  initialized_ = true;
  std::move(init_callback_).Run();
}

BluetoothAdapterFloss::UUIDList BluetoothAdapterFloss::GetUUIDs() const {
  return {};
}

std::string BluetoothAdapterFloss::GetAddress() const {
  if (IsPowered()) {
    return FlossDBusManager::Get()->GetAdapterClient()->GetAddress();
  }

  return std::string();
}

std::string BluetoothAdapterFloss::GetName() const {
  return std::string();
}

std::string BluetoothAdapterFloss::GetSystemName() const {
  return std::string();
}

void BluetoothAdapterFloss::SetName(const std::string& name,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterFloss::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterFloss::IsPresent() const {
  // No clients will be working if object manager isn't supported or dbus is
  // shut down
  if (dbus_is_shutdown_ ||
      !FlossDBusManager::Get()->IsObjectManagerSupported()) {
    VLOG(1) << "BluetoothAdapterFloss::IsPresent = false (no object manager "
               "support or dbus is shut down)";
    return false;
  }

  FlossManagerClient* manager = FlossDBusManager::Get()->GetManagerClient();
  auto present = manager->GetAdapterPresent(manager->GetDefaultAdapter());

  return present;
}

bool BluetoothAdapterFloss::IsPowered() const {
  auto powered = FlossDBusManager::Get()->HasActiveAdapter();

  return powered;
}

void BluetoothAdapterFloss::SetPowered(bool powered,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  if (!IsPresent()) {
    BLUETOOTH_LOG(ERROR) << "SetPowered: " << powered << ". Not Present!";
    std::move(error_callback).Run();
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << powered;

  FlossDBusManager::Get()->GetManagerClient()->SetAdapterEnabled(
      FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter(), powered,
      base::BindOnce(&BluetoothAdapterFloss::OnMethodResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

bool BluetoothAdapterFloss::IsDiscoverable() const {
  return false;
}

void BluetoothAdapterFloss::SetDiscoverable(bool discoverable,
                                            base::OnceClosure callback,
                                            ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterFloss::IsDiscovering() const {
  if (!IsPresent())
    return false;

  return NumScanningDiscoverySessions() > 0;
}

void BluetoothAdapterFloss::OnMethodResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    std::move(error_callback).Run();
    return;
  }

  std::move(callback).Run();
}

void BluetoothAdapterFloss::OnStartDiscovery(
    DiscoverySessionResultCallback callback,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    // Adapter path only exists if active adapter hasn't disappeared
    auto adapter_path = FlossDBusManager::Get()->HasActiveAdapter()
                            ? FlossDBusManager::Get()
                                  ->GetAdapterClient()
                                  ->GetObjectPath()
                                  ->value()
                            : std::string();
    BLUETOOTH_LOG(ERROR) << adapter_path
                         << ": Failed to start discovery: " << error->name
                         << ": " << error->message;
    std::move(callback).Run(true, TranslateDiscoveryErrorToUMA(error->name));

    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  if (IsPresent()) {
    std::move(callback).Run(false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
  } else {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_REMOVED);
  }
}

void BluetoothAdapterFloss::OnStopDiscovery(
    DiscoverySessionResultCallback callback,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    // Adapter path only exists if active adapter hasn't disappeared
    auto adapter_path = FlossDBusManager::Get()->HasActiveAdapter()
                            ? FlossDBusManager::Get()
                                  ->GetAdapterClient()
                                  ->GetObjectPath()
                                  ->value()
                            : std::string();
    BLUETOOTH_LOG(ERROR) << adapter_path
                         << ": Failed to stop discovery: " << error->name
                         << ": " << error->message;
    std::move(callback).Run(true, TranslateDiscoveryErrorToUMA(error->name));

    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  DCHECK_GE(NumDiscoverySessions(), 0);
  std::move(callback).Run(false, UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

// Announce to observers a change in the adapter state.
void BluetoothAdapterFloss::DiscoverableChanged(bool discoverable) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::DiscoveringChanged(bool discovering) {
  // If the adapter stopped discovery due to a reason other than a request by
  // us, reset the count to 0.
  BLUETOOTH_LOG(EVENT) << "Discovering changed: " << discovering;
  if (!discovering && NumScanningDiscoverySessions() > 0) {
    BLUETOOTH_LOG(DEBUG) << "Marking sessions as inactive.";
    MarkDiscoverySessionsAsInactive();
  }

  for (auto& observer : observers_) {
    observer.AdapterDiscoveringChanged(this, discovering);
  }
}

void BluetoothAdapterFloss::PresentChanged(bool present) {
  for (auto& observer : observers_) {
    observer.AdapterPresentChanged(this, present);
  }
}

void BluetoothAdapterFloss::NotifyAdapterPoweredChanged(bool powered) {
  for (auto& observer : observers_) {
    observer.AdapterPoweredChanged(this, powered);
  }
}

// Observers

void BluetoothAdapterFloss::AdapterPresent(int adapter, bool present) {
  VLOG(1) << "BluetoothAdapterFloss: Adapter " << adapter
          << ", present: " << present;
  // TODO(b/191906229) - Support non-default adapters
  if (adapter !=
      FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter()) {
    return;
  }

  // If default adapter isn't present, we need to clean up the dbus manager
  if (!present) {
    RemoveAdapter();
  } else {
    // Notify observers
    PresentChanged(present);
  }
}

void BluetoothAdapterFloss::AdapterEnabledChanged(int adapter, bool enabled) {
  VLOG(1) << "BluetoothAdapterFloss: Adapter " << adapter
          << ", enabled: " << enabled;

  // TODO(b/191906229) - Support non-default adapters
  if (adapter !=
      FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter()) {
    VLOG(0) << __func__ << ": Adapter not default: "
            << FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter();
    return;
  }

  if (enabled && !FlossDBusManager::Get()->HasActiveAdapter()) {
    FlossDBusManager::Get()->SwitchAdapter(adapter);
    AddAdapterObservers();
  } else if (!enabled && FlossDBusManager::Get()->HasActiveAdapter()) {
    FlossDBusManager::Get()->SwitchAdapter(FlossDBusManager::kInvalidAdapter);
  }

  NotifyAdapterPoweredChanged(enabled);
}

void BluetoothAdapterFloss::AdapterDiscoveringChanged(bool state) {
  DCHECK(IsPresent());

  DiscoveringChanged(state);
}

void BluetoothAdapterFloss::AdapterFoundDevice(
    const FlossDeviceId& device_found) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  auto device_floss =
      base::WrapUnique(new BluetoothDeviceFloss(this, device_found));

  if (!base::Contains(devices_, device_floss->GetAddress())) {
    // Take copy of pointer before moving ownership.
    BluetoothDeviceFloss* device_ptr = device_floss.get();
    devices_.emplace(device_floss->GetAddress(), std::move(device_floss));

    for (auto& observer : observers_)
      observer.DeviceAdded(this, device_ptr);
  } else {
    // TODO(abps) - Reset freshness value for device.
  }

  BLUETOOTH_LOG(EVENT) << __func__ << ": Address (" << device_found.address
                       << "), Name = " << device_found.name;
}

void BluetoothAdapterFloss::AdapterSspRequest(
    const FlossDeviceId& remote_device,
    uint32_t cod,
    FlossAdapterClient::BluetoothSspVariant variant,
    uint32_t passkey) {
  NOTIMPLEMENTED();
}

std::unordered_map<device::BluetoothDevice*, device::BluetoothDevice::UUIDSet>
BluetoothAdapterFloss::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const device::BluetoothDiscoveryFilter& discovery_filter) {
  NOTIMPLEMENTED();
  return {};
}

void BluetoothAdapterFloss::CreateRfcommService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::CreateL2capService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::ResetAdvertising(
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::ConnectDevice(
    const std::string& address,
    const absl::optional<device::BluetoothDevice::AddressType>& address_type,
    ConnectDeviceCallback callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

device::BluetoothLocalGattService* BluetoothAdapterFloss::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BluetoothAdapterFloss::SetServiceAllowList(const UUIDList& uuids,
                                                base::OnceClosure callback,
                                                ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

std::unique_ptr<device::BluetoothLowEnergyScanSession>
BluetoothAdapterFloss::StartLowEnergyScanSession(
    std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
  NOTIMPLEMENTED();
  return nullptr;
}

device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
BluetoothAdapterFloss::GetLowEnergyScanSessionHardwareOffloadingStatus() {
  NOTIMPLEMENTED();
  return LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported;
}
#endif

void BluetoothAdapterFloss::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTIMPLEMENTED();
}

base::WeakPtr<device::BluetoothAdapter> BluetoothAdapterFloss::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothAdapterFloss::SetPoweredImpl(bool powered) {
  NOTREACHED();
  return false;
}

void BluetoothAdapterFloss::StartScanWithFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // Also return ADAPTER_NOT_PRESENT if not powered.
  // TODO(b/193839304) - !IsPowered should return ADAPTER_NOT_POWERED
  if (!IsPresent() || !IsPowered()) {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  // TODO(b/192251662) - Support scan filtering. For now, start scanning with no
  // filters in place.
  FlossDBusManager::Get()->GetAdapterClient()->StartDiscovery(
      base::BindOnce(&BluetoothAdapterFloss::OnStartDiscovery,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothAdapterFloss::UpdateFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // Also return ADAPTER_NOT_PRESENT if not powered.
  // TODO(b/193839304) - !IsPowered should return ADAPTER_NOT_POWERED
  if (!IsPresent() || !IsPowered()) {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  // TODO(b/192251662) - Support scan filtering. For now, always return success
  std::move(callback).Run(false, UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

void BluetoothAdapterFloss::StopScan(DiscoverySessionResultCallback callback) {
  // Also return ADAPTER_NOT_PRESENT if not powered.
  // TODO(b/193839304) - !IsPowered should return ADAPTER_NOT_POWERED
  if (!IsPresent() || !IsPowered()) {
    std::move(callback).Run(
        /*is_error= */ false,
        UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  DCHECK_EQ(NumDiscoverySessions(), 0);
  FlossDBusManager::Get()->GetAdapterClient()->CancelDiscovery(
      base::BindOnce(&BluetoothAdapterFloss::OnStopDiscovery,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace floss
