// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

StyleFetchedImage* StyleImageCache::CacheStyleImage(Document& document,
                                                    FetchParameters& params,
                                                    OriginClean origin_clean,
                                                    bool is_ad_related) {
  auto result = fetched_image_map_.insert(params.Url().GetString(), nullptr);
  if (result.is_new_entry || !result.stored_value->value) {
    result.stored_value->value = MakeGarbageCollected<StyleFetchedImage>(
        ImageResourceContent::Fetch(params, document.Fetcher()), document,
        params.GetImageRequestBehavior() ==
            FetchParameters::ImageRequestBehavior::kDeferImageLoad,
        origin_clean == OriginClean::kTrue, is_ad_related, params.Url());
  }
  return result.stored_value->value;
}

void StyleImageCache::Trace(Visitor* visitor) const {
  visitor->Trace(fetched_image_map_);
}

}  // namespace blink
