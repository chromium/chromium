// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_JWT_SIGNER_H_
#define CONTENT_BROWSER_WEBID_JWT_SIGNER_H_

#include <optional>
#include <vector>
#include <string_view>
#include <memory>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace crypto {
class ECPrivateKey;
}

namespace content::sdjwt {

struct Jwk;
typedef base::OnceCallback<std::optional<std::vector<uint8_t>>(
    const std::string_view&)>
    Signer;

CONTENT_EXPORT std::optional<Jwk> ExportPublicKey(
    const crypto::ECPrivateKey& private_key);
CONTENT_EXPORT Signer
CreateJwtSigner(std::unique_ptr<crypto::ECPrivateKey> private_key);

}  // namespace content::sdjwt

#endif  // CONTENT_BROWSER_WEBID_JWT_SIGNER_H_
