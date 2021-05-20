// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
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
    RenderFrameHostImpl& initiator_render_frame_host) {
  DCHECK(attributes);
  TRACE_EVENT2(
      "navigation", "PrerenderHostRegistry::CreateAndStartHost", "attributes",
      attributes, "initiator_origin",
      initiator_render_frame_host.GetLastCommittedOrigin().GetURL().spec());

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
  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    if (iter.second->GetInitialUrl() == attributes->url)
      return iter.first;
  }

  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes), initiator_render_frame_host);
  const int frame_tree_node_id = prerender_host->frame_tree_node_id();

  CHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                        frame_tree_node_id));
  prerender_host_by_frame_tree_node_id_[frame_tree_node_id] =
      std::move(prerender_host);
  prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
      ->StartPrerendering();

  return frame_tree_node_id;
}

void PrerenderHostRegistry::AbandonHost(int frame_tree_node_id) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::AbandonHost",
               "frame_tree_node_id", frame_tree_node_id);
  AbandonHostInternal(frame_tree_node_id);
}

void PrerenderHostRegistry::AbandonHostAsync(
    int frame_tree_node_id,
    PrerenderHost::FinalStatus final_status) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::AbandonHostAsync",
               "frame_tree_node_id", frame_tree_node_id);
  // Remove the prerender host from the host maps so that it's not used for
  // activation during asynchronous deletion.
  std::unique_ptr<PrerenderHost> prerender_host =
      AbandonHostInternal(frame_tree_node_id);
  if (prerender_host) {
    // Report only if this is the first valid call for `frame_tree_node_id`.
    prerender_host->RecordFinalStatus(PassKey(), final_status);

    // Asynchronously delete the prerender host.
    to_be_deleted_hosts_.push_back(std::move(prerender_host));
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrerenderHostRegistry::DeleteAbandonedHosts,
                                  weak_factory_.GetWeakPtr()));
  }
}

int PrerenderHostRegistry::ReserveHostToActivate(
    const GURL& navigation_url,
    FrameTreeNode& frame_tree_node) {
  RenderFrameHostImpl* render_frame_host = frame_tree_node.current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::ReserveHostToActivate",
               "navigation_url", navigation_url.spec(), "render_frame_host",
               render_frame_host);

  // Disallow activation when the navigation is for prerendering.
  if (frame_tree_node.frame_tree()->is_prerendering())
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

  // Find an available host for the navigation URL.
  std::unique_ptr<PrerenderHost> host;
  for (auto iter = prerender_host_by_frame_tree_node_id_.begin();
       iter != prerender_host_by_frame_tree_node_id_.end(); ++iter) {
    if (iter->second->GetInitialUrl() == navigation_url) {
      host = std::move(iter->second);
      prerender_host_by_frame_tree_node_id_.erase(iter);
      break;
    }
  }
  if (!host)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // If the host is not ready for activation yet, destroys it and returns
  // an invalid id. This is because it is likely that the prerendered page is
  // never used from now on.
  if (!host->is_ready_for_activation())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Reserve the host for activation.
  const int prerender_frame_tree_node_id = host->frame_tree_node_id();
  auto result = reserved_prerender_host_by_frame_tree_node_id_.emplace(
      prerender_frame_tree_node_id, std::move(host));
  DCHECK(result.second);

  return prerender_frame_tree_node_id;
}

RenderFrameHostImpl* PrerenderHostRegistry::GetRenderFrameHostForReservedHost(
    int frame_tree_node_id) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == reserved_prerender_host_by_frame_tree_node_id_.end()) {
    return nullptr;
  }
  return iter->second->GetPrerenderedMainFrameHost();
}

std::unique_ptr<BackForwardCacheImpl::Entry>
PrerenderHostRegistry::ActivateReservedHost(
    int frame_tree_node_id,
    NavigationRequest& navigation_request) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  CHECK(iter != reserved_prerender_host_by_frame_tree_node_id_.end());
  std::unique_ptr<PrerenderHost> prerender_host = std::move(iter->second);
  reserved_prerender_host_by_frame_tree_node_id_.erase(iter);
  return prerender_host->Activate(navigation_request);
}

void PrerenderHostRegistry::AbandonReservedHost(int frame_tree_node_id) {
  // AbandonReservedHost() should not be called for non-reserved hosts.
  DCHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                         frame_tree_node_id));
  reserved_prerender_host_by_frame_tree_node_id_.erase(frame_tree_node_id);
}

PrerenderHost* PrerenderHostRegistry::FindNonReservedHostById(
    int frame_tree_node_id) {
  auto id_iter = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (id_iter == prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  return id_iter->second.get();
}

PrerenderHost* PrerenderHostRegistry::FindReservedHostById(
    int frame_tree_node_id) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == reserved_prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  return iter->second.get();
}

std::vector<RenderFrameHostImpl*>
PrerenderHostRegistry::GetPrerenderedMainFrames() {
  std::vector<RenderFrameHostImpl*> result;
  for (auto& i : prerender_host_by_frame_tree_node_id_) {
    result.push_back(i.second->GetPrerenderedMainFrameHost());
  }
  for (auto& i : reserved_prerender_host_by_frame_tree_node_id_) {
    result.push_back(i.second->GetPrerenderedMainFrameHost());
  }
  return result;
}

PrerenderHost* PrerenderHostRegistry::FindHostByUrlForTesting(
    const GURL& prerendering_url) {
  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    if (iter.second->GetInitialUrl() == prerendering_url)
      return iter.second.get();
  }
  return nullptr;
}

std::unique_ptr<PrerenderHost> PrerenderHostRegistry::AbandonHostInternal(
    int frame_tree_node_id) {
  auto found = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (found == prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  std::unique_ptr<PrerenderHost> prerender_host = std::move(found->second);
  prerender_host_by_frame_tree_node_id_.erase(found);
  return prerender_host;
}

void PrerenderHostRegistry::DeleteAbandonedHosts() {
  to_be_deleted_hosts_.clear();
}

void PrerenderHostRegistry::NotifyTrigger(const GURL& url) {
  for (Observer& obs : observers_)
    obs.OnTrigger(url);
}

}  // namespace content
