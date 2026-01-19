// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_REQUEST_COMMON_H_
#define DEVICE_FIDO_CTAP_REQUEST_COMMON_H_

#include <array>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "components/cbor/values.h"
#include "device/fido/public/fido_constants.h"

namespace device {

// HMACSecret contains the inputs to the hmac-secret extension:
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension
// and hmac-secret-mc extension:
// https://fidoalliance.org/specs/fido-v2.2-ps-20250714/fido-client-to-authenticator-protocol-v2.2-ps-20250714.html#sctn-hmac-secret-make-cred-extension
struct COMPONENT_EXPORT(DEVICE_FIDO) HMACSecret {
  HMACSecret(base::span<const uint8_t, kP256X962Length> public_key_x962,
             base::span<const uint8_t> encrypted_salts,
             base::span<const uint8_t> salts_auth,
             std::optional<PINUVAuthProtocol> pin_protocol);
  HMACSecret(const HMACSecret&);
  ~HMACSecret();
  HMACSecret& operator=(const HMACSecret&);

  // Decodes following CTAP extensions:
  // - hmac-secret extension in CTAP2 authenticatorGetAssertion request.
  // - hmac-secret-mc extension in CTAP2 authenticatorMakeCredential request.
  static std::optional<HMACSecret> Parse(const cbor::Value::MapValue&);

  cbor::Value::MapValue AsCBORMapValue(
      const std::optional<PINUVAuthProtocol>& request_pin_protocol) const;

  std::array<uint8_t, kP256X962Length> public_key_x962;
  std::vector<uint8_t> encrypted_salts;
  std::vector<uint8_t> salts_auth;
  // pin_protocol is ignored during serialisation and the request's PIN
  // protocol will be used instead.
  std::optional<PINUVAuthProtocol> pin_protocol;
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_REQUEST_COMMON_H_
