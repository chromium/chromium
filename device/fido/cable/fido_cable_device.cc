// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_device.h"

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/ble/fido_ble_connection.h"
#include "device/fido/ble/fido_ble_frames.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

// Maximum size of EncryptionData::read_sequence_num or
// EncryptionData::write_sequence_num allowed. If we encounter
// counter larger than |kMaxCounter| FidoCableDevice should error out.
constexpr uint32_t kMaxCounter = (1 << 24) - 1;

base::Optional<std::vector<uint8_t>> ConstructV1Nonce(
    base::span<const uint8_t> nonce,
    bool is_sender_client,
    uint32_t counter) {
  if (counter > kMaxCounter)
    return base::nullopt;

  auto constructed_nonce = fido_parsing_utils::Materialize(nonce);
  constructed_nonce.push_back(is_sender_client ? 0x00 : 0x01);
  constructed_nonce.push_back(counter >> 16 & 0xFF);
  constructed_nonce.push_back(counter >> 8 & 0xFF);
  constructed_nonce.push_back(counter & 0xFF);
  return constructed_nonce;
}

bool ConstructV2Nonce(base::span<uint8_t, 12> out_nonce, uint32_t counter) {
  if (counter > kMaxCounter) {
    return false;
  }

  // Nonce is just a little-endian counter.
  std::array<uint8_t, sizeof(counter)> counter_bytes;
  memcpy(counter_bytes.data(), &counter, sizeof(counter));
  auto remaining =
      std::copy(counter_bytes.begin(), counter_bytes.end(), out_nonce.begin());
  std::fill(remaining, out_nonce.end(), 0);
  return true;
}

}  // namespace

FidoCableDevice::EncryptionData::EncryptionData() = default;

FidoCableDevice::FidoCableDevice(BluetoothAdapter* adapter, std::string address)
    : FidoBleDevice(adapter, std::move(address)) {}

FidoCableDevice::FidoCableDevice(std::unique_ptr<FidoBleConnection> connection)
    : FidoBleDevice(std::move(connection)) {}

FidoCableDevice::~FidoCableDevice() = default;

FidoDevice::CancelToken FidoCableDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  if (!encryption_data_ ||
      !EncryptOutgoingMessage(*encryption_data_, &command)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    state_ = State::kDeviceError;
    FIDO_LOG(ERROR) << "Failed to encrypt outgoing caBLE message.";
    return 0;
  }

  ++encryption_data_->write_sequence_num;

  FIDO_LOG(DEBUG) << "Sending encrypted message to caBLE client";
  return AddToPendingFrames(FidoBleDeviceCommand::kMsg, std::move(command),
                            std::move(callback));
}

void FidoCableDevice::OnResponseFrame(FrameCallback callback,
                                      base::Optional<FidoBleFrame> frame) {
  // The request is done, time to reset |transaction_|.
  ResetTransaction();
  state_ = frame ? State::kReady : State::kDeviceError;

  if (frame && frame->command() != FidoBleDeviceCommand::kControl) {
    if (!encryption_data_ ||
        !DecryptIncomingMessage(*encryption_data_, &frame.value())) {
      state_ = State::kDeviceError;
      frame = base::nullopt;
    }

    ++encryption_data_->read_sequence_num;
  }

  auto self = GetWeakPtr();
  std::move(callback).Run(std::move(frame));

  // Executing callbacks may free |this|. Check |self| first.
  if (self)
    Transition();
}

base::WeakPtr<FidoDevice> FidoCableDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

void FidoCableDevice::SetV2EncryptionData(
    base::span<const uint8_t, 32> read_key,
    base::span<const uint8_t, 32> write_key) {
  DCHECK(!encryption_data_);
  encryption_data_.emplace();
  encryption_data_->read_key = fido_parsing_utils::Materialize(read_key);
  encryption_data_->write_key = fido_parsing_utils::Materialize(write_key);
  memset(encryption_data_->nonce.data(), 0, encryption_data_->nonce.size());
  encryption_data_->is_version_two = true;
}

FidoTransportProtocol FidoCableDevice::DeviceTransport() const {
  return FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
}

void FidoCableDevice::SetSequenceNumbersForTesting(uint32_t read_seq,
                                                   uint32_t write_seq) {
  encryption_data_->write_sequence_num = write_seq;
  encryption_data_->read_sequence_num = read_seq;
}

// static
bool FidoCableDevice::EncryptOutgoingMessage(
    const EncryptionData& encryption_data,
    std::vector<uint8_t>* message_to_encrypt) {
  return encryption_data.is_version_two
             ? EncryptV2OutgoingMessage(encryption_data, message_to_encrypt)
             : EncryptV1OutgoingMessage(encryption_data, message_to_encrypt);
}

// static
bool FidoCableDevice::DecryptIncomingMessage(
    const EncryptionData& encryption_data,
    FidoBleFrame* incoming_frame) {
  return encryption_data.is_version_two
             ? DecryptV2IncomingMessage(encryption_data, incoming_frame)
             : DecryptV1IncomingMessage(encryption_data, incoming_frame);
}

// static
bool FidoCableDevice::EncryptV1OutgoingMessage(
    const EncryptionData& encryption_data,
    std::vector<uint8_t>* message_to_encrypt) {
  const auto nonce =
      ConstructV1Nonce(encryption_data.nonce, /*is_sender_client=*/true,
                       encryption_data.write_sequence_num);
  if (!nonce)
    return false;

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data.write_key);
  DCHECK_EQ(nonce->size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {
      base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg)};
  std::vector<uint8_t> ciphertext =
      aes_key.Seal(*message_to_encrypt, *nonce, additional_data);
  message_to_encrypt->swap(ciphertext);
  return true;
}

// static
bool FidoCableDevice::DecryptV1IncomingMessage(
    const EncryptionData& encryption_data,
    FidoBleFrame* incoming_frame) {
  const auto nonce =
      ConstructV1Nonce(encryption_data.nonce, /*is_sender_client=*/false,
                       encryption_data.read_sequence_num);
  if (!nonce)
    return false;

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data.read_key);
  DCHECK_EQ(nonce->size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {
      base::strict_cast<uint8_t>(incoming_frame->command())};
  base::Optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(incoming_frame->data(), *nonce, additional_data);
  if (!plaintext) {
    FIDO_LOG(ERROR) << "Failed to decrypt caBLE message.";
    return false;
  }

  incoming_frame->data().swap(*plaintext);
  return true;
}

// static
bool FidoCableDevice::EncryptV2OutgoingMessage(
    const EncryptionData& encryption_data,
    std::vector<uint8_t>* message_to_encrypt) {
  // Messages will be padded in order to round their length up to a multiple of
  // kPaddingGranularity.
  constexpr size_t kPaddingGranularity = 32;
  static_assert(kPaddingGranularity > 0, "padding too small");
  static_assert(kPaddingGranularity < 256, "padding too large");
  static_assert((kPaddingGranularity & (kPaddingGranularity - 1)) == 0,
                "padding must be a power of two");

  // Padding consists of a some number of zero bytes appended to the message and
  // the final byte in the message is the number of zeros.
  base::CheckedNumeric<size_t> padded_size_checked = message_to_encrypt->size();
  padded_size_checked += 1;  // padding-length byte.
  padded_size_checked = (padded_size_checked + kPaddingGranularity - 1) &
                        ~(kPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    return false;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  DCHECK_GT(padded_size, message_to_encrypt->size());
  const size_t num_zeros = padded_size - message_to_encrypt->size() - 1;

  std::vector<uint8_t> padded_message(padded_size, 0);
  memcpy(padded_message.data(), message_to_encrypt->data(),
         message_to_encrypt->size());
  // The number of added zeros has to fit in a single byte so it has to be less
  // than 256.
  DCHECK_LT(num_zeros, 256u);
  padded_message[padded_message.size() - 1] = static_cast<uint8_t>(num_zeros);

  std::array<uint8_t, 12> nonce;
  if (!ConstructV2Nonce(nonce, encryption_data.write_sequence_num)) {
    return false;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data.write_key);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  const uint8_t additional_data[2] = {
      base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg), /*version=*/2};
  std::vector<uint8_t> ciphertext =
      aes_key.Seal(padded_message, nonce, additional_data);
  message_to_encrypt->swap(ciphertext);
  return true;
}

// static
bool FidoCableDevice::DecryptV2IncomingMessage(
    const EncryptionData& encryption_data,
    FidoBleFrame* incoming_frame) {
  std::array<uint8_t, 12> nonce;
  if (!ConstructV2Nonce(nonce, encryption_data.read_sequence_num)) {
    return false;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data.read_key);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  const uint8_t additional_data[2] = {
      base::strict_cast<uint8_t>(incoming_frame->command()), /*version=*/2};
  base::Optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(incoming_frame->data(), nonce, additional_data);
  if (!plaintext) {
    FIDO_LOG(ERROR) << "Failed to decrypt caBLE message.";
    return false;
  }

  if (plaintext->empty()) {
    return false;
  }

  const size_t padding_length = (*plaintext)[plaintext->size() - 1];
  if (padding_length + 1 > plaintext->size()) {
    return false;
  }
  plaintext->resize(plaintext->size() - padding_length - 1);

  incoming_frame->data().swap(*plaintext);
  return true;
}

}  // namespace device
