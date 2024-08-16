// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/webauthn/json/value_conversions.h"

#include <iterator>
#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/ranges/ranges.h"
#include "base/values.h"
#include "device/fido/attestation_object.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace webauthn {

namespace {

std::string Base64UrlEncode(base::span<const uint8_t> input) {
  // Byte strings, which appear in the WebAuthn IDL as ArrayBuffer or
  // ByteSource, are base64url-encoded without trailing '=' padding.
  std::string output;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(input.data()),
                       input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &output);
  return output;
}

bool Base64UrlDecode(std::string_view input, std::string* output) {
  return base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::DISALLOW_PADDING, output);
}

// Base64url-decodes the value of `key` from `dict`. Returns `nullopt` if the
// key isn't present or decoding failed.
std::optional<std::string> Base64UrlDecodeStringKey(
    const base::Value::Dict& dict,
    const std::string& key) {
  const std::string* b64url_data = dict.FindString(key);
  if (!b64url_data) {
    return std::nullopt;
  }
  std::string decoded;
  if (!Base64UrlDecode(*b64url_data, &decoded)) {
    return std::nullopt;
  }
  return decoded;
}

// Like `Base64UrlDecodeStringKey()` attempts to find and base64-decode the
// value of `key` in `dict`. However, the value is optional and so may be
// `base::Value::Type::NONE` or may be omitted. The style of omitted values
// is changing: initially they were expressed as `null` values in JSON objects,
// but as of https://github.com/w3c/webauthn/pull/1878 they'll be omitted
// instead. This code is in a transitional state where either form is accepted.
//
// Returns true on success and the decoded result if the value was a string.
// Returns `{false, std::nullopt}` if the key wasn't found or if decoding the
// string failed.
std::tuple<bool, std::optional<std::string>> Base64UrlDecodeOptionalStringKey(
    const base::Value::Dict& dict,
    const std::string& key) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    return {true, std::nullopt};
  }
  if (value->is_none()) {
    return {false, std::nullopt};
  }
  std::string decoded;
  if (!value->is_string() || !Base64UrlDecode(value->GetString(), &decoded)) {
    return {false, std::nullopt};
  }
  return {true, decoded};
}

std::vector<uint8_t> ToByteVector(const std::string& in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

base::Value ToValue(const device::PublicKeyCredentialRpEntity& relying_party) {
  base::Value::Dict value;
  value.Set("id", relying_party.id);
  // `PublicKeyCredentialEntity.name` is required in the IDL but optional on the
  // mojo struct.
  value.Set("name", relying_party.name.value_or(""));
  return base::Value(std::move(value));
}

base::Value ToValue(const device::PublicKeyCredentialUserEntity& user) {
  base::Value::Dict value;
  value.Set("id", Base64UrlEncode(user.id));
  // `PublicKeyCredentialEntity.name` is required in the IDL but optional on the
  // mojo struct.
  value.Set("name", user.name.value_or(""));
  // `PublicKeyCredentialUserEntity.displayName` is required in the IDL but
  // optional on the mojo struct.
  value.Set("displayName", user.display_name.value_or(""));
  return base::Value(std::move(value));
}

base::Value ToValue(
    const device::PublicKeyCredentialParams::CredentialInfo& params) {
  base::Value::Dict value;
  switch (params.type) {
    case device::CredentialType::kPublicKey:
      value.Set("type", device::kPublicKey);
  }
  value.Set("alg", params.algorithm);
  return base::Value(std::move(value));
}

base::Value ToValue(const device::PublicKeyCredentialDescriptor& descriptor) {
  base::Value::Dict value;
  switch (descriptor.credential_type) {
    case device::CredentialType::kPublicKey:
      value.Set("type", device::kPublicKey);
  }
  value.Set("id", Base64UrlEncode(descriptor.id));
  base::Value::List transports;
  for (const device::FidoTransportProtocol& transport : descriptor.transports) {
    transports.Append(ToString(transport));
  }
  if (!transports.empty()) {
    value.Set("transports", std::move(transports));
  }
  return base::Value(std::move(value));
}

base::Value ToValue(
    const device::AuthenticatorAttachment& authenticator_attachment) {
  switch (authenticator_attachment) {
    case device::AuthenticatorAttachment::kCrossPlatform:
      return base::Value("cross-platform");
    case device::AuthenticatorAttachment::kPlatform:
      return base::Value("platform");
    case device::AuthenticatorAttachment::kAny:
      // Any maps to the key being omitted, not a null value.
      NOTREACHED_IN_MIGRATION();
      return base::Value("invalid");
  }
}

base::Value ToValue(
    const device::ResidentKeyRequirement& resident_key_requirement) {
  switch (resident_key_requirement) {
    case device::ResidentKeyRequirement::kDiscouraged:
      return base::Value("discouraged");
    case device::ResidentKeyRequirement::kPreferred:
      return base::Value("preferred");
    case device::ResidentKeyRequirement::kRequired:
      return base::Value("required");
  }
}

base::Value ToValue(
    const device::UserVerificationRequirement& user_verification_requirement) {
  switch (user_verification_requirement) {
    case device::UserVerificationRequirement::kDiscouraged:
      return base::Value("discouraged");
    case device::UserVerificationRequirement::kPreferred:
      return base::Value("preferred");
    case device::UserVerificationRequirement::kRequired:
      return base::Value("required");
  }
}

base::Value ToValue(
    const device::AuthenticatorSelectionCriteria& authenticator_selection) {
  base::Value::Dict value;
  std::optional<std::string> attachment;
  if (authenticator_selection.authenticator_attachment !=
      device::AuthenticatorAttachment::kAny) {
    value.Set("authenticatorAttachment",
              ToValue(authenticator_selection.authenticator_attachment));
  }
  value.Set("residentKey", ToValue(authenticator_selection.resident_key));
  value.Set("userVerification",
            ToValue(authenticator_selection.user_verification_requirement));
  return base::Value(std::move(value));
}

base::Value ToValue(const device::AttestationConveyancePreference&
                        attestation_conveyance_preference) {
  switch (attestation_conveyance_preference) {
    case device::AttestationConveyancePreference::kNone:
      return base::Value("none");
    case device::AttestationConveyancePreference::kIndirect:
      return base::Value("indirect");
    case device::AttestationConveyancePreference::kDirect:
      return base::Value("direct");
    case device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
    case device::AttestationConveyancePreference::
        kEnterpriseIfRPListedOnAuthenticator:
      return base::Value("enterprise");
  }
}

base::Value ToValue(const blink::mojom::RemoteDesktopClientOverride&
                        remote_desktop_client_override) {
  base::Value::Dict value;
  value.Set("origin", remote_desktop_client_override.origin.Serialize());
  value.Set("sameOriginWithAncestors",
            remote_desktop_client_override.same_origin_with_ancestors);
  return base::Value(std::move(value));
}

base::Value ToValue(const blink::mojom::ProtectionPolicy policy) {
  switch (policy) {
    case blink::mojom::ProtectionPolicy::UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      return base::Value("invalid");
    case blink::mojom::ProtectionPolicy::NONE:
      return base::Value("userVerificationOptional");
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      return base::Value("userVerificationOptionalWithCredentialIDList");
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      return base::Value("userVerificationRequired");
  }
}

base::Value ToValue(const device::LargeBlobSupport large_blob) {
  switch (large_blob) {
    case device::LargeBlobSupport::kNotRequested:
      NOTREACHED_IN_MIGRATION();
      return base::Value("invalid");
    case device::LargeBlobSupport::kRequired:
      return base::Value("required");
    case device::LargeBlobSupport::kPreferred:
      return base::Value("preferred");
  }
}

base::Value ToValue(const device::CableDiscoveryData& cable_authentication) {
  base::Value::Dict value;
  switch (cable_authentication.version) {
    case device::CableDiscoveryData::Version::INVALID:
      NOTREACHED_IN_MIGRATION();
      break;
    case device::CableDiscoveryData::Version::V1:
      value.Set("version", 1);
      value.Set("clientEid",
                Base64UrlEncode(cable_authentication.v1->client_eid));
      value.Set("authenticatorEid",
                Base64UrlEncode(cable_authentication.v1->authenticator_eid));
      value.Set("sessionPreKey",
                Base64UrlEncode(cable_authentication.v1->session_pre_key));
      break;
    case device::CableDiscoveryData::Version::V2:
      value.Set("version", 2);
      value.Set("clientEid",
                Base64UrlEncode(cable_authentication.v2->experiments));
      value.Set("authenticatorEid", "");
      value.Set("sessionPreKey",
                Base64UrlEncode(cable_authentication.v2->server_link_data));
      break;
  }
  return base::Value(std::move(value));
}

base::Value ToValue(
    const blink::mojom::SupplementalPubKeysRequestPtr& supplemental_pub_keys) {
  base::Value::List scopes;
  if (supplemental_pub_keys->device_scope_requested) {
    scopes.Append("device");
  }
  if (supplemental_pub_keys->provider_scope_requested) {
    scopes.Append("provider");
  }

  base::Value::Dict value;
  value.Set("scopes", std::move(scopes));
  if (supplemental_pub_keys->attestation !=
      device::AttestationConveyancePreference::kIndirect) {
    value.Set("attestation", ToValue(supplemental_pub_keys->attestation));
  }
  if (supplemental_pub_keys->attestation_formats.size()) {
    base::Value::List formats;
    for (const std::string& format :
         supplemental_pub_keys->attestation_formats) {
      formats.Append(format);
    }
    value.Set("attestationFormats", std::move(formats));
  }

  return base::Value(std::move(value));
}

base::Value ToValue(const std::vector<std::string>& strings) {
  base::Value::List ret;
  ret.reserve(strings.size());
  for (const auto& string : strings) {
    ret.Append(string);
  }
  return base::Value(std::move(ret));
}

std::optional<device::FidoTransportProtocol> FidoTransportProtocolFromValue(
    const base::Value& value) {
  if (!value.is_string()) {
    return std::nullopt;
  }
  return device::ConvertToFidoTransportProtocol(value.GetString());
}

std::optional<device::AuthenticatorAttachment>
OptionalAuthenticatorAttachmentFromValue(const base::Value* value) {
  if (!value) {
    // PublicKeyCredential.authenticatorAttachment can be omitted,
    // which is equivalent to `AuthenticatorAttachment::kAny`.
    return device::AuthenticatorAttachment::kAny;
  }
  if (value->is_none() || !value->is_string()) {
    return std::nullopt;
  }
  const std::string& attachment_name = value->GetString();
  if (attachment_name == "platform") {
    return device::AuthenticatorAttachment::kPlatform;
  } else if (attachment_name == "cross-platform") {
    return device::AuthenticatorAttachment::kCrossPlatform;
  }
  return std::nullopt;
}

std::pair<blink::mojom::MakeCredentialAuthenticatorResponsePtr, std::string>
InvalidMakeCredentialField(const char* field_name) {
  return {nullptr, std::string("field missing or invalid: ") + field_name};
}

std::pair<blink::mojom::GetAssertionAuthenticatorResponsePtr, std::string>
InvalidGetAssertionField(const char* field_name) {
  return {nullptr, std::string("field missing or invalid: ") + field_name};
}

base::Value ToValue(const blink::mojom::PRFValuesPtr& prf_input) {
  base::Value::Dict prf_value;
  prf_value.Set("first", Base64UrlEncode(prf_input->first));
  if (prf_input->second) {
    prf_value.Set("second", Base64UrlEncode(*prf_input->second));
  }
  return base::Value(std::move(prf_value));
}

base::Value ToValue(const std::vector<blink::mojom::Hint>& hints) {
  base::Value::List ret;
  for (const auto& hint : hints) {
    switch (hint) {
      case blink::mojom::Hint::SECURITY_KEY:
        ret.Append(base::Value("security-key"));
        break;
      case blink::mojom::Hint::HYBRID:
        ret.Append(base::Value("hybrid"));
        break;
      case blink::mojom::Hint::CLIENT_DEVICE:
        ret.Append(base::Value("client-device"));
        break;
    }
  }
  return base::Value(std::move(ret));
}

}  // namespace

base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options) {
  base::Value::Dict value;
  value.Set("rp", ToValue(options->relying_party));
  value.Set("user", ToValue(options->user));
  value.Set("challenge", Base64UrlEncode(options->challenge));
  base::Value::List public_key_parameters;
  for (const device::PublicKeyCredentialParams::CredentialInfo& params :
       options->public_key_parameters) {
    public_key_parameters.Append(ToValue(params));
  }
  value.Set("pubKeyCredParams", std::move(public_key_parameters));
  base::Value::List exclude_credentials;
  for (const device::PublicKeyCredentialDescriptor& descriptor :
       options->exclude_credentials) {
    exclude_credentials.Append(ToValue(descriptor));
  }
  value.Set("excludeCredentials", std::move(exclude_credentials));
  if (options->authenticator_selection) {
    value.Set("authenticatorSelection",
              ToValue(*options->authenticator_selection));
  }
  if (!options->hints.empty()) {
    value.Set("hints", ToValue(options->hints));
  }
  value.Set("attestation", ToValue(options->attestation));

  if (!options->attestation_formats.empty()) {
    value.Set("attestationFormats", ToValue(options->attestation_formats));
  }

  base::Value::Dict extensions;

  if (options->hmac_create_secret) {
    extensions.Set("hmacCreateSecret", true);
  }

  if (options->protection_policy !=
      blink::mojom::ProtectionPolicy::UNSPECIFIED) {
    extensions.Set("credentialProtectionPolicy",
                   ToValue(options->protection_policy));
    extensions.Set("enforceCredentialProtectionPolicy",
                   options->enforce_protection_policy);
  }

  if (options->appid_exclude) {
    extensions.Set("appIdExclude", *options->appid_exclude);
  }

  if (options->cred_props) {
    extensions.Set("credProps", true);
  }

  if (options->large_blob_enable != device::LargeBlobSupport::kNotRequested) {
    base::Value::Dict large_blob_value;
    large_blob_value.Set("support", ToValue(options->large_blob_enable));
    extensions.Set("largeBlob", std::move(large_blob_value));
  }

  if (options->cred_blob) {
    extensions.Set("credBlob", Base64UrlEncode(*options->cred_blob));
  }

  if (options->min_pin_length_requested) {
    extensions.Set("minPinLength", true);
  }

  if (options->remote_desktop_client_override) {
    extensions.Set("remoteDesktopClientOverride",
                   ToValue(*options->remote_desktop_client_override));
  }

  if (options->prf_enable) {
    base::Value::Dict prf_value;
    if (options->prf_input) {
      prf_value.Set("eval", ToValue(options->prf_input));
    }
    extensions.Set("prf", std::move(prf_value));
  }

  // On Android, requests with the payments extension should not be forwarded to
  // CredMan and so shouldn't need to be serialized to JSON. But we might end
  // up sending such requests to an enclave.
  if (options->is_payment_credential_creation) {
    base::Value::Dict payments_value;
    payments_value.Set("isPayment", true);
    extensions.Set("payment", std::move(payments_value));
  }

  if (options->supplemental_pub_keys) {
    extensions.Set("supplementalPubKeys",
                   ToValue(options->supplemental_pub_keys));
  }

  if (!extensions.empty()) {
    value.Set("extensions", std::move(extensions));
  }

  return base::Value(std::move(value));
}

base::Value ToValue(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options) {
  CHECK(!options->extensions.is_null());
  base::Value::Dict value;
  value.Set("challenge", Base64UrlEncode(options->challenge));
  value.Set("rpId", options->relying_party_id);

  base::Value::List allow_credentials;
  for (const device::PublicKeyCredentialDescriptor& descriptor :
       options->allow_credentials) {
    allow_credentials.Append(ToValue(descriptor));
  }
  value.Set("allowCredentials", std::move(allow_credentials));

  value.Set("userVerification", ToValue(options->user_verification));
  if (!options->hints.empty()) {
    value.Set("hints", ToValue(options->hints));
  }

  base::Value::Dict extensions;

  if (options->extensions->appid) {
    extensions.Set("appid", *options->extensions->appid);
  }

  base::Value::List cable_authentication_data;
  for (const device::CableDiscoveryData& cable :
       options->extensions->cable_authentication_data) {
    cable_authentication_data.Append(ToValue(cable));
  }
  if (!cable_authentication_data.empty()) {
    extensions.Set("cableAuthentication", std::move(cable_authentication_data));
  }

  if (options->extensions->get_cred_blob) {
    extensions.Set("getCredBlob", true);
  }

  if (options->extensions->large_blob_read ||
      options->extensions->large_blob_write) {
    base::Value::Dict large_blob_value;
    if (options->extensions->large_blob_read) {
      large_blob_value.Set("read", true);
    }
    if (options->extensions->large_blob_write) {
      large_blob_value.Set(
          "write", Base64UrlEncode(*options->extensions->large_blob_write));
    }
    extensions.Set("largeBlob", std::move(large_blob_value));
  }

  if (options->extensions->remote_desktop_client_override) {
    extensions.Set(
        "remoteDesktopClientOverride",
        ToValue(*options->extensions->remote_desktop_client_override));
  }

  if (!options->extensions->prf_inputs.empty()) {
    // Hashed PRF inputs are only used when Chrome is acting as a caBLE
    // authenticator on Android. We can't convert the request to JSON in that
    // context and should never try.
    CHECK(!options->extensions->prf_inputs_hashed);

    base::Value::Dict prf_value;
    base::Value::Dict eval_by_cred;
    bool is_first = true;
    for (const blink::mojom::PRFValuesPtr& prf_input :
         options->extensions->prf_inputs) {
      // The first element of `prf_inputs` may be a default, which applies when
      // no specific credential ID matches. All other values must specify the
      // credential ID that they apply to.
      if (!prf_input->id) {
        CHECK(is_first);
        prf_value.Set("eval", ToValue(prf_input));
      } else {
        eval_by_cred.Set(Base64UrlEncode(*prf_input->id), ToValue(prf_input));
      }
      is_first = false;
    }
    if (!eval_by_cred.empty()) {
      prf_value.Set("evalByCredential", std::move(eval_by_cred));
    }
    extensions.Set("prf", std::move(prf_value));
  }

  if (options->extensions->supplemental_pub_keys) {
    extensions.Set("supplementalPubKeys",
                   ToValue(options->extensions->supplemental_pub_keys));
  }

  if (!extensions.empty()) {
    value.Set("extensions", std::move(extensions));
  }

  return base::Value(std::move(value));
}

std::optional<blink::mojom::PRFValuesPtr> ParsePRFResults(
    const base::Value::Dict* results) {
  const std::optional<std::string> first =
      Base64UrlDecodeStringKey(*results, "first");
  if (!first || first->size() != 32) {
    return std::nullopt;
  }

  auto [ok, second] = Base64UrlDecodeOptionalStringKey(*results, "second");
  if (!ok || (second && second->size() != 32)) {
    return std::nullopt;
  }

  return blink::mojom::PRFValues::New(
      /*id=*/std::nullopt, ToByteVector(*first),
      second ? std::optional<std::vector<uint8_t>>(ToByteVector(*second))
             : std::nullopt);
}

std::optional<blink::mojom::SupplementalPubKeysResponsePtr>
ParseSupplementalPubKeys(const base::Value::Dict* json) {
  const base::Value::List* signatures = json->FindList("signatures");
  if (!signatures || signatures->empty()) {
    return std::nullopt;
  }

  auto ret = blink::mojom::SupplementalPubKeysResponse::New();
  for (const base::Value& b64url_signature : *signatures) {
    if (!b64url_signature.is_string()) {
      return std::nullopt;
    }
    std::optional<std::vector<uint8_t>> signature =
        Base64UrlDecode(b64url_signature.GetString(),
                        base::Base64UrlDecodePolicy::DISALLOW_PADDING);
    if (!signature) {
      return std::nullopt;
    }
    ret->signatures.emplace_back(std::move(*signature));
  }

  return ret;
}

std::pair<blink::mojom::MakeCredentialAuthenticatorResponsePtr, std::string>
MakeCredentialResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    return {nullptr, "value is not a dict"};
  }

  const base::Value::Dict& dict = value.GetDict();
  const std::string* type = dict.FindString("type");
  if (!type || *type != device::kPublicKey) {
    return InvalidMakeCredentialField("type");
  }

  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  response->info = blink::mojom::CommonCredentialInfo::New();

  const std::string* id = dict.FindString("id");
  if (!id) {
    return InvalidMakeCredentialField("id");
  }
  response->info->id = *id;
  std::optional<std::string> raw_id = Base64UrlDecodeStringKey(dict, "rawId");
  if (!raw_id) {
    return InvalidMakeCredentialField("rawId");
  }
  response->info->raw_id = ToByteVector(*raw_id);

  std::optional<device::AuthenticatorAttachment> authenticator_attachment =
      OptionalAuthenticatorAttachmentFromValue(
          dict.Find("authenticatorAttachment"));
  if (!authenticator_attachment) {
    return InvalidMakeCredentialField("authenticatorAttachment");
  }
  response->authenticator_attachment = *authenticator_attachment;

  const base::Value::Dict* attestation_response = dict.FindDict("response");
  if (!attestation_response) {
    return InvalidMakeCredentialField("response");
  }

  std::optional<std::string> attestation_object =
      Base64UrlDecodeStringKey(*attestation_response, "attestationObject");
  if (!attestation_object) {
    return InvalidMakeCredentialField("attestationObject");
  }
  std::vector<uint8_t> attestation_object_bytes =
      ToByteVector(*attestation_object);

  std::optional<device::AttestationObject::ResponseFields> fields =
      device::AttestationObject::ParseForResponseFields(
          std::move(attestation_object_bytes),
          /*attestation_acceptable=*/true);
  if (!fields) {
    return InvalidMakeCredentialField("attestationObject");
  }
  response->attestation_object = std::move(fields->attestation_object_bytes);

  // These fields are checked against the calculated values to ensure that
  // bugs in providers don't sneak in.
  std::optional<int> opt_public_key_algo =
      attestation_response->FindInt("publicKeyAlgorithm");
  if (!opt_public_key_algo || *opt_public_key_algo != fields->public_key_algo) {
    return InvalidMakeCredentialField("publicKeyAlgorithm");
  }
  response->public_key_algo = *opt_public_key_algo;

  std::optional<std::string> opt_authenticator_data =
      Base64UrlDecodeStringKey(*attestation_response, "authenticatorData");
  if (!opt_authenticator_data) {
    return InvalidMakeCredentialField("authenticatorData");
  }
  response->info->authenticator_data = ToByteVector(*opt_authenticator_data);
  if (!base::ranges::equal(response->info->authenticator_data,
                           fields->authenticator_data)) {
    return InvalidMakeCredentialField("authenticatorData");
  }

  auto [ok, opt_public_key] =
      Base64UrlDecodeOptionalStringKey(*attestation_response, "publicKey");
  if (!ok) {
    return InvalidMakeCredentialField("publicKey");
  }
  if (opt_public_key) {
    response->public_key_der = ToByteVector(*opt_public_key);
  }
  // For P-256 and Ed25519 keys, providers must be able to provide the
  // publicKey.
  if ((response->public_key_algo ==
           static_cast<int>(device::CoseAlgorithmIdentifier::kEs256) ||
       response->public_key_algo ==
           static_cast<int>(device::CoseAlgorithmIdentifier::kEdDSA)) &&
      !opt_public_key) {
    return InvalidMakeCredentialField("publicKey");
  }
  // For any key, providers must calculate the same key as us.
  if (fields->public_key_der && opt_public_key &&
      !base::ranges::equal(*response->public_key_der,
                           *fields->public_key_der)) {
    return InvalidMakeCredentialField("publicKey");
  }

  std::optional<std::string> client_data_json =
      Base64UrlDecodeStringKey(*attestation_response, "clientDataJSON");
  // Providers can return an empty clientDataJson when it knows it will
  // be overridden by the caller.
  if (client_data_json) {
    response->info->client_data_json = ToByteVector(*client_data_json);
  }

  const base::Value::List* transports =
      attestation_response->FindList("transports");
  if (!transports) {
    return InvalidMakeCredentialField("transports");
  }
  for (const base::Value& transport_name : *transports) {
    std::optional<device::FidoTransportProtocol> transport =
        FidoTransportProtocolFromValue(transport_name);
    // Unknown transports are ignored because new transport values might be
    // introduced in the future. Plausibly we should pass them as opaque
    // strings, but our Mojo interface isn't shaped like that.
    if (transport) {
      response->transports.push_back(*transport);
    }
  }

  const base::Value::Dict* client_extension_results =
      dict.FindDict("clientExtensionResults");
  if (!client_extension_results) {
    return InvalidMakeCredentialField("clientExtensionResults");
  }
  std::optional<bool> cred_blob =
      client_extension_results->FindBool("credBlob");
  if (cred_blob) {
    response->echo_cred_blob = true;
    response->cred_blob = *cred_blob;
  }
  const base::Value::Dict* cred_props =
      client_extension_results->FindDict("credProps");
  if (cred_props) {
    response->echo_cred_props = true;
    std::optional<bool> rk = cred_props->FindBool("rk");
    if (rk) {
      response->has_cred_props_rk = true;
      response->cred_props_rk = *rk;
    }
  }
  const std::optional<bool> hmac_create_secret =
      client_extension_results->FindBool("hmacCreateSecret");
  if (hmac_create_secret) {
    response->echo_hmac_create_secret = true;
    response->hmac_create_secret = *hmac_create_secret;
  }
  const base::Value::Dict* large_blob =
      client_extension_results->FindDict("largeBlob");
  if (large_blob) {
    response->echo_large_blob = true;
    const std::optional<bool> supported = large_blob->FindBool("supported");
    if (!supported) {
      return InvalidMakeCredentialField("largeBlob");
    }
    response->supports_large_blob = *supported;
  }
  const base::Value::Dict* prf = client_extension_results->FindDict("prf");
  if (prf) {
    response->echo_prf = true;
    const std::optional<bool> enabled = prf->FindBool("enabled");
    if (!enabled) {
      return InvalidMakeCredentialField("prf");
    }
    response->prf = *enabled;

    const base::Value::Dict* results = prf->FindDict("results");
    if (results) {
      std::optional<blink::mojom::PRFValuesPtr> prf_results =
          ParsePRFResults(results);
      if (!prf_results) {
        return InvalidMakeCredentialField("prf");
      }
      response->prf_results = std::move(*prf_results);
    }
  }
  const base::Value::Dict* supplemental_pub_keys =
      client_extension_results->FindDict("supplementalPubKeys");
  if (supplemental_pub_keys) {
    auto maybe_result = ParseSupplementalPubKeys(supplemental_pub_keys);
    if (!maybe_result) {
      return InvalidMakeCredentialField("supplementalPubKeys");
    }
    response->supplemental_pub_keys = std::move(*maybe_result);
  }

  return {std::move(response), ""};
}

std::pair<blink::mojom::GetAssertionAuthenticatorResponsePtr, std::string>
GetAssertionResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    return {nullptr, "value is not a dict"};
  }

  const base::Value::Dict& dict = value.GetDict();
  const std::string* type = dict.FindString("type");
  if (!type || *type != device::kPublicKey) {
    return InvalidGetAssertionField("type");
  }

  auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
  response->info = blink::mojom::CommonCredentialInfo::New();
  response->extensions =
      blink::mojom::AuthenticationExtensionsClientOutputs::New();

  const std::string* id = dict.FindString("id");
  if (!id) {
    return InvalidGetAssertionField("id");
  }
  response->info->id = *id;
  std::optional<std::string> raw_id = Base64UrlDecodeStringKey(dict, "rawId");
  if (!raw_id) {
    return InvalidGetAssertionField("rawId");
  }
  response->info->raw_id = ToByteVector(*raw_id);

  std::optional<device::AuthenticatorAttachment> authenticator_attachment =
      OptionalAuthenticatorAttachmentFromValue(
          dict.Find("authenticatorAttachment"));
  if (!authenticator_attachment) {
    return InvalidGetAssertionField("authenticatorAttachment");
  }
  response->authenticator_attachment = *authenticator_attachment;

  const base::Value::Dict* assertion_response = dict.FindDict("response");
  if (!assertion_response) {
    return InvalidGetAssertionField("response");
  }

  std::optional<std::string> client_data_json =
      Base64UrlDecodeStringKey(*assertion_response, "clientDataJSON");
  // Providers can return an empty clientDataJson when it knows it will
  // be overridden by the caller.
  if (client_data_json) {
    response->info->client_data_json = ToByteVector(*client_data_json);
  }

  std::optional<std::string> authenticator_data =
      Base64UrlDecodeStringKey(*assertion_response, "authenticatorData");
  if (!authenticator_data) {
    return InvalidGetAssertionField("authenticatorData");
  }
  response->info->authenticator_data = ToByteVector(*authenticator_data);

  std::optional<std::string> signature =
      Base64UrlDecodeStringKey(*assertion_response, "signature");
  if (!signature) {
    return InvalidGetAssertionField("signature");
  }
  response->signature = ToByteVector(*signature);

  auto [ok, opt_user_handle] =
      Base64UrlDecodeOptionalStringKey(*assertion_response, "userHandle");
  if (!ok) {
    return InvalidGetAssertionField("userHandle");
  }
  if (opt_user_handle) {
    response->user_handle = ToByteVector(*opt_user_handle);
  }

  const base::Value::Dict* client_extension_results =
      dict.FindDict("clientExtensionResults");
  if (!client_extension_results) {
    return InvalidGetAssertionField("clientExtensionResults");
  }
  const std::optional<bool> app_id =
      client_extension_results->FindBool("appid");
  if (app_id) {
    response->extensions->echo_appid_extension = true;
    response->extensions->appid_extension = *app_id;
  }
  if (client_extension_results->contains("getCredBlob")) {
    std::optional<std::string> cred_blob =
        Base64UrlDecodeStringKey(*client_extension_results, "getCredBlob");
    if (!cred_blob) {
      return InvalidGetAssertionField("credBlob");
    }
    response->extensions->get_cred_blob = ToByteVector(*cred_blob);
  }
  const base::Value::Dict* large_blob =
      client_extension_results->FindDict("largeBlob");
  if (large_blob) {
    response->extensions->echo_large_blob = true;
    if (large_blob->contains("blob")) {
      std::optional<std::string> blob =
          Base64UrlDecodeStringKey(*large_blob, "blob");
      if (!blob) {
        return InvalidGetAssertionField("largeBlob");
      }
      response->extensions->large_blob = ToByteVector(*blob);
    }
    const std::optional<bool> written = large_blob->FindBool("written");
    if (written) {
      response->extensions->echo_large_blob_written = true;
      response->extensions->large_blob_written = *written;
    }
  }
  const base::Value::Dict* prf = client_extension_results->FindDict("prf");
  if (prf) {
    const base::Value::Dict* results = prf->FindDict("results");
    if (results) {
      std::optional<blink::mojom::PRFValuesPtr> prf_results =
          ParsePRFResults(results);
      if (!prf_results) {
        return InvalidGetAssertionField("prf");
      }

      response->extensions->echo_prf = true;
      response->extensions->prf_results = std::move(*prf_results);
    }
  }
  const base::Value::Dict* supplemental_pub_keys =
      client_extension_results->FindDict("supplementalPubKeys");
  if (supplemental_pub_keys) {
    auto maybe_result = ParseSupplementalPubKeys(supplemental_pub_keys);
    if (!maybe_result) {
      return InvalidGetAssertionField("supplementalPubKeys");
    }
    response->extensions->supplemental_pub_keys = std::move(*maybe_result);
  }

  return {std::move(response), ""};
}

}  // namespace webauthn
