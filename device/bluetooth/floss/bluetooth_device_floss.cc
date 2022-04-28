// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_device_floss.h"

#include <memory>

#include "base/bind.h"
#include "base/notreached.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {

namespace {

void OnCreateBond(const absl::optional<bool>& ret,
                  const absl::optional<Error>& error) {
  if (ret.has_value() && !*ret) {
    BLUETOOTH_LOG(ERROR) << "CreateBond returned failure";
  }

  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to create bond: " << error->name << ": "
                         << error->message;
  }
}

void OnRemoveBond(base::OnceClosure callback,
                  const absl::optional<bool>& ret,
                  const absl::optional<Error>& error) {
  if (ret.has_value() && !*ret) {
    BLUETOOTH_LOG(ERROR) << "RemoveBond returned failure";
  }

  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to remove bond: " << error->name << ": "
                         << error->message;
  }

  std::move(callback).Run();
}

}  // namespace

using AddressType = device::BluetoothDevice::AddressType;
using VendorIDSource = device::BluetoothDevice::VendorIDSource;

BluetoothDeviceFloss::~BluetoothDeviceFloss() = default;

uint32_t BluetoothDeviceFloss::GetBluetoothClass() const {
  return cod_;
}

device::BluetoothTransport BluetoothDeviceFloss::GetType() const {
  return transport_;
}

std::string BluetoothDeviceFloss::GetAddress() const {
  return address_;
}

AddressType BluetoothDeviceFloss::GetAddressType() const {
  NOTIMPLEMENTED();

  return AddressType::ADDR_TYPE_UNKNOWN;
}

VendorIDSource BluetoothDeviceFloss::GetVendorIDSource() const {
  NOTIMPLEMENTED();

  return VendorIDSource::VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothDeviceFloss::GetVendorID() const {
  NOTIMPLEMENTED();

  return 0;
}

uint16_t BluetoothDeviceFloss::GetProductID() const {
  NOTIMPLEMENTED();

  return 0;
}

uint16_t BluetoothDeviceFloss::GetDeviceID() const {
  NOTIMPLEMENTED();

  return 0;
}

uint16_t BluetoothDeviceFloss::GetAppearance() const {
  NOTIMPLEMENTED();

  return 0;
}

absl::optional<std::string> BluetoothDeviceFloss::GetName() const {
  if (name_.length() == 0)
    return absl::nullopt;

  return name_;
}

bool BluetoothDeviceFloss::IsPaired() const {
  return bond_state_ == FlossAdapterClient::BondState::kBonded;
}

#if BUILDFLAG(IS_CHROMEOS)
bool BluetoothDeviceFloss::IsBonded() const {
  // TODO(b/220387308): Update the implementation to return whether the device
  // is bonded, and not just whether it is paired.
  NOTIMPLEMENTED();

  return IsPaired();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool BluetoothDeviceFloss::IsConnected() const {
  return is_connected_;
}

bool BluetoothDeviceFloss::IsGattConnected() const {
  NOTIMPLEMENTED();

  return false;
}

bool BluetoothDeviceFloss::IsConnectable() const {
  // Mark all devices as connectable for now.
  // TODO(b/211126690): Implement based on supported profiles.
  return true;
}

bool BluetoothDeviceFloss::IsConnecting() const {
  NOTIMPLEMENTED();

  return false;
}

device::BluetoothDevice::UUIDSet BluetoothDeviceFloss::GetUUIDs() const {
  return device_uuids_.GetUUIDs();
}

absl::optional<int8_t> BluetoothDeviceFloss::GetInquiryRSSI() const {
  NOTIMPLEMENTED();

  return absl::nullopt;
}

absl::optional<int8_t> BluetoothDeviceFloss::GetInquiryTxPower() const {
  NOTIMPLEMENTED();

  return absl::nullopt;
}

bool BluetoothDeviceFloss::ExpectingPinCode() const {
  if (!pairing_)
    return false;

  return pairing_->pairing_expectation() ==
         BluetoothPairingFloss::PairingExpectation::kPinCode;
}

bool BluetoothDeviceFloss::ExpectingPasskey() const {
  if (!pairing_)
    return false;

  return pairing_->pairing_expectation() ==
         BluetoothPairingFloss::PairingExpectation::kPasskey;
}

bool BluetoothDeviceFloss::ExpectingConfirmation() const {
  if (!pairing_)
    return false;

  return pairing_->pairing_expectation() ==
         BluetoothPairingFloss::PairingExpectation::kConfirmation;
}

void BluetoothDeviceFloss::GetConnectionInfo(ConnectionInfoCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::Connect(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback) {
  BLUETOOTH_LOG(EVENT) << "Connecting to " << address_;

  // To simulate BlueZ API behavior, we don't reply the callback as soon as
  // Floss CreateBond API returns, but rather we trigger the callback later
  // after pairing is done and profiles are connected.
  pending_callback_on_connect_profiles_ = std::move(callback);

  if (IsPaired() || !pairing_delegate) {
    // No need to pair, or unable to, skip straight to connection.
    ConnectAllEnabledProfiles();
  } else {
    pairing_ = std::make_unique<BluetoothPairingFloss>(pairing_delegate);
    FlossDBusManager::Get()->GetAdapterClient()->CreateBond(
        base::BindOnce(&OnCreateBond), AsFlossDeviceId(),
        FlossAdapterClient::BluetoothTransport::kAuto);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothDeviceFloss::ConnectClassic(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback) {
  // TODO(b/215621933): Explicitly create a classic Bluetooth connection.
  // Currently Floss doesn't have the BlueZ-equivalent of ConnectClassic() at
  // the stack level, so just call the existing Connect().
  Connect(pairing_delegate, std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothDeviceFloss::SetPinCode(const std::string& pincode) {
  std::vector<uint8_t> pin(pincode.begin(), pincode.end());
  FlossDBusManager::Get()->GetAdapterClient()->SetPin(
      base::DoNothing(), AsFlossDeviceId(), /*accept=*/true, pin);
}

void BluetoothDeviceFloss::SetPasskey(uint32_t passkey) {
  // No use case in Chrome OS.
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::ConfirmPairing() {
  FlossDBusManager::Get()->GetAdapterClient()->SetPairingConfirmation(
      base::DoNothing(), AsFlossDeviceId(), /*accept=*/true);
}

void BluetoothDeviceFloss::RejectPairing() {
  FlossDBusManager::Get()->GetAdapterClient()->SetPairingConfirmation(
      base::DoNothing(), AsFlossDeviceId(), /*accept=*/false);
}

void BluetoothDeviceFloss::CancelPairing() {
  FlossDBusManager::Get()->GetAdapterClient()->CancelBondProcess(
      base::DoNothing(), AsFlossDeviceId());
}

void BluetoothDeviceFloss::Disconnect(base::OnceClosure callback,
                                      ErrorCallback error_callback) {
  // TODO (b/223832034): Create API that does hard disconnect of a peer device
  FlossDBusManager::Get()->GetAdapterClient()->DisconnectAllEnabledProfiles(
      base::BindOnce(&BluetoothDeviceFloss::OnDisconnectAllEnabledProfiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::Forget(base::OnceClosure callback,
                                  ErrorCallback error_callback) {
  FlossDBusManager::Get()->GetAdapterClient()->RemoveBond(
      base::BindOnce(&OnRemoveBond, std::move(callback)), AsFlossDeviceId());
}

void BluetoothDeviceFloss::ConnectToService(
    const device::BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

std::unique_ptr<device::BluetoothGattConnection>
BluetoothDeviceFloss::CreateBluetoothGattConnectionObject() {
  NOTIMPLEMENTED();

  return nullptr;
}

void BluetoothDeviceFloss::SetGattServicesDiscoveryComplete(bool complete) {
  NOTIMPLEMENTED();
}

bool BluetoothDeviceFloss::IsGattServicesDiscoveryComplete() const {
  NOTIMPLEMENTED();

  return false;
}

void BluetoothDeviceFloss::Pair(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback) {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothDeviceFloss::ExecuteWrite(
    base::OnceClosure callback,
    ExecuteWriteErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::AbortWrite(base::OnceClosure callback,
                                      AbortWriteErrorCallback error_callback) {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

FlossDeviceId BluetoothDeviceFloss::AsFlossDeviceId() const {
  return FlossDeviceId{.address = address_, .name = name_};
}

void BluetoothDeviceFloss::SetName(const std::string& name) {
  name_ = name;
}

void BluetoothDeviceFloss::SetBondState(
    FlossAdapterClient::BondState bond_state) {
  bond_state_ = bond_state;
}

void BluetoothDeviceFloss::SetIsConnected(bool is_connected) {
  is_connected_ = is_connected;
}

void BluetoothDeviceFloss::ConnectAllEnabledProfiles() {
  FlossDBusManager::Get()->GetAdapterClient()->ConnectAllEnabledProfiles(
      base::BindOnce(&BluetoothDeviceFloss::OnConnectAllEnabledProfiles,
                     weak_ptr_factory_.GetWeakPtr()),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::ResetPairing() {
  pairing_.reset();
}

void BluetoothDeviceFloss::CreateGattConnectionImpl(
    absl::optional<device::BluetoothUUID> service_uuid) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::DisconnectGatt() {
  NOTIMPLEMENTED();
}

BluetoothDeviceFloss::BluetoothDeviceFloss(BluetoothAdapterFloss* adapter,
                                           const FlossDeviceId& device)
    : BluetoothDevice(adapter), address_(device.address), name_(device.name) {
  // TODO(abps): Add observers and cache data here.
}

void BluetoothDeviceFloss::ConnectInternal(ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnConnect(ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnConnectError(ConnectCallback callback,
                                          const Error& error) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnPairDuringConnect(ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnPairDuringConnectError(ConnectCallback callback,
                                                    const Error& error) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnDisconnect(base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnDisconnectError(ErrorCallback error_callback,
                                             const Error& error) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnPair(ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnPairError(ConnectCallback callback,
                                       const Error& error) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnCancelPairingError(const Error& error) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnForgetError(ErrorCallback error_callback,
                                         const Error& error) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceFloss::OnGetRemoteType(
    const absl::optional<FlossAdapterClient::BluetoothDeviceType>& ret,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "GetRemoteType() failed: " << error->message;
    return;
  }

  switch (*ret) {
    case FlossAdapterClient::BluetoothDeviceType::kBredr:
      transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC;
      break;
    case FlossAdapterClient::BluetoothDeviceType::kBle:
      transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE;
      break;
    case FlossAdapterClient::BluetoothDeviceType::kDual:
      transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL;
      break;
    default:
      transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID;
  }
}

void BluetoothDeviceFloss::OnGetRemoteClass(
    const absl::optional<uint32_t>& ret,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "GetRemoteClass() failed: " << error->message;
    return;
  }

  cod_ = *ret;
}

void BluetoothDeviceFloss::OnGetRemoteUuids(
    const absl::optional<UUIDList>& ret,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "GetRemoteUuids() failed: " << error->message;
    return;
  }

  device_uuids_.ReplaceServiceUUIDs(*ret);
}

void BluetoothDeviceFloss::OnConnectAllEnabledProfiles(
    const absl::optional<Void>& ret,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to connect all enabled profiles: "
                         << error->name << ": " << error->message;
    // TODO(b/202874707): Design a proper new errors for Floss.
    if (pending_callback_on_connect_profiles_)
      TriggerConnectCallback(BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
  }

  TriggerConnectCallback(absl::nullopt);
}

void BluetoothDeviceFloss::TriggerConnectCallback(
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (pending_callback_on_connect_profiles_) {
    std::move(*pending_callback_on_connect_profiles_).Run(error_code);
    pending_callback_on_connect_profiles_ = absl::nullopt;
  }
}

void BluetoothDeviceFloss::OnDisconnectAllEnabledProfiles(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    const absl::optional<Void>& ret,
    const absl::optional<Error>& error) {
  if (error.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to discconnect all enabled profiles: "
                         << error->name << ": " << error->message;
    std::move(error_callback).Run();
    return;
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::InitializeDeviceProperties() {
  // TODO(b/204708206): Update with property framework when available
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteType(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteType,
                     weak_ptr_factory_.GetWeakPtr()),
      AsFlossDeviceId());
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteClass(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteClass,
                     weak_ptr_factory_.GetWeakPtr()),
      AsFlossDeviceId());
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteUuids(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteUuids,
                     weak_ptr_factory_.GetWeakPtr()),
      AsFlossDeviceId());
}

}  // namespace floss
