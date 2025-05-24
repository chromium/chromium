// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_device_cast.h"

#include <inttypes.h>

#include <unordered_set>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_characteristic_cast.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_service_cast.h"
#include "device/bluetooth/cast/bluetooth_utils.h"

namespace device {
namespace {

BluetoothDevice::UUIDSet ExtractServiceUuids(
    const chromecast::bluetooth::LeScanResult& result) {
  BluetoothDevice::UUIDSet ret;
  auto uuids = result.AllServiceUuids();
  if (!uuids)
    return ret;

  for (const auto& uuid : *uuids)
    ret.insert(UuidToBluetoothUUID(uuid));
  return ret;
}

BluetoothDevice::ServiceDataMap ExtractServiceData(
    const chromecast::bluetooth::LeScanResult& result) {
  BluetoothDevice::ServiceDataMap service_data;
  for (const auto& it : result.AllServiceData()) {
    service_data.insert(
        std::make_pair(UuidToBluetoothUUID(it.first), it.second));
  }
  return service_data;
}

BluetoothDevice::ManufacturerDataMap ExtractManufacturerData(
    const chromecast::bluetooth::LeScanResult& result) {
  BluetoothDevice::ManufacturerDataMap ret;
  for (const auto& it : result.ManufacturerData())
    ret.insert(std::make_pair(it.first, it.second));
  return ret;
}

}  // namespace

BluetoothDeviceCast::BluetoothDeviceCast(
    BluetoothAdapter* adapter,
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device)
    : BluetoothDevice(adapter),
      connected_(device->IsConnected()),
      remote_device_(std::move(device)),
      address_(GetCanonicalBluetoothAddress(remote_device_->addr())),
      weak_factory_(this) {
  if (connected_) {
    remote_device_->GetServices(base::BindOnce(
        &BluetoothDeviceCast::OnGetServices, weak_factory_.GetWeakPtr()));
  }
}

BluetoothDeviceCast::~BluetoothDeviceCast() {}

uint32_t BluetoothDeviceCast::GetBluetoothClass() const {
  // Return the code for miscellaneous device.
  return 0x1F00;
}

BluetoothTransport BluetoothDeviceCast::GetType() const {
  return BLUETOOTH_TRANSPORT_LE;
}

std::string BluetoothDeviceCast::GetAddress() const {
  return address_;
}

BluetoothDevice::AddressType BluetoothDeviceCast::GetAddressType() const {
  NOTIMPLEMENTED();
  return ADDR_TYPE_UNKNOWN;
}

BluetoothDevice::VendorIDSource BluetoothDeviceCast::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothDeviceCast::GetVendorID() const {
  return 0;
}

uint16_t BluetoothDeviceCast::GetProductID() const {
  return 0;
}

uint16_t BluetoothDeviceCast::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothDeviceCast::GetAppearance() const {
  return 0;
}

std::optional<std::string> BluetoothDeviceCast::GetName() const {
  return name_;
}

bool BluetoothDeviceCast::IsPaired() const {
  return false;
}

bool BluetoothDeviceCast::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceCast::IsGattConnected() const {
  return IsConnected();
}

bool BluetoothDeviceCast::IsConnectable() const {
  NOTREACHED() << "This is only called on ChromeOS";
}

bool BluetoothDeviceCast::IsConnecting() const {
  return pending_connect_;
}

std::optional<int8_t> BluetoothDeviceCast::GetInquiryRSSI() const {
  // TODO(slan): Plumb this from the type_to_data field of ScanResult.
  return BluetoothDevice::GetInquiryRSSI();
}

std::optional<int8_t> BluetoothDeviceCast::GetInquiryTxPower() const {
  // TODO(slan): Remove if we do not need this.
  return BluetoothDevice::GetInquiryTxPower();
}

bool BluetoothDeviceCast::ExpectingPinCode() const {
  // TODO(slan): Implement this or rely on lower layers to do so.
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceCast::ExpectingPasskey() const {
  NOTIMPLEMENTED() << "Only BLE functionality is supported.";
  return false;
}

bool BluetoothDeviceCast::ExpectingConfirmation() const {
  NOTIMPLEMENTED() << "Only BLE functionality is supported.";
  return false;
}

void BluetoothDeviceCast::GetConnectionInfo(ConnectionInfoCallback callback) {
  // TODO(slan): Implement this?
  NOTIMPLEMENTED();
}

void BluetoothDeviceCast::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  // TODO(slan): This many be needed for some high-performance BLE devices.
  NOTIMPLEMENTED();
  std::move(error_callback).Run();
}

void BluetoothDeviceCast::Connect(PairingDelegate* pairing_delegate,
                                  ConnectCallback callback) {
  // This method is used only for Bluetooth classic.
  NOTIMPLEMENTED() << __func__ << " Only BLE functionality is supported.";
  std::move(callback).Run(BluetoothDevice::ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothDeviceCast::Pair(PairingDelegate* pairing_delegate,
                               ConnectCallback callback) {
  // TODO(slan): Implement this or delegate to lower level.
  NOTIMPLEMENTED();
  std::move(callback).Run(BluetoothDevice::ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothDeviceCast::SetPinCode(const std::string& pincode) {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::SetPasskey(uint32_t passkey) {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::ConfirmPairing() {
  NOTREACHED() << "Pairing not supported.";
}

// Rejects a pairing or connection request from a remote device.
void BluetoothDeviceCast::RejectPairing() {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::CancelPairing() {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::Disconnect(base::OnceClosure callback,
                                     ErrorCallback error_callback) {
  // This method is used only for Bluetooth classic.
  NOTIMPLEMENTED() << __func__ << " Only BLE functionality is supported.";
  std::move(error_callback).Run();
}

void BluetoothDeviceCast::Forget(base::OnceClosure callback,
                                 ErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " Only BLE functionality is supported.";
  std::move(error_callback).Run();
}

void BluetoothDeviceCast::ConnectToService(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothDeviceCast::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  std::move(error_callback).Run("Not Implemented");
}

bool BluetoothDeviceCast::UpdateWithScanResult(
    const chromecast::bluetooth::LeScanResult& result) {
  DVLOG(3) << __func__;
  bool changed = false;

  std::optional<std::string> result_name = result.Name();

  // Advertisements for the same device can use different names. For now, the
  // last name wins. An empty string represents no name.
  // TODO(slan): Make sure that this doesn't spam us with name changes.
  if (result_name != name_) {
    changed = true;
    name_ = std::move(result_name);
  }

  // Replace |device_uuids_| with newly advertised services. Currently this just
  // replaces them, but depending on what we see in the field, we may need to
  // take the union here instead. Note that this would require eviction of stale
  // services, preferably from the LeScanManager.
  // TODO(slan): Think about whether this is needed.
  UUIDSet prev_uuids = device_uuids_.GetUUIDs();
  UUIDSet new_uuids = ExtractServiceUuids(result);
  if (prev_uuids != new_uuids) {
    device_uuids_.ReplaceAdvertisedUUIDs(
        UUIDList(new_uuids.begin(), new_uuids.end()));
    changed = true;
  }

  // Extract service data from the advertisement.
  ServiceDataMap service_data = ExtractServiceData(result);
  if (service_data != service_data_) {
    service_data_ = std::move(service_data);
    changed = true;
  }

  // Extract manufacturer data from the advertisement.
  ManufacturerDataMap manufacturer_data = ExtractManufacturerData(result);
  if (manufacturer_data_ != manufacturer_data) {
    manufacturer_data_ = manufacturer_data;
    changed = true;
  }

  return changed;
}

bool BluetoothDeviceCast::SetConnected(bool connected) {
  DVLOG(2) << __func__ << " connected: " << connected;
  bool was_connected = connected_;

  // Set the new state *before* calling the protected methods below. They may
  // synchronously query the state of the device.
  connected_ = connected;

  // Update state in the base class. This will cause pending callbacks to be
  // fired.
  if (!was_connected && connected) {
    DidConnectGatt(/*error_code=*/std::nullopt);
    remote_device_->GetServices(base::BindOnce(
        &BluetoothDeviceCast::OnGetServices, weak_factory_.GetWeakPtr()));
  } else if (was_connected && !connected) {
    DidDisconnectGatt();
  }

  // Return true if the value of |connected_| changed.
  return was_connected != connected;
}

void BluetoothDeviceCast::OnGetServices(
    std::vector<scoped_refptr<chromecast::bluetooth::RemoteService>> services) {
  DVLOG(2) << __func__;
  gatt_services_.clear();

  // Add new services.
  for (auto& service : services) {
    auto key = GetCanonicalBluetoothUuid(service->uuid());
    auto cast_service = std::make_unique<BluetoothRemoteGattServiceCast>(
        this, std::move(service));
    DCHECK_EQ(key, cast_service->GetIdentifier());
    gatt_services_[key] = std::move(cast_service);
  }

  device_uuids_.ReplaceServiceUUIDs(gatt_services_);
  SetGattServicesDiscoveryComplete(true);
  adapter_->NotifyGattServicesDiscovered(this);
}

bool BluetoothDeviceCast::UpdateCharacteristicValue(
    scoped_refptr<chromecast::bluetooth::RemoteCharacteristic> characteristic,
    std::vector<uint8_t> value,
    OnValueUpdatedCallback callback) {
  auto uuid = UuidToBluetoothUUID(characteristic->uuid());
  // TODO(slan): Consider using a look-up to find characteristics instead. This
  // approach could be inefficient if a device has a lot of characteristics.
  for (const auto& it : gatt_services_) {
    for (auto* c : it.second->GetCharacteristics()) {
      if (c->GetUUID() == uuid) {
        static_cast<BluetoothRemoteGattCharacteristicCast*>(c)->SetValue(value);
        std::move(callback).Run(c, value);
        return true;
      }
    }
  }
  LOG(WARNING) << GetAddress() << " does not have a service with "
               << " characteristic " << uuid.canonical_value();
  return false;
}

void BluetoothDeviceCast::CreateGattConnectionImpl(
    std::optional<BluetoothUUID> service_uuid) {
  DVLOG(2) << __func__ << " " << pending_connect_;
  if (pending_connect_)
    return;
  pending_connect_ = true;
  remote_device_->Connect(base::BindOnce(&BluetoothDeviceCast::OnConnect,
                                         weak_factory_.GetWeakPtr()));
}

void BluetoothDeviceCast::DisconnectGatt() {
  // The device is intentionally not disconnected.
}

void BluetoothDeviceCast::OnConnect(
    chromecast::bluetooth::RemoteDevice::ConnectStatus status) {
  bool success =
      (status == chromecast::bluetooth::RemoteDevice::ConnectStatus::kSuccess);
  DVLOG(2) << __func__ << " success:" << success;
  pending_connect_ = false;
  if (!success) {
    DidConnectGatt(ERROR_FAILED);
  }
}

}  // namespace device
