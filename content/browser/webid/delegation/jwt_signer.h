// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_JWT_SIGNER_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_JWT_SIGNER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "crypto/keypair.h"

namespace content::sdjwt {

struct Jwk;
struct Header;

typedef base::OnceCallback<std::optional<std::vector<uint8_t>>(
    const std::string_view&)>
    Signer;
typedef base::OnceCallback<bool(const std::string_view&,
                                base::span<const uint8_t>)>
    Verifier;

CONTENT_EXPORT std::optional<Jwk> ExportPublicKey(
    const crypto::keypair::PrivateKey& private_key);
CONTENT_EXPORT Signer CreateJwtSigner(crypto::keypair::PrivateKey private_key);
CONTENT_EXPORT Verifier CreateJwtVerifier(const Jwk& jwk, const Header& header);

}  // namespace content::sdjwt

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_JWT_SIGNER_H_
