// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
#define DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

// Object containing request parameters for AuthenticatorMakeCredential command
// as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
class COMPONENT_EXPORT(DEVICE_FIDO) CtapMakeCredentialRequest {
 public:
  CtapMakeCredentialRequest(
      base::span<const uint8_t, kClientDataHashLength> client_data_hash,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      PublicKeyCredentialParams public_key_credential_params);
  CtapMakeCredentialRequest(const CtapMakeCredentialRequest& that);
  CtapMakeCredentialRequest(CtapMakeCredentialRequest&& that);
  CtapMakeCredentialRequest& operator=(const CtapMakeCredentialRequest& that);
  CtapMakeCredentialRequest& operator=(CtapMakeCredentialRequest&& that);
  ~CtapMakeCredentialRequest();

  // Serializes MakeCredential request parameter into CBOR encoded map with
  // integer keys and CBOR encoded values as defined by the CTAP spec.
  // https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20180305.html#authenticatorMakeCredential
  std::vector<uint8_t> EncodeAsCBOR() const;

  CtapMakeCredentialRequest& SetUserVerificationRequired(
      bool user_verfication_required);
  CtapMakeCredentialRequest& SetResidentKeySupported(bool resident_key);
  CtapMakeCredentialRequest& SetExcludeList(
      std::vector<PublicKeyCredentialDescriptor> exclude_list);
  CtapMakeCredentialRequest& SetPinAuth(std::vector<uint8_t> pin_auth);
  CtapMakeCredentialRequest& SetPinProtocol(uint8_t pin_protocol);
  CtapMakeCredentialRequest& SetIsIndividualAttestation(
      bool is_individual_attestation);
  CtapMakeCredentialRequest& SetHmacSecret(bool hmac_secret);

  const std::array<uint8_t, kClientDataHashLength>& client_data_hash() const {
    return client_data_hash_;
  }
  const PublicKeyCredentialRpEntity& rp() const { return rp_; }
  const PublicKeyCredentialUserEntity user() const { return user_; }
  const PublicKeyCredentialParams& public_key_credential_params() const {
    return public_key_credential_params_;
  }
  bool user_verification_required() const {
    return user_verification_required_;
  }
  bool resident_key_supported() const { return resident_key_supported_; }
  bool is_individual_attestation() const { return is_individual_attestation_; }
  bool hmac_secret() const { return hmac_secret_; }
  const base::Optional<std::vector<PublicKeyCredentialDescriptor>>&
  exclude_list() const {
    return exclude_list_;
  }
  const base::Optional<std::vector<uint8_t>>& pin_auth() const {
    return pin_auth_;
  }

 private:
  std::array<uint8_t, kClientDataHashLength> client_data_hash_;
  PublicKeyCredentialRpEntity rp_;
  PublicKeyCredentialUserEntity user_;
  PublicKeyCredentialParams public_key_credential_params_;
  bool user_verification_required_ = false;
  bool resident_key_supported_ = false;
  bool is_individual_attestation_ = false;
  // hmac_secret_ indicates whether the "hmac-secret" extension should be
  // asserted to CTAP2 authenticators.
  bool hmac_secret_ = false;

  base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list_;
  base::Optional<std::vector<uint8_t>> pin_auth_;
  base::Optional<uint8_t> pin_protocol_;
};

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<CtapMakeCredentialRequest> ParseCtapMakeCredentialRequest(
    base::span<const uint8_t> request_bytes);

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
