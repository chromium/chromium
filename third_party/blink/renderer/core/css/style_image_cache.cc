// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

StyleFetchedImage* StyleImageCache::CacheStyleImage(
    Document& document,
    FetchParameters& params,
    OriginClean origin_clean,
    bool is_ad_related,
    const float override_image_resolution) {
  CHECK(!params.Url().IsNull());
  // TODO: Investigate key/val change to
  // "URL (sans fragment) -> ImageResourceContent"
  // see https://crbug.com/1417158

  std::pair<String, float> key{params.Url().GetString(),
                               override_image_resolution};

  auto result = fetched_image_map_.insert(key, nullptr);

  if (result.is_new_entry || !result.stored_value->value ||
      result.stored_value->value->ErrorOccurred()) {
    result.stored_value->value = MakeGarbageCollected<StyleFetchedImage>(
        ImageResourceContent::Fetch(params, document.Fetcher()), document,
        params.GetImageRequestBehavior() ==
            FetchParameters::ImageRequestBehavior::kDeferImageLoad,
        origin_clean == OriginClean::kTrue, is_ad_related, params.Url(),
        override_image_resolution);
  }

  return result.stored_value->value;
}

void StyleImageCache::Trace(Visitor* visitor) const {
  visitor->Trace(fetched_image_map_);
}

}  // namespace blink
