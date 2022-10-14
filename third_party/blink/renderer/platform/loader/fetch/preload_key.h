// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PRELOAD_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PRELOAD_KEY_H_

#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

// PreloadKey is a key type of the preloads map in a fetch group (a.k.a.
// blink::ResourceFetcher).
struct PreloadKey final {
 public:
  struct Hash {
    STATIC_ONLY(Hash);

   public:
    static unsigned GetHash(const PreloadKey& key) {
      return KURLHash::GetHash(key.url);
    }
    static bool Equal(const PreloadKey& x, const PreloadKey& y) {
      return x == y;
    }
    static constexpr bool safe_to_compare_to_empty_or_deleted = false;
  };

  PreloadKey() = default;
  PreloadKey(const KURL& url, ResourceType type)
      : url(RemoveFragmentFromUrl(url)), type(type) {}

  bool operator==(const PreloadKey& x) const {
    return url == x.url && type == x.type;
  }

  static KURL RemoveFragmentFromUrl(const KURL& src) {
    if (!src.HasFragmentIdentifier())
      return src;
    KURL url = src;
    url.RemoveFragmentIdentifier();
    return url;
  }

  KURL url;
  ResourceType type = ResourceType::kImage;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::PreloadKey> : blink::PreloadKey::Hash {};

template <>
struct HashTraits<blink::PreloadKey>
    : public SimpleClassHashTraits<blink::PreloadKey> {
  static const bool kEmptyValueIsZero = false;

  static bool IsDeletedValue(const blink::PreloadKey& value) {
    return HashTraits<blink::KURL>::IsDeletedValue(value.url);
  }

  static void ConstructDeletedValue(blink::PreloadKey& slot, bool zero_value) {
    HashTraits<blink::KURL>::ConstructDeletedValue(slot.url, zero_value);
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PRELOAD_KEY_H_
