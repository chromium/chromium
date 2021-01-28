// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderHostRegistry::PrerenderHostRegistry(BrowserContext& browser_context)
    : browser_context_(browser_context) {
  DCHECK(blink::features::IsPrerender2Enabled());
}

PrerenderHostRegistry::~PrerenderHostRegistry() = default;

int PrerenderHostRegistry::CreateAndStartHost(
    blink::mojom::PrerenderAttributesPtr attributes,
    const url::Origin& initiator_origin) {
  DCHECK(attributes);

  // Ignore prerendering requests for the same URL.
  const GURL prerendering_url = attributes->url;
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::CreateAndStartHost",
               "Prerender Attributes",
               base::trace_event::ToTracedValue(*attributes),
               "initiator_origin", initiator_origin.GetURL().spec());

  auto found = frame_tree_node_id_by_url_.find(prerendering_url);
  if (found != frame_tree_node_id_by_url_.end())
    return found->second;

  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes), initiator_origin, browser_context_);
  const int frame_tree_node_id = prerender_host->frame_tree_node_id();

  // Start prerendering before adding the host to `frame_tree_node_id_by_url_`
  // to make sure navigation for prerendering doesn't select itself.
  // TODO(https://crbug.com/1132746): FindHostToActivate() should avoid
  // selecting a prerender host when the current NavigationRequest is for
  // prerendering regardless of the calling order of StartPrerendering(). At
  // this point, RenderFrameHostImpl doesn't know its prerendering state until
  // it receives NavigationRequest, so it cannot guarantee to provide
  // PrerenderHostRegistry with a stable prerendering state. This issue will be
  // fixed after landing the new approach of depending on FrameTree's
  // prerendering state.
  CHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                        frame_tree_node_id));
  prerender_host_by_frame_tree_node_id_[frame_tree_node_id] =
      std::move(prerender_host);
  prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
      ->StartPrerendering();

  // Make sure StartPrerendering() doesn't call AbandonHost().
  DCHECK(base::Contains(prerender_host_by_frame_tree_node_id_,
                        frame_tree_node_id));

  frame_tree_node_id_by_url_[prerendering_url] = frame_tree_node_id;
  return frame_tree_node_id;
}

void PrerenderHostRegistry::AbandonHost(int frame_tree_node_id) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::AbandonHost",
               "frame_tree_node_id", frame_tree_node_id);
  auto found = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (found != prerender_host_by_frame_tree_node_id_.end()) {
    auto initial_url = found->second->GetInitialUrl();
    frame_tree_node_id_by_url_.erase(initial_url);
    prerender_host_by_frame_tree_node_id_.erase(found);
  }
}

std::unique_ptr<PrerenderHost> PrerenderHostRegistry::FindHostToActivate(
    const GURL& navigation_url,
    FrameTreeNode& frame_tree_node) {
  RenderFrameHostImpl* render_frame_host = frame_tree_node.current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::FindHostToActivate",
               "navigation_url", navigation_url.spec(), "render_frame_host",
               base::trace_event::ToTracedValue(render_frame_host));

  // Disallow activation when the navigation is for prerendering.
  if (render_frame_host->IsPrerendering())
    return nullptr;

  // Disallow activation when the render frame host is for a nested browsing
  // context (e.g., iframes). This is because nested browsing contexts are
  // supposed to be created in the parent's browsing context group and can
  // script with the parent, but prerendered pages are created in new browsing
  // context groups.
  if (render_frame_host->GetParent())
    return nullptr;

  // Disallow activation when other auxiliary browsing contexts (e.g., pop-up
  // windows) exist in the same browsing context group. This is because these
  // browsing contexts should be able to script each other, but prerendered
  // pages are created in new browsing context groups.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (site_instance->GetRelatedActiveContentsCount() != 1u)
    return nullptr;

  auto id_iter = frame_tree_node_id_by_url_.find(navigation_url);
  if (id_iter == frame_tree_node_id_by_url_.end())
    return nullptr;
  const int prerender_frame_tree_node_id = id_iter->second;
  frame_tree_node_id_by_url_.erase(id_iter);

  auto host_iter =
      prerender_host_by_frame_tree_node_id_.find(prerender_frame_tree_node_id);
  DCHECK(host_iter != prerender_host_by_frame_tree_node_id_.end());
  std::unique_ptr<PrerenderHost> host = std::move(host_iter->second);
  prerender_host_by_frame_tree_node_id_.erase(host_iter);

  // If the host is not ready for activation yet, destroys it and returns
  // nullptr. This is because it is likely that the prerendered page is never
  // used from now on.
  if (!host->is_ready_for_activation())
    return nullptr;

  return host;
}

PrerenderHost* PrerenderHostRegistry::FindHostByUrlForTesting(
    const GURL& prerendering_url) {
  auto id_iter = frame_tree_node_id_by_url_.find(prerendering_url);
  if (id_iter == frame_tree_node_id_by_url_.end())
    return nullptr;
  const int prerender_frame_tree_node_id = id_iter->second;
  auto host_iter =
      prerender_host_by_frame_tree_node_id_.find(prerender_frame_tree_node_id);
  DCHECK(host_iter != prerender_host_by_frame_tree_node_id_.end());
  return host_iter->second.get();
}

}  // namespace content
