// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

namespace {

bool CanReuseImageContent(const ImageResourceContent& image_content) {
  if (image_content.ErrorOccurred()) {
    return false;
  }
  return true;
}

}  // namespace

ImageResourceContent* StyleImageCache::CacheImageContent(
    ResourceFetcher* fetcher,
    FetchParameters& params) {
  CHECK(!params.Url().IsNull());

  const KURL url_without_fragment =
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  auto& image_content =
      fetched_image_map_.insert(url_without_fragment.GetString(), nullptr)
          .stored_value->value;
  if (!image_content || !CanReuseImageContent(*image_content)) {
    image_content = ImageResourceContent::Fetch(params, fetcher);
  }
  return image_content.Get();
}

void StyleImageCache::Trace(Visitor* visitor) const {
  visitor->Trace(fetched_image_map_);
}

}  // namespace blink
