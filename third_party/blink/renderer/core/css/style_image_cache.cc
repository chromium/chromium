// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

namespace {

bool ShouldBypassCache(const FetchParameters& params) {
  if (!params.GetIntegrityMetadata().empty()) {
    return true;
  }
  if (params.IsFromOriginDirtyStyleSheet()) {
    return true;
  }
  const ResourceLoaderOptions& options = params.Options();
  if ((options.WorldForCsp() && options.WorldForCsp()->IsIsolatedWorld()) ||
      (options.TargetWorld() && options.TargetWorld()->IsIsolatedWorld())) {
    return true;
  }
  return false;
}

}  // namespace

bool StyleImageCache::CanReuseImageContent(
    const ImageResourceContent& image_content,
    const FetchParameters& params) const {
  if (image_content.ErrorOccurred() || ShouldBypassCache(params)) {
    return false;
  }
  if (params.GetCrossOriginAttributeValue() != kCrossOriginAttributeNotSet &&
      !image_content.IsCorsSameOrigin()) {
    return false;
  }
  auto it = cached_fetch_properties_.find(&image_content);
  if (it == cached_fetch_properties_.end()) {
    return false;
  }
  const ResourceRequest& request = params.GetResourceRequest();
  const CachedFetchProperties& cached = it->value;
  if (cached.mode != request.GetMode() ||
      cached.credentials_mode != request.GetCredentialsMode() ||
      cached.referrer_policy != request.GetReferrerPolicy()) {
    return false;
  }
  return true;
}

ImageResourceContent* StyleImageCache::CacheImageContent(
    ResourceFetcher* fetcher,
    FetchParameters& params) {
  CHECK(!params.Url().IsNull());

  // Bypass StyleImageCache for requests with SRI metadata, isolated-world
  // provenance, or origin-dirty stylesheet provenance so ResourceFetcher
  // handles validation and partitioning without clobbering cached entries.
  if (ShouldBypassCache(params)) {
    return ImageResourceContent::Fetch(params, fetcher);
  }

  const KURL url_without_fragment =
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  auto& image_content =
      fetched_image_map_.insert(url_without_fragment.GetString(), nullptr)
          .stored_value->value;
  if (image_content && !image_content->ErrorOccurred()) {
    if (CanReuseImageContent(*image_content, params)) {
      return image_content.Get();
    }
    // Defer mismatched requests (e.g. different CORS/credentials mode or
    // referrer policy) to ResourceFetcher without clobbering the valid entry.
    return ImageResourceContent::Fetch(params, fetcher);
  }

  const ResourceRequest& request = params.GetResourceRequest();
  CachedFetchProperties properties{
      request.GetMode(),
      request.GetCredentialsMode(),
      request.GetReferrerPolicy(),
  };
  image_content = ImageResourceContent::Fetch(params, fetcher);
  if (image_content) {
    cached_fetch_properties_.Set(image_content.Get(), properties);
  }
  return image_content.Get();
}

void StyleImageCache::Trace(Visitor* visitor) const {
  visitor->Trace(fetched_image_map_);
  visitor->Trace(cached_fetch_properties_);
}

}  // namespace blink
