// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
#define NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/values.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/sign.h"
#include "net/base/net_export.h"

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

// Converts a public key in SPKI format to a JWK (JSON Web Key). Only supports
// ES256 and RS256 keys.
NET_EXPORT base::DictValue ConvertPkeySpkiToJwk(
    crypto::sign::SignatureKind algorithm,
    base::span<const uint8_t> pkey_spki);

// Creates a JWK thumbprint as defined in RFC 7638. Returns an empty
// string for failure to create a JWK from `pkey_spki`.
NET_EXPORT std::string CreateJwkThumbprint(
    crypto::sign::SignatureKind algorithm,
    base::span<const uint8_t> pkey_spki);

// Returns the RFC 7638 JWK thumbprint of the public key of `key_id`, or
// `std::nullopt` if it can't be computed.
NET_EXPORT std::optional<std::string> GetJwkThumbprint(
    const unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableSigningKeyId key_id);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
