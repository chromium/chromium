// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_get_assertion_response.h"

#include <optional>
#include <utility>

#include "components/cbor/values.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"

namespace device {

namespace {

constexpr size_t kFlagIndex = 0;
constexpr size_t kFlagLength = 1;
constexpr size_t kCounterIndex = 1;
constexpr size_t kCounterLength = 4;
constexpr size_t kSignatureIndex = 5;

}  // namespace

// static
std::optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
    base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
    base::span<const uint8_t> u2f_data,
    base::span<const uint8_t> key_handle,
    std::optional<FidoTransportProtocol> transport_used) {
  if (u2f_data.size() <= kSignatureIndex) {
    return std::nullopt;
  }

  if (key_handle.empty()) {
    return std::nullopt;
  }

  auto flags = u2f_data.subspan<kFlagIndex, kFlagLength>()[0];
  if (flags &
      (static_cast<uint8_t>(AuthenticatorData::Flag::kExtensionDataIncluded) |
       static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation))) {
    // U2F responses cannot assert CTAP2 features.
    return std::nullopt;
  }
  auto counter = u2f_data.subspan<kCounterIndex, kCounterLength>();
  AuthenticatorData authenticator_data(relying_party_id_hash, flags, counter,
                                       std::nullopt);

  auto signature =
      fido_parsing_utils::Materialize(u2f_data.subspan(kSignatureIndex));

  bssl::UniquePtr<ECDSA_SIG> parsed_sig(
      ECDSA_SIG_from_bytes(signature.data(), signature.size()));
  if (!parsed_sig) {
    FIDO_LOG(ERROR)
        << "Rejecting U2F assertion response with invalid signature";
    return std::nullopt;
  }

  AuthenticatorGetAssertionResponse response(
      std::move(authenticator_data), std::move(signature), transport_used);
  response.credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, fido_parsing_utils::Materialize(key_handle));
  return std::move(response);
}

AuthenticatorGetAssertionResponse::AuthenticatorGetAssertionResponse(
    AuthenticatorData authenticator_data,
    std::vector<uint8_t> signature,
    std::optional<FidoTransportProtocol> transport_used)
    : authenticator_data(std::move(authenticator_data)),
      signature(std::move(signature)),
      transport_used(transport_used) {}

AuthenticatorGetAssertionResponse::AuthenticatorGetAssertionResponse(
    AuthenticatorGetAssertionResponse&& that) = default;

AuthenticatorGetAssertionResponse& AuthenticatorGetAssertionResponse::operator=(
    AuthenticatorGetAssertionResponse&& other) = default;

AuthenticatorGetAssertionResponse::~AuthenticatorGetAssertionResponse() =
    default;

}  // namespace device
