// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_HASH_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"

namespace blink {

// TODO(https://crbug.com/1199077): This needs to be re-implemented for the
// actual StorageKey content once it's stable. Right now it's just (almost) a
// shim for `SecurityOriginHash`.
struct BlinkStorageKeyHash {
  STATIC_ONLY(BlinkStorageKeyHash);

  static unsigned GetHash(const BlinkStorageKey* storage_key) {
    return SecurityOriginHash::GetHash(storage_key->GetSecurityOrigin());
  }

  static unsigned GetHash(
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

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_HASH_H_
