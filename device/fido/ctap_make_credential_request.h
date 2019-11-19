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
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace cbor {
class Value;
}

namespace device {

// Object containing request parameters for AuthenticatorMakeCredential command
// as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
struct COMPONENT_EXPORT(DEVICE_FIDO) CtapMakeCredentialRequest {
 public:
  using ClientDataHash = std::array<uint8_t, kClientDataHashLength>;

  CtapMakeCredentialRequest(
      std::string client_data_json,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      PublicKeyCredentialParams public_key_credential_params);
  CtapMakeCredentialRequest(const CtapMakeCredentialRequest& that);
  CtapMakeCredentialRequest(CtapMakeCredentialRequest&& that);
  CtapMakeCredentialRequest& operator=(const CtapMakeCredentialRequest& that);
  CtapMakeCredentialRequest& operator=(CtapMakeCredentialRequest&& that);
  ~CtapMakeCredentialRequest();

  std::string client_data_json;
  ClientDataHash client_data_hash;
  PublicKeyCredentialRpEntity rp;
  PublicKeyCredentialUserEntity user;
  PublicKeyCredentialParams public_key_credential_params;
  UserVerificationRequirement user_verification =
      UserVerificationRequirement::kDiscouraged;
  AuthenticatorAttachment authenticator_attachment =
      AuthenticatorAttachment::kAny;
  bool resident_key_required = false;
  // hmac_secret_ indicates whether the "hmac-secret" extension should be
  // asserted to CTAP2 authenticators.
  bool hmac_secret = false;

  // If true, instruct the request handler only to dispatch this request via
  // U2F.
  bool is_u2f_only = false;
  bool is_incognito_mode = false;

  std::vector<PublicKeyCredentialDescriptor> exclude_list;
  base::Optional<std::vector<uint8_t>> pin_auth;
  base::Optional<uint8_t> pin_protocol;
  AttestationConveyancePreference attestation_preference =
      AttestationConveyancePreference::kNone;
  // U2F AppID for excluding credentials.
  base::Optional<std::string> app_id;

  // cred_protect indicates the level of protection afforded to a credential.
  // This depends on a CTAP2 extension that not all authenticators will support.
  // The second element is true if the indicated protection level must be
  // provided by the target authenticator for the MakeCredential request to be
  // sent.
  base::Optional<std::pair<CredProtect, bool>> cred_protect;
};

// Serializes MakeCredential request parameter into CBOR encoded map with
// integer keys and CBOR encoded values as defined by the CTAP spec.
// https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20180305.html#authenticatorMakeCredential
COMPONENT_EXPORT(DEVICE_FIDO)
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapMakeCredentialRequest& request);

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
