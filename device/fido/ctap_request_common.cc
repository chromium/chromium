// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_request_common.h"

#include "base/logging.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/pin.h"

namespace device {

HMACSecret::HMACSecret(
    base::span<const uint8_t, kP256X962Length> in_public_key_x962,
    base::span<const uint8_t> in_encrypted_salts,
    base::span<const uint8_t> in_salts_auth,
    std::optional<PINUVAuthProtocol> in_pin_protocol)
    : public_key_x962(fido_parsing_utils::Materialize(in_public_key_x962)),
      encrypted_salts(fido_parsing_utils::Materialize(in_encrypted_salts)),
      salts_auth(fido_parsing_utils::Materialize(in_salts_auth)),
      pin_protocol(in_pin_protocol) {}

HMACSecret::HMACSecret(const HMACSecret&) = default;
HMACSecret::~HMACSecret() = default;
HMACSecret& HMACSecret::operator=(const HMACSecret&) = default;

// static
std::optional<HMACSecret> HMACSecret::Parse(
    const cbor::Value::MapValue& hmac_extension) {
  auto hmac_it = hmac_extension.find(cbor::Value(1));
  if (hmac_it == hmac_extension.end() || !hmac_it->second.is_map()) {
    return std::nullopt;
  }
  const std::optional<pin::KeyAgreementResponse> key(
      pin::KeyAgreementResponse::ParseFromCOSE(hmac_it->second.GetMap()));
  if (!key) {
    return std::nullopt;
  }

  hmac_it = hmac_extension.find(cbor::Value(2));
  if (hmac_it == hmac_extension.end() || !hmac_it->second.is_bytestring()) {
    return std::nullopt;
  }
  const std::vector<uint8_t>& encrypted_salts = hmac_it->second.GetBytestring();

  hmac_it = hmac_extension.find(cbor::Value(3));
  if (hmac_it == hmac_extension.end() || !hmac_it->second.is_bytestring()) {
    return std::nullopt;
  }
  const std::vector<uint8_t>& salts_auth = hmac_it->second.GetBytestring();

  std::optional<PINUVAuthProtocol> pin_protocol;
  const auto pin_protocol_it = hmac_extension.find(cbor::Value(4));
  if (pin_protocol_it != hmac_extension.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max()) {
      return std::nullopt;
    }
    pin_protocol = ToPINUVAuthProtocol(pin_protocol_it->second.GetUnsigned());
    if (!pin_protocol) {
      return std::nullopt;
    }
  }
  return HMACSecret(key->X962(), encrypted_salts, salts_auth, pin_protocol);
}

cbor::Value::MapValue HMACSecret::AsCBORMapValue(
    const std::optional<PINUVAuthProtocol>& request_pin_protocol) const {
  cbor::Value::MapValue hmac_extension;
  hmac_extension.emplace(1, pin::EncodeCOSEPublicKey(public_key_x962));
  hmac_extension.emplace(2, encrypted_salts);
  hmac_extension.emplace(3, salts_auth);
  if (request_pin_protocol &&
      static_cast<unsigned>(*request_pin_protocol) >= 2) {
    // If the request is using a PIN protocol other than v1, it must be
    // specified here too:
    // https://fidoalliance.org/specs/fido-v2.2-rd-20230321/fido-client-to-authenticator-protocol-v2.2-rd-20230321.html#sctn-hmac-secret-extension:~:text=pinuvauthprotocol(0x04)%3A%20(optional)%20as%20selected%20when%20getting%20the%20shared%20secret.%20ctap2.1%20platforms%20must%20include%20this%20parameter%20if%20the%20value%20of%20pinuvauthprotocol%20is%20not%201
    hmac_extension.emplace(4, static_cast<int64_t>(*request_pin_protocol));
  }
  return hmac_extension;
}

}  // namespace device
