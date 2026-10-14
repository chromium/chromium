// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
#define NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "net/base/net_export.h"

namespace crypto::keypair {
class PublicKey;
}

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

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
