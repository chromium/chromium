// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace device {

BluetoothAdapter::ServiceOptions::ServiceOptions() = default;
BluetoothAdapter::ServiceOptions::~ServiceOptions() = default;

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && !defined(OS_MACOSX) && \
    !defined(OS_WIN) && !defined(OS_LINUX)
// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    InitCallback init_callback) {
  return base::WeakPtr<BluetoothAdapter>();
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_WIN) && !defined(OS_MACOSX)

base::WeakPtr<BluetoothAdapter> BluetoothAdapter::GetWeakPtrForTesting() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if defined(OS_LINUX)
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

bool BluetoothAdapter::HasObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  return observers_.HasObserver(observer);
}

bool BluetoothAdapter::CanPower() const {
  return IsPresent();
}

void BluetoothAdapter::SetPowered(bool powered,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  if (set_powered_callbacks_) {
    // Only allow one pending callback at a time.
    ui_task_runner_->PostTask(FROM_HERE, error_callback);
    return;
  }

  if (powered == IsPowered()) {
    // Return early in case no change of power state is needed.
    ui_task_runner_->PostTask(FROM_HERE, callback);
    return;
  }

  if (!SetPoweredImpl(powered)) {
    ui_task_runner_->PostTask(FROM_HERE, error_callback);
    return;
  }

  set_powered_callbacks_ = std::make_unique<SetPoweredCallbacks>();
  set_powered_callbacks_->powered = powered;
  set_powered_callbacks_->callback = callback;
  set_powered_callbacks_->error_callback = error_callback;
}

std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
BluetoothAdapter::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const BluetoothDiscoveryFilter& discovery_filter) {
  return std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>();
}

void BluetoothAdapter::StartDiscoverySession(
    const DiscoverySessionCallback& callback,
    const ErrorCallback& error_callback) {
  StartDiscoverySessionWithFilter(nullptr, callback, error_callback);
}

void BluetoothAdapter::StartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const DiscoverySessionCallback& callback,
    const ErrorCallback& error_callback) {
  BluetoothDiscoveryFilter* ptr = discovery_filter.get();
  AddDiscoverySession(
      ptr, base::Bind(&BluetoothAdapter::OnStartDiscoverySession,
                      weak_ptr_factory_.GetWeakPtr(),
                      base::Passed(&discovery_filter), callback),
      base::Bind(&BluetoothAdapter::OnStartDiscoverySessionError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

std::unique_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilter() const {
  return GetMergedDiscoveryFilterHelper(nullptr, false);
}

std::unique_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilterMasked(
    BluetoothDiscoveryFilter* masked_filter) const {
  return GetMergedDiscoveryFilterHelper(masked_filter, true);
}

BluetoothAdapter::DeviceList BluetoothAdapter::GetDevices() {
  ConstDeviceList const_devices =
    const_cast<const BluetoothAdapter *>(this)->GetDevices();

  DeviceList devices;
  for (ConstDeviceList::const_iterator i = const_devices.begin();
       i != const_devices.end(); ++i)
    devices.push_back(const_cast<BluetoothDevice *>(*i));

  return devices;
}

BluetoothAdapter::ConstDeviceList BluetoothAdapter::GetDevices() const {
  ConstDeviceList devices;
  for (const auto& device : devices_)
    devices.push_back(device.second.get());

  return devices;
}

BluetoothDevice* BluetoothAdapter::GetDevice(const std::string& address) {
  return const_cast<BluetoothDevice *>(
      const_cast<const BluetoothAdapter *>(this)->GetDevice(address));
}

const BluetoothDevice* BluetoothAdapter::GetDevice(
    const std::string& address) const {
  std::string canonicalized_address =
      BluetoothDevice::CanonicalizeAddress(address);
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

std::vector<BluetoothAdvertisement*>
BluetoothAdapter::GetPendingAdvertisementsForTesting() const {
  return {};
}

void BluetoothAdapter::NotifyAdapterPoweredChanged(bool powered) {
  for (auto& observer : observers_)
    observer.AdapterPoweredChanged(this, powered);
}

void BluetoothAdapter::NotifyDeviceChanged(BluetoothDevice* device) {
  DCHECK(device);
  DCHECK_EQ(device->GetAdapter(), this);

  for (auto& observer : observers_)
    observer.DeviceChanged(this, device);
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
void BluetoothAdapter::NotifyDevicePairedChanged(BluetoothDevice* device,
                                                 bool new_paired_status) {
  for (auto& observer : observers_)
    observer.DevicePairedChanged(this, device, new_paired_status);
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

  for (auto& observer : observers_)
    observer.GattCharacteristicValueChanged(this, characteristic, value);
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

BluetoothAdapter::SetPoweredCallbacks::SetPoweredCallbacks() = default;
BluetoothAdapter::SetPoweredCallbacks::~SetPoweredCallbacks() = default;

BluetoothAdapter::BluetoothAdapter() : weak_ptr_factory_(this) {}

BluetoothAdapter::~BluetoothAdapter() {
  // If there's a pending powered request, run its error callback.
  if (set_powered_callbacks_) {
    set_powered_callbacks_->error_callback.Run();
  }
}

void BluetoothAdapter::RunPendingPowerCallbacks() {
  if (set_powered_callbacks_) {
    // Move into a local variable to clear out both callbacks at the end of the
    // scope and to allow scheduling another SetPowered() call in either of the
    // callbacks.
    auto callbacks = std::move(set_powered_callbacks_);
    callbacks->powered == IsPowered() ? std::move(callbacks->callback).Run()
                                      : callbacks->error_callback.Run();
  }
}

void BluetoothAdapter::OnStartDiscoverySession(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const DiscoverySessionCallback& callback) {
  VLOG(1) << "BluetoothAdapter::OnStartDiscoverySession";
  RecordBluetoothDiscoverySessionStartOutcome(
      UMABluetoothDiscoverySessionOutcome::SUCCESS);

  std::unique_ptr<BluetoothDiscoverySession> discovery_session(
      new BluetoothDiscoverySession(scoped_refptr<BluetoothAdapter>(this),
                                    std::move(discovery_filter)));
  discovery_sessions_.insert(discovery_session.get());
  callback.Run(std::move(discovery_session));
}

void BluetoothAdapter::OnStartDiscoverySessionError(
    const ErrorCallback& callback,
    UMABluetoothDiscoverySessionOutcome outcome) {
  VLOG(1) << "OnStartDiscoverySessionError: " << static_cast<int>(outcome);
  RecordBluetoothDiscoverySessionStartOutcome(outcome);
  callback.Run();
}

void BluetoothAdapter::MarkDiscoverySessionsAsInactive() {
  // As sessions are marked as inactive they will notify the adapter that they
  // have become inactive, upon which the adapter will remove them from
  // |discovery_sessions_|. To avoid invalidating the iterator, make a copy
  // here.
  std::set<BluetoothDiscoverySession*> temp(discovery_sessions_);
  for (auto iter = temp.begin(); iter != temp.end(); ++iter) {
    (*iter)->MarkAsInactive();
  }
}

void BluetoothAdapter::DiscoverySessionBecameInactive(
    BluetoothDiscoverySession* discovery_session) {
  DCHECK(!discovery_session->IsActive());
  discovery_sessions_.erase(discovery_session);
}

void BluetoothAdapter::DeleteDeviceForTesting(const std::string& address) {
  devices_.erase(address);
}

std::unique_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilterHelper(
    const BluetoothDiscoveryFilter* masked_filter,
    bool omit) const {
  std::unique_ptr<BluetoothDiscoveryFilter> result;
  bool first_merge = true;

  std::set<BluetoothDiscoverySession*> temp(discovery_sessions_);
  for (auto* iter : temp) {
    const BluetoothDiscoveryFilter* curr_filter = iter->GetDiscoveryFilter();

    if (!iter->IsActive())
      continue;

    if (omit && curr_filter == masked_filter) {
      // if masked_filter is pointing to empty filter, and there are
      // multiple empty filters in discovery_sessions_, make sure we'll
      // process next empty sessions.
      omit = false;
      continue;
    }

    if (first_merge) {
      first_merge = false;
      if (curr_filter) {
        result.reset(new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_DUAL));
        result->CopyFrom(*curr_filter);
      }
      continue;
    }

    result = BluetoothDiscoveryFilter::Merge(result.get(), curr_filter);
  }

  return result;
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
    VLOG(3) << "device: " << device->GetAddress()
            << ", last_update: " << last_update_time
            << ", exp: " << device_expired;

    if (!device_expired) {
      ++it;
      continue;
    }

    VLOG(1) << "Removing device: " << device->GetAddress();
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
void BluetoothAdapter::RecordBluetoothDiscoverySessionStartOutcome(
    UMABluetoothDiscoverySessionOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.DiscoverySession.Start.Outcome", static_cast<int>(outcome),
      static_cast<int>(UMABluetoothDiscoverySessionOutcome::COUNT));
}

// static
void BluetoothAdapter::RecordBluetoothDiscoverySessionStopOutcome(
    UMABluetoothDiscoverySessionOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.DiscoverySession.Stop.Outcome", static_cast<int>(outcome),
      static_cast<int>(UMABluetoothDiscoverySessionOutcome::COUNT));
}

// static
constexpr base::TimeDelta BluetoothAdapter::timeoutSec =
    base::TimeDelta::FromSeconds(180);

}  // namespace device
