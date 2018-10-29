// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_device.h"

#include <bitset>

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "components/apdu/apdu_response.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_frames.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoBleDevice::FidoBleDevice(BluetoothAdapter* adapter, std::string address)
    : weak_factory_(this) {
  connection_ = std::make_unique<FidoBleConnection>(
      adapter, std::move(address),
      base::BindRepeating(&FidoBleDevice::OnStatusMessage,
                          base::Unretained(this)));
}

FidoBleDevice::FidoBleDevice(std::unique_ptr<FidoBleConnection> connection)
    : connection_(std::move(connection)), weak_factory_(this) {}

FidoBleDevice::~FidoBleDevice() = default;

void FidoBleDevice::Connect() {
  if (state_ != State::kInit)
    return;

  StartTimeout();
  state_ = State::kBusy;
  connection_->Connect(
      base::BindOnce(&FidoBleDevice::OnConnected, base::Unretained(this)));
}

void FidoBleDevice::SendPing(std::vector<uint8_t> data,
                             DeviceCallback callback) {
  AddToPendingFrames(FidoBleDeviceCommand::kPing, std::move(data),
                     std::move(callback));
}

// static
std::string FidoBleDevice::GetId(base::StringPiece address) {
  return std::string("ble:").append(address.begin(), address.end());
}

void FidoBleDevice::TryWink(WinkCallback callback) {
  // U2F over BLE does not support winking.
  std::move(callback).Run();
}

void FidoBleDevice::Cancel() {
  if (state_ != State::kReady && state_ != State::kBusy)
    return;

  AddToPendingFrames(FidoBleDeviceCommand::kCancel, std::vector<uint8_t>(),
                     base::DoNothing());
}

std::string FidoBleDevice::GetId() const {
  return GetId(connection_->address());
}

base::string16 FidoBleDevice::GetDisplayName() const {
  auto* device = connection_->GetBleDevice();
  if (!device)
    return base::string16();

  return device->GetNameForDisplay();
}

FidoTransportProtocol FidoBleDevice::DeviceTransport() const {
  return FidoTransportProtocol::kBluetoothLowEnergy;
}

bool FidoBleDevice::IsInPairingMode() const {
  const BluetoothDevice* const ble_device = connection_->GetBleDevice();
  if (!ble_device)
    return false;

  // The spec requires exactly one of the LE Limited Discoverable Mode and LE
  // General Discoverable Mode bits to be set to one when in pairing mode.
  // https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-client-to-authenticator-protocol-v2.0-id-20180227.html#ble-advertising-format
  const base::Optional<uint8_t> flags = ble_device->GetAdvertisingDataFlags();
  if (flags.has_value()) {
    const std::bitset<8> flags_set = *flags;
    return flags_set[kLeLimitedDiscoverableModeBit] ^
           flags_set[kLeGeneralDiscoverableModeBit];
  }

  // Since the advertisement flags might not be available due to platform
  // limitations, authenticators should also provide a specific pairing mode bit
  // in FIDO's service data.
  // https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-client-to-authenticator-protocol-v2.0-id-20180227.html#ble-pairing-authnr-considerations
  const std::vector<uint8_t>* const fido_service_data =
      ble_device->GetServiceDataForUUID(BluetoothUUID(kFidoServiceUUID));
  if (!fido_service_data)
    return false;

  return !fido_service_data->empty() &&
         (fido_service_data->front() &
          static_cast<int>(FidoServiceDataFlags::kPairingMode)) != 0;
}

FidoBleConnection::ReadCallback FidoBleDevice::GetReadCallbackForTesting() {
  return base::BindRepeating(&FidoBleDevice::OnStatusMessage,
                             base::Unretained(this));
}

void FidoBleDevice::DeviceTransact(std::vector<uint8_t> command,
                                   DeviceCallback callback) {
  AddToPendingFrames(FidoBleDeviceCommand::kMsg, std::move(command),
                     std::move(callback));
}

base::WeakPtr<FidoDevice> FidoBleDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FidoBleDevice::OnResponseFrame(FrameCallback callback,
                                    base::Optional<FidoBleFrame> frame) {
  // The request is done, time to reset |transaction_|.
  ResetTransaction();

  state_ = frame ? State::kReady : State::kDeviceError;
  auto self = GetWeakPtr();
  std::move(callback).Run(std::move(frame));
  // Executing callbacks may free |this|. Check |self| first.
  if (self)
    Transition();
}

void FidoBleDevice::ResetTransaction() {
  transaction_.reset();
}

void FidoBleDevice::Transition() {
  switch (state_) {
    case State::kInit:
      Connect();
      break;
    case State::kConnected:
      StartTimeout();
      state_ = State::kBusy;
      connection_->ReadControlPointLength(base::BindOnce(
          &FidoBleDevice::OnReadControlPointLength, base::Unretained(this)));
      break;
    case State::kReady:
      if (!pending_frames_.empty()) {
        FidoBleFrame frame;
        FrameCallback callback;
        std::tie(frame, callback) = std::move(pending_frames_.front());
        pending_frames_.pop();
        SendRequestFrame(std::move(frame), std::move(callback));
      }
      break;
    case State::kBusy:
      break;
    case State::kMsgError:
    case State::kDeviceError:
      auto self = GetWeakPtr();
      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_frames_.empty()) {
        // Respond to any pending frames.
        FrameCallback cb = std::move(pending_frames_.front().second);
        pending_frames_.pop();
        std::move(cb).Run(base::nullopt);
      }
      break;
  }
}

void FidoBleDevice::AddToPendingFrames(FidoBleDeviceCommand cmd,
                                       std::vector<uint8_t> request,
                                       DeviceCallback callback) {
  pending_frames_.emplace(
      FidoBleFrame(cmd, std::move(request)),
      base::BindOnce(&FidoBleDevice::OnBleResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  Transition();
}

void FidoBleDevice::OnConnected(bool success) {
  StopTimeout();
  state_ = success ? State::kConnected : State::kDeviceError;
  Transition();
}

void FidoBleDevice::OnReadControlPointLength(base::Optional<uint16_t> length) {
  StopTimeout();
  if (length) {
    control_point_length_ = *length;
    state_ = State::kReady;
  } else {
    state_ = State::kDeviceError;
  }
  Transition();
}

void FidoBleDevice::OnStatusMessage(std::vector<uint8_t> data) {
  if (transaction_)
    transaction_->OnResponseFragment(std::move(data));
}

void FidoBleDevice::SendRequestFrame(FidoBleFrame frame,
                                     FrameCallback callback) {
  state_ = State::kBusy;
  transaction_.emplace(connection_.get(), control_point_length_);
  transaction_->WriteRequestFrame(
      std::move(frame),
      base::BindOnce(&FidoBleDevice::OnResponseFrame, base::Unretained(this),
                     std::move(callback)));
}

void FidoBleDevice::StartTimeout() {
  timer_.Start(FROM_HERE, kDeviceTimeout, this, &FidoBleDevice::OnTimeout);
}

void FidoBleDevice::StopTimeout() {
  timer_.Stop();
}

void FidoBleDevice::OnTimeout() {
  state_ = State::kDeviceError;
}

void FidoBleDevice::OnBleResponseReceived(DeviceCallback callback,
                                          base::Optional<FidoBleFrame> frame) {
  if (!frame || !frame->IsValid()) {
    state_ = State::kDeviceError;
    std::move(callback).Run(base::nullopt);
    return;
  }

  if (frame->command() == FidoBleDeviceCommand::kError) {
    ProcessBleDeviceError(frame->data());
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::move(callback).Run(frame->data());
}

void FidoBleDevice::ProcessBleDeviceError(base::span<const uint8_t> data) {
  DCHECK_EQ(1u, data.size());
  const auto error_constant = data[0];
  if (error_constant ==
          base::strict_cast<uint8_t>(FidoBleFrame::ErrorCode::INVALID_CMD) ||
      error_constant ==
          base::strict_cast<uint8_t>(FidoBleFrame::ErrorCode::INVALID_PAR) ||
      error_constant ==
          base::strict_cast<uint8_t>(FidoBleFrame::ErrorCode::INVALID_LEN)) {
    state_ = State::kMsgError;
  } else {
    state_ = State::kDeviceError;
  }
}

}  // namespace device
