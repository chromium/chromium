// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace device {

BluetoothAdapter::ServiceOptions::ServiceOptions() = default;
BluetoothAdapter::ServiceOptions::~ServiceOptions() = default;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX)
// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return nullptr;
}
#endif  // Not supported platforms.

base::WeakPtr<BluetoothAdapter> BluetoothAdapter::GetWeakPtrForTesting() {
  return GetWeakPtr();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapter::Shutdown() {
  NOTIMPLEMENTED();
}
#endif

void BluetoothAdapter::AddObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapter::RemoveObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string BluetoothAdapter::GetSystemName() const {
  NOTIMPLEMENTED();
  return std::string();
}

bool BluetoothAdapter::HasObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  return observers_.HasObserver(observer);
}

bool BluetoothAdapter::CanPower() const {
  return IsPresent();
}

BluetoothAdapter::PermissionStatus BluetoothAdapter::GetOsPermissionStatus()
    const {
  // If this is not overridden that means OS permission is not
  // required on this platform so act as though we already have
  // permission.
  return PermissionStatus::kAllowed;
}

void BluetoothAdapter::RequestSystemPermission(
    BluetoothAdapter::RequestSystemPermissionCallback callback) {
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetOsPermissionStatus()));
}

void BluetoothAdapter::SetPowered(bool powered,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) {
  if (set_powered_callbacks_) {
    // Only allow one pending callback at a time.
    ui_task_runner_->PostTask(FROM_HERE, std::move(error_callback));
    return;
  }

  if (powered == IsPowered()) {
    // Return early in case no change of power state is needed.
    ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
    return;
  }

  if (!SetPoweredImpl(powered)) {
    ui_task_runner_->PostTask(FROM_HERE, std::move(error_callback));
    return;
  }

  set_powered_callbacks_ = std::make_unique<SetPoweredCallbacks>();
  set_powered_callbacks_->powered = powered;
  set_powered_callbacks_->callback = std::move(callback);
  set_powered_callbacks_->error_callback = std::move(error_callback);
}

bool BluetoothAdapter::IsPeripheralRoleSupported() const {
  // TODO(crbug.com/40685201): Implement this for more platforms.
  return true;
}

std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
BluetoothAdapter::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const BluetoothDiscoveryFilter& discovery_filter) {
  return std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>();
}

void BluetoothAdapter::StartDiscoverySession(const std::string& client_name,
                                             DiscoverySessionCallback callback,
                                             ErrorCallback error_callback) {
  StartDiscoverySessionWithFilter(nullptr, client_name, std::move(callback),
                                  std::move(error_callback));
}

void BluetoothAdapter::StartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const std::string& client_name,
    DiscoverySessionCallback callback,
    ErrorCallback error_callback) {
  if (!client_name.empty()) {
    BLUETOOTH_LOG(EVENT) << client_name
                         << " initiating Bluetooth discovery session";
  } else {
    BLUETOOTH_LOG(EVENT)
        << "Unknown client initiating Bluetooth discovery session";
  }

  std::unique_ptr<BluetoothDiscoverySession> new_session(
      new BluetoothDiscoverySession(this, std::move(discovery_filter)));
  discovery_sessions_.insert(new_session.get());

  auto new_session_callbacks = std::make_unique<StartOrStopDiscoveryCallback>(
      base::BindOnce(std::move(callback), std::move(new_session)),
      std::move(error_callback));

  // Queue up the callbacks to be handled when we process the discovery queue.
  discovery_callback_queue_.push(std::move(new_session_callbacks));

  // If OS is already working on a discovery request we must wait to process the
  // queue until that request is complete.
  if (discovery_request_pending_) {
    return;
  }

  // The OS is ready to start a request so process the queue now.
  ProcessDiscoveryQueue();
}

void BluetoothAdapter::MaybeUpdateFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  if (discovery_filter->Equals(current_discovery_filter_)) {
    std::move(callback).Run(/*is_error=*/false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
    return;
  }

  UpdateFilter(std::move(discovery_filter), std::move(callback));
}

void BluetoothAdapter::RemoveDiscoverySession(
    BluetoothDiscoverySession* discovery_session,
    base::OnceClosure callback,
    DiscoverySessionErrorCallback error_callback) {
  size_t erased = discovery_sessions_.erase(discovery_session);
  DCHECK_EQ(1u, erased);

  auto removal_callbacks = std::make_unique<StartOrStopDiscoveryCallback>(
      std::move(callback), std::move(error_callback));

  // Queue up the callbacks to be handled when we process the discovery queue.
  discovery_callback_queue_.push(std::move(removal_callbacks));

  // If OS is already working on a discovery request we must wait to process the
  // queue until that request is complete.
  if (discovery_request_pending_) {
    return;
  }

  // The OS is ready to start a request so process the queue now.
  ProcessDiscoveryQueue();
}

std::unique_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilter() const {
  auto result =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_DUAL);
  bool first_merge = true;

  for (BluetoothDiscoverySession* iter : discovery_sessions_) {
    if (!iter->IsActive())
      continue;

    const BluetoothDiscoveryFilter* curr_filter = iter->GetDiscoveryFilter();

    if (first_merge) {
      first_merge = false;
      if (curr_filter) {
        result->CopyFrom(*curr_filter);
      }
      continue;
    }
    result = BluetoothDiscoveryFilter::Merge(result.get(), curr_filter);
  }
  return result;
}

BluetoothAdapter::DeviceList BluetoothAdapter::GetDevices() {
  ConstDeviceList const_devices =
      const_cast<const BluetoothAdapter*>(this)->GetDevices();

  DeviceList devices;
  for (ConstDeviceList::const_iterator i = const_devices.begin();
       i != const_devices.end(); ++i)
    devices.push_back(const_cast<BluetoothDevice*>(i->get()));

  return devices;
}

BluetoothAdapter::ConstDeviceList BluetoothAdapter::GetDevices() const {
  ConstDeviceList devices;
  for (const auto& device : devices_)
    devices.push_back(device.second.get());

  return devices;
}

BluetoothDevice* BluetoothAdapter::GetDevice(const std::string& address) {
  return const_cast<BluetoothDevice*>(
      const_cast<const BluetoothAdapter*>(this)->GetDevice(address));
}

const BluetoothDevice* BluetoothAdapter::GetDevice(
    const std::string& address) const {
  std::string canonicalized_address = CanonicalizeBluetoothAddress(address);
  if (canonicalized_address.empty())
    return nullptr;

  auto iter = devices_.find(canonicalized_address);
  if (iter != devices_.end())
    return iter->second.get();

  return nullptr;
}

void BluetoothAdapter::AddPairingDelegate(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    PairingDelegatePriority priority) {
  // Remove the delegate, if it already exists, before inserting to allow a
  // change of priority.
  RemovePairingDelegate(pairing_delegate);

  // Find the first point with a lower priority, or the end of the list.
  auto iter = pairing_delegates_.begin();
  while (iter != pairing_delegates_.end() && iter->second >= priority)
    ++iter;

  pairing_delegates_.insert(iter, std::make_pair(pairing_delegate, priority));
}

void BluetoothAdapter::RemovePairingDelegate(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  for (auto iter = pairing_delegates_.begin(); iter != pairing_delegates_.end();
       ++iter) {
    if (iter->first == pairing_delegate) {
      RemovePairingDelegateInternal(pairing_delegate);
      pairing_delegates_.erase(iter);
      return;
    }
  }
}

BluetoothDevice::PairingDelegate* BluetoothAdapter::DefaultPairingDelegate() {
  if (pairing_delegates_.empty())
    return NULL;

  return pairing_delegates_.front().first;
}

// Default to assume the controller doesn't supports ext adv.
bool BluetoothAdapter::IsExtendedAdvertisementsAvailable() const {
  return false;
}

std::vector<BluetoothAdvertisement*>
BluetoothAdapter::GetPendingAdvertisementsForTesting() const {
  return {};
}

base::WeakPtr<BluetoothLocalGattService>
BluetoothAdapter::CreateLocalGattService(
    const BluetoothUUID& uuid,
    bool is_primary,
    BluetoothLocalGattService::Delegate* delegate) {
  return nullptr;
}

void BluetoothAdapter::NotifyAdapterPresentChanged(bool present) {
  BLUETOOTH_LOG(EVENT) << "Adapter " << (present ? "present" : "not present");
  for (auto& observer : observers_)
    observer.AdapterPresentChanged(this, present);
}

void BluetoothAdapter::NotifyAdapterPoweredChanged(bool powered) {
  BLUETOOTH_LOG(EVENT) << "Adapter powered " << (powered ? "on" : "off");
  for (auto& observer : observers_)
    observer.AdapterPoweredChanged(this, powered);
}

void BluetoothAdapter::NotifyDeviceChanged(BluetoothDevice* device) {
  DCHECK(device);
  DCHECK_EQ(device->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.DeviceChanged(this, device);
}

void BluetoothAdapter::NotifyAdapterDiscoveryChangeCompletedForTesting() {
  for (auto& observer : observers_)
    observer.DiscoveryChangeCompletedForTesting();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void BluetoothAdapter::NotifyDevicePairedChanged(BluetoothDevice* device,
                                                 bool new_paired_status) {
  for (auto& observer : observers_)
    observer.DevicePairedChanged(this, device, new_paired_status);
}

void BluetoothAdapter::NotifyDeviceConnectedStateChanged(
    BluetoothDevice* device,
    bool is_connected) {
  for (auto& observer : observers_) {
    observer.DeviceConnectedStateChanged(this, device, is_connected);
  }
}
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void BluetoothAdapter::NotifyDeviceBatteryChanged(
    BluetoothDevice* device,
    BluetoothDevice::BatteryType type) {
  DCHECK_EQ(device->GetAdapter(), this);

  for (auto& observer : observers_) {
    observer.DeviceBatteryChanged(this, device, type);
  }
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapter::NotifyDeviceBondedChanged(BluetoothDevice* device,
                                                 bool new_bonded_status) {
  for (auto& observer : observers_)
    observer.DeviceBondedChanged(this, device, new_bonded_status);
}

void BluetoothAdapter::NotifyDeviceIsBlockedByPolicyChanged(
    BluetoothDevice* device,
    bool new_blocked_status) {
  DCHECK_EQ(device->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.DeviceBlockedByPolicyChanged(this, device, new_blocked_status);
}

void BluetoothAdapter::NotifyGattNeedsDiscovery(BluetoothDevice* device) {
  for (auto& observer : observers_) {
    observer.GattNeedsDiscovery(device);
  }
}
#endif

void BluetoothAdapter::NotifyGattServiceAdded(
    BluetoothRemoteGattService* service) {
  DCHECK_EQ(service->GetDevice()->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.GattServiceAdded(this, service->GetDevice(), service);
}

void BluetoothAdapter::NotifyGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  DCHECK_EQ(service->GetDevice()->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.GattServiceRemoved(this, service->GetDevice(), service);
}

void BluetoothAdapter::NotifyGattServiceChanged(
    BluetoothRemoteGattService* service) {
  DCHECK_EQ(service->GetDevice()->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.GattServiceChanged(this, service);
}

int BluetoothAdapter::NumDiscoverySessions() const {
  return discovery_sessions_.size();
}

int BluetoothAdapter::NumScanningDiscoverySessions() const {
  int count = 0;
  for (BluetoothDiscoverySession* session : discovery_sessions_) {
    if (session->status() ==
        BluetoothDiscoverySession::SessionStatus::SCANNING) {
      ++count;
    }
  }

  return count;
}

void BluetoothAdapter::ClearAllDevices() {
  // Move all elements of the original devices list to a new list here,
  // leaving the original list empty so that when we send DeviceRemoved(),
  // GetDevices() returns no devices.
  DevicesMap devices_swapped;
  devices_swapped.swap(devices_);
  for (auto& iter : devices_swapped) {
    for (auto& observer : observers_) {
      observer.DeviceRemoved(this, iter.second.get());
    }
  }
}

void BluetoothAdapter::NotifyGattServicesDiscovered(BluetoothDevice* device) {
  DCHECK(device->GetAdapter() == this);

  for (auto& observer : observers_)
    observer.GattServicesDiscovered(this, device);
}

void BluetoothAdapter::NotifyGattDiscoveryComplete(
    BluetoothRemoteGattService* service) {
  DCHECK_EQ(service->GetDevice()->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.GattDiscoveryCompleteForService(this, service);
}

void BluetoothAdapter::NotifyGattCharacteristicAdded(
    BluetoothRemoteGattCharacteristic* characteristic) {
  DCHECK_EQ(characteristic->GetService()->GetDevice()->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.GattCharacteristicAdded(this, characteristic);
}

void BluetoothAdapter::NotifyGattCharacteristicRemoved(
    BluetoothRemoteGattCharacteristic* characteristic) {
  DCHECK_EQ(characteristic->GetService()->GetDevice()->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.GattCharacteristicRemoved(this, characteristic);
}

void BluetoothAdapter::NotifyGattDescriptorAdded(
    BluetoothRemoteGattDescriptor* descriptor) {
  DCHECK_EQ(
      descriptor->GetCharacteristic()->GetService()->GetDevice()->GetAdapter(),
      this);

  for (auto& observer : observers_)
    observer.GattDescriptorAdded(this, descriptor);
}

void BluetoothAdapter::NotifyGattDescriptorRemoved(
    BluetoothRemoteGattDescriptor* descriptor) {
  DCHECK_EQ(
      descriptor->GetCharacteristic()->GetService()->GetDevice()->GetAdapter(),
      this);

  for (auto& observer : observers_)
    observer.GattDescriptorRemoved(this, descriptor);
}

void BluetoothAdapter::NotifyGattCharacteristicValueChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  DCHECK_EQ(characteristic->GetService()->GetDevice()->GetAdapter(), this);

  base::WeakPtr<BluetoothRemoteGattCharacteristic> weak_characteristic =
      characteristic->GetWeakPtr();
  for (auto& observer : observers_) {
    if (!weak_characteristic)
      break;
    observer.GattCharacteristicValueChanged(this, characteristic, value);
  }
}

void BluetoothAdapter::NotifyGattDescriptorValueChanged(
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  DCHECK_EQ(
      descriptor->GetCharacteristic()->GetService()->GetDevice()->GetAdapter(),
      this);

  for (auto& observer : observers_)
    observer.GattDescriptorValueChanged(this, descriptor, value);
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapter::
    NotifyLowEnergyScanSessionHardwareOffloadingStatusChanged(
        LowEnergyScanSessionHardwareOffloadingStatus status) {
  for (auto& observer : observers_)
    observer.LowEnergyScanSessionHardwareOffloadingStatusChanged(status);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

BluetoothAdapter::SetPoweredCallbacks::SetPoweredCallbacks() = default;
BluetoothAdapter::SetPoweredCallbacks::~SetPoweredCallbacks() = default;

BluetoothAdapter::StartOrStopDiscoveryCallback::StartOrStopDiscoveryCallback(
    base::OnceClosure start_callback,
    ErrorCallback start_error_callback) {
  this->start_callback = std::move(start_callback);
  this->start_error_callback = std::move(start_error_callback);
}
BluetoothAdapter::StartOrStopDiscoveryCallback::StartOrStopDiscoveryCallback(
    base::OnceClosure stop_callback,
    DiscoverySessionErrorCallback stop_error_callback) {
  this->stop_callback = std::move(stop_callback);
  this->stop_error_callback = std::move(stop_error_callback);
}
BluetoothAdapter::StartOrStopDiscoveryCallback::
    ~StartOrStopDiscoveryCallback() = default;

BluetoothAdapter::BluetoothAdapter() {}

BluetoothAdapter::~BluetoothAdapter() {
  // If there's a pending powered request, run its error callback.
  if (set_powered_callbacks_)
    std::move(set_powered_callbacks_->error_callback).Run();
}

void BluetoothAdapter::RunPendingPowerCallbacks() {
  if (set_powered_callbacks_) {
    // Move into a local variable to clear out both callbacks at the end of the
    // scope and to allow scheduling another SetPowered() call in either of the
    // callbacks.
    auto callbacks = std::move(set_powered_callbacks_);
    callbacks->powered == IsPowered()
        ? std::move(callbacks->callback).Run()
        : std::move(callbacks->error_callback).Run();
  }
}

void BluetoothAdapter::OnDiscoveryChangeComplete(
    bool is_error,
    UMABluetoothDiscoverySessionOutcome outcome) {
  UpdateDiscoveryState(is_error);

  // Take a weak reference to |this| in case a callback frees the adapter.
  base::WeakPtr<BluetoothAdapter> self = GetWeakPtr();

  if (is_error) {
    NotifyDiscoveryError(std::move(callbacks_awaiting_response_));

    if (!self)
      return;

    discovery_request_pending_ = false;
    NotifyAdapterDiscoveryChangeCompletedForTesting();
    ProcessDiscoveryQueue();

    return;
  }

  // Inform BluetoothDiscoverySession that updates being processed have
  // completed.
  for (BluetoothDiscoverySession* session : discovery_sessions_) {
    session->StartingSessionsScanning();
  }

  current_discovery_filter_.CopyFrom(filter_being_set_);

  auto callbacks_awaiting_response = std::move(callbacks_awaiting_response_);
  while (!callbacks_awaiting_response.empty()) {
    std::unique_ptr<StartOrStopDiscoveryCallback> callbacks =
        std::move(callbacks_awaiting_response.front());
    callbacks_awaiting_response.pop();
    if (callbacks->start_callback)
      std::move(callbacks->start_callback).Run();
    if (callbacks->stop_callback)
      std::move(callbacks->stop_callback).Run();
  }

  if (!self)
    return;

  discovery_request_pending_ = false;
  NotifyAdapterDiscoveryChangeCompletedForTesting();
  ProcessDiscoveryQueue();
}

void BluetoothAdapter::UpdateDiscoveryState(bool is_error) {
  if (is_error) {
    if (internal_discovery_state_ == DiscoveryState::kStarting)
      internal_discovery_state_ = DiscoveryState::kIdle;
    // If there was an error stopping we still assume it worked as there is not
    // much we can do about the device messing up.
    if (internal_discovery_state_ == DiscoveryState::kStopping)
      internal_discovery_state_ = DiscoveryState::kIdle;
    return;
  }

  if (internal_discovery_state_ == DiscoveryState::kStarting)
    internal_discovery_state_ = DiscoveryState::kDiscovering;
  if (internal_discovery_state_ == DiscoveryState::kStopping)
    internal_discovery_state_ = DiscoveryState::kIdle;
}

void BluetoothAdapter::ProcessDiscoveryQueue() {
  if (discovery_callback_queue_.empty())
    return;
  DCHECK(callbacks_awaiting_response_.empty());
  callbacks_awaiting_response_.swap(discovery_callback_queue_);

  if (NumDiscoverySessions() == 0) {
    if (internal_discovery_state_ == DiscoveryState::kIdle) {
      OnDiscoveryChangeComplete(false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS);
      return;
    }
    internal_discovery_state_ = DiscoveryState::kStopping;
    discovery_request_pending_ = true;
    StopScan(base::BindOnce(&BluetoothAdapter::OnDiscoveryChangeComplete,
                            GetWeakPtr()));

    return;
  }

  // Inform BluetoothDiscoverySession that any updates they have made are being
  // processed.
  for (BluetoothDiscoverySession* session : discovery_sessions_) {
    session->PendingSessionsStarting();
  }

  auto result_callback = base::BindOnce(
      &BluetoothAdapter::OnDiscoveryChangeComplete, GetWeakPtr());
  auto new_desired_filter = GetMergedDiscoveryFilter();
  discovery_request_pending_ = true;
  filter_being_set_.CopyFrom(*new_desired_filter.get());
  if (internal_discovery_state_ == DiscoveryState::kDiscovering) {
    MaybeUpdateFilter(std::move(new_desired_filter),
                      std::move(result_callback));
    return;
  }
  internal_discovery_state_ = DiscoveryState::kStarting;
  StartScanWithFilter(std::move(new_desired_filter),
                      std::move(result_callback));
}

void BluetoothAdapter::NotifyDiscoveryError(CallbackQueue callback_queue) {
  while (!callback_queue.empty()) {
    std::unique_ptr<StartOrStopDiscoveryCallback> callbacks =
        std::move(callback_queue.front());
    callback_queue.pop();
    if (callbacks->start_error_callback)
      std::move(callbacks->start_error_callback).Run();
    // We never return error when stopping. If the physical adapter is messing
    // up and not stopping we are still just going to continue like it did stop.
    if (callbacks->stop_callback)
      std::move(callbacks->stop_callback).Run();
  }
}

void BluetoothAdapter::MarkDiscoverySessionsAsInactive() {
  // All sessions are becoming inactive so any pending requests should now fail
  if (!discovery_callback_queue_.empty())
    NotifyDiscoveryError(std::move(discovery_callback_queue_));
  // As sessions are marked as inactive they will notify the adapter that they
  // have become inactive, upon which the adapter will remove them from
  // |discovery_sessions_|. To avoid invalidating the iterator, make a copy
  // here.
  std::set<raw_ptr<BluetoothDiscoverySession, SetExperimental>> temp(
      discovery_sessions_);
  for (auto iter = temp.begin(); iter != temp.end(); ++iter) {
    (*iter)->MarkAsInactive();
    RemoveDiscoverySession(*iter, base::DoNothing(), base::DoNothing());
  }
}

void BluetoothAdapter::DeleteDeviceForTesting(const std::string& address) {
  devices_.erase(address);
}

void BluetoothAdapter::RemoveTimedOutDevices() {
  for (auto it = devices_.begin(); it != devices_.end();) {
    BluetoothDevice* device = it->second.get();
    if (device->IsPaired() || device->IsConnected() ||
        device->IsGattConnected()) {
      ++it;
      continue;
    }

    base::Time last_update_time = device->GetLastUpdateTime();

    bool device_expired =
        (base::Time::NowFromSystemTime() - last_update_time) > timeoutSec;
    DVLOG(3) << "device: " << device->GetAddress()
             << ", last_update: " << last_update_time
             << ", exp: " << device_expired;

    if (!device_expired) {
      ++it;
      continue;
    }

    DVLOG(1) << "Removing device: " << device->GetAddress();
#if BUILDFLAG(IS_MAC)
    if (!device->IsLowEnergyDevice()) {
      BLUETOOTH_LOG(EVENT) << "Classic device removed: "
                           << device->GetAddress();
    }
#endif  // BUILDFLAG(IS_MAC)
    auto next = it;
    next++;
    std::unique_ptr<BluetoothDevice> removed_device = std::move(it->second);
    devices_.erase(it);
    it = next;

    for (auto& observer : observers_)
      observer.DeviceRemoved(this, removed_device.get());
  }
}

// static
const base::TimeDelta BluetoothAdapter::timeoutSec = base::Seconds(180);

}  // namespace device
