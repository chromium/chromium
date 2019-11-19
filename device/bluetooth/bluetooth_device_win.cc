// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace {

const char kApiUnavailable[] = "This API is not implemented on this platform.";

}  // namespace

namespace device {

BluetoothDeviceWin::BluetoothDeviceWin(
    BluetoothAdapterWin* adapter,
    const BluetoothTaskManagerWin::DeviceState& device_state,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread)
    : BluetoothDevice(adapter),
      ui_task_runner_(std::move(ui_task_runner)),
      socket_thread_(std::move(socket_thread)) {
  Update(device_state);
}

BluetoothDeviceWin::~BluetoothDeviceWin() {
  // Explicitly take and erase GATT services one by one to ensure that calling
  // GetGattService on removed service in GattServiceRemoved returns null.
  std::vector<std::string> service_keys;
  for (const auto& gatt_service : gatt_services_) {
    service_keys.push_back(gatt_service.first);
  }
  for (const auto& key : service_keys) {
    std::unique_ptr<BluetoothRemoteGattService> service =
        std::move(gatt_services_[key]);
    gatt_services_.erase(key);
  }
}

uint32_t BluetoothDeviceWin::GetBluetoothClass() const {
  return bluetooth_class_;
}

std::string BluetoothDeviceWin::GetAddress() const {
  return address_;
}

BluetoothDevice::VendorIDSource
BluetoothDeviceWin::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothDeviceWin::GetVendorID() const {
  return 0;
}

uint16_t BluetoothDeviceWin::GetProductID() const {
  return 0;
}

uint16_t BluetoothDeviceWin::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothDeviceWin::GetAppearance() const {
  // TODO(crbug.com/588083): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

base::Optional<std::string> BluetoothDeviceWin::GetName() const {
  return name_;
}

bool BluetoothDeviceWin::IsPaired() const {
  return paired_;
}

bool BluetoothDeviceWin::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceWin::IsGattConnected() const {
  return gatt_connected_;
}

bool BluetoothDeviceWin::IsConnectable() const {
  return false;
}

bool BluetoothDeviceWin::IsConnecting() const {
  return false;
}

BluetoothDevice::UUIDSet BluetoothDeviceWin::GetUUIDs() const {
  return uuids_;
}

base::Optional<int8_t> BluetoothDeviceWin::GetInquiryRSSI() const {
  // In windows, we can only get connected devices and connected
  // devices don't have an Inquiry RSSI.
  return base::nullopt;
}

base::Optional<int8_t> BluetoothDeviceWin::GetInquiryTxPower() const {
  // In windows, we can only get connected devices and connected
  // devices don't have an Inquiry Tx Power.
  return base::nullopt;
}

bool BluetoothDeviceWin::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWin::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(ConnectionInfo());
}

void BluetoothDeviceWin::SetConnectionLatency(
    ConnectionLatency connection_latency,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Forget(const base::Closure& callback,
                                const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketWin> socket(
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_));
  socket->Connect(this, uuid, base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceWin::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  error_callback.Run(kApiUnavailable);
}

const BluetoothServiceRecordWin* BluetoothDeviceWin::GetServiceRecord(
    const device::BluetoothUUID& uuid) const {
  for (const auto& record : service_record_list_)
    if (record->uuid() == uuid)
      return record.get();

  return nullptr;
}

bool BluetoothDeviceWin::IsEqual(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  if (address_ != device_state.address || name_ != device_state.name ||
      bluetooth_class_ != device_state.bluetooth_class ||
      visible_ != device_state.visible ||
      connected_ != device_state.connected ||
      gatt_connected_ == device_state.is_bluetooth_classic() ||
      paired_ != device_state.authenticated) {
    return false;
  }

  // Checks service collection
  UUIDSet new_services;
  std::unordered_map<std::string, std::unique_ptr<BluetoothServiceRecordWin>>
      new_service_records;
  for (auto iter = device_state.service_record_states.begin();
       iter != device_state.service_record_states.end(); ++iter) {
    auto service_record = std::make_unique<BluetoothServiceRecordWin>(
        address_, (*iter)->name, (*iter)->sdp_bytes, (*iter)->gatt_uuid);
    new_services.insert(service_record->uuid());
    new_service_records[service_record->uuid().canonical_value()] =
        std::move(service_record);
  }

  // Check that no new services have been added or removed.
  if (uuids_ != new_services) {
    return false;
  }

  for (const auto& service_record : service_record_list_) {
    BluetoothServiceRecordWin* new_service_record =
        new_service_records[service_record->uuid().canonical_value()].get();
    if (!service_record->IsEqual(*new_service_record))
      return false;
  }
  return true;
}

void BluetoothDeviceWin::Update(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  address_ = device_state.address;
  // Note: Callers are responsible for providing a canonicalized address.
  DCHECK_EQ(address_, BluetoothDevice::CanonicalizeAddress(address_));
  name_ = device_state.name;
  bluetooth_class_ = device_state.bluetooth_class;
  visible_ = device_state.visible;
  connected_ = device_state.connected;
  // If a BLE device is not GATT connected, Windows will automatically
  // reconnect.
  gatt_connected_ = !device_state.is_bluetooth_classic();
  paired_ = device_state.authenticated;
  UpdateServices(device_state);
}

void BluetoothDeviceWin::GattServiceDiscoveryComplete(
    BluetoothRemoteGattServiceWin* service) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(BluetoothDeviceWin::IsGattServiceDiscovered(
      service->GetUUID(), service->GetAttributeHandle()));

  discovery_completed_included_services_.insert(
      {service->GetUUID(), service->GetAttributeHandle()});
  if (discovery_completed_included_services_.size() != gatt_services_.size())
    return;

  SetGattServicesDiscoveryComplete(true);
  adapter_->NotifyGattServicesDiscovered(this);
}

void BluetoothDeviceWin::CreateGattConnectionImpl() {
  // Windows will create the Gatt connection as needed.  See:
  // https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-client#connecting-to-the-device
}

void BluetoothDeviceWin::DisconnectGatt() {
  // On Windows, the adapter cannot force a disconnection.
}

void BluetoothDeviceWin::SetVisible(bool visible) {
  visible_ = visible;
}

void BluetoothDeviceWin::UpdateServices(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  uuids_.clear();
  service_record_list_.clear();

  for (const auto& record_state : device_state.service_record_states) {
    auto service_record = std::make_unique<BluetoothServiceRecordWin>(
        device_state.address, record_state->name, record_state->sdp_bytes,
        record_state->gatt_uuid);
    uuids_.insert(service_record->uuid());
    service_record_list_.push_back(std::move(service_record));
  }

  if (!device_state.is_bluetooth_classic())
    UpdateGattServices(device_state.service_record_states);
}

bool BluetoothDeviceWin::IsGattServiceDiscovered(const BluetoothUUID& uuid,
                                                 uint16_t attribute_handle) {
  for (const auto& gatt_service : gatt_services_) {
    uint16_t it_att_handle =
        static_cast<BluetoothRemoteGattServiceWin*>(gatt_service.second.get())
            ->GetAttributeHandle();
    BluetoothUUID it_uuid = gatt_service.second->GetUUID();
    if (attribute_handle == it_att_handle && uuid == it_uuid) {
      return true;
    }
  }
  return false;
}

bool BluetoothDeviceWin::DoesGattServiceExist(
    const std::vector<std::unique_ptr<
        BluetoothTaskManagerWin::ServiceRecordState>>& service_state,
    BluetoothRemoteGattService* service) {
  uint16_t attribute_handle =
      static_cast<BluetoothRemoteGattServiceWin*>(service)
          ->GetAttributeHandle();
  BluetoothUUID uuid = service->GetUUID();
  for (const auto& record_state : service_state) {
    if (attribute_handle == record_state->attribute_handle &&
        uuid == record_state->gatt_uuid) {
      return true;
    }
  }
  return false;
}

void BluetoothDeviceWin::UpdateGattServices(
    const std::vector<
        std::unique_ptr<BluetoothTaskManagerWin::ServiceRecordState>>&
        service_state) {
  // First, remove no longer existent GATT service.
  {
    std::vector<std::string> to_be_removed_services;
    for (const auto& gatt_service : gatt_services_) {
      if (!DoesGattServiceExist(service_state, gatt_service.second.get())) {
        to_be_removed_services.push_back(gatt_service.first);
      }
    }
    for (const auto& service : to_be_removed_services) {
      std::unique_ptr<BluetoothRemoteGattService> service_ptr =
          std::move(gatt_services_[service]);
      gatt_services_.erase(service);
    }
    // Update previously discovered services.
    for (const auto& gatt_service : gatt_services_) {
      static_cast<BluetoothRemoteGattServiceWin*>(gatt_service.second.get())
          ->Update();
    }
  }

  // Return if no new services have been added.
  if (gatt_services_.size() == service_state.size())
    return;

  // Add new services.
  for (const auto& record_state : service_state) {
    if (!IsGattServiceDiscovered(record_state->gatt_uuid,
                                 record_state->attribute_handle)) {
      BluetoothRemoteGattServiceWin* primary_service =
          new BluetoothRemoteGattServiceWin(
              this, record_state->path, record_state->gatt_uuid,
              record_state->attribute_handle, true, nullptr, ui_task_runner_);
      gatt_services_[primary_service->GetIdentifier()] =
          base::WrapUnique(primary_service);
      adapter_->NotifyGattServiceAdded(primary_service);
    }
  }
}

}  // namespace device
