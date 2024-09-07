// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
#define NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_

#include "base/containers/span.h"
#include "base/values.h"
#include "crypto/signature_verifier.h"
#include "net/base/net_export.h"

namespace net::device_bound_sessions {
// Converts a public key in SPKI format to a JWK (JSON Web Key). Only supports
// ES256 and RS256 keys.
base::Value::Dict NET_EXPORT
ConvertPkeySpkiToJwk(crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                     base::span<const uint8_t> pkey_spki);
}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_JWK_UTILS_H_
