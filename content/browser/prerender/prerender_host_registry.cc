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

void PrerenderHostRegistry::RegisterHost(
    const GURL& prerendering_url,
    std::unique_ptr<PrerenderHost> prerender_host) {
  // Ignore prerendering requests for the same URL.
  if (base::Contains(prerender_host_by_url_, prerendering_url)) {
    // TODO(https://crbug.com/1132746): In addition to this check, we may have
    // to avoid duplicate requests in the PrerenderHost layer so that the
    // prerender host doesn't start navigation for the duplicate requests.
    return;
  }
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
