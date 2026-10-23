// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/types/optional_ref.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "device/fido/attestation_object.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/prf_input.h"

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

// The byte length of the WebauthnCredentialSpecifics `hmac_secret` field.
inline constexpr size_t kHmacSecretSize = 32u;

// The maximum allowed compressed size (in bytes) of a large blob in GPM.
inline constexpr size_t kLargeBlobMaxCompressedSize = 2048u;

// The maximum allowed uncompressed size (in bytes) of a large blob in GPM.
inline constexpr size_t kLargeBlobMaxUncompressedSize = 1024u * 1024u;

// Extension output data for passkey creation and assertion.
struct ExtensionOutputData {
  ExtensionOutputData();
  ExtensionOutputData(const ExtensionOutputData&);
  ~ExtensionOutputData();

  std::vector<uint8_t> prf_result;
};

// Extension input data for passkey creation and assertion.
struct ExtensionInputData {
  ExtensionInputData();
  explicit ExtensionInputData(std::optional<device::PRFInput> prf_input);
  ExtensionInputData(bool has_prf, std::optional<device::PRFInput> prf_input);

  ExtensionInputData(const ExtensionInputData&);
  ~ExtensionInputData();

  // Generates the extensions output data from the input data and encrypted
  // secrets. See:
  // https://w3c.github.io/webauthn/#sctn-defined-client-extensions
  ExtensionOutputData ToOutputData(
      const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted) const;

  // Whether the PRF extension is present in the request.
  bool has_prf = false;
  // Optional PRF evaluation inputs.
  std::optional<device::PRFInput> prf_input;
};

// Evaluates the PRF extension HMAC for `prf_input` using the hmac secret in
// `encrypted` if present, or deriving one from the private key otherwise.
std::vector<uint8_t> EvaluateHMAC(
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted,
    const device::PRFInput& prf_input);

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
// the security domain secret of the `hw_protected` domain.
// `device_authorization_key` must have the most recent version. Returns a
// passkey sync entity with the sealed `encrypted` or
// `security_domain_encrypted` member set, and the unsealed private key. If
// `extension_input_data` has PRF enabled, the hmac_secret will be created as
// part of the sealed `encrypted` member. If PRF is enabled and
// `extension_output_data` is non null, `extension_output_data` will be written
// to.
// TODO(crbug.com/570578262): Pass `device_authorization_key` using
// `crypto::ProcessBound`.
std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
GeneratePasskeyAndEncryptSecrets(
    std::string_view rp_id,
    const PasskeyModel::UserEntity& user_entity,
    base::span<const uint8_t> trusted_vault_key,
    int32_t trusted_vault_key_version,
    base::optional_ref<const DeviceAuthorizationKey> device_authorization_key,
    const ExtensionInputData& extension_input_data,
    ExtensionOutputData* extension_output_data);

// Attempts to decrypt data from the `encrypted_data` field of `in` and
// deserialize it into `out`. The return value indicates whether decryption and
// message parsing succeeded. `trusted_vault_key` must be the security domain
// secret of the `hw_protected` domain. `device_authorization_key` is only used
// for passkeys stored in the `security_domain_encrypted` format, and must be
// the key whose version is `in.device_authorization_key_version()`.
// TODO(crbug.com/570578262): Pass `device_authorization_key` using
// `crypto::ProcessBound`.
bool DecryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    base::span<const uint8_t> device_authorization_key,
    const sync_pb::WebauthnCredentialSpecifics& in,
    sync_pb::WebauthnCredentialSpecifics_Encrypted* out);

// Attempts to encrypt data from a `WebauthnCredentialSpecifics_Encrypted`
// entity and writes it to the `encrypted_data` field of `out`, which must be
// non-null. `trusted_vault_key` must be the security domain secret of the
// `hw_protected` domain. `device_authorization_key` must have the most recent
// version. The return value indicates whether serialization and encryption
// succeeded.
// TODO(crbug.com/570578262): Pass `device_authorization_key` using
// `crypto::ProcessBound`.
bool EncryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    base::optional_ref<const DeviceAuthorizationKey> device_authorization_key,
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& in,
    sync_pb::WebauthnCredentialSpecifics* out);

// Returns the WebAuthn authenticator data for the GPM authenticator.
// Set `did_complete_uv` iff the user has completed user verification (e.g.,
// biometrics or PIN) during this operation.
// For assertion signatures, the AT flag MUST NOT be set and the
// attestedCredentialData MUST NOT be included. See
// https://w3c.github.io/webauthn/#authenticator-data
device::AuthenticatorData MakeAuthenticatorDataForAssertion(
    std::string_view rp_id,
    bool did_complete_uv);

// Returns the WebAuthn attestation object for the GPM authenticator.
// Set `did_complete_uv` iff the user has completed user verification (e.g.,
// biometrics or PIN) during this operation.
// For attestation signatures, the authenticator MUST set the AT flag and
// include the attestedCredentialData. See
// https://w3c.github.io/webauthn/#authenticator-data
device::AttestationObject MakeAttestationObjectForCreation(
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
