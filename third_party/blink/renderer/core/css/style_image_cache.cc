// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include <sstream>

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
  // Special-case for null request KURL's, because they cannot be hashed.
  if (params.Url().IsNull()) {
    if (!null_url_image_) {
      null_url_image_ = CreateImage(document, params, origin_clean,
                                    is_ad_related, override_image_resolution);
    }
    return null_url_image_;
  }
  // TODO: Investigate key/val change to
  // "URL (sans fragment) -> ImageResourceContent"
  // see https://crbug.com/1417158

  std::pair<String, float> key{params.Url().GetString(),
                               override_image_resolution};

  auto result = fetched_image_map_.insert(key, nullptr);

  if (result.is_new_entry || !result.stored_value->value) {
    result.stored_value->value =
        CreateImage(document, params, origin_clean, is_ad_related,
                    override_image_resolution);
  }

  return result.stored_value->value;
}

StyleFetchedImage* StyleImageCache::CreateImage(
    Document& document,
    FetchParameters& params,
    OriginClean origin_clean,
    bool is_ad_related,
    const float override_image_resolution) {
  return MakeGarbageCollected<StyleFetchedImage>(
      ImageResourceContent::Fetch(params, document.Fetcher()), document,
      params.GetImageRequestBehavior() ==
          FetchParameters::ImageRequestBehavior::kDeferImageLoad,
      origin_clean == OriginClean::kTrue, is_ad_related, params.Url(),
      override_image_resolution);
}

void StyleImageCache::Trace(Visitor* visitor) const {
  visitor->Trace(fetched_image_map_);
  visitor->Trace(null_url_image_);
}

}  // namespace blink
