// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_

#include <vector>

#include "base/containers/span.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
class WebauthnCredentialSpecifics_Encrypted;
}  // namespace sync_pb

namespace webauthn::passkey_model_utils {

// Returns a list containing members from `passkeys` that are not shadowed.
// A credential is shadowed if another credential contains it in its
// `newly_shadowed_credential_ids` member, or if another credential for the same
// {User ID, RP ID} pair is newer.
// It is safe (and recommended) to filter credentials by RP ID before calling
// this function, if applicable for the use case.
std::vector<sync_pb::WebauthnCredentialSpecifics> FilterShadowedCredentials(
    base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys);

std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
GeneratePasskeyAndEncryptSecrets(std::string_view rp_id,
                                 const PasskeyModel::UserEntity& user_entity,
                                 base::span<const uint8_t> trusted_vault_key,
                                 int32_t trusted_vault_key_version);

// Attempts to decrypt data from the `encrypted_data` field of `in` and
// deserialize it into `out`. The return value indicates whether decryption and
// message parsing succeeded.
bool DecryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> key,
    const sync_pb::WebauthnCredentialSpecifics& in,
    sync_pb::WebauthnCredentialSpecifics_Encrypted* out);

// Attempts to encrypt data from a `WebauthnCredentialSpecifics_Entity` entity
// and writes it to the `encrypted_data` field of `out`, which must be non-null.
// The return value indicates whether serialization and encryption succeeded.
bool EncryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> key,
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& in,
    sync_pb::WebauthnCredentialSpecifics* out);

}  // namespace webauthn::passkey_model_utils

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
