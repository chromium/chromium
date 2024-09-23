// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_device.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/fido_ble_frames.h"
#include "device/fido/cable/fido_ble_uuids.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

// Maximum size of EncryptionData::read_sequence_num or
// EncryptionData::write_sequence_num allowed. If we encounter
// counter larger than |kMaxCounter| FidoCableDevice should error out.
constexpr uint32_t kMaxCounter = (1 << 24) - 1;

std::optional<std::vector<uint8_t>> ConstructV1Nonce(
    base::span<const uint8_t> nonce,
    bool is_sender_client,
    uint32_t counter) {
  if (counter > kMaxCounter)
    return std::nullopt;

  auto constructed_nonce = fido_parsing_utils::Materialize(nonce);
  constructed_nonce.push_back(is_sender_client ? 0x00 : 0x01);
  constructed_nonce.push_back(counter >> 16 & 0xFF);
  constructed_nonce.push_back(counter >> 8 & 0xFF);
  constructed_nonce.push_back(counter & 0xFF);
  return constructed_nonce;
}

}  // namespace

FidoCableDevice::EncryptionData::EncryptionData() = default;
FidoCableDevice::EncryptionData::~EncryptionData() = default;

FidoCableDevice::FidoCableDevice(BluetoothAdapter* adapter,
                                 std::string address) {
  connection_ = std::make_unique<FidoBleConnection>(
      adapter, std::move(address), BluetoothUUID(kGoogleCableUUID128),
      base::BindRepeating(&FidoCableDevice::OnStatusMessage,
                          weak_factory_.GetWeakPtr()));
}

FidoCableDevice::FidoCableDevice(std::unique_ptr<FidoBleConnection> connection)
    : connection_(std::move(connection)) {}

FidoCableDevice::~FidoCableDevice() = default;

// static
std::string FidoCableDevice::GetIdForAddress(const std::string& address) {
  return "ble-" + address;
}

std::string FidoCableDevice::GetAddress() {
  return connection_->address();
}

void FidoCableDevice::Connect() {
  if (state_ != State::kInit)
    return;

  StartTimeout();
  state_ = State::kConnecting;
  connection_->Connect(base::BindOnce(&FidoCableDevice::OnConnected,
                                      weak_factory_.GetWeakPtr()));
}

void FidoCableDevice::SendPing(std::vector<uint8_t> data,
                               DeviceCallback callback) {
  AddToPendingFrames(FidoBleDeviceCommand::kPing, std::move(data),
                     std::move(callback));
}

FidoBleConnection::ReadCallback FidoCableDevice::GetReadCallbackForTesting() {
  return base::BindRepeating(&FidoCableDevice::OnStatusMessage,
                             weak_factory_.GetWeakPtr());
}

void FidoCableDevice::Cancel(CancelToken token) {
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

std::string FidoCableDevice::GetId() const {
  return GetIdForAddress(connection_->address());
}

FidoTransportProtocol FidoCableDevice::DeviceTransport() const {
  return FidoTransportProtocol::kHybrid;
}

FidoDevice::CancelToken FidoCableDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  if (!encryption_data_ || !EncryptOutgoingMessage(&command)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    state_ = State::kDeviceError;
    FIDO_LOG(ERROR) << "Failed to encrypt outgoing caBLE message.";
    return 0;
  }

  FIDO_LOG(DEBUG) << "Sending encrypted message to caBLE client";
  return AddToPendingFrames(FidoBleDeviceCommand::kMsg, std::move(command),
                            std::move(callback));
}

void FidoCableDevice::OnResponseFrame(FrameCallback callback,
                                      std::optional<FidoBleFrame> frame) {
  // The request is done, time to reset |transaction_|.
  ResetTransaction();
  state_ = frame ? State::kReady : State::kDeviceError;

  if (frame && frame->command() != FidoBleDeviceCommand::kControl) {
    if (!encryption_data_ || !DecryptIncomingMessage(&frame.value())) {
      state_ = State::kDeviceError;
      frame = std::nullopt;
    }
  }

  auto self = GetWeakPtr();
  std::move(callback).Run(std::move(frame));

  // Executing callbacks may free |this|. Check |self| first.
  if (self)
    Transition();
}

void FidoCableDevice::ResetTransaction() {
  transaction_.reset();
  current_token_.reset();
}

void FidoCableDevice::Transition() {
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
        std::move(cb).Run(std::nullopt);
      }
      break;
  }
}

FidoDevice::CancelToken FidoCableDevice::AddToPendingFrames(
    FidoBleDeviceCommand cmd,
    std::vector<uint8_t> request,
    DeviceCallback callback) {
  const auto token = next_cancel_token_++;
  pending_frames_.emplace_back(
      FidoBleFrame(cmd, std::move(request)),
      base::BindOnce(&FidoCableDevice::OnBleResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      token);

  Transition();
  return token;
}

void FidoCableDevice::SendHandshakeMessage(
    std::vector<uint8_t> handshake_message,
    DeviceCallback callback) {
  AddToPendingFrames(FidoBleDeviceCommand::kControl,
                     std::move(handshake_message), std::move(callback));
}

void FidoCableDevice::SetV1EncryptionData(
    base::span<const uint8_t, 32> session_key,
    base::span<const uint8_t, 8> nonce) {
  // Encryption data must be set at most once during Cable handshake protocol.
  DCHECK(!encryption_data_);
  encryption_data_.emplace();
  encryption_data_->read_key = fido_parsing_utils::Materialize(session_key);
  encryption_data_->write_key = fido_parsing_utils::Materialize(session_key);
  encryption_data_->nonce = fido_parsing_utils::Materialize(nonce);
}

void FidoCableDevice::SetSequenceNumbersForTesting(uint32_t read_seq,
                                                   uint32_t write_seq) {
  encryption_data_->write_sequence_num = write_seq;
  encryption_data_->read_sequence_num = read_seq;
}

base::WeakPtr<FidoDevice> FidoCableDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

FidoCableDevice::PendingFrame::PendingFrame(FidoBleFrame in_frame,
                                            FrameCallback in_callback,
                                            CancelToken in_token)
    : frame(std::move(in_frame)),
      callback(std::move(in_callback)),
      token(in_token) {}

FidoCableDevice::PendingFrame::PendingFrame(PendingFrame&&) = default;

FidoCableDevice::PendingFrame::~PendingFrame() = default;

void FidoCableDevice::OnConnected(bool success) {
  if (state_ != State::kConnecting) {
    return;
  }
  StopTimeout();
  if (!success) {
    FIDO_LOG(ERROR) << "FidoCableDevice::Connect() failed";
    state_ = State::kDeviceError;
    Transition();
    return;
  }
  FIDO_LOG(EVENT) << "FidoCableDevice connected";
  DCHECK_EQ(State::kConnecting, state_);
  StartTimeout();
  connection_->ReadControlPointLength(base::BindOnce(
      &FidoCableDevice::OnReadControlPointLength, weak_factory_.GetWeakPtr()));
}

void FidoCableDevice::OnStatusMessage(std::vector<uint8_t> data) {
  if (transaction_)
    transaction_->OnResponseFragment(std::move(data));
}

void FidoCableDevice::OnReadControlPointLength(std::optional<uint16_t> length) {
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

void FidoCableDevice::SendRequestFrame(FidoBleFrame frame,
                                       FrameCallback callback) {
  state_ = State::kBusy;
  transaction_.emplace(connection_.get(), control_point_length_);
  transaction_->WriteRequestFrame(
      std::move(frame),
      base::BindOnce(&FidoCableDevice::OnResponseFrame,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FidoCableDevice::StartTimeout() {
  timer_.Start(FROM_HERE, kDeviceTimeout, this, &FidoCableDevice::OnTimeout);
}

void FidoCableDevice::StopTimeout() {
  timer_.Stop();
}

void FidoCableDevice::OnTimeout() {
  FIDO_LOG(ERROR) << "FIDO Cable device timeout for " << GetId();
  state_ = State::kDeviceError;
  Transition();
}

void FidoCableDevice::OnBleResponseReceived(DeviceCallback callback,
                                            std::optional<FidoBleFrame> frame) {
  if (!frame || !frame->IsValid()) {
    state_ = State::kDeviceError;
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (frame->command() == FidoBleDeviceCommand::kError) {
    ProcessBleDeviceError(frame->data());
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(frame->data());
}

void FidoCableDevice::ProcessBleDeviceError(base::span<const uint8_t> data) {
  if (data.size() != 1) {
    FIDO_LOG(ERROR) << "Unknown BLE error received: " << base::HexEncode(data);
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

bool FidoCableDevice::EncryptOutgoingMessage(
    std::vector<uint8_t>* message_to_encrypt) {
  const auto nonce =
      ConstructV1Nonce(encryption_data_->nonce, /*is_sender_client=*/true,
                       encryption_data_->write_sequence_num++);
  if (!nonce)
    return false;

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data_->write_key);
  DCHECK_EQ(nonce->size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {
      base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg)};
  std::vector<uint8_t> ciphertext =
      aes_key.Seal(*message_to_encrypt, *nonce, additional_data);
  message_to_encrypt->swap(ciphertext);
  return true;
}

bool FidoCableDevice::DecryptIncomingMessage(FidoBleFrame* incoming_frame) {
  const auto nonce =
      ConstructV1Nonce(encryption_data_->nonce, /*is_sender_client=*/false,
                       encryption_data_->read_sequence_num);
  if (!nonce)
    return false;

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data_->read_key);
  DCHECK_EQ(nonce->size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {
      base::strict_cast<uint8_t>(incoming_frame->command())};
  std::optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(incoming_frame->data(), *nonce, additional_data);
  if (!plaintext) {
    FIDO_LOG(ERROR) << "Failed to decrypt caBLE message.";
    return false;
  }

  encryption_data_->read_sequence_num++;
  incoming_frame->data().swap(*plaintext);
  return true;
}

}  // namespace device
