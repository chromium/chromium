// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "content/browser/prerender/prerender_host.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderHostRegistry::PrerenderHostRegistry() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2));
}

PrerenderHostRegistry::~PrerenderHostRegistry() = default;

void PrerenderHostRegistry::CreateAndStartHost(
    blink::mojom::PrerenderAttributesPtr attributes,
    const GlobalFrameRoutingId& initiator_render_frame_host_id,
    const url::Origin& initiator_origin) {
  DCHECK(attributes);

  // Ignore prerendering requests for the same URL.
  const GURL prerendering_url = attributes->url;
  if (base::Contains(prerender_host_by_url_, prerendering_url))
    return;

  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes), initiator_render_frame_host_id, initiator_origin);

  // Start prerendering before adding the host to the map to make sure
  // navigation for prerendering doesn't select itself.
  // TODO(https://crbug.com/1132746): SelectForNavigation() should avoid select
  // a prerender host when the current NavigationRequest is for prerendering
  // regardless of the calling order of StartPrerendering(). This modification
  // would require the proper `is_prerendering` state in NavigationRequest,
  // RenderFrameHostImpl, or somewhere else.
  prerender_host->StartPrerendering();

  prerender_host_by_url_[prerendering_url] = std::move(prerender_host);
}

void PrerenderHostRegistry::AbandonHost(const GURL& prerendering_url) {
  prerender_host_by_url_.erase(prerendering_url);
}

std::unique_ptr<PrerenderHost> PrerenderHostRegistry::SelectForNavigation(
    const GURL& url) {
  auto found = prerender_host_by_url_.find(url);
  if (found == prerender_host_by_url_.end())
    return nullptr;

  std::unique_ptr<PrerenderHost> host = std::move(found->second);
  prerender_host_by_url_.erase(found);

  // If the host is not ready for activation yet, destroys it and returns
  // nullptr. This is because it is likely that the prerendered page is never
  // used from now on.
  if (!host->is_ready_for_activation())
    return nullptr;

  return host;
}

PrerenderHost* PrerenderHostRegistry::FindHostByUrlForTesting(
    const GURL& prerendering_url) {
  auto found = prerender_host_by_url_.find(prerendering_url);
  if (found == prerender_host_by_url_.end())
    return nullptr;
  return found->second.get();
}

}  // namespace content
