// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_adapter_cast.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/le_scan_manager.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/cast/bluetooth_device_cast.h"
#include "device/bluetooth/cast/bluetooth_utils.h"

namespace device {
namespace {

BluetoothAdapterCast::FactoryCb& GetFactory() {
  static base::NoDestructor<BluetoothAdapterCast::FactoryCb> factory_cb;
  return *factory_cb;
}

}  // namespace

BluetoothAdapterCast::DiscoveryParams::DiscoveryParams(
    std::unique_ptr<device::BluetoothDiscoveryFilter> filter,
    base::OnceClosure success_callback,
    DiscoverySessionErrorCallback error_callback)
    : filter(std::move(filter)),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

BluetoothAdapterCast::DiscoveryParams::DiscoveryParams(
    DiscoveryParams&& params) noexcept = default;
BluetoothAdapterCast::DiscoveryParams& BluetoothAdapterCast::DiscoveryParams::
operator=(DiscoveryParams&& params) = default;
BluetoothAdapterCast::DiscoveryParams::~DiscoveryParams() = default;

BluetoothAdapterCast::BluetoothAdapterCast(
    chromecast::bluetooth::GattClientManager* gatt_client_manager,
    chromecast::bluetooth::LeScanManager* le_scan_manager)
    : gatt_client_manager_(gatt_client_manager),
      le_scan_manager_(le_scan_manager),
      weak_factory_(this) {
  DCHECK(gatt_client_manager_);
  DCHECK(le_scan_manager_);
  gatt_client_manager_->AddObserver(this);
  le_scan_manager_->AddObserver(this);
}

BluetoothAdapterCast::~BluetoothAdapterCast() {
  gatt_client_manager_->RemoveObserver(this);
  le_scan_manager_->RemoveObserver(this);
}

void BluetoothAdapterCast::Initialize(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialized_ = true;
  std::move(callback).Run();
}

std::string BluetoothAdapterCast::GetAddress() const {
  // TODO(slan|bcf): Right now, we aren't surfacing the address of the GATT
  // client to the caller, because there is no apparent need and this
  // information is potentially PII. Implement this when it's needed.
  return std::string();
}

std::string BluetoothAdapterCast::GetName() const {
  return name_;
}

void BluetoothAdapterCast::SetName(const std::string& name,
                                   base::OnceClosure callback,
                                   ErrorCallback error_callback) {
  name_ = name;
  std::move(callback).Run();
}

bool BluetoothAdapterCast::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initialized_;
}

bool BluetoothAdapterCast::IsPresent() const {
  // The BluetoothAdapter is always present on Cast devices.
  return true;
}

bool BluetoothAdapterCast::IsPowered() const {
  return powered_;
}

void BluetoothAdapterCast::SetPowered(bool powered,
                                      base::OnceClosure callback,
                                      ErrorCallback error_callback) {
  // This class cannot actually change the powered state of the BT stack.
  // We simulate these changes for the benefit of testing. However, we may
  // want to actually delegate this call to the bluetooth service, at least
  // as a signal of the client's intent.
  // TODO(slan): Determine whether this would be useful.
  powered_ = powered;
  NotifyAdapterPoweredChanged(powered_);
  std::move(callback).Run();
}

bool BluetoothAdapterCast::IsDiscoverable() const {
  DVLOG(2) << __func__ << " GATT server mode not supported";
  return false;
}

void BluetoothAdapterCast::SetDiscoverable(bool discoverable,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback).Run();
}

bool BluetoothAdapterCast::IsDiscovering() const {
  return num_discovery_sessions_ > 0;
}

BluetoothAdapter::UUIDList BluetoothAdapterCast::GetUUIDs() const {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  return UUIDList();
}

void BluetoothAdapterCast::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothAdapterCast::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothAdapterCast::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback)
      .Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterCast::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback)
      .Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterCast::ConnectDevice(
    const std::string& address,
    const std::optional<BluetoothDevice::AddressType>& address_type,
    ConnectDeviceCallback callback,
    ConnectDeviceErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback).Run(/*error_message=*/std::string());
}

void BluetoothAdapterCast::ResetAdvertising(
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback)
      .Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterCast::GetGattService(
    const std::string& identifier) const {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  return nullptr;
}

base::WeakPtr<BluetoothAdapter> BluetoothAdapterCast::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<BluetoothAdapterCast> BluetoothAdapterCast::GetCastWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

bool BluetoothAdapterCast::SetPoweredImpl(bool powered) {
  NOTREACHED() << "This method is not invoked when SetPowered() is overridden.";
}

void BluetoothAdapterCast::StartScanWithFilter(
    [[maybe_unused]] std::unique_ptr<device::BluetoothDiscoveryFilter>
        discovery_filter,
    DiscoverySessionResultCallback callback) {
  // The discovery filter is unused for now, as the Cast bluetooth stack does
  // not expose scan filters yet. However, implementation of filtering would
  // save numerous UI<->IO threadhops by eliminating unnecessary calls to
  // GetDevice().
  // TODO(bcf|slan): Wire this up once scan filters are implemented and remove
  // the [[maybe_unused]].

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  // Add this request to the queue.
  pending_discovery_requests_.emplace(BluetoothAdapterCast::DiscoveryParams(
      std::move(discovery_filter),
      base::BindOnce(std::move(split_callback.first), /*is_error=*/false,
                     UMABluetoothDiscoverySessionOutcome::SUCCESS),
      base::BindOnce(std::move(split_callback.second), /*is_error=*/true)));

  // If the queue length is greater than 1 (i.e. there was a pending request
  // when this method was called), exit early. This request will be processed
  // after the pending requests.
  if (pending_discovery_requests_.size() > 1u)
    return;

  // There is no active discovery session, and no pending requests. Enable
  // scanning.
  le_scan_manager_->RequestScan(base::BindOnce(
      &BluetoothAdapterCast::OnScanEnabled, weak_factory_.GetWeakPtr()));
}

void BluetoothAdapterCast::UpdateFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // The discovery filter is unused for now, as the Cast bluetooth stack does
  // not expose scan filters yet. However, implementation of filtering would
  // save numerous UI<->IO threadhops by eliminating unnecessary calls to
  // GetDevice().
  // TODO(bcf|slan): Wire this up once scan filters are implemented.

  // If calling update then there must be other sessions actively scanning
  // besides this one.
  DCHECK_GT(NumDiscoverySessions(), 1);

  // Since filters are not used simply return success.
  std::move(callback).Run(/*is_error=*/false,
                          UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

void BluetoothAdapterCast::StopScan(DiscoverySessionResultCallback callback) {
  DCHECK(scan_handle_);
  scan_handle_.reset();
  std::move(callback).Run(/*is_error*/ false,
                          UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

void BluetoothAdapterCast::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  // TODO(slan): Implement this or properly stub.
  NOTIMPLEMENTED();
}

void BluetoothAdapterCast::OnConnectChanged(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string address = GetCanonicalBluetoothAddress(device->addr());
  DVLOG(1) << __func__ << " " << address << " connected: " << connected;

  // This method could be called before this device is detected in a scan and
  // GetDevice() is called. Add it if needed.
  if (!base::Contains(devices_, address)) {
    AddDevice(std::move(device));
  }

  BluetoothDeviceCast* cast_device = GetCastDevice(address);
  cast_device->SetConnected(connected);
}

void BluetoothAdapterCast::OnMtuChanged(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    int mtu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(3) << __func__ << " " << GetCanonicalBluetoothAddress(device->addr())
           << " mtu: " << mtu;
}

void BluetoothAdapterCast::OnCharacteristicNotification(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    scoped_refptr<chromecast::bluetooth::RemoteCharacteristic> characteristic,
    std::vector<uint8_t> value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string address = GetCanonicalBluetoothAddress(device->addr());
  BluetoothDeviceCast* cast_device = GetCastDevice(address);
  if (!cast_device)
    return;

  cast_device->UpdateCharacteristicValue(
      std::move(characteristic), std::move(value),
      base::BindOnce(
          &BluetoothAdapterCast::NotifyGattCharacteristicValueChanged,
          weak_factory_.GetWeakPtr()));
}

void BluetoothAdapterCast::OnNewScanResult(
    chromecast::bluetooth::LeScanResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__;

  std::string address = GetCanonicalBluetoothAddress(result.addr);

  // If we haven't created a BluetoothDeviceCast for this address yet, we need
  // to send an async request to |gatt_client_manager_| for a handle to the
  // device.
  if (!base::Contains(devices_, address)) {
    bool first_time_seen = !base::Contains(pending_scan_results_, address);
    // These results will be used to construct the BluetoothDeviceCast.
    pending_scan_results_[address].push_back(result);

    // Only send a request if this is the first time we've seen this |address|
    // in a scan. This may happen if we pick up additional GAP advertisements
    // while the first request is in-flight.
    if (first_time_seen) {
      gatt_client_manager_->GetDevice(
          result.addr, base::BindOnce(&BluetoothAdapterCast::OnGetDevice,
                                      weak_factory_.GetWeakPtr()));
    }
    return;
  }

  // Update the device with the ScanResult.
  BluetoothDeviceCast* device = GetCastDevice(address);
  if (device->UpdateWithScanResult(result)) {
    for (auto& observer : observers_)
      observer.DeviceChanged(this, device);
  }
}

void BluetoothAdapterCast::OnScanEnableChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the scan function has been disabled during discovery, something has
  // gone wrong. We should consider re-enabling it here.
  LOG_IF(WARNING, IsDiscovering() && !enabled)
      << "BLE scan has been disabled during WebBluetooth discovery!";
}

BluetoothDeviceCast* BluetoothAdapterCast::GetCastDevice(
    const std::string& address) {
  auto it = devices_.find(address);
  return it == devices_.end()
             ? nullptr
             : static_cast<BluetoothDeviceCast*>(it->second.get());
}

void BluetoothAdapterCast::AddDevice(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> remote_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This method should not be called if we already have a BluetoothDeviceCast
  // registered for this device.
  std::string address = GetCanonicalBluetoothAddress(remote_device->addr());
  DCHECK(!base::Contains(devices_, address));

  devices_[address] =
      std::make_unique<BluetoothDeviceCast>(this, remote_device);
  BluetoothDeviceCast* device = GetCastDevice(address);

  const auto scan_results = std::move(pending_scan_results_[address]);
  pending_scan_results_.erase(address);

  // Update the device with the ScanResults.
  for (const auto& result : scan_results)
    device->UpdateWithScanResult(result);

  // Update the observers of the new device.
  for (auto& observer : observers_)
    observer.DeviceAdded(this, device);
}

void BluetoothAdapterCast::OnScanEnabled(
    std::unique_ptr<chromecast::bluetooth::LeScanManager::ScanHandle>
        scan_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!scan_handle) {
    LOG(WARNING) << "Failed to start scan.";

    // Run the error callback.
    DCHECK(!pending_discovery_requests_.empty());
    std::move(pending_discovery_requests_.front().error_callback)
        .Run(UMABluetoothDiscoverySessionOutcome::FAILED);
    pending_discovery_requests_.pop();

    // If there is another pending request, try again.
    if (pending_discovery_requests_.size() > 0u) {
      le_scan_manager_->RequestScan(base::BindOnce(
          &BluetoothAdapterCast::OnScanEnabled, weak_factory_.GetWeakPtr()));
    }
    return;
  }

  scan_handle_ = std::move(scan_handle);

  // The scan has been successfully enabled. Request the initial scan results
  // from the scan manager.
  le_scan_manager_->GetScanResults(base::BindOnce(
      &BluetoothAdapterCast::OnGetScanResults, weak_factory_.GetWeakPtr()));

  // For each pending request, increment the count and run the success callback.
  while (!pending_discovery_requests_.empty()) {
    num_discovery_sessions_++;
    std::move(pending_discovery_requests_.front().success_callback).Run();
    pending_discovery_requests_.pop();
  }
}

void BluetoothAdapterCast::OnGetDevice(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> remote_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string address = GetCanonicalBluetoothAddress(remote_device->addr());

  // This callback could run before or after the device becomes connected and
  // OnConnectChanged() is called for a particular device. If that happened,
  // |remote_device| already has a handle. In this case, there should be no
  // |pending_scan_results_| and we should fast-return.
  if (base::Contains(devices_, address)) {
    DCHECK(!base::Contains(pending_scan_results_, address));
    return;
  }

  // If there is not a device already, there should be at least one ScanResult
  // which triggered the GetDevice() call.
  DCHECK(!base::Contains(pending_scan_results_, address));
  AddDevice(std::move(remote_device));
}

void BluetoothAdapterCast::OnGetScanResults(
    std::vector<chromecast::bluetooth::LeScanResult> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& result : results)
    OnNewScanResult(result);
}

// static
void BluetoothAdapterCast::SetFactory(FactoryCb factory_cb) {
  FactoryCb& factory = GetFactory();
  DCHECK(!factory);
  factory = std::move(factory_cb);
}

// static
void BluetoothAdapterCast::ResetFactoryForTest() {
  GetFactory().Reset();
}

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapterCast::Create() {
  FactoryCb& factory = GetFactory();
  DCHECK(factory) << "SetFactory() must be called before this method!";
  return factory.Run();
}

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return BluetoothAdapterCast::Create();
}

}  // namespace device
