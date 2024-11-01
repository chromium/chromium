// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_ble_connection.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/cable/fido_ble_uuids.h"

namespace device {

namespace {

using ServiceRevisionsCallback =
    base::OnceCallback<void(std::vector<FidoBleConnection::ServiceRevision>)>;

constexpr const char* ToString(BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      return "ERROR_AUTH_CANCELED";
    case BluetoothDevice::ERROR_AUTH_FAILED:
      return "ERROR_AUTH_FAILED";
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      return "ERROR_AUTH_REJECTED";
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      return "ERROR_AUTH_TIMEOUT";
    case BluetoothDevice::ERROR_FAILED:
      return "ERROR_FAILED";
    case BluetoothDevice::ERROR_INPROGRESS:
      return "ERROR_INPROGRESS";
    case BluetoothDevice::ERROR_UNKNOWN:
      return "ERROR_UNKNOWN";
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      return "ERROR_UNSUPPORTED_DEVICE";
    default:
      NOTREACHED();
  }
}

constexpr const char* ToString(BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case BluetoothGattService::GattErrorCode::kUnknown:
      return "GATT_ERROR_UNKNOWN";
    case BluetoothGattService::GattErrorCode::kFailed:
      return "GATT_ERROR_FAILED";
    case BluetoothGattService::GattErrorCode::kInProgress:
      return "GATT_ERROR_IN_PROGRESS";
    case BluetoothGattService::GattErrorCode::kInvalidLength:
      return "GATT_ERROR_INVALID_LENGTH";
    case BluetoothGattService::GattErrorCode::kNotPermitted:
      return "GATT_ERROR_NOT_PERMITTED";
    case BluetoothGattService::GattErrorCode::kNotAuthorized:
      return "GATT_ERROR_NOT_AUTHORIZED";
    case BluetoothGattService::GattErrorCode::kNotPaired:
      return "GATT_ERROR_NOT_PAIRED";
    case BluetoothGattService::GattErrorCode::kNotSupported:
      return "GATT_ERROR_NOT_SUPPORTED";
    default:
      NOTREACHED();
  }
}

std::ostream& operator<<(std::ostream& os,
                         FidoBleConnection::ServiceRevision revision) {
  switch (revision) {
    case FidoBleConnection::ServiceRevision::kU2f11:
      return os << "U2F 1.1";
    case FidoBleConnection::ServiceRevision::kU2f12:
      return os << "U2F 1.2";
    case FidoBleConnection::ServiceRevision::kFido2:
      return os << "FIDO2";
  }

  NOTREACHED();
}

void OnWriteRemoteCharacteristic(FidoBleConnection::WriteCallback callback) {
  FIDO_LOG(DEBUG) << "Writing Remote Characteristic Succeeded.";
  std::move(callback).Run(true);
}

void OnWriteRemoteCharacteristicError(
    FidoBleConnection::WriteCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  FIDO_LOG(ERROR) << "Writing Remote Characteristic Failed: "
                  << ToString(error_code);
  std::move(callback).Run(false);
}

void OnReadServiceRevisionBitfield(
    ServiceRevisionsCallback callback,
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  if (error_code.has_value()) {
    FIDO_LOG(ERROR) << "Error while reading Service Revision Bitfield: "
                    << ToString(error_code.value());
    std::move(callback).Run({});
    return;
  }
  if (value.empty()) {
    FIDO_LOG(DEBUG) << "Service Revision Bitfield is empty.";
    std::move(callback).Run({});
    return;
  }

  if (value.size() != 1u) {
    FIDO_LOG(DEBUG) << "Service Revision Bitfield has unexpected size: "
                    << value.size() << ". Ignoring all but the first byte.";
  }

  const uint8_t bitset = value[0];
  if (bitset & 0x1F) {
    FIDO_LOG(DEBUG) << "Service Revision Bitfield has unexpected bits set: "
                    << base::StringPrintf("0x%02X", bitset)
                    << ". Ignoring all but the first three bits.";
  }

  std::vector<FidoBleConnection::ServiceRevision> service_revisions;
  for (auto revision : {FidoBleConnection::ServiceRevision::kU2f11,
                        FidoBleConnection::ServiceRevision::kU2f12,
                        FidoBleConnection::ServiceRevision::kFido2}) {
    if (bitset & static_cast<uint8_t>(revision)) {
      FIDO_LOG(DEBUG) << "Detected Support for " << revision << ".";
      service_revisions.push_back(revision);
    }
  }

  std::move(callback).Run(std::move(service_revisions));
}

}  // namespace

FidoBleConnection::FidoBleConnection(BluetoothAdapter* adapter,
                                     std::string device_address,
                                     BluetoothUUID service_uuid,
                                     ReadCallback read_callback)
    : adapter_(adapter),
      address_(std::move(device_address)),
      read_callback_(std::move(read_callback)),
      service_uuid_(service_uuid) {
  DCHECK(adapter_);
  adapter_->AddObserver(this);
  DCHECK(!address_.empty());
}

FidoBleConnection::~FidoBleConnection() {
  adapter_->RemoveObserver(this);
}

BluetoothDevice* FidoBleConnection::GetBleDevice() {
  return adapter_->GetDevice(address());
}

const BluetoothDevice* FidoBleConnection::GetBleDevice() const {
  return adapter_->GetDevice(address());
}

void FidoBleConnection::Connect(ConnectionCallback callback) {
  auto* device = GetBleDevice();
  if (!device) {
    FIDO_LOG(ERROR) << "Failed to get Device.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  pending_connection_callback_ = std::move(callback);
  FIDO_LOG(DEBUG) << "Creating a GATT connection...";
  device->CreateGattConnection(
      base::BindOnce(&FidoBleConnection::OnCreateGattConnection,
                     weak_factory_.GetWeakPtr()),
      BluetoothUUID(kGoogleCableUUID128));
}

void FidoBleConnection::ReadControlPointLength(
    ControlPointLengthCallback callback) {
  const auto* fido_service = GetFidoService();
  if (!fido_service) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  if (!control_point_length_id_) {
    FIDO_LOG(ERROR) << "Failed to get Control Point Length.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  BluetoothRemoteGattCharacteristic* control_point_length =
      fido_service->GetCharacteristic(*control_point_length_id_);
  if (!control_point_length) {
    FIDO_LOG(ERROR) << "No Control Point Length characteristic present.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  FIDO_LOG(DEBUG) << "Read Control Point Length";
  control_point_length->ReadRemoteCharacteristic(
      base::BindOnce(OnReadControlPointLength, std::move(callback)));
}

void FidoBleConnection::WriteControlPoint(const std::vector<uint8_t>& data,
                                          WriteCallback callback) {
  const auto* fido_service = GetFidoService();
  if (!fido_service) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (!control_point_id_) {
    FIDO_LOG(ERROR) << "Failed to get Control Point.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  BluetoothRemoteGattCharacteristic* control_point =
      fido_service->GetCharacteristic(*control_point_id_);
  if (!control_point) {
    FIDO_LOG(ERROR) << "Control Point characteristic not present.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Attempt a write without response for performance reasons. Fall back to a
  // confirmed write when the characteristic does not provide the required
  // property.
  BluetoothRemoteGattCharacteristic::WriteType write_type =
      (control_point->GetProperties() &
       BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE)
          ? BluetoothRemoteGattCharacteristic::WriteType::kWithoutResponse
          : BluetoothRemoteGattCharacteristic::WriteType::kWithResponse;

  FIDO_LOG(DEBUG) << "Wrote Control Point.";
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  control_point->WriteRemoteCharacteristic(
      data, write_type,
      base::BindOnce(OnWriteRemoteCharacteristic,
                     std::move(split_callback.first)),
      base::BindOnce(OnWriteRemoteCharacteristicError,
                     std::move(split_callback.second)));
}

void FidoBleConnection::OnCreateGattConnection(
    std::unique_ptr<BluetoothGattConnection> connection,
    std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  FIDO_LOG(DEBUG) << "GATT connection created";
  DCHECK(pending_connection_callback_);
  if (error_code.has_value()) {
    FIDO_LOG(ERROR) << "CreateGattConnection() failed: "
                    << ToString(error_code.value());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }
  connection_ = std::move(connection);

  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (!device) {
    FIDO_LOG(ERROR) << "Failed to get Device.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  if (!device->IsGattServicesDiscoveryComplete()) {
    FIDO_LOG(DEBUG) << "Waiting for GATT service discovery to complete";
    waiting_for_gatt_discovery_ = true;
    return;
  }

  ConnectToFidoService();
}

void FidoBleConnection::ConnectToFidoService() {
  FIDO_LOG(EVENT) << "Attempting to connect to a Fido service.";
  DCHECK(pending_connection_callback_);
  const auto* fido_service = GetFidoService();
  if (!fido_service) {
    FIDO_LOG(ERROR) << "Failed to get Fido Service.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  for (const auto* characteristic : fido_service->GetCharacteristics()) {
    std::string uuid = characteristic->GetUUID().canonical_value();
    if (uuid == kFidoControlPointLengthUUID) {
      control_point_length_id_ = characteristic->GetIdentifier();
      FIDO_LOG(DEBUG) << "Got Fido Control Point Length: "
                      << *control_point_length_id_;
      continue;
    }

    if (uuid == kFidoControlPointUUID) {
      control_point_id_ = characteristic->GetIdentifier();
      FIDO_LOG(DEBUG) << "Got Fido Control Point: " << *control_point_id_;
      continue;
    }

    if (uuid == kFidoStatusUUID) {
      status_id_ = characteristic->GetIdentifier();
      FIDO_LOG(DEBUG) << "Got Fido Status: " << *status_id_;
      continue;
    }

    if (uuid == kFidoServiceRevisionUUID) {
      service_revision_id_ = characteristic->GetIdentifier();
      FIDO_LOG(DEBUG) << "Got Fido Service Revision: " << *service_revision_id_;
      continue;
    }

    if (uuid == kFidoServiceRevisionBitfieldUUID) {
      service_revision_bitfield_id_ = characteristic->GetIdentifier();
      FIDO_LOG(DEBUG) << "Got Fido Service Revision Bitfield: "
                      << *service_revision_bitfield_id_;
      continue;
    }

    FIDO_LOG(DEBUG) << "Unknown FIDO service characteristic: " << uuid;
  }

  if (!control_point_length_id_ || !control_point_id_ || !status_id_ ||
      (!service_revision_id_ && !service_revision_bitfield_id_)) {
    FIDO_LOG(ERROR) << "Fido Characteristics missing.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  // In case the bitfield characteristic is present, the client has to select a
  // supported version by writing the corresponding bit. Reference:
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#ble-protocol-overview
  if (service_revision_bitfield_id_) {
    // This callback is only repeating so that it can be bound to two different
    // callbacks.
    auto callback = base::BindRepeating(
        &FidoBleConnection::OnReadServiceRevisions, weak_factory_.GetWeakPtr());
    fido_service->GetCharacteristic(*service_revision_bitfield_id_)
        ->ReadRemoteCharacteristic(
            base::BindOnce(OnReadServiceRevisionBitfield, callback));
    return;
  }

  StartNotifySession();
}

void FidoBleConnection::OnReadServiceRevisions(
    std::vector<ServiceRevision> service_revisions) {
  DCHECK(pending_connection_callback_);
  if (service_revisions.empty()) {
    FIDO_LOG(ERROR) << "Could not obtain Service Revisions.";
    std::move(pending_connection_callback_).Run(false);
    return;
  }

  // Write the most recent supported service revision back to the
  // characteristic. Note that this information is currently not used in another
  // way, as we will still attempt a CTAP GetInfo() command, even if only U2F is
  // supported.
  // TODO(crbug.com/40547449): Consider short circuiting to the
  // U2F logic if FIDO2 is not supported.
  DCHECK_EQ(
      *std::min_element(service_revisions.begin(), service_revisions.end()),
      service_revisions.back());
  WriteServiceRevision(service_revisions.back());
}

void FidoBleConnection::WriteServiceRevision(ServiceRevision service_revision) {
  auto callback = base::BindOnce(&FidoBleConnection::OnServiceRevisionWritten,
                                 weak_factory_.GetWeakPtr());

  const auto* fido_service = GetFidoService();
  if (!fido_service) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  DCHECK(service_revision_bitfield_id_);
  fido_service->GetCharacteristic(*service_revision_bitfield_id_)
      ->WriteRemoteCharacteristic(
          {static_cast<uint8_t>(service_revision)},
          BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
          base::BindOnce(OnWriteRemoteCharacteristic,
                         std::move(split_callback.first)),
          base::BindOnce(OnWriteRemoteCharacteristicError,
                         std::move(split_callback.second)));
}

void FidoBleConnection::OnServiceRevisionWritten(bool success) {
  DCHECK(pending_connection_callback_);
  if (success) {
    FIDO_LOG(DEBUG) << "Service Revision successfully written.";
    StartNotifySession();
    return;
  }

  FIDO_LOG(ERROR) << "Failed to write Service Revision.";
  std::move(pending_connection_callback_).Run(false);
}

void FidoBleConnection::StartNotifySession() {
  DCHECK(pending_connection_callback_);
  const auto* fido_service = GetFidoService();
  if (!fido_service) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  DCHECK(status_id_);
  fido_service->GetCharacteristic(*status_id_)
      ->StartNotifySession(
          base::BindOnce(&FidoBleConnection::OnStartNotifySession,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&FidoBleConnection::OnStartNotifySessionError,
                         weak_factory_.GetWeakPtr()));
}

void FidoBleConnection::OnStartNotifySession(
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  notify_session_ = std::move(notify_session);
  FIDO_LOG(DEBUG) << "Created notification session. Connection established.";
  std::move(pending_connection_callback_).Run(true);
}

void FidoBleConnection::OnStartNotifySessionError(
    BluetoothGattService::GattErrorCode error_code) {
  FIDO_LOG(ERROR) << "StartNotifySession() failed: " << ToString(error_code);
  std::move(pending_connection_callback_).Run(false);
}

void FidoBleConnection::DeviceAddressChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             const std::string& old_address) {
  if (address_ == old_address)
    address_ = device->GetAddress();
}

void FidoBleConnection::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  if (characteristic->GetIdentifier() != status_id_)
    return;
  FIDO_LOG(DEBUG) << "Status characteristic value changed.";
  read_callback_.Run(value);
}

void FidoBleConnection::GattServicesDiscovered(BluetoothAdapter* adapter,
                                               BluetoothDevice* device) {
  if (adapter != adapter_ || device->GetAddress() != address_) {
    return;
  }

  FIDO_LOG(DEBUG) << "GATT services discovered for " << device->GetAddress();

  if (waiting_for_gatt_discovery_) {
    waiting_for_gatt_discovery_ = false;
    ConnectToFidoService();
  }
}

const BluetoothRemoteGattService* FidoBleConnection::GetFidoService() {
  if (!connection_ || !connection_->IsConnected()) {
    FIDO_LOG(ERROR) << "No BLE connection.";
    return nullptr;
  }

  DCHECK_EQ(address_, connection_->GetDeviceAddress());
  BluetoothDevice* device = GetBleDevice();

  for (const auto* service : device->GetGattServices()) {
    if (service->GetUUID() == service_uuid_) {
      return service;
    }
  }

  FIDO_LOG(ERROR) << "No Fido service present.";
  return nullptr;
}

// static
void FidoBleConnection::OnReadControlPointLength(
    ControlPointLengthCallback callback,
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  if (error_code.has_value()) {
    FIDO_LOG(ERROR) << "Error reading Control Point Length: "
                    << ToString(error_code.value());
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (value.size() != 2) {
    FIDO_LOG(ERROR) << "Wrong Control Point Length: " << value.size()
                    << " bytes";
    std::move(callback).Run(std::nullopt);
    return;
  }

  uint16_t length = (value[0] << 8) | value[1];
  FIDO_LOG(DEBUG) << "Control Point Length: " << length;
  std::move(callback).Run(length);
}

}  // namespace device
