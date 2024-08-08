// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/noise.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {

// Length of CBOR encoded authenticator hello message concatenated with
// 16 byte message authentication code.
constexpr size_t kCableAuthenticatorHandshakeMessageSize = 66;

// Length of CBOR encoded client hello message concatenated with 16 byte message
// authenticator code.
constexpr size_t kClientHelloMessageSize = 58;

constexpr size_t kCableHandshakeMacMessageSize = 16;

std::optional<std::array<uint8_t, kClientHelloMessageSize>>
ConstructHandshakeMessage(std::string_view handshake_key,
                          base::span<const uint8_t, 16> client_random_nonce) {
  cbor::Value::MapValue map;
  map.emplace(0, kCableClientHelloMessage);
  map.emplace(1, client_random_nonce);
  auto client_hello = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(client_hello);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key))
    return std::nullopt;

  std::array<uint8_t, kCableHandshakeMacMessageSize> client_hello_mac;
  if (!hmac.Sign(fido_parsing_utils::ConvertToStringView(*client_hello),
                 client_hello_mac.data(), client_hello_mac.size())) {
    return std::nullopt;
  }

  DCHECK_EQ(kClientHelloMessageSize,
            client_hello->size() + client_hello_mac.size());
  std::array<uint8_t, kClientHelloMessageSize> handshake_message;
  base::ranges::copy(*client_hello, handshake_message.begin());
  base::ranges::copy(client_hello_mac,
                     handshake_message.begin() + client_hello->size());

  return handshake_message;
}

}  // namespace

FidoCableHandshakeHandler::~FidoCableHandshakeHandler() {}

FidoCableV1HandshakeHandler::FidoCableV1HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, 32> session_pre_key)
    : cable_device_(cable_device),
      nonce_(fido_parsing_utils::Materialize(nonce)),
      session_pre_key_(fido_parsing_utils::Materialize(session_pre_key)),
      handshake_key_(crypto::HkdfSha256(
          fido_parsing_utils::ConvertToStringView(session_pre_key_),
          fido_parsing_utils::ConvertToStringView(nonce_),
          kCableHandshakeKeyInfo,
          /*derived_key_size=*/32)) {
  crypto::RandBytes(client_session_random_);
}

FidoCableV1HandshakeHandler::~FidoCableV1HandshakeHandler() = default;

void FidoCableV1HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  auto handshake_message =
      ConstructHandshakeMessage(handshake_key_, client_session_random_);
  if (!handshake_message) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  FIDO_LOG(DEBUG) << "Sending the caBLE handshake message";
  cable_device_->SendHandshakeMessage(
      fido_parsing_utils::Materialize(*handshake_message), std::move(callback));
}

bool FidoCableV1HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
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
          fido_parsing_utils::ConvertToStringView(authenticator_hello),
          fido_parsing_utils::ConvertToStringView(
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

  const auto* authenticator_random_nonce =
      base::FindOrNull(authenticator_hello_cbor->GetMap(), cbor::Value(1));
  if (!authenticator_random_nonce ||
      !authenticator_random_nonce->is_bytestring()) {
    return false;
  }

  auto sized_nonce_span =
      base::span(authenticator_random_nonce->GetBytestring())
          .to_fixed_extent<16>();
  if (!sized_nonce_span) {
    return false;
  }

  cable_device_->SetV1EncryptionData(
      *base::span(GetEncryptionKeyAfterSuccessfulHandshake(*sized_nonce_span))
           .to_fixed_extent<32>(),
      nonce_);

  return true;
}

std::vector<uint8_t>
FidoCableV1HandshakeHandler::GetEncryptionKeyAfterSuccessfulHandshake(
    base::span<const uint8_t, 16> authenticator_random_nonce) const {
  std::vector<uint8_t> nonce_message;
  fido_parsing_utils::Append(&nonce_message, nonce_);
  fido_parsing_utils::Append(&nonce_message, client_session_random_);
  fido_parsing_utils::Append(&nonce_message, authenticator_random_nonce);
  return crypto::HkdfSha256(session_pre_key_, crypto::SHA256Hash(nonce_message),
                            kCableDeviceEncryptionKeyInfo,
                            /*derived_key_length=*/32);
}

}  // namespace device
