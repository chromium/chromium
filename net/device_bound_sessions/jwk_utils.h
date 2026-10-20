// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
#define NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"

namespace crypto::keypair {
class PublicKey;
}

namespace unexportable_keys {
class UnexportableKeyService;
}  // namespace unexportable_keys

namespace net::device_bound_sessions {

// Creates a JWK (JSON Web Key, RFC 7517) from `key`. Only supports EC P-256
// and RSA keys, and returns `std::nullopt` for any other key type. The JWK
// depends only on the key, not on a signature algorithm, so callers are
// responsible for ensuring that `key` matches the `alg` they advertise.
NET_EXPORT std::optional<base::DictValue> CreateJwk(
    const crypto::keypair::PublicKey& key);

// Creates a JWK thumbprint (RFC 7638) from `key`. Only supports EC P-256 and
// RSA keys, and returns `std::nullopt` for any other key type.
NET_EXPORT std::optional<std::string> CreateJwkThumbprint(
    const crypto::keypair::PublicKey& key);

// Returns the JWK thumbprint (RFC 7638) of the public key of `key_id`. Returns
// `std::nullopt` if `key_service` does not know `key_id`, if its
// SubjectPublicKeyInfo cannot be parsed, or if `CreateJwkThumbprint()` does not
// support its key type.
NET_EXPORT std::optional<std::string> GetJwkThumbprint(
    const unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableSigningKeyId key_id);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
