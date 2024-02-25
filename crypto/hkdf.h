// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_HKDF_H_
#define CRYPTO_HKDF_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"

namespace crypto {

CRYPTO_EXPORT
std::string HkdfSha256(std::string_view secret,
                       std::string_view salt,
                       std::string_view info,
                       size_t derived_key_size);

CRYPTO_EXPORT
std::vector<uint8_t> HkdfSha256(base::span<const uint8_t> secret,
                                base::span<const uint8_t> salt,
                                base::span<const uint8_t> info,
                                size_t derived_key_size);

}  // namespace crypto

#endif  // CRYPTO_HKDF_H_
