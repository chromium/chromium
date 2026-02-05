// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_cache_entry_hasher.h"

#include <array>
#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "crypto/hmac.h"
#include "crypto/process_bound_string.h"

namespace network::enterprise_encryption {

EncryptedCacheEntryHasher::EncryptedCacheEntryHasher(
    crypto::ProcessBoundString primary_key)
    : primary_key_(std::move(primary_key)) {}

EncryptedCacheEntryHasher::~EncryptedCacheEntryHasher() = default;

uint64_t EncryptedCacheEntryHasher::GetEntryHashKey(
    const std::string& key) const {
  const auto digest = crypto::hmac::SignSha256(
      base::as_byte_span(primary_key_.secure_value()), base::as_byte_span(key));
  auto hash = base::U64FromLittleEndian(base::span(digest).first<8u>());
  return hash;
}

}  // namespace network::enterprise_encryption
