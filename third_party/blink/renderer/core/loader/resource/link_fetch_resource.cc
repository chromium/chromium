// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/link_fetch_resource.h"

#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

Resource* LinkFetchResource::Fetch(ResourceType type,
                                   FetchParameters& params,
                                   ResourceFetcher* fetcher) {
  DCHECK_EQ(type, ResourceType::kLinkPrefetch);
  return fetcher->RequestResource(params, LinkResourceFactory(type), nullptr);
}

LinkFetchResource::LinkFetchResource(const ResourceRequest& request,
                                     ResourceType type,
                                     const ResourceLoaderOptions& options)
    : Resource(request, type, options) {}

LinkFetchResource::~LinkFetchResource() = default;

}  // namespace blink
