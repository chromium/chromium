// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#include <IOKit/IOKitLib.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_advertisement_mac.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_socket_mac.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

extern "C" {
// Undocumented IOBluetooth Preference API [1]. Used by `blueutil` [2] and
// `Karabiner` [3] to programmatically control the Bluetooth state. Calling the
// method with `1` powers the adapter on, calling it with `0` powers it off.
// Using this API has the same effect as turning Bluetooth on or off using the
// macOS System Preferences [4], and will effect all adapters.
//
// [1] https://goo.gl/Gbjm1x
// [2] http://www.frederikseiffert.de/blueutil/
// [3] https://pqrs.org/osx/karabiner/
// [4] https://support.apple.com/kb/PH25091
void IOBluetoothPreferenceSetControllerPowerState(int state);
}

namespace {

// The frequency with which to poll the adapter for updates.
const int kPollIntervalMs = 500;

bool IsDeviceSystemPaired(const std::string& device_address) {
  IOBluetoothDevice* device = [IOBluetoothDevice
      deviceWithAddressString:base::SysUTF8ToNSString(device_address)];
  return device && [device isPaired];
}

}  // namespace

namespace device {

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return BluetoothAdapterMac::CreateAdapter();
}

// static
scoped_refptr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterMac());
}

// static
scoped_refptr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapterForTest(
    std::string name,
    std::string address,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  auto adapter = base::WrapRefCounted(new BluetoothAdapterMac());
  adapter->InitForTest(ui_task_runner);
  adapter->name_ = name;
  adapter->should_update_name_ = false;
  adapter->address_ = address;
  return adapter;
}

BluetoothAdapterMac::BluetoothAdapterMac()
    : controller_state_function_(
          base::BindRepeating(&BluetoothAdapterMac::GetHostControllerState,
                              base::Unretained(this))),
      power_state_function_(
          base::BindRepeating(IOBluetoothPreferenceSetControllerPowerState)),
      classic_discovery_manager_(
          BluetoothDiscoveryManagerMac::CreateClassic(this)),
      device_paired_status_callback_(
          base::BindRepeating(&IsDeviceSystemPaired)) {
  DCHECK(classic_discovery_manager_);
}

BluetoothAdapterMac::~BluetoothAdapterMac() = default;

std::string BluetoothAdapterMac::GetAddress() const {
  const_cast<BluetoothAdapterMac*>(this)->LazyInitialize();
  return address_;
}

std::string BluetoothAdapterMac::GetName() const {
  if (!should_update_name_) {
    return name_;
  }

  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];
  name_ = controller != nil ? base::SysNSStringToUTF8([controller nameAsString])
                            : std::string();
  should_update_name_ = name_.empty();
  return name_;
}

void BluetoothAdapterMac::SetName(const std::string& name,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsPresent() const {
  // Avoid calling LazyInitialize() so that a Bluetooth permission prompt
  // doesn't appear when simply trying to detect whether the system supports
  // Bluetooth.

  if (is_present_for_testing_.has_value())
    return is_present_for_testing_.value();

  base::mac::ScopedIOObject<io_iterator_t> iterator;
  IOReturn result = IOServiceGetMatchingServices(
      kIOMasterPortDefault, IOServiceMatching("IOBluetoothHCIController"),
      iterator.InitializeInto());
  if (result != kIOReturnSuccess) {
    BLUETOOTH_LOG(ERROR) << "Failed to enumerate Bluetooth controller: "
                         << std::hex << result << ".";
    return false;
  }

  base::mac::ScopedIOObject<io_service_t> service(IOIteratorNext(iterator));
  if (!service) {
    return false;
  }

  base::ScopedCFTypeRef<CFBooleanRef> connected(
      base::mac::CFCast<CFBooleanRef>(IORegistryEntryCreateCFProperty(
          service, CFSTR("BluetoothTransportConnected"), kCFAllocatorDefault,
          0)));
  return CFBooleanGetValue(connected);
}

BluetoothAdapter::PermissionStatus BluetoothAdapterMac::GetOsPermissionStatus()
    const {
  if (@available(macOS 10.15.0, *)) {
    switch (CBCentralManager.authorization) {
      case CBManagerAuthorizationNotDetermined:
        return PermissionStatus::kUndetermined;
      case CBManagerAuthorizationRestricted:
      case CBManagerAuthorizationDenied:
        return PermissionStatus::kDenied;
      case CBManagerAuthorizationAllowedAlways:
        return PermissionStatus::kAllowed;
    }
  }

  // There are no Core Bluetooth permissions before macOS 10.15 so assume we
  // always have permission.
  return PermissionStatus::kAllowed;
}

bool BluetoothAdapterMac::IsPowered() const {
  const_cast<BluetoothAdapterMac*>(this)->LazyInitialize();
  return classic_powered_ || IsLowEnergyPowered();
}

// TODO(krstnmnlsn): If this information is retrievable form IOBluetooth we
// should return the discoverable status.
bool BluetoothAdapterMac::IsDiscoverable() const {
  return false;
}

void BluetoothAdapterMac::SetDiscoverable(bool discoverable,
                                          base::OnceClosure callback,
                                          ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsDiscovering() const {
  return classic_discovery_manager_->IsDiscovering() ||
         BluetoothLowEnergyAdapterApple::IsDiscovering();
}

void BluetoothAdapterMac::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  LazyInitialize();
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingRfcomm(this, uuid, options,
                            base::BindOnce(std::move(callback), socket),
                            std::move(error_callback));
}

void BluetoothAdapterMac::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  LazyInitialize();
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingL2cap(this, uuid, options,
                           base::BindOnce(std::move(callback), socket),
                           std::move(error_callback));
}

void BluetoothAdapterMac::ClassicDeviceFound(IOBluetoothDevice* device) {
  ClassicDeviceAdded(device);
}

void BluetoothAdapterMac::ClassicDiscoveryStopped(bool unexpected) {
  if (unexpected) {
    DVLOG(1) << "Discovery stopped unexpectedly";
    MarkDiscoverySessionsAsInactive();
  }
  for (auto& observer : observers_)
    observer.AdapterDiscoveringChanged(this, false);
}

void BluetoothAdapterMac::DeviceConnected(IOBluetoothDevice* device) {
  // TODO(isherman): Investigate whether this method can be replaced with a call
  // to +registerForConnectNotifications:selector:.
  DVLOG(1) << "Adapter registered a new connection from device with address: "
           << BluetoothClassicDeviceMac::GetDeviceAddress(device);
  ClassicDeviceAdded(device);
}

base::WeakPtr<BluetoothAdapter> BluetoothAdapterMac::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothAdapterMac::SetPoweredImpl(bool powered) {
  power_state_function_.Run(base::strict_cast<int>(powered));
  return true;
}

base::WeakPtr<BluetoothLowEnergyAdapterApple>
BluetoothAdapterMac::GetLowEnergyWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BluetoothAdapterMac::LazyInitialize() {
  if (lazy_initialized_)
    return;

  BluetoothLowEnergyAdapterApple::LazyInitialize();
  PollAdapter();
}

void BluetoothAdapterMac::InitForTest(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  BluetoothLowEnergyAdapterApple::InitForTest(ui_task_runner);
  is_present_for_testing_ = false;
}

BluetoothLowEnergyAdapterApple::GetDevicePairedStatusCallback
BluetoothAdapterMac::GetDevicePariedStatus() const {
  return device_paired_status_callback_;
}

BluetoothAdapterMac::HostControllerState
BluetoothAdapterMac::GetHostControllerState() {
  HostControllerState state;
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];
  if (controller != nil) {
    state.classic_powered =
        ([controller powerState] == kBluetoothHCIPowerStateON);
    state.address = CanonicalizeBluetoothAddress(
        base::SysNSStringToUTF8([controller addressAsString]));
    state.is_present = !state.address.empty();
  }
  return state;
}

void BluetoothAdapterMac::SetPresentForTesting(bool present) {
  is_present_for_testing_ = present;
}

void BluetoothAdapterMac::SetHostControllerStateFunctionForTesting(
    HostControllerStateFunction controller_state_function) {
  controller_state_function_ = std::move(controller_state_function);
}

void BluetoothAdapterMac::SetPowerStateFunctionForTesting(
    SetControllerPowerStateFunction power_state_function) {
  power_state_function_ = std::move(power_state_function);
}

void BluetoothAdapterMac::SetGetDevicePairedStatusCallbackForTesting(
    BluetoothLowEnergyAdapterApple::GetDevicePairedStatusCallback
        device_paired_status_callback) {
  device_paired_status_callback_ = std::move(device_paired_status_callback);
}

void BluetoothAdapterMac::StartScanWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // Default to dual discovery if |discovery_filter| is NULL.  IOBluetooth seems
  // to allow starting low energy and classic discovery at once.
  BluetoothTransport transport = BLUETOOTH_TRANSPORT_DUAL;
  if (discovery_filter) {
    transport = discovery_filter->GetTransport();
  }

  if ((transport & BLUETOOTH_TRANSPORT_CLASSIC) &&
      !classic_discovery_manager_->IsDiscovering()) {
    // We do not update the filter if already discovering.  This will all be
    // deprecated soon though.
    if (!classic_discovery_manager_->StartDiscovery()) {
      DVLOG(1) << "Failed to add a classic discovery session";
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), /*is_error=*/true,
                         UMABluetoothDiscoverySessionOutcome::UNKNOWN));
      return;
    }
  }

  if (transport & BLUETOOTH_TRANSPORT_LE) {
    StartScanLowEnergy();
  }
  for (auto& observer : observers_)
    observer.AdapterDiscoveringChanged(this, true);
  DCHECK(callback);
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*is_error=*/false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void BluetoothAdapterMac::StopScan(DiscoverySessionResultCallback callback) {
  StopScanLowEnergy();

  if (classic_discovery_manager_->IsDiscovering() &&
      !classic_discovery_manager_->StopDiscovery()) {
    DVLOG(1) << "Failed to stop classic discovery";
    // TODO: Provide a more precise error here.
    std::move(callback).Run(/*is_error=*/true,
                            UMABluetoothDiscoverySessionOutcome::UNKNOWN);
    return;
  }

  DVLOG(1) << "Discovery stopped";
  std::move(callback).Run(/*is_error=*/false,
                          UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

void BluetoothAdapterMac::PollAdapter() {
  const bool was_present = IsPresent();
  HostControllerState state = controller_state_function_.Run();

  if (address_ != state.address)
    should_update_name_ = true;
  address_ = std::move(state.address);

  if (was_present != state.is_present) {
    for (auto& observer : observers_)
      observer.AdapterPresentChanged(this, state.is_present);
  }

  if (classic_powered_ != state.classic_powered) {
    classic_powered_ = state.classic_powered;
    RunPendingPowerCallbacks();
    NotifyAdapterPoweredChanged(classic_powered_);
  }

  RemoveTimedOutDevices();
  AddPairedDevices();

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BluetoothAdapterMac::PollAdapter,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kPollIntervalMs));
}

void BluetoothAdapterMac::ClassicDeviceAdded(IOBluetoothDevice* device) {
  std::string device_address =
      BluetoothClassicDeviceMac::GetDeviceAddress(device);

  BluetoothDevice* device_classic = GetDevice(device_address);

  // Only notify observers once per device.
  if (device_classic != nullptr) {
    DVLOG(3) << "Updating classic device: " << device_classic->GetAddress();
    device_classic->UpdateTimestamp();
    return;
  }

  device_classic = new BluetoothClassicDeviceMac(this, device);
  devices_[device_address] = base::WrapUnique(device_classic);
  DVLOG(1) << "Adding new classic device: " << device_classic->GetAddress();

  for (auto& observer : observers_)
    observer.DeviceAdded(this, device_classic);
}

void BluetoothAdapterMac::AddPairedDevices() {
  // Add any new paired devices.
  for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
    // pairedDevices sometimes includes unknown devices that are not paired.
    // Radar issue with id 2282763004 has been filed about it.
    if ([device isPaired]) {
      ClassicDeviceAdded(device);
    }
  }
}

}  // namespace device
