// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_ENTRY_HASHER_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_ENTRY_HASHER_H_

#include <stdint.h>

#include "crypto/process_bound_string.h"
#include "net/disk_cache/cache_entry_hasher.h"

namespace network::enterprise_encryption {

// A CacheEntryHasher that uses a primary key to create salted hashes of cache
// keys. This prevents attackers from knowing the urls comparing against known
// url hash tables, as well as from creating hash collisions without knowing the
// primary key.
class EncryptedCacheEntryHasher : public disk_cache::CacheEntryHasher {
 public:
  explicit EncryptedCacheEntryHasher(crypto::ProcessBoundString primary_key);

  EncryptedCacheEntryHasher(const EncryptedCacheEntryHasher&) = delete;
  EncryptedCacheEntryHasher& operator=(const EncryptedCacheEntryHasher&) =
      delete;
  ~EncryptedCacheEntryHasher() override;

  // disk_cache::CacheEntryHasher:
  uint64_t GetEntryHashKey(const std::string& key) const override;

 private:
  const crypto::ProcessBoundString primary_key_;
};

}  // namespace network::enterprise_encryption

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_ENTRY_HASHER_H_
