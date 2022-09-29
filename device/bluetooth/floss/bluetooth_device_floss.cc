// Copyright 2021 The Chromium Authors
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
#include "device/bluetooth/floss/bluetooth_gatt_connection_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_socket_floss.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_gatt_client.h"
#include "device/bluetooth/floss/floss_socket_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif

namespace floss {

namespace {

void OnCreateBond(DBusResult<bool> ret) {
  if (ret.has_value() && !*ret) {
    BLUETOOTH_LOG(ERROR) << "CreateBond returned failure";
  }

  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to create bond: " << ret.error();
  }
}

void OnRemoveBond(base::OnceClosure callback, DBusResult<bool> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to remove bond: " << ret.error();
  } else if (!*ret) {
    BLUETOOTH_LOG(ERROR) << "RemoveBond returned failure";
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool success = ret.has_value() && *ret;
  device::RecordForgetResult(success ? device::ForgetResult::kSuccess
                                     : device::ForgetResult::kFailure);
#endif

  std::move(callback).Run();
}

}  // namespace

using AddressType = device::BluetoothDevice::AddressType;
using VendorIDSource = device::BluetoothDevice::VendorIDSource;

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
  return IsBondedImpl() ||
         FlossAdapterClient::IsConnectionPaired(connection_state_);
}

#if BUILDFLAG(IS_CHROMEOS)
bool BluetoothDeviceFloss::IsBonded() const {
  return IsBondedImpl();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool BluetoothDeviceFloss::IsConnected() const {
  return is_acl_connected_;
}

bool BluetoothDeviceFloss::IsGattConnected() const {
  return is_gatt_connected_;
}

bool BluetoothDeviceFloss::IsConnectable() const {
  // Mark all devices as connectable for now.
  // TODO(b/211126690): Implement based on supported profiles.
  return true;
}

bool BluetoothDeviceFloss::IsConnecting() const {
  return num_connecting_calls_ > 0;
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

  if (num_connecting_calls_++ == 0)
    adapter_->NotifyDeviceChanged(this);

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
  TriggerConnectCallback(BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
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
  BLUETOOTH_LOG(EVENT) << address_
                       << ": Connecting to service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketFloss> socket =
      BluetoothSocketFloss::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);

  socket->Connect(this, FlossSocketManager::Security::kSecure, uuid,
                  base::BindOnce(std::move(callback), socket),
                  base::BindOnce(&BluetoothDeviceFloss::OnConnectToServiceError,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(error_callback)));
}

void BluetoothDeviceFloss::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  BLUETOOTH_LOG(EVENT) << address_
                       << ": Connecting to service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketFloss> socket =
      BluetoothSocketFloss::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);

  socket->Connect(this, FlossSocketManager::Security::kInsecure, uuid,
                  base::BindOnce(std::move(callback), socket),
                  base::BindOnce(&BluetoothDeviceFloss::OnConnectToServiceError,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(error_callback)));
}

std::unique_ptr<device::BluetoothGattConnection>
BluetoothDeviceFloss::CreateBluetoothGattConnectionObject() {
  return std::make_unique<BluetoothGattConnectionFloss>(adapter_,
                                                        AsFlossDeviceId());
}

void BluetoothDeviceFloss::SetGattServicesDiscoveryComplete(bool complete) {
  NOTIMPLEMENTED();
  // This is not necessary for Floss which already knows of discovery complete.
}

bool BluetoothDeviceFloss::IsGattServicesDiscoveryComplete() const {
  // Services are only considered resolved if connection was established without
  // a specific search uuid or was subsequently upgraded to full discovery.
  return svc_resolved_ && !search_uuid.has_value();
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
  is_acl_connected_ = is_connected;

  // Update connection state to "ConnectedOnly" if it was previously
  // disconnected and we are now connected. Also, update any connection state
  // back to disconnected if acl state disconnects.
  if (is_acl_connected_ &&
      connection_state_ ==
          static_cast<uint32_t>(
              FlossAdapterClient::ConnectionState::kDisconnected)) {
    connection_state_ = static_cast<uint32_t>(
        FlossAdapterClient::ConnectionState::kConnectedOnly);
  } else if (!is_acl_connected_) {
    connection_state_ = static_cast<uint32_t>(
        FlossAdapterClient::ConnectionState::kDisconnected);
  }
}

void BluetoothDeviceFloss::SetConnectionState(uint32_t connection_state) {
  connection_state_ = connection_state;
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
  if (num_connecting_calls_++ == 0)
    adapter()->NotifyDeviceChanged(this);

  // Save the service uuid to trigger service discovery later.
  search_uuid = service_uuid;

  // Gatt connections establish over LE.
  FlossDBusManager::Get()->GetGattClient()->Connect(
      base::BindOnce(&BluetoothDeviceFloss::OnConnectGatt,
                     weak_ptr_factory_.GetWeakPtr()),
      address_, FlossDBusClient::BluetoothTransport::kLe);
}

void BluetoothDeviceFloss::OnConnectGatt(DBusResult<Void> ret) {
  if (!ret.has_value()) {
    if (--num_connecting_calls_ == 0)
      adapter()->NotifyDeviceChanged(this);
  }
}

void BluetoothDeviceFloss::UpgradeToFullDiscovery() {
  if (!search_uuid.has_value()) {
    LOG(ERROR) << "Attempting to upgrade to full discovery without having "
                  "searched any uuid.";
    return;
  }

  // Clear previous search uuid.
  search_uuid.reset();

  FlossDBusManager::Get()->GetGattClient()->DiscoverAllServices(
      base::DoNothing(), address_);
}

void BluetoothDeviceFloss::DisconnectGatt() {
  if (IsPaired()) {
    BLUETOOTH_LOG(ERROR) << "Leaking connection to paired device.";
    return;
  }

  FlossDBusManager::Get()->GetGattClient()->Disconnect(base::DoNothing(),
                                                       address_);
}

BluetoothDeviceFloss::BluetoothDeviceFloss(
    BluetoothAdapterFloss* adapter,
    const FlossDeviceId& device,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread)
    : BluetoothDevice(adapter),
      address_(device.address),
      name_(device.name),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread) {
  FlossDBusManager::Get()->GetGattClient()->AddObserver(this);

  // Enable service specific discovery. This allows gatt connections to
  // immediately trigger service discovery for specific uuids without
  // requiring full discovery.
  supports_service_specific_discovery_ = true;
}

BluetoothDeviceFloss::~BluetoothDeviceFloss() {
  FlossDBusManager::Get()->GetGattClient()->RemoveObserver(this);
}

bool BluetoothDeviceFloss::IsBondedImpl() const {
  return bond_state_ == FlossAdapterClient::BondState::kBonded;
}

void BluetoothDeviceFloss::OnGetRemoteType(
    DBusResult<FlossAdapterClient::BluetoothDeviceType> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "GetRemoteType() failed: " << ret.error();
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

void BluetoothDeviceFloss::OnGetRemoteClass(DBusResult<uint32_t> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "GetRemoteClass() failed: " << ret.error();
    return;
  }

  cod_ = *ret;
}

void BluetoothDeviceFloss::OnGetRemoteUuids(DBusResult<UUIDList> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "GetRemoteUuids() failed: " << ret.error();
    return;
  }

  device_uuids_.ReplaceServiceUUIDs(*ret);
}

void BluetoothDeviceFloss::OnConnectAllEnabledProfiles(DBusResult<Void> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to connect all enabled profiles: "
                         << ret.error();
    // TODO(b/202874707): Design a proper new errors for Floss.
    if (pending_callback_on_connect_profiles_)
      TriggerConnectCallback(BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
    return;
  }

  TriggerConnectCallback(absl::nullopt);
}

void BluetoothDeviceFloss::TriggerConnectCallback(
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);

  if (pending_callback_on_connect_profiles_) {
    std::move(*pending_callback_on_connect_profiles_).Run(error_code);
    pending_callback_on_connect_profiles_ = absl::nullopt;
  }
}

void BluetoothDeviceFloss::OnDisconnectAllEnabledProfiles(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
#if BUILDFLAG(IS_CHROMEOS)
    device::RecordUserInitiatedDisconnectResult(
        device::DisconnectResult::kFailure,
        /*transport=*/GetType());
#endif
    BLUETOOTH_LOG(ERROR) << "Failed to discconnect all enabled profiles: "
                         << ret.error();
    std::move(error_callback).Run();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  device::RecordUserInitiatedDisconnectResult(
      device::DisconnectResult::kSuccess,
      /*transport=*/GetType());
#endif

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnConnectToServiceError(
    ConnectToServiceErrorCallback error_callback,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << address_
                       << ": Failed to connect to service: " << error_message;

  // TODO - Log service connection failures for metrics.

  std::move(error_callback).Run(error_message);
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

void BluetoothDeviceFloss::GattClientConnectionState(GattStatus status,
                                                     int32_t client_id,
                                                     bool connected,
                                                     std::string address) {
  // We only care about connections for this device.
  if (address != address_)
    return;

  absl::optional<ConnectErrorCode> err = absl::nullopt;

  if (status == GattStatus::kSuccess) {
    is_gatt_connected_ = connected;
  } else {
    // TODO(b/193686094) - Convert GattStatus to other connect error codes.
    err = ERROR_UNKNOWN;
  }

  if (--num_connecting_calls_ == 0)
    adapter()->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);

  // Trigger service discovery.
  if (search_uuid.has_value()) {
    FlossDBusManager::Get()->GetGattClient()->DiscoverServiceByUuid(
        base::DoNothing(), address_, search_uuid.value());
  } else {
    FlossDBusManager::Get()->GetGattClient()->DiscoverAllServices(
        base::DoNothing(), address_);
  }

  // Complete GATT connection callback.
  DidConnectGatt(err);
}

void BluetoothDeviceFloss::GattSearchComplete(
    std::string address,
    const std::vector<GattService>& services,
    GattStatus status) {
  if (address != address_)
    return;

  if (status != GattStatus::kSuccess) {
    LOG(ERROR) << "Failed Gatt service discovery with result: "
               << static_cast<uint32_t>(status);
    return;
  }

  svc_resolved_ = true;

  for (const auto& service : services) {
    BLUETOOTH_LOG(EVENT) << "Adding new remote GATT service for device: "
                         << address_;

    std::unique_ptr<BluetoothRemoteGattServiceFloss> remote_service =
        BluetoothRemoteGattServiceFloss::Create(adapter(), this, service,
                                                /*primary=*/true);

    BluetoothRemoteGattServiceFloss* remote_service_ptr = remote_service.get();

    gatt_services_[remote_service_ptr->GetIdentifier()] =
        std::move(remote_service);
    DCHECK(remote_service_ptr->GetUUID().IsValid());
  }

  adapter()->NotifyGattServicesDiscovered(this);
}

}  // namespace floss
