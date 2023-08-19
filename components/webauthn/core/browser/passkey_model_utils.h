// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_

#include <vector>

#include "base/containers/span.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace webauthn::passkey_model_utils {

// Returns a list containing members from |passkeys| that are not shadowed.
// A credential is shadowed if another credential contains it in its
// `newly_shadowed_credential_ids` member, or if another credential for the same
// {User ID, RP ID} pair is newer.
// It is safe (and recommended) to filter credentials by RP ID before calling
// this function, if applicable for the use case.
std::vector<sync_pb::WebauthnCredentialSpecifics> FilterShadowedCredentials(
    base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys);

}  // namespace webauthn::passkey_model_utils

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_UTILS_H_
