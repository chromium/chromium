// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_device.h"

#include <bitset>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/apdu/apdu_response.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_frames.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoBleDevice::FidoBleDevice(BluetoothAdapter* adapter, std::string address) {
  connection_ = std::make_unique<FidoBleConnection>(
      adapter, std::move(address),
      base::BindRepeating(&FidoBleDevice::OnStatusMessage,
                          weak_factory_.GetWeakPtr()));
}

FidoBleDevice::FidoBleDevice(std::unique_ptr<FidoBleConnection> connection)
    : connection_(std::move(connection)) {}

FidoBleDevice::~FidoBleDevice() = default;

void FidoBleDevice::Connect() {
  if (state_ != State::kInit)
    return;

  StartTimeout();
  state_ = State::kConnecting;
  connection_->Connect(
      base::BindOnce(&FidoBleDevice::OnConnected, weak_factory_.GetWeakPtr()));
}

void FidoBleDevice::SendPing(std::vector<uint8_t> data,
                             DeviceCallback callback) {
  AddToPendingFrames(FidoBleDeviceCommand::kPing, std::move(data),
                     std::move(callback));
}

// static
std::string FidoBleDevice::GetIdForAddress(const std::string& address) {
  return "ble:" + address;
}

void FidoBleDevice::Cancel(CancelToken token) {
  if (current_token_ && *current_token_ == token) {
    transaction_->Cancel();
    return;
  }

  for (auto it = pending_frames_.begin(); it != pending_frames_.end(); it++) {
    if (it->token != token) {
      continue;
    }

    auto callback = std::move(it->callback);
    pending_frames_.erase(it);
    std::vector<uint8_t> cancel_reply = {
        static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel)};
    std::move(callback).Run(
        FidoBleFrame(FidoBleDeviceCommand::kMsg, std::move(cancel_reply)));
    break;
  }
}

std::string FidoBleDevice::GetId() const {
  return GetIdForAddress(connection_->address());
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
          static_cast<uint8_t>(FidoServiceDataFlags::kPairingMode)) != 0;
}

bool FidoBleDevice::IsPaired() const {
  const BluetoothDevice* const ble_device = connection_->GetBleDevice();
  if (!ble_device)
    return false;

  return ble_device->IsPaired();
}

bool FidoBleDevice::RequiresBlePairingPin() const {
  const BluetoothDevice* const ble_device = connection_->GetBleDevice();
  if (!ble_device)
    return true;

  const std::vector<uint8_t>* const fido_service_data =
      ble_device->GetServiceDataForUUID(BluetoothUUID(kFidoServiceUUID));
  if (!fido_service_data)
    return true;

  return !fido_service_data->empty() &&
         (fido_service_data->front() &
          static_cast<uint8_t>(FidoServiceDataFlags::kPasskeyEntry));
}

FidoBleConnection::ReadCallback FidoBleDevice::GetReadCallbackForTesting() {
  return base::BindRepeating(&FidoBleDevice::OnStatusMessage,
                             weak_factory_.GetWeakPtr());
}

FidoDevice::CancelToken FidoBleDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  return AddToPendingFrames(FidoBleDeviceCommand::kMsg, std::move(command),
                            std::move(callback));
}

base::WeakPtr<FidoDevice> FidoBleDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FidoBleDevice::OnResponseFrame(FrameCallback callback,
                                    base::Optional<FidoBleFrame> frame) {
  // The request is done, time to reset |transaction_|.
  ResetTransaction();

  if (frame) {
    state_ = State::kReady;
  } else {
    state_ = State::kDeviceError;
  }
  auto self = GetWeakPtr();
  std::move(callback).Run(std::move(frame));
  // Executing callbacks may free |this|. Check |self| first.
  if (self)
    Transition();
}

void FidoBleDevice::ResetTransaction() {
  transaction_.reset();
  current_token_.reset();
}

void FidoBleDevice::Transition() {
  switch (state_) {
    case State::kInit:
      Connect();
      break;
    case State::kReady:
      if (!pending_frames_.empty()) {
        PendingFrame pending(std::move(pending_frames_.front()));
        pending_frames_.pop_front();
        current_token_ = pending.token;
        SendRequestFrame(std::move(pending.frame), std::move(pending.callback));
      }
      break;
    case State::kConnecting:
    case State::kBusy:
      break;
    case State::kMsgError:
    case State::kDeviceError:
      auto self = GetWeakPtr();
      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_frames_.empty()) {
        // Respond to any pending frames.
        FrameCallback cb = std::move(pending_frames_.front().callback);
        pending_frames_.pop_front();
        std::move(cb).Run(base::nullopt);
      }
      break;
  }
}

FidoDevice::CancelToken FidoBleDevice::AddToPendingFrames(
    FidoBleDeviceCommand cmd,
    std::vector<uint8_t> request,
    DeviceCallback callback) {
  const auto token = next_cancel_token_++;
  pending_frames_.emplace_back(
      FidoBleFrame(cmd, std::move(request)),
      base::BindOnce(&FidoBleDevice::OnBleResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      token);

  Transition();
  return token;
}

FidoBleDevice::PendingFrame::PendingFrame(FidoBleFrame in_frame,
                                          FrameCallback in_callback,
                                          CancelToken in_token)
    : frame(std::move(in_frame)),
      callback(std::move(in_callback)),
      token(in_token) {}

FidoBleDevice::PendingFrame::PendingFrame(PendingFrame&&) = default;

FidoBleDevice::PendingFrame::~PendingFrame() = default;

void FidoBleDevice::OnConnected(bool success) {
  if (state_ == State::kDeviceError) {
    return;
  }

  StopTimeout();
  if (!success) {
    FIDO_LOG(ERROR) << "Error while attempting to connect to BLE device.";
    state_ = State::kDeviceError;
    Transition();
    return;
  }

  FIDO_LOG(EVENT) << "BLE device connected successfully.";
  DCHECK_EQ(State::kConnecting, state_);
  StartTimeout();
  connection_->ReadControlPointLength(base::BindOnce(
      &FidoBleDevice::OnReadControlPointLength, weak_factory_.GetWeakPtr()));
}

void FidoBleDevice::OnReadControlPointLength(base::Optional<uint16_t> length) {
  if (state_ == State::kDeviceError) {
    return;
  }

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
      base::BindOnce(&FidoBleDevice::OnResponseFrame,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FidoBleDevice::StartTimeout() {
  timer_.Start(FROM_HERE, kDeviceTimeout, this, &FidoBleDevice::OnTimeout);
}

void FidoBleDevice::StopTimeout() {
  timer_.Stop();
}

void FidoBleDevice::OnTimeout() {
  FIDO_LOG(ERROR) << "FIDO BLE device timeout for " << GetId();
  state_ = State::kDeviceError;
  Transition();
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
  if (data.size() != 1) {
    FIDO_LOG(ERROR) << "Unknown BLE error received: "
                    << base::HexEncode(data.data(), data.size());
    state_ = State::kDeviceError;
    return;
  }

  switch (static_cast<FidoBleFrame::ErrorCode>(data[0])) {
    case FidoBleFrame::ErrorCode::INVALID_CMD:
    case FidoBleFrame::ErrorCode::INVALID_PAR:
    case FidoBleFrame::ErrorCode::INVALID_LEN:
      state_ = State::kMsgError;
      break;
    default:
      FIDO_LOG(ERROR) << "BLE error received: " << static_cast<int>(data[0]);
      state_ = State::kDeviceError;
  }
}

}  // namespace device
