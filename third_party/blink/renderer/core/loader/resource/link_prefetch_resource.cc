// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/link_prefetch_resource.h"

#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

Resource* LinkPrefetchResource::Fetch(FetchParameters& params,
                                      ResourceFetcher* fetcher) {
  return fetcher->RequestResource(params, Factory(), nullptr);
}

LinkPrefetchResource::LinkPrefetchResource(const ResourceRequest& request,
                                           const ResourceLoaderOptions& options)
    : Resource(request, ResourceType::kLinkPrefetch, options) {}

LinkPrefetchResource::~LinkPrefetchResource() = default;

LinkPrefetchResource::Factory::Factory()
    : NonTextResourceFactory(ResourceType::kLinkPrefetch) {}

Resource* LinkPrefetchResource::Factory::Create(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options) const {
  return MakeGarbageCollected<LinkPrefetchResource>(request, options);
}

}  // namespace blink
