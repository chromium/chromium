// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_connection.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_uuids.h"

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
      return "";
  }
}

constexpr const char* ToString(BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case BluetoothGattService::GATT_ERROR_UNKNOWN:
      return "GATT_ERROR_UNKNOWN";
    case BluetoothGattService::GATT_ERROR_FAILED:
      return "GATT_ERROR_FAILED";
    case BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      return "GATT_ERROR_IN_PROGRESS";
    case BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      return "GATT_ERROR_INVALID_LENGTH";
    case BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      return "GATT_ERROR_NOT_PERMITTED";
    case BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      return "GATT_ERROR_NOT_AUTHORIZED";
    case BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      return "GATT_ERROR_NOT_PAIRED";
    case BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      return "GATT_ERROR_NOT_SUPPORTED";
    default:
      NOTREACHED();
      return "";
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
  return os;
}

const BluetoothRemoteGattService* GetFidoService(
    const BluetoothDevice* device) {
  if (!device) {
    LOG(ERROR) << "No device present.";
    return nullptr;
  }

  for (const auto* service : device->GetGattServices()) {
    if (service->GetUUID() == BluetoothUUID(kFidoServiceUUID))
      return service;
  }

  LOG(ERROR) << "No Fido service present.";
  return nullptr;
}

void OnWriteRemoteCharacteristic(FidoBleConnection::WriteCallback callback) {
  VLOG(2) << "Writing Remote Characteristic Succeeded.";
  std::move(callback).Run(true);
}

void OnWriteRemoteCharacteristicError(
    FidoBleConnection::WriteCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  LOG(ERROR) << "Writing Remote Characteristic Failed: "
             << ToString(error_code);
  std::move(callback).Run(false);
}

void OnReadServiceRevisionBitfield(ServiceRevisionsCallback callback,
                                   const std::vector<uint8_t>& value) {
  if (value.empty()) {
    VLOG(2) << "Service Revision Bitfield is empty.";
    std::move(callback).Run({});
    return;
  }

  if (value.size() != 1u) {
    VLOG(2) << "Service Revision Bitfield has unexpected size: " << value.size()
            << ". Ignoring all but the first byte.";
  }

  const uint8_t bitset = value[0];
  if (bitset & 0x1F) {
    VLOG(2) << "Service Revision Bitfield has unexpected bits set: "
            << base::StringPrintf("0x%02X", bitset)
            << ". Ignoring all but the first three bits.";
  }

  std::vector<FidoBleConnection::ServiceRevision> service_revisions;
  for (auto revision : {FidoBleConnection::ServiceRevision::kU2f11,
                        FidoBleConnection::ServiceRevision::kU2f12,
                        FidoBleConnection::ServiceRevision::kFido2}) {
    if (bitset & static_cast<uint8_t>(revision)) {
      VLOG(2) << "Detected Support for " << revision << ".";
      service_revisions.push_back(revision);
    }
  }

  std::move(callback).Run(std::move(service_revisions));
}

void OnReadServiceRevisionBitfieldError(
    ServiceRevisionsCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  LOG(ERROR) << "Error while reading Service Revision Bitfield: "
             << ToString(error_code);
  std::move(callback).Run({});
}

}  // namespace

FidoBleConnection::FidoBleConnection(
    BluetoothAdapter* adapter,
    std::string device_address,
    ReadCallback read_callback)
    : adapter_(adapter),
      address_(std::move(device_address)),
      read_callback_(std::move(read_callback)),
      weak_factory_(this) {
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

FidoBleConnection::FidoBleConnection(BluetoothAdapter* adapter,
                                     std::string device_address)
    : adapter_(adapter),
      address_(std::move(device_address)),
      weak_factory_(this) {
  adapter_->AddObserver(this);
}

void FidoBleConnection::Connect(ConnectionCallback callback) {
  auto* device = GetBleDevice();
  if (!device) {
    LOG(ERROR) << "Failed to get Device.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  pending_connection_callback_ = std::move(callback);
  device->CreateGattConnection(
      base::Bind(&FidoBleConnection::OnCreateGattConnection,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&FidoBleConnection::OnCreateGattConnectionError,
                 weak_factory_.GetWeakPtr()));
}

void FidoBleConnection::ReadControlPointLength(
    ControlPointLengthCallback callback) {
  const auto* fido_service = GetFidoService(GetBleDevice());
  if (!fido_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  if (!control_point_length_id_) {
    LOG(ERROR) << "Failed to get Control Point Length.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  BluetoothRemoteGattCharacteristic* control_point_length =
      fido_service->GetCharacteristic(*control_point_length_id_);
  if (!control_point_length) {
    LOG(ERROR) << "No Control Point Length characteristic present.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  // Work around legacy APIs. Only one of the callbacks to
  // ReadRemoteCharacteristic() gets invoked, but we don't know which one.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  control_point_length->ReadRemoteCharacteristic(
      base::Bind(OnReadControlPointLength, copyable_callback),
      base::Bind(OnReadControlPointLengthError, copyable_callback));
}

void FidoBleConnection::WriteControlPoint(const std::vector<uint8_t>& data,
                                          WriteCallback callback) {
  const auto* fido_service = GetFidoService(GetBleDevice());
  if (!fido_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (!control_point_id_) {
    LOG(ERROR) << "Failed to get Control Point.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  BluetoothRemoteGattCharacteristic* control_point =
      fido_service->GetCharacteristic(*control_point_id_);
  if (!control_point) {
    LOG(ERROR) << "Control Point characteristic not present.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

#if defined(OS_MACOSX)
  // Attempt a write without response for performance reasons. Fall back to a
  // confirmed write in case of failure, e.g. when the characteristic does not
  // provide the required property.
  if (control_point->WriteWithoutResponse(data)) {
    VLOG(2) << "Write without response succeeded.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
#endif  // defined(OS_MACOSX)

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  control_point->WriteRemoteCharacteristic(
      data, base::Bind(OnWriteRemoteCharacteristic, copyable_callback),
      base::Bind(OnWriteRemoteCharacteristicError, copyable_callback));
}

void FidoBleConnection::OnCreateGattConnection(
    std::unique_ptr<BluetoothGattConnection> connection) {
  DCHECK(pending_connection_callback_);
  connection_ = std::move(connection);

  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (!device) {
    LOG(ERROR) << "Failed to get Device.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  if (device->IsGattServicesDiscoveryComplete())
    ConnectToFidoService();
  else
    waiting_for_gatt_discovery_ = true;
}

void FidoBleConnection::OnCreateGattConnectionError(
    BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK(pending_connection_callback_);
  LOG(ERROR) << "CreateGattConnection() failed: " << ToString(error_code);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(pending_connection_callback_), false));
}

void FidoBleConnection::ConnectToFidoService() {
  DCHECK(pending_connection_callback_);
  const auto* fido_service = GetFidoService(GetBleDevice());
  if (!fido_service) {
    LOG(ERROR) << "Failed to get Fido Service.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  for (const auto* characteristic : fido_service->GetCharacteristics()) {
    std::string uuid = characteristic->GetUUID().canonical_value();
    if (uuid == kFidoControlPointLengthUUID) {
      control_point_length_id_ = characteristic->GetIdentifier();
      VLOG(2) << "Got Fido Control Point Length: " << *control_point_length_id_;
      continue;
    }

    if (uuid == kFidoControlPointUUID) {
      control_point_id_ = characteristic->GetIdentifier();
      VLOG(2) << "Got Fido Control Point: " << *control_point_id_;
      continue;
    }

    if (uuid == kFidoStatusUUID) {
      status_id_ = characteristic->GetIdentifier();
      VLOG(2) << "Got Fido Status: " << *status_id_;
      continue;
    }

    if (uuid == kFidoServiceRevisionUUID) {
      service_revision_id_ = characteristic->GetIdentifier();
      VLOG(2) << "Got Fido Service Revision: " << *service_revision_id_;
      continue;
    }

    if (uuid == kFidoServiceRevisionBitfieldUUID) {
      service_revision_bitfield_id_ = characteristic->GetIdentifier();
      VLOG(2) << "Got Fido Service Revision Bitfield: "
              << *service_revision_bitfield_id_;
    }
  }

  if (!control_point_length_id_ || !control_point_id_ || !status_id_ ||
      (!service_revision_id_ && !service_revision_bitfield_id_)) {
    LOG(ERROR) << "Fido Characteristics missing.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  // In case the bitfield characteristic is present, the client has to select a
  // supported bersion by writing the corresponding bit. Reference:
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#ble-protocol-overview
  if (service_revision_bitfield_id_) {
    auto callback = base::Bind(&FidoBleConnection::OnReadServiceRevisions,
                               weak_factory_.GetWeakPtr());
    fido_service->GetCharacteristic(*service_revision_bitfield_id_)
        ->ReadRemoteCharacteristic(
            base::Bind(OnReadServiceRevisionBitfield, callback),
            base::Bind(OnReadServiceRevisionBitfieldError, callback));
    return;
  }

  StartNotifySession();
}

void FidoBleConnection::OnReadServiceRevisions(
    std::vector<ServiceRevision> service_revisions) {
  DCHECK(pending_connection_callback_);
  if (service_revisions.empty()) {
    LOG(ERROR) << "Could not obtain Service Revisions.";
    std::move(pending_connection_callback_).Run(false);
    return;
  }

  // Write the most recent supported service revision back to the
  // characteristic. Note that this information is currently not used in another
  // way, as we will still attempt a CTAP GetInfo() command, even if only U2F is
  // supported.
  // TODO(https://crbug.com/780078): Consider short circuiting to the
  // U2F logic if FIDO2 is not supported.
  DCHECK_EQ(
      *std::min_element(service_revisions.begin(), service_revisions.end()),
      service_revisions.back());
  WriteServiceRevision(service_revisions.back());
}

void FidoBleConnection::WriteServiceRevision(ServiceRevision service_revision) {
  auto callback = base::BindOnce(&FidoBleConnection::OnServiceRevisionWritten,
                                 weak_factory_.GetWeakPtr());

  const auto* fido_service = GetFidoService(GetBleDevice());
  if (!fido_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  DCHECK(service_revision_bitfield_id_);
  fido_service->GetCharacteristic(*service_revision_bitfield_id_)
      ->WriteRemoteCharacteristic(
          {static_cast<uint8_t>(service_revision)},
          base::Bind(OnWriteRemoteCharacteristic, copyable_callback),
          base::Bind(OnWriteRemoteCharacteristicError, copyable_callback));
}

void FidoBleConnection::OnServiceRevisionWritten(bool success) {
  DCHECK(pending_connection_callback_);
  if (success) {
    StartNotifySession();
    return;
  }

  std::move(pending_connection_callback_).Run(false);
}

void FidoBleConnection::StartNotifySession() {
  DCHECK(pending_connection_callback_);
  const auto* fido_service = GetFidoService(GetBleDevice());
  if (!fido_service) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_connection_callback_), false));
    return;
  }

  DCHECK(status_id_);
  fido_service->GetCharacteristic(*status_id_)
      ->StartNotifySession(
          base::Bind(&FidoBleConnection::OnStartNotifySession,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&FidoBleConnection::OnStartNotifySessionError,
                     weak_factory_.GetWeakPtr()));
}

void FidoBleConnection::OnStartNotifySession(
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  notify_session_ = std::move(notify_session);
  VLOG(2) << "Created notification session. Connection established.";
  std::move(pending_connection_callback_).Run(true);
}

void FidoBleConnection::OnStartNotifySessionError(
    BluetoothGattService::GattErrorCode error_code) {
  LOG(ERROR) << "StartNotifySession() failed: " << ToString(error_code);
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
  VLOG(2) << "Status characteristic value changed.";
  read_callback_.Run(value);
}

void FidoBleConnection::GattServicesDiscovered(BluetoothAdapter* adapter,
                                               BluetoothDevice* device) {
  if (adapter != adapter_ || device->GetAddress() != address_)
    return;

  if (waiting_for_gatt_discovery_) {
    waiting_for_gatt_discovery_ = false;
    ConnectToFidoService();
  }
}

// static
void FidoBleConnection::OnReadControlPointLength(
    ControlPointLengthCallback callback,
    const std::vector<uint8_t>& value) {
  if (value.size() != 2) {
    LOG(ERROR) << "Wrong Control Point Length: " << value.size() << " bytes";
    std::move(callback).Run(base::nullopt);
    return;
  }

  uint16_t length = (value[0] << 8) | value[1];
  VLOG(2) << "Control Point Length: " << length;
  std::move(callback).Run(length);
}

// static
void FidoBleConnection::OnReadControlPointLengthError(
    ControlPointLengthCallback callback,
    BluetoothGattService::GattErrorCode error_code) {
  LOG(ERROR) << "Error reading Control Point Length: " << ToString(error_code);
  std::move(callback).Run(base::nullopt);
}

}  // namespace device
