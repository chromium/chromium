// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_IMAGE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_IMAGE_CACHE_H_

#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FetchParameters;
class ImageResourceContent;
class ResourceFetcher;

// A per-StyleEngine cache for ImageResourceContent for StyleImages. A
// CSSImageValue points to a StyleImage, but different CSSImageValue objects
// with the same URL may not have shared the same ImageResourceContent without
// this cache.
class CORE_EXPORT StyleImageCache {
  DISALLOW_NEW();

 public:
  StyleImageCache() = default;

  // Look up an existing ImageResourceContent in the cache, or create a new one,
  // add it to the cache, and start the fetch.
  ImageResourceContent* CacheImageContent(ResourceFetcher*, FetchParameters&);

  void Trace(Visitor*) const;

 private:
  struct CachedFetchProperties {
    network::mojom::RequestMode mode;
    network::mojom::CredentialsMode credentials_mode;
    network::mojom::ReferrerPolicy referrer_policy;
  };

  bool CanReuseImageContent(const ImageResourceContent&,
                            const FetchParameters&) const;

  // Map from resolved URL (string) to ImageResourceContent. A weak reference
  // makes sure the entry is removed when no style declarations nor computed
  // styles have a reference to the image.
  HeapHashMap<String, WeakMember<ImageResourceContent>> fetched_image_map_;

  // Fetch properties recorded when each cached ImageResourceContent was
  // fetched, used to prevent cross-mode, cross-credentials, or cross-referrer
  // reuse. Weak keys are collected automatically with the ImageResourceContent.
  HeapHashMap<WeakMember<const ImageResourceContent>, CachedFetchProperties>
      cached_fetch_properties_;

  friend class StyleImageCacheTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_IMAGE_CACHE_H_
