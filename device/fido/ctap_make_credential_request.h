// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
#define DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/fido_constants.h"
#include "device/fido/json_request.h"
#include "device/fido/pin.h"
#include "device/fido/prf_input.h"
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

  // ParseOpts are optional parameters passed to Parse().
  struct ParseOpts {
    // reject_all_extensions makes parsing fail if any extensions are present.
    bool reject_all_extensions = false;
  };

  // Decodes a CTAP2 authenticatorMakeCredential request message. The request's
  // |client_data_json| will be empty and |client_data_hash| will be set.
  static std::optional<CtapMakeCredentialRequest> Parse(
      const cbor::Value::MapValue& request_map) {
    return Parse(request_map, ParseOpts());
  }
  static std::optional<CtapMakeCredentialRequest> Parse(
      const cbor::Value::MapValue& request_map,
      const ParseOpts& opts);

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

  // hmac_secret indicates whether the "hmac-secret" extension should be
  // asserted to CTAP2 authenticators.
  bool hmac_secret = false;

  // prf indicates that the "prf" extension should be asserted to request that
  // the authenticator associate a PRF with the credential.
  bool prf = false;

  // prf_input contains the hashed salts for doing a PRF evaluation at
  // credential creation time. This is only possible when the authenticator
  // supports the "prf" extension, i.e. over hybrid CTAP.
  std::optional<PRFInput> prf_input;

  // large_blob_support indicates whether support for largeBlobs should be
  // requested using the `largeBlob` extension. This should be mutually
  // exclusive with `large_blob_key`.
  LargeBlobSupport large_blob_support = LargeBlobSupport::kNotRequested;

  // large_blob_key indicates whether a large blob key should be associated to
  // the new credential through the "largeBlobKey" extension. This should be
  // mutually exclusive with `large_blob_support`.
  bool large_blob_key = false;

  std::vector<PublicKeyCredentialDescriptor> exclude_list;

  // The pinUvAuthParam field. This is the result of calling
  // |pin::TokenResponse::PinAuth(client_data_hash)| with the PIN/UV Auth Token
  // response obtained from the authenticator.
  std::optional<std::vector<uint8_t>> pin_auth;

  // The pinUvAuthProtocol field. It is the version of the PIN/UV Auth Token
  // response obtained from the authenticator.
  std::optional<PINUVAuthProtocol> pin_protocol;

  // The PIN/UV Auth Token response obtained from the authenticator. This field
  // is only used for computing a fresh pinUvAuthParam for getAssertion requests
  // during silent probing of |exclude_list| credentials. It is ignored when
  // encoding this request to CBOR (|pin_auth| and |pin_protocol| are used for
  // that).
  std::optional<pin::TokenResponse> pin_token_for_exclude_list_probing;

  AttestationConveyancePreference attestation_preference =
      AttestationConveyancePreference::kNone;

  // U2F AppID for excluding credentials.
  std::optional<std::string> app_id_exclude;

  // cred_protect indicates the level of protection afforded to a credential.
  // This depends on a CTAP2 extension that not all authenticators will support.
  // This is filled out by |MakeCredentialRequestHandler|.
  std::optional<CredProtect> cred_protect;

  // If |cred_protect| is not |nullopt|, this is true if the credProtect level
  // must be provided by the target authenticator for the MakeCredential request
  // to be sent. This only makes sense when there is a collection of
  // authenticators to consider, i.e. for the Windows API.
  bool cred_protect_enforce = false;

  // min_pin_length_requested indicates that the minPinLength extension[1]
  // should be sent to request that the authenticator report the minimum allowed
  // PIN length configured.
  //
  // [1]
  // https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-20210615.html#sctn-minpinlength-extension
  bool min_pin_length_requested = false;

  // cred_blob contains an optional credBlob extension.
  // https://fidoalliance.org/specs/fido-v2.1-rd-20201208/fido-client-to-authenticator-protocol-v2.1-rd-20201208.html#sctn-credBlob-extension
  std::optional<std::vector<uint8_t>> cred_blob;
};

// MakeCredentialOptions contains higher-level request parameters that aren't
// part of the makeCredential request itself, or that need to be combined with
// knowledge of the specific authenticator, thus don't live in
// |CtapMakeCredentialRequest|.
struct COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialOptions {
  MakeCredentialOptions();
  explicit MakeCredentialOptions(
      const AuthenticatorSelectionCriteria& authenticator_selection_criteria);
  ~MakeCredentialOptions();
  MakeCredentialOptions(const MakeCredentialOptions&);
  MakeCredentialOptions(MakeCredentialOptions&&);
  MakeCredentialOptions& operator=(const MakeCredentialOptions&);
  MakeCredentialOptions& operator=(MakeCredentialOptions&&);

  // The JSON form of the request. (May be nullptr.)
  scoped_refptr<JSONRequest> json;

  // authenticator_attachment is a constraint on the type of authenticator
  // that a credential should be created on.
  AuthenticatorAttachment authenticator_attachment =
      AuthenticatorAttachment::kAny;

  // resident_key indicates whether the request should result in the creation
  // of a client-side discoverable credential (aka resident key).
  ResidentKeyRequirement resident_key = ResidentKeyRequirement::kDiscouraged;

  // user_verification indicates whether the authenticator should (or must)
  // perform user verficiation before creating the credential.
  UserVerificationRequirement user_verification =
      UserVerificationRequirement::kPreferred;

  // cred_protect_request extends |CredProtect| to include information that
  // applies at request-routing time. The second element is true if the
  // indicated protection level must be provided by the target authenticator
  // for the MakeCredential request to be sent.
  std::optional<std::pair<CredProtectRequest, bool>> cred_protect_request;

  // allow_skipping_pin_touch causes the handler to forego the first
  // "touch-only" step to collect a PIN if exactly one authenticator is
  // discovered.
  bool allow_skipping_pin_touch = false;

  // large_blob_support indicates whether the request should select for
  // authenticators supporting the largeBlobs extension (kRequired), merely
  // indicate support on the response (kPreferred), or ignore it
  // (kNotRequested).
  // Values other than kNotRequested will attempt to initialize the large blob
  // on the authenticator.
  LargeBlobSupport large_blob_support = LargeBlobSupport::kNotRequested;

  // Indicates whether the request was created in an off-the-record
  // BrowserContext (e.g. Chrome Incognito mode).
  bool is_off_the_record_context = false;
};

// Serializes MakeCredential request parameter into CBOR encoded map with
// integer keys and CBOR encoded values as defined by the CTAP spec.
// https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20180305.html#authenticatorMakeCredential
COMPONENT_EXPORT(DEVICE_FIDO)
std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapMakeCredentialRequest& request);

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
