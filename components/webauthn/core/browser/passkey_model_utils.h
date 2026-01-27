// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_

#include <vector>

#include "base/containers/span.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "device/fido/prf_input.h"

namespace cbor {
class Value;
}  // namespace cbor

namespace sync_pb {
class WebauthnCredentialSpecifics;
class WebauthnCredentialSpecifics_Encrypted;
}  // namespace sync_pb

namespace webauthn::passkey_model_utils {

// The byte length of the WebauthnCredentialSpecifics `sync_id` field.
inline constexpr size_t kSyncIdLength = 16u;

// The maximum byte length of the WebauthnCredentialSpecifics `user_id` field.
inline constexpr size_t kUserIdMaxLength = 64u;

// Lower bound for credential ID length
// (https://www.w3.org/TR/webauthn-2/#credential-id).
inline constexpr size_t kCredentialIdMinLength = 16u;

// Upper bound for credential ID length
// (https://www.w3.org/TR/webauthn-3/#credential-id).
inline constexpr size_t kCredentialIdMaxLength = 1023u;

// The byte length of the WebauthnCredentialSpecifics `credential_id` field for
// passkeys created by GPM.
inline constexpr size_t kGpmCreatedCredentialIdLength = 16u;

// Extension output data for passkey creation and assertion.
struct ExtensionOutputData {
  ExtensionOutputData();
  ExtensionOutputData(const ExtensionOutputData&);
  ~ExtensionOutputData();

  std::vector<uint8_t> prf_result;
};

// PRF extension input data for passkey creation and assertion.
struct PRFInputData {
  PRFInputData(base::span<const uint8_t> prf_input1,
               std::optional<base::span<const uint8_t>> prf_input2);

  PRFInputData(const PRFInputData&);
  PRFInputData(PRFInputData&&);
  PRFInputData& operator=(PRFInputData&& other);
  ~PRFInputData();

  inline const device::PRFInput& prf_input() const { return input; }

 private:
  device::PRFInput input;
};

// Extension input data for passkey creation and assertion.
struct ExtensionInputData {
  // This constructor must be used if there is an extension present in the
  // passkey request. Even if there's no PRF data, this constructor will
  // initialize `prf_input` so that `hasPRF` can later return true, so that PRF
  // support can be returned as part of the creation or assertion response.
  explicit ExtensionInputData(PRFInputData prf_input_data);

  // This constructor must be used when there are no extensions present in the
  // passkey request.
  ExtensionInputData();

  ExtensionInputData(const ExtensionInputData&);
  ~ExtensionInputData();

  // Returns whether the extension data contains the PRF extension.
  bool hasPRF() const;

  // Generates the extensions output data from the input data and encrypted
  // secrets. See:
  // https://w3c.github.io/webauthn/#sctn-defined-client-extensions
  ExtensionOutputData ToOutputData(
      const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted) const;

 private:
  // Evaluates HMAC to produce the PRF result, using the hmac secret if it
  // exists, or derives one from the private key otherwise.
  std::vector<uint8_t> EvaluateHMAC(
      const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted) const;

  std::optional<PRFInputData> prf_input_data;
};

// Serialized versions of the attestation object and authenticator data.
struct SerializedAttestationObject {
  SerializedAttestationObject();
  SerializedAttestationObject(SerializedAttestationObject&& other);
  ~SerializedAttestationObject();

  std::vector<uint8_t> attestation_object;
  std::vector<uint8_t> authenticator_data;
};

// Returns a list containing members from `passkeys` that are not shadowed.
// A credential is shadowed if another credential contains it in its
// `newly_shadowed_credential_ids` member, or if another credential for the same
// {User ID, RP ID} pair is newer.
// It is safe (and recommended) to filter credentials by RP ID before calling
// this function, if applicable for the use case.
std::vector<sync_pb::WebauthnCredentialSpecifics> FilterShadowedCredentials(
    base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys);

// Returns whether the passkey is of the expected format. The conditions checked
// in this function should apply to every passkey stored in Google Password
// Manager, regardless of whether they were actually created by GPM or imported
// through Credential Exchange. For more specific functions based on the source
// of passkeys, use one of the following:
// * `passkey_model_utils::IsGpmPasskeyValid()`
// * `webauthn::CheckImportedPasskey()`
bool IsPasskeyValid(const sync_pb::WebauthnCredentialSpecifics& passkey);

// Returns whether the passkey created by the Google Password Manager is of the
// expected format. This function might make some stricter assumptions than what
// might be allowed in the WebAuthn spec (e.g. GPM uses a specific length when
// generating credential IDs). This function should not be used for passkeys
// created elsewhere (e.g. by a different credential manager, imported through
// Credential Exchange Protocol).
bool IsGpmPasskeyValid(const sync_pb::WebauthnCredentialSpecifics& passkey);

// Generates a passkey for the given RP ID and user. `trusted_vault_key` must be
// the security domain secret of the `hw_protected` domain. Returns a passkey
// sync entity with the sealed `encrypted` member set, and the unsealed private
// key. If `extension_input_data` has PRF enabled, the hmac_secret will be
// created as part of the sealed `encrypted` member. If PRF is enabled and
// `extension_output_data` is non null, `extension_output_data` will be written
// to.
std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
GeneratePasskeyAndEncryptSecrets(std::string_view rp_id,
                                 const PasskeyModel::UserEntity& user_entity,
                                 base::span<const uint8_t> trusted_vault_key,
                                 int32_t trusted_vault_key_version,
                                 const ExtensionInputData& extension_input_data,
                                 ExtensionOutputData* extension_output_data);

// Attempts to decrypt data from the `encrypted_data` field of `in` and
// deserialize it into `out`. The return value indicates whether decryption and
// message parsing succeeded. `trusted_vault_key` must be the security domain
// secret of the `hw_protected` domain.
bool DecryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    const sync_pb::WebauthnCredentialSpecifics& in,
    sync_pb::WebauthnCredentialSpecifics_Encrypted* out);

// Attempts to encrypt data from a `WebauthnCredentialSpecifics_Encrypted`
// entity and writes it to the `encrypted_data` field of `out`, which must be
// non-null. `trusted_vault_key` must be the security domain secret of the
// `hw_protected` domain. The return value indicates whether serialization and
// encryption succeeded.
bool EncryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& in,
    sync_pb::WebauthnCredentialSpecifics* out);

// Returns the WebAuthn authenticator data for the GPM authenticator.
// Set `did_complete_uv` iff the user has completed user verification (e.g.,
// biometrics or PIN) during this operation.
// For assertion signatures, the AT flag MUST NOT be set and the
// attestedCredentialData MUST NOT be included. See
// https://w3c.github.io/webauthn/#authenticator-data
std::vector<uint8_t> MakeAuthenticatorDataForAssertion(std::string_view rp_id,
                                                       bool did_complete_uv);

// Returns the WebAuthn attestation object for the GPM authenticator and
// the serialized authenticator data used to create the attestation object.
// Set `did_complete_uv` iff the user has completed user verification (e.g.,
// biometrics or PIN) during this operation.
// For attestation signatures, the authenticator MUST set the AT flag and
// include the attestedCredentialData. See
// https://w3c.github.io/webauthn/#authenticator-data
SerializedAttestationObject MakeAttestationObjectForCreation(
    std::string_view rp_id,
    bool did_complete_uv,
    base::span<const uint8_t> credential_id,
    base::span<const uint8_t> public_key_spki_der);

// Performs the signing operation over the signed over data using the private
// key. The signed over data is the concatenation to the authenticator data and
// the client data hash. See:
// https://w3c.github.io/webauthn/#fig-signature
std::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data);

// Returns whether the provided algorithm is supported.
bool IsSupportedAlgorithm(int32_t algorithm);

}  // namespace webauthn::passkey_model_utils

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
