// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_device_floss.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
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
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/floss/floss_socket_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif

namespace floss {

namespace {

// Connection intervals for LE connections.
// The unit for connection interval values are in multiples of 1.25ms.
const int32_t kMinConnectionIntervalLow = 6;
const int32_t kMaxConnectionIntervalLow = 6;
const int32_t kMinConnectionIntervalMedium = 40;
const int32_t kMaxConnectionIntervalMedium = 56;
const int32_t kMinConnectionIntervalHigh = 80;
const int32_t kMaxConnectionIntervalHigh = 100;

// Default connection latency for LE connections.
const int32_t kDefaultConnectionLatency = 0;

// Link supervision timeout for LE connections.
const int32_t kDefaultConnectionTimeout = 2000;

// Maximum MTU size that can be requested by Android.
const int32_t kMaxMtuSize = 517;

// Timeout for connection response after Connect() method is called.
constexpr base::TimeDelta kDefaultConnectTimeout = base::Seconds(10);

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
  return address_type_;
}

VendorIDSource BluetoothDeviceFloss::GetVendorIDSource() const {
  return static_cast<VendorIDSource>(vpi_.vendorIdSrc);
}

uint16_t BluetoothDeviceFloss::GetVendorID() const {
  return vpi_.vendorId;
}

uint16_t BluetoothDeviceFloss::GetProductID() const {
  return vpi_.productId;
}

uint16_t BluetoothDeviceFloss::GetDeviceID() const {
  return vpi_.version;
}

uint16_t BluetoothDeviceFloss::GetAppearance() const {
  return appearance_;
}

std::optional<std::string> BluetoothDeviceFloss::GetName() const {
  if (name_.length() == 0)
    return std::nullopt;

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
  return gatt_connecting_state_ == GattConnectingState::kGattConnected;
}

bool BluetoothDeviceFloss::IsConnectable() const {
  // Mimic current BlueZ behavior that Non-HID is connectable
  switch (GetDeviceType()) {
    case device::BluetoothDeviceType::PERIPHERAL:
    case device::BluetoothDeviceType::JOYSTICK:
    case device::BluetoothDeviceType::KEYBOARD:
    case device::BluetoothDeviceType::MOUSE:
    case device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return false;
    default:
      return true;
  }
}

bool BluetoothDeviceFloss::IsConnecting() const {
  return (connecting_state_ == ConnectingState::kACLConnecting) ||
         (connecting_state_ == ConnectingState::kProfilesConnecting) ||
         (gatt_connecting_state_ == GattConnectingState::kGattConnecting);
}

device::BluetoothDevice::UUIDSet BluetoothDeviceFloss::GetUUIDs() const {
  return device_uuids_.GetUUIDs();
}

std::optional<int8_t> BluetoothDeviceFloss::GetInquiryTxPower() const {
  NOTIMPLEMENTED();

  return std::nullopt;
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
  // TODO(b/255650738): Floss doesn't currently provide max_transmit_power.
  std::move(callback).Run(ConnectionInfo(inquiry_rssi_.value_or(0),
                                         inquiry_tx_power_.value_or(0),
                                         /*max_transmit_power=*/0));
}

void BluetoothDeviceFloss::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  int32_t min_connection_interval = kMinConnectionIntervalMedium;
  int32_t max_connection_interval = kMaxConnectionIntervalMedium;

  switch (connection_latency) {
    case ConnectionLatency::CONNECTION_LATENCY_LOW:
      min_connection_interval = kMinConnectionIntervalLow;
      max_connection_interval = kMaxConnectionIntervalLow;
      break;
    case ConnectionLatency::CONNECTION_LATENCY_MEDIUM:
      min_connection_interval = kMinConnectionIntervalMedium;
      max_connection_interval = kMaxConnectionIntervalMedium;
      break;
    case ConnectionLatency::CONNECTION_LATENCY_HIGH:
      min_connection_interval = kMinConnectionIntervalHigh;
      max_connection_interval = kMaxConnectionIntervalHigh;
      break;
    default:
      NOTREACHED();
  }

  BLUETOOTH_LOG(EVENT) << "Setting LE connection parameters: min="
                       << min_connection_interval
                       << ", max=" << max_connection_interval;

  FlossDBusManager::Get()->GetGattManagerClient()->UpdateConnectionParameters(
      base::BindOnce(&BluetoothDeviceFloss::OnSetConnectionLatency,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      GetAddress(), min_connection_interval, max_connection_interval,
      kDefaultConnectionLatency, kDefaultConnectionTimeout,
      /*min_ce_len=*/min_connection_interval * 2,
      /*max_ce_len=*/max_connection_interval * 2);
}

void BluetoothDeviceFloss::OnSetConnectionLatency(base::OnceClosure callback,
                                                  ErrorCallback error_callback,
                                                  DBusResult<Void> ret) {
  if (!ret.has_value()) {
    std::move(error_callback).Run();
    return;
  }

  // If we already had a pending call, fail it.
  if (pending_set_connection_latency_.has_value()) {
    auto& [pending_cb, pending_error_cb] =
        pending_set_connection_latency_.value();
    std::move(pending_error_cb).Run();
    pending_set_connection_latency_ = std::nullopt;
  }

  // If there is no active connection, UpdateConnectionParameters succeeds
  // silently and won't generates any callbacks. Run callback right here.
  if (!IsConnected()) {
    std::move(callback).Run();
    return;
  }

  pending_set_connection_latency_ =
      std::make_pair(std::move(callback), std::move(error_callback));
}

void BluetoothDeviceFloss::Connect(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback) {
  ConnectWithTransport(pairing_delegate, std::move(callback),
                       FlossAdapterClient::BluetoothTransport::kAuto);
}

void BluetoothDeviceFloss::ConnectWithTransport(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback,
    FlossAdapterClient::BluetoothTransport transport) {
  BLUETOOTH_LOG(EVENT) << "Connecting to " << address_;

  if ((connecting_state_ == ConnectingState::kACLConnecting) ||
      (connecting_state_ == ConnectingState::kProfilesConnecting) ||
      (pending_callback_on_connect_profiles_ != std::nullopt)) {
    std::move(callback).Run(
        BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS);
    return;
  } else if (connecting_state_ == ConnectingState::kProfilesConnected) {
    std::move(callback).Run(
        BluetoothDevice::ConnectErrorCode::ERROR_ALREADY_CONNECTED);
    return;
  }

  // To simulate BlueZ API behavior, we don't reply the callback as soon as
  // Floss CreateBond API returns success, but rather we trigger the callback
  // later after pairing is done and profiles are connected. In the event of
  // immediate failure OnCreateBond will handle invoking the callback.
  pending_callback_on_connect_profiles_ = std::move(callback);

  if (IsPaired() || !pairing_delegate) {
    // No need to pair, or unable to, skip straight to connection.
    ConnectAllEnabledProfiles();
  } else {
    pairing_ = std::make_unique<BluetoothPairingFloss>(pairing_delegate);
    if (FlossDBusManager::Get()->GetFlossApiVersion() >=
        base::Version("0.4.0")) {
      FlossDBusManager::Get()->GetAdapterClient()->CreateBond(
          base::BindOnce(static_cast<void (BluetoothDeviceFloss::*)(
                             DBusResult<FlossDBusClient::BtifStatus>)>(
                             &BluetoothDeviceFloss::OnCreateBond),
                         weak_ptr_factory_.GetWeakPtr()),
          AsFlossDeviceId(), transport);
    } else {
      FlossDBusManager::Get()->GetAdapterClient()->CreateBond(
          base::BindOnce(
              static_cast<void (BluetoothDeviceFloss::*)(DBusResult<bool>)>(
                  &BluetoothDeviceFloss::OnCreateBond),
              weak_ptr_factory_.GetWeakPtr()),
          AsFlossDeviceId(), transport);
    }
  }
}

void BluetoothDeviceFloss::OnCreateBond(DBusResult<bool> ret) {
  if (ret.has_value() && !*ret) {
    BLUETOOTH_LOG(ERROR) << "CreateBond returned failure";
    TriggerConnectCallback(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
  } else if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to create bond: " << ret.error();
    TriggerConnectCallback(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
  }
}

void BluetoothDeviceFloss::OnCreateBond(
    DBusResult<FlossDBusClient::BtifStatus> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to create bond, D-Bus error: "
                         << ret.error();
    TriggerConnectCallback(BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
    return;
  }

  if (*ret != FlossDBusClient::BtifStatus::kSuccess) {
    BLUETOOTH_LOG(ERROR) << "Failed to create bond, status: "
                         << static_cast<uint32_t>(*ret);
    TriggerConnectCallback(FlossDBusClient::BtifStatusToConnectErrorCode(*ret));
    return;
  }
}

void BluetoothDeviceFloss::ConnectionIncomplete() {
  UpdateConnectingState(
      ConnectingState::kIdle,
      BluetoothDevice::ConnectErrorCode::ERROR_NON_AUTH_TIMEOUT);
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothDeviceFloss::ConnectClassic(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback) {
  ConnectWithTransport(pairing_delegate, std::move(callback),
                       FlossAdapterClient::BluetoothTransport::kBrEdr);
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
                                 weak_ptr_factory_.GetWeakPtr(), socket,
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
                                 weak_ptr_factory_.GetWeakPtr(), socket,
                                 std::move(error_callback)));
}

std::unique_ptr<device::BluetoothGattConnection>
BluetoothDeviceFloss::CreateBluetoothGattConnectionObject() {
  return std::make_unique<BluetoothGattConnectionFloss>(adapter_,
                                                        AsFlossDeviceId());
}

void BluetoothDeviceFloss::SetGattServicesDiscoveryComplete(bool complete) {
  // This API actually refers to all services including SDP, not just GATT.
  // This function is called by BluetoothSocketFloss because a connected socket
  // implies SDP is completed, but Floss doesn't emit the UUIDs to the clients.
  if (complete && !svc_resolved_) {
    svc_resolved_ = true;
    adapter()->NotifyGattServicesDiscovered(this);
  } else if (!complete) {
    svc_resolved_ = false;
  }
}

bool BluetoothDeviceFloss::IsGattServicesDiscoveryComplete() const {
  // This API actually refers to all services including SDP, not just GATT.
  // In GATT case, services are only considered resolved if connection was
  // established without a specific search uuid or was subsequently upgraded to
  // full discovery.
  return svc_resolved_ && !search_uuid.has_value();
}

void BluetoothDeviceFloss::Pair(
    device::BluetoothDevice::PairingDelegate* pairing_delegate,
    ConnectCallback callback) {
  // Pair is the same as Connect due to influence from BlueZ.
  // TODO(b/269516642): We should make distinction between them in the future.
  Connect(pairing_delegate, std::move(callback));
}

BluetoothPairingFloss* BluetoothDeviceFloss::BeginPairing(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  pairing_ = std::make_unique<BluetoothPairingFloss>(pairing_delegate);
  return pairing_.get();
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothDeviceFloss::OnExecuteWrite(
    base::OnceClosure callback,
    ExecuteWriteErrorCallback error_callback,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }

  pending_execute_write_ =
      std::make_pair(std::move(callback), std::move(error_callback));
}

void BluetoothDeviceFloss::BeginReliableWrite() {
  DCHECK(!using_reliable_write_);

  if (!using_reliable_write_) {
    using_reliable_write_ = true;

    FlossDBusManager::Get()->GetGattManagerClient()->BeginReliableWrite(
        base::DoNothing(), address_);
  }
}

void BluetoothDeviceFloss::ExecuteWrite(
    base::OnceClosure callback,
    ExecuteWriteErrorCallback error_callback) {
  // Only one pending execute allowed at a time.
  if (pending_execute_write_) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kInProgress);
    return;
  }

  if (!using_reliable_write_) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }

  FlossDBusManager::Get()->GetGattManagerClient()->EndReliableWrite(
      base::BindOnce(&BluetoothDeviceFloss::OnExecuteWrite,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      address_, /*execute=*/true);
}

void BluetoothDeviceFloss::AbortWrite(base::OnceClosure callback,
                                      AbortWriteErrorCallback error_callback) {
  // Only one pending execute allowed at a time.
  if (pending_execute_write_) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kInProgress);
    return;
  }

  if (!using_reliable_write_) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }

  FlossDBusManager::Get()->GetGattManagerClient()->EndReliableWrite(
      base::BindOnce(&BluetoothDeviceFloss::OnExecuteWrite,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      address_, /*execute=*/false);
}

void BluetoothDeviceFloss::GattExecuteWrite(std::string address,
                                            GattStatus status) {
  if (address != address_) {
    return;
  }

  if (!pending_execute_write_) {
    return;
  }

  if (status != GattStatus::kSuccess) {
    std::move(pending_execute_write_->second)
        .Run(
            floss::BluetoothGattServiceFloss::GattStatusToServiceError(status));
  } else {
    std::move(pending_execute_write_->first).Run();
  }

  pending_execute_write_ = std::nullopt;
}

#endif  // BUILDFLAG(IS_CHROMEOS)

FlossDeviceId BluetoothDeviceFloss::AsFlossDeviceId() const {
  return FlossDeviceId{.address = address_, .name = name_};
}

void BluetoothDeviceFloss::SetName(const std::string& name) {
  name_ = name;
}

void BluetoothDeviceFloss::SetBondState(
    FlossAdapterClient::BondState bond_state,
    std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  bond_state_ = bond_state;

  switch (bond_state_) {
    case FlossAdapterClient::BondState::kNotBonded:
      UpdateConnectingState(ConnectingState::kIdle, error_code);
      break;
    case FlossAdapterClient::BondState::kBondingInProgress:
      UpdateConnectingState(ConnectingState::kACLConnecting, std::nullopt);
      break;
    case FlossAdapterClient::BondState::kBonded:
      if (connecting_state_ == ConnectingState::kACLConnecting) {
        ConnectAllEnabledProfiles();
      }
      break;
    default:
      NOTREACHED();
  }
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

  if (!is_connected) {
    UpdateConnectingState(
        ConnectingState::kIdle,
        BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_UNCONNECTED);
  } else if (is_connected &&
             connecting_state_ == ConnectingState::kProfilesConnecting) {
    UpdateConnectingState(ConnectingState::kProfilesConnected, std::nullopt);
  }
}

void BluetoothDeviceFloss::SetConnectionState(uint32_t connection_state) {
  connection_state_ = connection_state;
}

void BluetoothDeviceFloss::ConnectAllEnabledProfiles() {
  UpdateConnectingState(ConnectingState::kProfilesConnecting, std::nullopt);

  connection_incomplete_timer_.Start(
      FROM_HERE, kDefaultConnectTimeout,
      base::BindOnce(&BluetoothDeviceFloss::ConnectionIncomplete,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FlossDBusManager::Get()->GetFlossApiVersion() >= base::Version("0.4.0")) {
    FlossDBusManager::Get()->GetAdapterClient()->ConnectAllEnabledProfiles(
        base::BindOnce(static_cast<void (BluetoothDeviceFloss::*)(
                           DBusResult<FlossDBusClient::BtifStatus>)>(
                           &BluetoothDeviceFloss::OnConnectAllEnabledProfiles),
                       weak_ptr_factory_.GetWeakPtr()),
        AsFlossDeviceId());
  } else {
    FlossDBusManager::Get()->GetAdapterClient()->ConnectAllEnabledProfiles(
        base::BindOnce(
            static_cast<void (BluetoothDeviceFloss::*)(DBusResult<Void>)>(
                &BluetoothDeviceFloss::OnConnectAllEnabledProfiles),
            weak_ptr_factory_.GetWeakPtr()),
        AsFlossDeviceId());
  }
}

void BluetoothDeviceFloss::ResetPairing() {
  pairing_.reset();
}

void BluetoothDeviceFloss::CreateGattConnectionImpl(
    std::optional<device::BluetoothUUID> service_uuid) {
  // Generally, the first ever connection to a device should be direct and
  // subsequent connections to known devices should be invoked with is_direct =
  // false. Refer to |autoConnect| on BluetoothGatt.java.
  bool is_direct =
      gatt_connecting_state_ == GattConnectingState::kGattConnectionInit ||
      !IsBondedImpl();

  UpdateGattConnectingState(GattConnectingState::kGattConnecting);

  // Save the service uuid to trigger service discovery later.
  search_uuid = service_uuid;

  // Gatt connections establish over LE.
  FlossDBusManager::Get()->GetGattManagerClient()->Connect(
      base::BindOnce(&BluetoothDeviceFloss::OnConnectGatt,
                     weak_ptr_factory_.GetWeakPtr()),
      address_, FlossDBusClient::BluetoothTransport::kLe, is_direct);
}

void BluetoothDeviceFloss::OnConnectGatt(DBusResult<Void> ret) {
  if (!ret.has_value()) {
    UpdateGattConnectingState(GattConnectingState::kGattDisconnected);
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
  svc_resolved_ = false;

  FlossDBusManager::Get()->GetGattManagerClient()->DiscoverAllServices(
      base::DoNothing(), address_);
}

void BluetoothDeviceFloss::DisconnectGatt() {
  svc_resolved_ = false;
  FlossDBusManager::Get()->GetGattManagerClient()->Disconnect(base::DoNothing(),
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
  FlossDBusManager::Get()->GetGattManagerClient()->AddObserver(this);

  // Enable service specific discovery. This allows gatt connections to
  // immediately trigger service discovery for specific uuids without
  // requiring full discovery.
  supports_service_specific_discovery_ = true;
}

BluetoothDeviceFloss::~BluetoothDeviceFloss() {
  FlossDBusManager::Get()->GetGattManagerClient()->RemoveObserver(this);
}

bool BluetoothDeviceFloss::IsBondedImpl() const {
  return bond_state_ == FlossAdapterClient::BondState::kBonded;
}

void BluetoothDeviceFloss::OnGetRemoteType(
    base::OnceClosure callback,
    DBusResult<FlossAdapterClient::BluetoothDeviceType> ret) {
  if (ret.has_value()) {
    switch (*ret) {
      case FlossAdapterClient::BluetoothDeviceType::kBle:
        transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE;
        break;
      case FlossAdapterClient::BluetoothDeviceType::kDual:
        transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL;
        break;
      // Default to BrEdr. ARC++ for example doesn't know how to translate the
      // Invalid state.
      case FlossAdapterClient::BluetoothDeviceType::kBredr:
        [[fallthrough]];
      default:
        transport_ = device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC;
        break;
    }
  } else {
    BLUETOOTH_LOG(ERROR) << "GetRemoteType() failed: " << ret.error();
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnGetRemoteClass(base::OnceClosure callback,
                                            DBusResult<uint32_t> ret) {
  if (ret.has_value()) {
    cod_ = *ret;
  } else {
    BLUETOOTH_LOG(ERROR) << "GetRemoteClass() failed: " << ret.error();
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnGetRemoteAppearance(base::OnceClosure callback,
                                                 DBusResult<uint16_t> ret) {
  if (ret.has_value()) {
    appearance_ = *ret;
  } else {
    BLUETOOTH_LOG(ERROR) << "GetRemoteAppearance() failed: " << ret.error();
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnGetRemoteUuids(base::OnceClosure callback,
                                            DBusResult<UUIDList> ret) {
  if (ret.has_value()) {
    device_uuids_.ReplaceServiceUUIDs(*ret);
  } else {
    BLUETOOTH_LOG(ERROR) << "GetRemoteUuids() failed: " << ret.error();
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnGetRemoteVendorProductInfo(
    base::OnceClosure callback,
    DBusResult<FlossAdapterClient::VendorProductInfo> ret) {
  if (ret.has_value()) {
    vpi_ = *ret;
    if (vpi_.vendorIdSrc >
        static_cast<uint8_t>(VendorIDSource::VENDOR_ID_MAX_VALUE)) {
      vpi_.vendorIdSrc =
          static_cast<uint8_t>(VendorIDSource::VENDOR_ID_UNKNOWN);
    }
  } else {
    BLUETOOTH_LOG(ERROR) << "GetRemoteVendorProductInfo() failed: "
                         << ret.error();
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnGetRemoteAddressType(
    base::OnceClosure callback,
    DBusResult<FlossAdapterClient::BtAddressType> ret) {
  if (ret.has_value()) {
    switch (*ret) {
      case FlossAdapterClient::BtAddressType::kPublic:
      case FlossAdapterClient::BtAddressType::kPublicId:
        address_type_ = AddressType::ADDR_TYPE_PUBLIC;
        break;
      case FlossAdapterClient::BtAddressType::kRandom:
      case FlossAdapterClient::BtAddressType::kRandomId:
        address_type_ = AddressType::ADDR_TYPE_RANDOM;
        break;
      default:
        address_type_ = AddressType::ADDR_TYPE_UNKNOWN;
        break;
    }
  } else {
    BLUETOOTH_LOG(ERROR) << "GetRemoteAddressType() failed: " << ret.error();
  }

  std::move(callback).Run();
}

void BluetoothDeviceFloss::OnConnectAllEnabledProfiles(DBusResult<Void> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed to connect all enabled profiles: "
                         << ret.error();
    // TODO(b/202874707): Design a proper new errors for Floss.
    UpdateConnectingState(ConnectingState::kIdle,
                          BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
    return;
  }

  // Floss does not send any notifications that profiles have successfully
  // connected if we are already ACL connected.
  if (is_acl_connected_) {
    UpdateConnectingState(ConnectingState::kProfilesConnected, std::nullopt);
  }
}

void BluetoothDeviceFloss::OnConnectAllEnabledProfiles(
    DBusResult<FlossDBusClient::BtifStatus> ret) {
  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR)
        << "Failed to connect all enabled profiles, D-Bus error: "
        << ret.error();
    UpdateConnectingState(ConnectingState::kIdle,
                          BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
    return;
  }

  if (*ret != FlossDBusClient::BtifStatus::kSuccess) {
    BLUETOOTH_LOG(ERROR) << "Failed to connect all enabled profiles, status: "
                         << static_cast<uint32_t>(*ret);
    UpdateConnectingState(ConnectingState::kIdle,
                          FlossDBusClient::BtifStatusToConnectErrorCode(*ret));
    return;
  }

  // Floss does not send any notifications that profiles have successfully
  // connected if we are already ACL connected.
  if (is_acl_connected_) {
    UpdateConnectingState(ConnectingState::kProfilesConnected, std::nullopt);
  }
}

void BluetoothDeviceFloss::UpdateConnectingState(
    ConnectingState state,
    std::optional<BluetoothDevice::ConnectErrorCode> error) {
  if ((state == ConnectingState::kIdle) &&
      ((connecting_state_ == ConnectingState::kACLConnecting) ||
       (connecting_state_ == ConnectingState::kProfilesConnecting))) {
    // Something went wrong during connecting
    TriggerConnectCallback(error);
  } else if ((state == ConnectingState::kProfilesConnected) &&
             (connecting_state_ == ConnectingState::kProfilesConnecting)) {
    // Successful profile connection
    TriggerConnectCallback(std::nullopt);
  }

  if (connecting_state_ != state) {
    connecting_state_ = state;
    adapter_->NotifyDeviceChanged(this);
  }
}

void BluetoothDeviceFloss::UpdateGattConnectingState(
    GattConnectingState state) {
  if (gatt_connecting_state_ != state) {
    gatt_connecting_state_ = state;
    adapter_->NotifyDeviceChanged(this);
  }
}

void BluetoothDeviceFloss::TriggerConnectCallback(
    std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  connection_incomplete_timer_.Stop();

  if (pending_callback_on_connect_profiles_) {
    // We need to move it first and set pending_callback_on_connect_profiles_
    // to nullopt before Run-ing the callback, because this may trigger arriving
    // at this same location.
    auto callback = std::move(*pending_callback_on_connect_profiles_);
    pending_callback_on_connect_profiles_ = std::nullopt;
    std::move(callback).Run(error_code);
  }

  pairing_ = nullptr;
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
    scoped_refptr<BluetoothSocketFloss> socket,
    ConnectToServiceErrorCallback error_callback,
    const std::string& error_message) {
  BLUETOOTH_LOG(ERROR) << address_
                       << ": Failed to connect to service: " << error_message;

  // TODO - Log service connection failures for metrics.

  std::move(error_callback).Run(error_message);
}

void BluetoothDeviceFloss::FetchRemoteType(base::OnceClosure callback) {
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteType(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteType,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::FetchRemoteClass(base::OnceClosure callback) {
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteClass(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteClass,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::FetchRemoteAppearance(base::OnceClosure callback) {
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteAppearance(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteAppearance,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::FetchRemoteUuids(base::OnceClosure callback) {
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteUuids(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteUuids,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::FetchRemoteVendorProductInfo(
    base::OnceClosure callback) {
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteVendorProductInfo(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteVendorProductInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::FetchRemoteAddressType(base::OnceClosure callback) {
  FlossDBusManager::Get()->GetAdapterClient()->GetRemoteAddressType(
      base::BindOnce(&BluetoothDeviceFloss::OnGetRemoteAddressType,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      AsFlossDeviceId());
}

void BluetoothDeviceFloss::InitializeDeviceProperties(
    PropertiesState state,
    base::OnceClosure callback) {
  // If a property read is already active, don't re-run it.
  if (IsReadingProperties()) {
    return;
  }

  property_reads_triggered_ = state;
  pending_callback_on_init_props_ = std::move(callback);
  // This must be incremented when adding more properties below
  // and followed up with a TriggerInitDevicePropertiesCallback()
  // in the callback.
  num_pending_properties_ += 6;
  FetchRemoteType(
      base::BindOnce(&BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  FetchRemoteClass(
      base::BindOnce(&BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  FetchRemoteAppearance(
      base::BindOnce(&BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  FetchRemoteUuids(
      base::BindOnce(&BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  FetchRemoteVendorProductInfo(
      base::BindOnce(&BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  FetchRemoteAddressType(
      base::BindOnce(&BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceFloss::TriggerInitDevicePropertiesCallback() {
  if (--num_pending_properties_ == 0 && pending_callback_on_init_props_) {
    property_reads_completed_ = static_cast<PropertiesState>(
        property_reads_completed_ | property_reads_triggered_);
    property_reads_triggered_ = PropertiesState::kNotRead;
    std::move(*pending_callback_on_init_props_).Run();
    pending_callback_on_init_props_ = std::nullopt;
  }

  DCHECK(num_pending_properties_ >= 0);
}

void BluetoothDeviceFloss::GattClientConnectionState(GattStatus status,
                                                     int32_t client_id,
                                                     bool connected,
                                                     std::string address) {
  // We only care about connections for this device.
  if (address != address_)
    return;

  std::optional<ConnectErrorCode> err = std::nullopt;

  if (status != GattStatus::kSuccess) {
    // TODO(b/193686094) - Convert GattStatus to other connect error codes.
    err = ERROR_UNKNOWN;
  }

  if (connected) {
    UpdateGattConnectingState(GattConnectingState::kGattConnected);

    // Request for maximum MTU only when connected.
    FlossDBusManager::Get()->GetGattManagerClient()->ConfigureMTU(
        base::DoNothing(), address_, kMaxMtuSize);
    return;
  }

  UpdateGattConnectingState(GattConnectingState::kGattDisconnected);

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

  // Copy the GATT services list here and clear the original so that when we
  // send GattServiceRemoved(), GetGattServices() returns no services.
  GattServiceMap gatt_services_swapped;
  gatt_services_swapped.swap(gatt_services_);

  for (const auto& service : services) {
    BLUETOOTH_LOG(EVENT) << "Adding new remote GATT service for device: "
                         << address_;

    std::unique_ptr<BluetoothRemoteGattServiceFloss> remote_service =
        BluetoothRemoteGattServiceFloss::Create(adapter(), this, service);

    BluetoothRemoteGattServiceFloss* remote_service_ptr = remote_service.get();

    gatt_services_[remote_service_ptr->GetIdentifier()] =
        std::move(remote_service);
    DCHECK(remote_service_ptr->GetUUID().IsValid());

    // GattDiscoveryCompleteForService is deprecated. Currently only Fast Pair
    // needs to listen to this.
    //
    // TODO(b/269478974): We should remove this once all callers migrate to
    // GattServicesDiscovered.
    adapter()->NotifyGattDiscoveryComplete(remote_service_ptr);
  }

  adapter()->NotifyGattServicesDiscovered(this);
}

void BluetoothDeviceFloss::GattConnectionUpdated(std::string address,
                                                 int32_t interval,
                                                 int32_t latency,
                                                 int32_t timeout,
                                                 GattStatus status) {
  if (address != GetAddress())
    return;

  VLOG(1) << "Gatt connection updated on " << GetAddress()
          << " with status=" << static_cast<uint32_t>(status);

  if (pending_set_connection_latency_.has_value()) {
    auto& [pending_cb, pending_error_cb] =
        pending_set_connection_latency_.value();
    if (status == GattStatus::kSuccess) {
      std::move(pending_cb).Run();
    } else {
      std::move(pending_error_cb).Run();
    }

    pending_set_connection_latency_ = std::nullopt;
  }
}

void BluetoothDeviceFloss::GattConfigureMtu(std::string address,
                                            int32_t mtu,
                                            GattStatus status) {
  if (address != GetAddress())
    return;

  VLOG(1) << "GattConfigureMtu on " << GetAddress() << "; mtu=" << mtu
          << "; status=" << static_cast<uint32_t>(status);

  // Discover services after configuring MTU
  // This can be done even if configuring MTU failed.
  if (search_uuid.has_value()) {
    FlossDBusManager::Get()->GetGattManagerClient()->DiscoverServiceByUuid(
        base::DoNothing(), address_, search_uuid.value());
  } else if (!IsGattServicesDiscoveryComplete()) {
    FlossDBusManager::Get()->GetGattManagerClient()->DiscoverAllServices(
        base::DoNothing(), address_);
  }

  DidConnectGatt(std::nullopt);
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothDeviceFloss::GattServiceChanged(std::string address) {
  if (address != GetAddress()) {
    return;
  }

  adapter()->NotifyGattNeedsDiscovery(this);
}
#endif

}  // namespace floss
