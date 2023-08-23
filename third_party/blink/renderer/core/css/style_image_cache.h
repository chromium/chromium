// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_IMAGE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_IMAGE_CACHE_H_

#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_origin_clean.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class FetchParameters;
class StyleFetchedImage;

// A per-StyleEngine cache for StyleImages. A CSSImageValue points to a
// StyleImage, but different CSSImageValue objects with the same URL would not
// have shared the same StyleImage without this cache.
class CORE_EXPORT StyleImageCache {
  DISALLOW_NEW();

 public:
  StyleImageCache() = default;

  // Look up an existing StyleFetchedImage in the cache, or create a new one,
  // add it to the cache, and start the fetch.
  StyleFetchedImage* CacheStyleImage(
      Document&,
      FetchParameters&,
      OriginClean,
      bool is_ad_related,
      const float override_image_resolution = 0.0f);

  void Trace(Visitor*) const;

 private:
  // Map from URL to style image. A weak reference makes sure the entry is
  // removed when no style declarations nor computed styles have a reference to
  // the image.
  HeapHashMap<std::pair<String, float>, WeakMember<StyleFetchedImage>>
      fetched_image_map_;

  friend class StyleImageCacheTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_IMAGE_CACHE_H_
