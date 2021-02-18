// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderHostRegistry::PrerenderHostRegistry() {
  DCHECK(blink::features::IsPrerender2Enabled());
}

PrerenderHostRegistry::~PrerenderHostRegistry() {
  for (Observer& obs : observers_)
    obs.OnRegistryDestroyed();
}

void PrerenderHostRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrerenderHostRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int PrerenderHostRegistry::CreateAndStartHost(
    blink::mojom::PrerenderAttributesPtr attributes,
    WebContentsImpl& web_contents,
    const url::Origin& initiator_origin) {
  DCHECK(attributes);

  // Ensure observers are notified that a trigger occurred.
  base::ScopedClosureRunner notify_trigger(
      base::BindOnce(&PrerenderHostRegistry::NotifyTrigger,
                     base::Unretained(this), attributes->url));

  // Don't prerender on low-end devices.
  // TODO(https://crbug.com/1176120): Fallback to NoStatePrefetch
  // if the memory requirements are different.
  if (base::SysInfo::IsLowEndDevice()) {
    base::UmaHistogramEnumeration(
        "Prerender.Experimental.PrerenderHostFinalStatus",
        PrerenderHost::FinalStatus::kLowEndDevice);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // Ignore prerendering requests for the same URL.
  const GURL prerendering_url = attributes->url;
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::CreateAndStartHost",
               "attributes", attributes, "initiator_origin",
               initiator_origin.GetURL().spec());

  auto found = frame_tree_node_id_by_url_.find(prerendering_url);
  if (found != frame_tree_node_id_by_url_.end())
    return found->second;

  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes), initiator_origin, web_contents);
  const int frame_tree_node_id = prerender_host->frame_tree_node_id();

  // Start prerendering before adding the host to `frame_tree_node_id_by_url_`
  // to make sure navigation for prerendering doesn't select itself.
  // TODO(https://crbug.com/1132746): ReserveHostToActivate() should avoid
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

int PrerenderHostRegistry::ReserveHostToActivate(
    const GURL& navigation_url,
    FrameTreeNode& frame_tree_node) {
  RenderFrameHostImpl* render_frame_host = frame_tree_node.current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::ReserveHostToActivate",
               "navigation_url", navigation_url.spec(), "render_frame_host",
               base::trace_event::ToTracedValue(render_frame_host));

  // Disallow activation when the navigation is for prerendering.
  if (render_frame_host->IsPrerendering())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when the render frame host is for a nested browsing
  // context (e.g., iframes). This is because nested browsing contexts are
  // supposed to be created in the parent's browsing context group and can
  // script with the parent, but prerendered pages are created in new browsing
  // context groups.
  if (render_frame_host->GetParent())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when other auxiliary browsing contexts (e.g., pop-up
  // windows) exist in the same browsing context group. This is because these
  // browsing contexts should be able to script each other, but prerendered
  // pages are created in new browsing context groups.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (site_instance->GetRelatedActiveContentsCount() != 1u)
    return RenderFrameHost::kNoFrameTreeNodeId;

  auto id_iter = frame_tree_node_id_by_url_.find(navigation_url);
  if (id_iter == frame_tree_node_id_by_url_.end())
    return RenderFrameHostImpl::kNoFrameTreeNodeId;
  const int prerender_frame_tree_node_id = id_iter->second;
  frame_tree_node_id_by_url_.erase(id_iter);

  auto host_iter =
      prerender_host_by_frame_tree_node_id_.find(prerender_frame_tree_node_id);
  DCHECK(host_iter != prerender_host_by_frame_tree_node_id_.end());
  std::unique_ptr<PrerenderHost> host = std::move(host_iter->second);
  prerender_host_by_frame_tree_node_id_.erase(host_iter);

  // If the host is not ready for activation yet, destroys it and returns
  // an invalid id. This is because it is likely that the prerendered page is
  // never used from now on.
  if (!host->is_ready_for_activation())
    return RenderFrameHost::kNoFrameTreeNodeId;

  switch (blink::features::kPrerender2ImplementationParam.Get()) {
    case blink::features::Prerender2Implementation::kWebContents:
      break;
    case blink::features::Prerender2Implementation::kMPArch:
      // The feature param disallows activation of the prerendered page for
      // testing. Destroy the host to dispose of the prerendered page and return
      // an invalid id.
      // TODO(https://crbug.com/1170277): Remove once activation support is
      // added to MPArch.
      return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // Reserve the host for activation.
  auto result = reserved_prerender_host_by_frame_tree_node_id_.emplace(
      prerender_frame_tree_node_id, std::move(host));
  DCHECK(result.second);

  return prerender_frame_tree_node_id;
}

bool PrerenderHostRegistry::ActivateReservedHost(
    int frame_tree_node_id,
    RenderFrameHostImpl& current_render_frame_host) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == reserved_prerender_host_by_frame_tree_node_id_.end())
    return false;
  std::unique_ptr<PrerenderHost> prerender_host = std::move(iter->second);
  reserved_prerender_host_by_frame_tree_node_id_.erase(iter);
  return prerender_host->ActivatePrerenderedContents(current_render_frame_host);
}

void PrerenderHostRegistry::AbandonReservedHost(int frame_tree_node_id) {
  // AbandonReservedHost() should not be called for non-reserved hosts.
  DCHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                         frame_tree_node_id));
  reserved_prerender_host_by_frame_tree_node_id_.erase(frame_tree_node_id);
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

void PrerenderHostRegistry::NotifyTrigger(const GURL& url) {
  for (Observer& obs : observers_)
    obs.OnTrigger(url);
}

}  // namespace content
