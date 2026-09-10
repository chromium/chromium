// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_HASH_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions_memory.h"

namespace blink {

struct BlinkStorageKeyHashTraits
    : GenericHashTraits<std::unique_ptr<const BlinkStorageKey>> {
  static uint32_t GetHash(const BlinkStorageKey* storage_key) {
    std::optional<base::UnguessableToken> nonce = storage_key->GetNonce();
    size_t nonce_hash = nonce ? base::UnguessableTokenHash()(*nonce) : 0;
    uint32_t hash_codes[] = {
        blink::GetHash(storage_key->GetSecurityOrigin()),
        blink::GetHash(storage_key->GetTopLevelSite()),
        static_cast<uint32_t>(storage_key->GetAncestorChainBit()),
#if ARCH_CPU_32_BITS
        nonce_hash,
#elif ARCH_CPU_64_BITS
        static_cast<uint32_t>(nonce_hash),
        static_cast<uint32_t>(nonce_hash >> 32),
#else
#error "Unknown bits"
#endif
    };
    return HashMemory32(base::as_byte_span(hash_codes));
  }

  static uint32_t GetHash(
      const std::unique_ptr<const BlinkStorageKey>& storage_key) {
    return GetHash(storage_key.get());
  }

  static bool Equal(const BlinkStorageKey* a, const BlinkStorageKey* b) {
    return *a == *b;
  }

  static bool Equal(const std::unique_ptr<const BlinkStorageKey>& a,
                    const BlinkStorageKey* b) {
    return Equal(a.get(), b);
  }

  static bool Equal(const BlinkStorageKey* a,
                    const std::unique_ptr<const BlinkStorageKey>& b) {
    return Equal(a, b.get());
  }

  static bool Equal(const std::unique_ptr<const BlinkStorageKey>& a,
                    const std::unique_ptr<const BlinkStorageKey>& b) {
    return Equal(a.get(), b.get());
  }

  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_HASH_H_
