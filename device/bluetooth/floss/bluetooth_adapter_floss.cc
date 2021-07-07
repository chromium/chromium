// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_adapter_floss.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "components/device_event_log/device_event_log.h"

namespace floss {

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
}

void BluetoothAdapterFloss::Shutdown() {
  BLUETOOTH_LOG(EVENT) << "BluetoothAdapterFloss::Shutdown";
}

BluetoothAdapterFloss::UUIDList BluetoothAdapterFloss::GetUUIDs() const {
  return {};
}

std::string BluetoothAdapterFloss::GetAddress() const {
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
  return false;
}

bool BluetoothAdapterFloss::IsPresent() const {
  return false;
}

bool BluetoothAdapterFloss::IsPowered() const {
  return false;
}

void BluetoothAdapterFloss::SetPowered(bool powered,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  NOTIMPLEMENTED();
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
  return false;
}

std::unordered_map<device::BluetoothDevice*, device::BluetoothDevice::UUIDSet>
BluetoothAdapterFloss::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const device::BluetoothDiscoveryFilter& discovery_filter) {
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
#endif

void BluetoothAdapterFloss::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTIMPLEMENTED();
}

base::WeakPtr<device::BluetoothAdapter> BluetoothAdapterFloss::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothAdapterFloss::SetPoweredImpl(bool powered) {
  return false;
}

void BluetoothAdapterFloss::StartScanWithFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::UpdateFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterFloss::StopScan(DiscoverySessionResultCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace floss
