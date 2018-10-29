// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

// Length of CBOR encoded authenticator hello message concatenated with
// 16 byte message authentication code.
constexpr size_t kCableAuthenticatorHandshakeMessageSize = 66;

// Length of CBOR encoded client hello message concatenated with 16 byte message
// authenticator code.
constexpr size_t kClientHelloMessageSize = 58;

constexpr size_t kCableHandshakeMacMessageSize = 16;

// Derives key of size 32 bytes using HDKF algorithm.
// See https://tools.ietf.org/html/rfc5869 for details.
std::string GenerateKey(base::StringPiece secret,
                        base::StringPiece salt,
                        base::StringPiece info) {
  return crypto::HkdfSha256(secret, salt, info, 32);
}

base::Optional<std::array<uint8_t, kClientHelloMessageSize>>
ConstructHandshakeMessage(base::StringPiece handshake_key,
                          base::span<const uint8_t, 16> client_random_nonce) {
  cbor::Value::MapValue map;
  map.emplace(0, kCableClientHelloMessage);
  map.emplace(1, client_random_nonce);
  auto client_hello = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(client_hello);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key))
    return base::nullopt;

  std::array<uint8_t, 32> client_hello_mac;
  if (!hmac.Sign(fido_parsing_utils::ConvertToStringPiece(*client_hello),
                 client_hello_mac.data(), client_hello_mac.size())) {
    return base::nullopt;
  }

  DCHECK_EQ(kClientHelloMessageSize,
            client_hello->size() + kCableHandshakeMacMessageSize);
  std::array<uint8_t, kClientHelloMessageSize> handshake_message;
  std::copy(client_hello->begin(), client_hello->end(),
            handshake_message.begin());
  std::copy(client_hello_mac.begin(),
            client_hello_mac.begin() + kCableHandshakeMacMessageSize,
            handshake_message.begin() + client_hello->size());

  return handshake_message;
}

}  // namespace

FidoCableHandshakeHandler::FidoCableHandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, 32> session_pre_key)
    : cable_device_(cable_device),
      nonce_(fido_parsing_utils::Materialize(nonce)),
      session_pre_key_(fido_parsing_utils::Materialize(session_pre_key)),
      handshake_key_(GenerateKey(
          fido_parsing_utils::ConvertToStringPiece(session_pre_key_),
          fido_parsing_utils::ConvertToStringPiece(nonce_),
          kCableHandshakeKeyInfo)),
      weak_factory_(this) {
  crypto::RandBytes(client_session_random_.data(),
                    client_session_random_.size());
}

FidoCableHandshakeHandler::~FidoCableHandshakeHandler() = default;

void FidoCableHandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  auto handshake_message =
      ConstructHandshakeMessage(handshake_key_, client_session_random_);
  if (!handshake_message) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  cable_device_->SendHandshakeMessage(
      fido_parsing_utils::Materialize(*handshake_message), std::move(callback));
}

bool FidoCableHandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key_))
    return false;

  if (response.size() != kCableAuthenticatorHandshakeMessageSize) {
    return false;
  }

  const auto authenticator_hello = response.first(
      kCableAuthenticatorHandshakeMessageSize - kCableHandshakeMacMessageSize);
  if (!hmac.VerifyTruncated(
          fido_parsing_utils::ConvertToStringPiece(authenticator_hello),
          fido_parsing_utils::ConvertToStringPiece(
              response.subspan(authenticator_hello.size())))) {
    return false;
  }

  const auto authenticator_hello_cbor = cbor::Reader::Read(authenticator_hello);
  if (!authenticator_hello_cbor || !authenticator_hello_cbor->is_map() ||
      authenticator_hello_cbor->GetMap().size() != 2) {
    return false;
  }

  const auto authenticator_hello_msg =
      authenticator_hello_cbor->GetMap().find(cbor::Value(0));
  if (authenticator_hello_msg == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_hello_msg->second.is_string() ||
      authenticator_hello_msg->second.GetString() !=
          kCableAuthenticatorHelloMessage) {
    return false;
  }

  const auto authenticator_random_nonce =
      authenticator_hello_cbor->GetMap().find(cbor::Value(1));
  if (authenticator_random_nonce == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_random_nonce->second.is_bytestring() ||
      authenticator_random_nonce->second.GetBytestring().size() != 16) {
    return false;
  }

  cable_device_->SetEncryptionData(
      GetEncryptionKeyAfterSuccessfulHandshake(base::make_span<16>(
          authenticator_random_nonce->second.GetBytestring())),
      nonce_);

  return true;
}

std::string FidoCableHandshakeHandler::GetEncryptionKeyAfterSuccessfulHandshake(
    base::span<const uint8_t, 16> authenticator_random_nonce) const {
  std::vector<uint8_t> nonce_message;
  fido_parsing_utils::Append(&nonce_message, nonce_);
  fido_parsing_utils::Append(&nonce_message, client_session_random_);
  fido_parsing_utils::Append(&nonce_message, authenticator_random_nonce);
  return GenerateKey(
      fido_parsing_utils::ConvertToStringPiece(session_pre_key_),
      fido_parsing_utils::ConvertToStringPiece(
          fido_parsing_utils::CreateSHA256Hash(
              fido_parsing_utils::ConvertToStringPiece(nonce_message))),
      kCableDeviceEncryptionKeyInfo);
}

}  // namespace device
