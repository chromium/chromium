// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <string>
#include <unordered_map>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
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

BluetoothDeviceWin::~BluetoothDeviceWin() = default;

uint32_t BluetoothDeviceWin::GetBluetoothClass() const {
  return bluetooth_class_;
}

std::string BluetoothDeviceWin::GetAddress() const {
  return address_;
}

BluetoothDevice::AddressType BluetoothDeviceWin::GetAddressType() const {
  NOTIMPLEMENTED();
  return ADDR_TYPE_UNKNOWN;
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
  // TODO(crbug.com/41240161): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

std::optional<std::string> BluetoothDeviceWin::GetName() const {
  return name_;
}

bool BluetoothDeviceWin::IsPaired() const {
  return paired_;
}

bool BluetoothDeviceWin::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceWin::IsGattConnected() const {
  return false;
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

std::optional<int8_t> BluetoothDeviceWin::GetInquiryRSSI() const {
  // In windows, we can only get connected devices and connected
  // devices don't have an Inquiry RSSI.
  return std::nullopt;
}

std::optional<int8_t> BluetoothDeviceWin::GetInquiryTxPower() const {
  // In windows, we can only get connected devices and connected
  // devices don't have an Inquiry Tx Power.
  return std::nullopt;
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

void BluetoothDeviceWin::GetConnectionInfo(ConnectionInfoCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(ConnectionInfo());
}

void BluetoothDeviceWin::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Connect(PairingDelegate* pairing_delegate,
                                 ConnectCallback callback) {
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

void BluetoothDeviceWin::Disconnect(base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Forget(base::OnceClosure callback,
                                ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConnectToService(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  scoped_refptr<BluetoothSocketWin> socket(
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_));
  socket->Connect(this, uuid, base::BindOnce(std::move(callback), socket),
                  std::move(error_callback));
}

void BluetoothDeviceWin::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  std::move(error_callback).Run(kApiUnavailable);
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
  DCHECK_EQ(address_, CanonicalizeBluetoothAddress(address_));
  name_ = device_state.name;
  bluetooth_class_ = device_state.bluetooth_class;
  visible_ = device_state.visible;
  connected_ = device_state.connected;
  paired_ = device_state.authenticated;
  UpdateServices(device_state);
}

void BluetoothDeviceWin::CreateGattConnectionImpl(
    std::optional<BluetoothUUID> service_uuid) {
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
}

}  // namespace device
