// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "crypto/hash.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
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

std::vector<uint8_t> ConstructHandshakeMessage(
    std::string_view handshake_key,
    base::span<const uint8_t, 16> client_random_nonce) {
  cbor::Value::MapValue map;
  map.emplace(0, kCableClientHelloMessage);
  map.emplace(1, client_random_nonce);
  auto hello = *cbor::Writer::Write(cbor::Value(std::move(map)));

  const auto mac =
      crypto::hmac::SignSha256(base::as_byte_span(handshake_key), hello);

  constexpr size_t kMacOffset =
      kClientHelloMessageSize - kCableHandshakeMacMessageSize;

  CHECK_EQ(hello.size(), kMacOffset);
  hello.resize(kClientHelloMessageSize);
  auto out_mac = base::span(hello).subspan(kMacOffset);
  out_mac.copy_from(base::span(mac).first<kCableHandshakeMacMessageSize>());

  return hello;
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
      handshake_key_(crypto::HkdfSha256(base::as_string_view(session_pre_key_),
                                        base::as_string_view(nonce_),
                                        kCableHandshakeKeyInfo,
                                        /*derived_key_size=*/32)) {
  crypto::RandBytes(client_session_random_);
}

FidoCableV1HandshakeHandler::~FidoCableV1HandshakeHandler() = default;

void FidoCableV1HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  auto handshake_message =
      ConstructHandshakeMessage(handshake_key_, client_session_random_);

  FIDO_LOG(DEBUG) << "Sending the caBLE handshake message";
  cable_device_->SendHandshakeMessage(std::move(handshake_message),
                                      std::move(callback));
}

bool FidoCableV1HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  if (response.size() != kCableAuthenticatorHandshakeMessageSize) {
    return false;
  }

  constexpr size_t kMacOffset =
      kCableAuthenticatorHandshakeMessageSize - kCableHandshakeMacMessageSize;
  const auto [authenticator_hello, expected_truncated_mac] =
      response.split_at(kMacOffset);
  const auto actual_mac = crypto::hmac::SignSha256(
      base::as_byte_span(handshake_key_), authenticator_hello);

  const auto actual_truncated_mac =
      base::span(actual_mac).first(std::size(expected_truncated_mac));
  if (!crypto::SecureMemEqual(expected_truncated_mac, actual_truncated_mac)) {
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
      base::as_byte_span(
          GetEncryptionKeyAfterSuccessfulHandshake(*sized_nonce_span)),
      nonce_);

  return true;
}

std::array<uint8_t, 32>
FidoCableV1HandshakeHandler::GetEncryptionKeyAfterSuccessfulHandshake(
    base::span<const uint8_t, 16> authenticator_random_nonce) const {
  std::vector<uint8_t> nonce_message;
  fido_parsing_utils::Append(&nonce_message, nonce_);
  fido_parsing_utils::Append(&nonce_message, client_session_random_);
  fido_parsing_utils::Append(&nonce_message, authenticator_random_nonce);
  return crypto::HkdfSha256<32>(session_pre_key_,
                                crypto::hash::Sha256(nonce_message),
                                kCableDeviceEncryptionKeyInfo);
}

}  // namespace device
