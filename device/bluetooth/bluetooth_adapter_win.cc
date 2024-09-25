// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_winrt.h"
#include "device/bluetooth/bluetooth_classic_win.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return BluetoothAdapterWin::CreateAdapter();
}

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapterWin::CreateAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterWinrt());
}

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapterWin::CreateClassicAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterWin());
}

BluetoothAdapterWin::BluetoothAdapterWin() = default;

BluetoothAdapterWin::~BluetoothAdapterWin() {
  if (task_manager_.get())
    task_manager_->RemoveObserver(this);
}

std::string BluetoothAdapterWin::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterWin::GetName() const {
  return name_;
}

void BluetoothAdapterWin::SetName(const std::string& name,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

// TODO(youngki): Return true when |task_manager_| initializes the adapter
// state.
bool BluetoothAdapterWin::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterWin::IsPresent() const {
  return !address_.empty();
}

bool BluetoothAdapterWin::IsPowered() const {
  return powered_;
}

void BluetoothAdapterWin::SetPowered(bool powered,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) {
  task_manager_->PostSetPoweredBluetoothTask(powered, std::move(callback),
                                             std::move(error_callback));
}

bool BluetoothAdapterWin::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetDiscoverable(bool discoverable,
                                          base::OnceClosure callback,
                                          ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsDiscovering() const {
  return discovery_status_ == DISCOVERING ||
      discovery_status_ == DISCOVERY_STOPPING;
}

void BluetoothAdapterWin::DiscoveryStarted(bool success) {
  discovery_status_ = success ? DISCOVERING : NOT_DISCOVERING;
  std::move(discovery_changed_callback_)
      .Run(/*is_error=*/!success, UMABluetoothDiscoverySessionOutcome::UNKNOWN);

  if (success) {
    for (auto& observer : observers_)
      observer.AdapterDiscoveringChanged(this, true);
  }
}

void BluetoothAdapterWin::DiscoveryStopped() {
  discovered_devices_.clear();
  bool was_discovering = IsDiscovering();
  discovery_status_ = NOT_DISCOVERING;
  std::move(discovery_changed_callback_)
      .Run(/*is_error=*/false, UMABluetoothDiscoverySessionOutcome::SUCCESS);
  if (was_discovering)
    for (auto& observer : observers_)
      observer.AdapterDiscoveringChanged(this, false);
}

BluetoothAdapter::UUIDList BluetoothAdapterWin::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterWin::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  scoped_refptr<BluetoothSocketWin> socket =
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_);
  socket->Listen(this, uuid, options,
                 base::BindOnce(std::move(callback), socket),
                 std::move(error_callback));
}

void BluetoothAdapterWin::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  // TODO(keybuk): implement.
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED();
  std::move(error_callback)
      .Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterWin::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

void BluetoothAdapterWin::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
}

void BluetoothAdapterWin::AdapterStateChanged(
    const BluetoothTaskManagerWin::AdapterState& state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  name_ = state.name;
  bool was_present = IsPresent();
  bool is_present = !state.address.empty();
  address_ = CanonicalizeBluetoothAddress(state.address);
  if (was_present != is_present) {
    for (auto& observer : observers_)
      observer.AdapterPresentChanged(this, is_present);
  }
  if (powered_ != state.powered) {
    powered_ = state.powered;
    for (auto& observer : observers_)
      observer.AdapterPoweredChanged(this, powered_);
  }
  if (!initialized_) {
    initialized_ = true;
    std::move(init_callback_).Run();
  }

  // When the Bluetooth adapter is powered off or not present, all Bluetooth
  // devices should be removed.
  if (!powered_ || !is_present) {
    ClearAllDevices();
  }
}

void BluetoothAdapterWin::DevicesPolled(
    const std::vector<std::unique_ptr<BluetoothTaskManagerWin::DeviceState>>&
        devices) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // We are receiving a new list of all devices known to the system. Merge this
  // new list with the list we know of (|devices_|) and raise corresponding
  // DeviceAdded, DeviceRemoved and DeviceChanged events.

  using DeviceAddressSet = std::set<std::string>;
  DeviceAddressSet known_devices;
  for (const auto& device : devices_)
    known_devices.insert(device.first);

  DeviceAddressSet new_devices;
  for (const auto& device_state : devices)
    new_devices.insert(device_state->address);

  // Process device removal first.
  DeviceAddressSet removed_devices =
      base::STLSetDifference<DeviceAddressSet>(known_devices, new_devices);
  for (const auto& device : removed_devices) {
    auto it = devices_.find(device);
    std::unique_ptr<BluetoothDevice> device_win = std::move(it->second);
    devices_.erase(it);
    for (auto& observer : observers_)
      observer.DeviceRemoved(this, device_win.get());
  }

  // Process added and (maybe) changed devices in one pass.
  DeviceAddressSet added_devices =
      base::STLSetDifference<DeviceAddressSet>(new_devices, known_devices);
  DeviceAddressSet changed_devices =
      base::STLSetIntersection<DeviceAddressSet>(known_devices, new_devices);
  for (const auto& device_state : devices) {
    if (base::Contains(added_devices, device_state->address)) {
      auto device_win = std::make_unique<BluetoothDeviceWin>(
          this, *device_state, ui_task_runner_, socket_thread_);
      BluetoothDeviceWin* device_win_raw = device_win.get();
      devices_[device_state->address] = std::move(device_win);
      for (auto& observer : observers_)
        observer.DeviceAdded(this, device_win_raw);
    } else if (base::Contains(changed_devices, device_state->address)) {
      auto iter = devices_.find(device_state->address);
      CHECK(iter != devices_.end(), base::NotFatalUntil::M130);
      BluetoothDeviceWin* device_win =
          static_cast<BluetoothDeviceWin*>(iter->second.get());
      if (!device_win->IsEqual(*device_state)) {
        device_win->Update(*device_state);
        for (auto& observer : observers_)
          observer.DeviceChanged(this, device_win);
      }
      // Above IsEqual returns true if device name, address, status and services
      // (primary services of BLE device) are the same. However, in BLE tests,
      // we may simulate characteristic, descriptor and secondary GATT service
      // after device has been initialized.
      if (force_update_device_for_test_) {
        device_win->Update(*device_state);
      }
    }
  }
}

base::WeakPtr<BluetoothAdapter> BluetoothAdapterWin::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// BluetoothAdapterWin should override SetPowered() instead.
bool BluetoothAdapterWin::SetPoweredImpl(bool powered) {
  NOTREACHED();
}

void BluetoothAdapterWin::UpdateFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  DCHECK(discovery_status_ == DISCOVERING ||
         discovery_status_ == DISCOVERY_STARTING);
  if (discovery_status_ == DISCOVERING) {
    std::move(callback).Run(false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
    return;
  }
}

void BluetoothAdapterWin::StartScanWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  discovery_changed_callback_ = std::move(callback);
  MaybePostStartDiscoveryTask();
}

void BluetoothAdapterWin::StopScan(DiscoverySessionResultCallback callback) {
  if (discovery_status_ == NOT_DISCOVERING) {
    std::move(callback).Run(/*is_error=*/true,
                            UMABluetoothDiscoverySessionOutcome::NOT_ACTIVE);
    return;
  }
  discovery_changed_callback_ = std::move(callback);
  MaybePostStopDiscoveryTask();
}

void BluetoothAdapterWin::Initialize(base::OnceClosure init_callback) {
  init_callback_ = std::move(init_callback);
  ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  socket_thread_ = BluetoothSocketThread::Get();
  task_manager_ =
      base::MakeRefCounted<BluetoothTaskManagerWin>(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->Initialize();
}

void BluetoothAdapterWin::InitForTest(
    base::OnceClosure init_callback,
    std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner) {
  init_callback_ = std::move(init_callback);
  ui_task_runner_ = ui_task_runner;
  if (!ui_task_runner_)
    ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  task_manager_ = BluetoothTaskManagerWin::CreateForTesting(
      std::move(classic_wrapper), ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->InitializeWithBluetoothTaskRunner(bluetooth_task_runner);
}

void BluetoothAdapterWin::MaybePostStartDiscoveryTask() {
  if (discovery_status_ == NOT_DISCOVERING) {
    discovery_status_ = DISCOVERY_STARTING;
    task_manager_->PostStartDiscoveryTask();
  }
}

void BluetoothAdapterWin::MaybePostStopDiscoveryTask() {
  if (discovery_status_ != DISCOVERING)
    return;

  discovery_status_ = DISCOVERY_STOPPING;
  task_manager_->PostStopDiscoveryTask();
}

}  // namespace device
