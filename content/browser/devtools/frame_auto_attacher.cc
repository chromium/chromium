// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/frame_auto_attacher.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "content/browser/devtools/auction_worklet_devtools_agent_host.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_storage_worklet_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {

using ScopeAgentsMap =
    std::map<GURL, std::unique_ptr<ServiceWorkerDevToolsAgentHost::List>>;

void GetMatchingHostsByScopeMap(
    const ServiceWorkerDevToolsAgentHost::List& agent_hosts,
    const base::flat_set<GURL>& urls,
    ScopeAgentsMap* scope_agents_map) {
  base::flat_set<GURL> host_name_set;
  for (const GURL& url : urls)
    host_name_set.insert(url.DeprecatedGetOriginAsURL());
  for (const auto& host : agent_hosts) {
    if (!base::Contains(host_name_set,
                        host->scope().DeprecatedGetOriginAsURL())) {
      continue;
    }
    const auto& it = scope_agents_map->find(host->scope());
    if (it == scope_agents_map->end()) {
      std::unique_ptr<ServiceWorkerDevToolsAgentHost::List> new_list(
          new ServiceWorkerDevToolsAgentHost::List());
      new_list->push_back(host);
      (*scope_agents_map)[host->scope()] = std::move(new_list);
    } else {
      it->second->push_back(host);
    }
  }
}

void AddEligibleHosts(const ServiceWorkerDevToolsAgentHost::List& list,
                      ServiceWorkerDevToolsAgentHost::Map* result) {
  base::Time last_installed_time;
  base::Time last_doomed_time;
  for (const auto& host : list) {
    if (host->version_installed_time() > last_installed_time)
      last_installed_time = host->version_installed_time();
    if (host->version_doomed_time() > last_doomed_time)
      last_doomed_time = host->version_doomed_time();
  }
  for (const auto& host : list) {
    // We don't attech old redundant Service Workers when there is newer
    // installed Service Worker.
    if (host->version_doomed_time().is_null() ||
        (last_installed_time < last_doomed_time &&
         last_doomed_time == host->version_doomed_time())) {
      (*result)[host->GetId()] = host;
    }
  }
}

ServiceWorkerDevToolsAgentHost::Map GetMatchingServiceWorkers(
    BrowserContext* browser_context,
    const base::flat_set<GURL>& urls) {
  ServiceWorkerDevToolsAgentHost::Map result;
  if (!browser_context)
    return result;

  ServiceWorkerDevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()
      ->AddAllAgentHostsForBrowserContext(browser_context, &agent_hosts);

  ScopeAgentsMap scope_agents_map;
  GetMatchingHostsByScopeMap(agent_hosts, urls, &scope_agents_map);

  for (const auto& it : scope_agents_map)
    AddEligibleHosts(*it.second.get(), &result);

  return result;
}

base::flat_set<GURL> GetFrameUrls(RenderFrameHostImpl* render_frame_host) {
  // We try to attach to a service worker in the following cases:
  // 1. SW is created while user is inspecting frame (from WorkerCreated).
  // 2. Frame has navigated and we are picking up new SW corresponding to new
  //    url (from DidFinishNavigation).
  // 3. Frame is trying to navigate and it spawns a new SW which we pick up
  //    (from WorkerCreated). See also https://crbug.com/907072
  //
  // We are not attaching in the following case:
  // 4. Frame is trying to navigate and we _should_ pick up an existing SW but
  //    we don't. We _could_ do this, but since we are not pausing the
  //    navigation, there is no principal difference between picking up SW
  //    earlier or later.
  //
  // We also try to detach from SW picked up for [3] if navigation has failed
  // (from DidFinishNavigation).

  base::flat_set<GURL> frame_urls;
  if (render_frame_host) {
    for (FrameTreeNode* node : render_frame_host->frame_tree()->Nodes()) {
      frame_urls.insert(node->current_url());
      // We use both old and new frame urls to support [3], where we attach
      // while navigation is still ongoing.
      if (node->navigation_request()) {
        frame_urls.insert(node->navigation_request()->common_params().url);
      }
    }
  }
  return frame_urls;
}

}  // namespace

FrameAutoAttacher::FrameAutoAttacher(DevToolsRendererChannel* renderer_channel)
    : RendererAutoAttacherBase(renderer_channel) {}

FrameAutoAttacher::~FrameAutoAttacher() = default;

void FrameAutoAttacher::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  if (!auto_attach())
    return;
  UpdateFrames();
  ReattachServiceWorkers();
}

void FrameAutoAttacher::DidFinishNavigation(
    NavigationRequest* navigation_request) {
  if (!auto_attach() || !render_frame_host_)
    return;

  if (navigation_request->frame_tree_node() ==
      render_frame_host_->frame_tree_node()) {
    ReattachServiceWorkers();
    return;
  }

  // We only care about subframes that have |render_frame_host_| as their
  // local root.
  if (!navigation_request->HasCommitted())
    return;
  RenderFrameHostImpl* parent = navigation_request->GetParentFrame();
  while (parent && !parent->is_local_root())
    parent = parent->GetParent();
  if (parent != render_frame_host_)
    return;

  // Some subframes may not be attached through
  // TargetHandler::ResponseThrottle because DevTools wasn't attached when the
  // navigation started, so no throttle was installed. We auto-attach them
  // here instead (note that we cannot honor |wait_for_debugger_on_start_| in
  // this case).
  constexpr bool wait_for_debugger_on_start = false;
  scoped_refptr<RenderFrameDevToolsAgentHost> agent_host =
      HandleNavigation(navigation_request, wait_for_debugger_on_start);
  if (agent_host)
    DispatchAutoAttach(agent_host.get(), wait_for_debugger_on_start);
}

void FrameAutoAttacher::AutoAttachToPage(FrameTree* frame_tree,
                                         bool wait_for_debugger_on_start) {
  if (!auto_attach())
    return;
  scoped_refptr<DevToolsAgentHost> agent_host =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(frame_tree->root());
  DispatchAutoAttach(agent_host.get(), wait_for_debugger_on_start);
}

void FrameAutoAttacher::UpdateAutoAttach(base::OnceClosure callback) {
  if (auto_attach()) {
    UpdateFrames();
    if (render_frame_host_ && !render_frame_host_->GetParent() &&
        !observing_service_workers_) {
      observing_service_workers_ = true;
      ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
    }
    if (observing_service_workers_) {
      // Update service workers even if we've already been observing them,
      // to notify new clients about existing service workers.
      // This is similar to frames and pages above.
      ReattachServiceWorkers();
    }
    if (render_frame_host_ && !observing_auction_worklets_) {
      observing_auction_worklets_ = true;
      DebuggableAuctionWorkletTracker::GetInstance()->AddObserver(this);
    }
    if (render_frame_host_ && !observing_shared_storage_worklets_) {
      observing_shared_storage_worklets_ = true;
      SharedStorageWorkletDevToolsManager::GetInstance()->AddObserver(this);
    }
  } else {
    if (observing_service_workers_) {
      ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
      observing_service_workers_ = false;
    }
    if (observing_auction_worklets_) {
      DebuggableAuctionWorkletTracker::GetInstance()->RemoveObserver(this);
      observing_auction_worklets_ = false;
    }
    if (observing_shared_storage_worklets_) {
      SharedStorageWorkletDevToolsManager::GetInstance()->RemoveObserver(this);
      observing_shared_storage_worklets_ = false;
    }
  }
  RendererAutoAttacherBase::UpdateAutoAttach(std::move(callback));
}

void FrameAutoAttacher::WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                                      bool* should_pause_on_start) {
  if (!render_frame_host_)
    return;
  BrowserContext* browser_context =
      render_frame_host_->GetProcess()->GetBrowserContext();
  auto hosts = GetMatchingServiceWorkers(browser_context,
                                         GetFrameUrls(render_frame_host_));
  if (!base::Contains(hosts, host->GetId())) {
    return;
  }

  *should_pause_on_start = wait_for_debugger_on_start();
  DispatchAutoAttach(host, *should_pause_on_start);
}

void FrameAutoAttacher::WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers();
}

void FrameAutoAttacher::AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                                              bool& should_pause_on_start) {
  if (!render_frame_host_)
    return;
  if (!AuctionWorkletDevToolsAgentHost::IsRelevantTo(render_frame_host_,
                                                     worklet)) {
    return;
  }
  should_pause_on_start = wait_for_debugger_on_start();
  DispatchAutoAttach(AuctionWorkletDevToolsAgentHostManager::GetInstance()
                         .GetOrCreateFor(worklet)
                         .get(),
                     should_pause_on_start);
}

void FrameAutoAttacher::SharedStorageWorkletCreated(
    SharedStorageWorkletDevToolsAgentHost* host,
    bool& should_pause_on_start) {
  if (!render_frame_host_) {
    return;
  }

  if (!host->IsRelevantTo(render_frame_host_)) {
    return;
  }

  should_pause_on_start = wait_for_debugger_on_start();
  DispatchAutoAttach(host, should_pause_on_start);
}

void FrameAutoAttacher::SharedStorageWorkletDestroyed(
    SharedStorageWorkletDevToolsAgentHost* host) {
  if (!render_frame_host_) {
    return;
  }

  DispatchAutoDetach(host);
}

void FrameAutoAttacher::ReattachServiceWorkers() {
  if (!observing_service_workers_ || !render_frame_host_)
    return;
  BrowserContext* browser_context =
      render_frame_host_->GetProcess()->GetBrowserContext();
  auto matching = GetMatchingServiceWorkers(browser_context,
                                            GetFrameUrls(render_frame_host_));
  Hosts new_hosts;
  for (const auto& pair : matching)
    new_hosts.insert(pair.second);
  DispatchSetAttachedTargetsOfType(new_hosts,
                                   DevToolsAgentHost::kTypeServiceWorker);
}

void FrameAutoAttacher::UpdateFrames() {
  DCHECK(auto_attach());

  Hosts new_hosts;
  DevToolsAgentHost::List new_auction_worklet_hosts;
  DevToolsAgentHost::List new_shared_storage_worklet_hosts;
  if (render_frame_host_) {
    render_frame_host_->ForEachRenderFrameHostWithAction(
        [root = render_frame_host_, &new_hosts](RenderFrameHostImpl* rfh) {
          if (rfh == root || !rfh->is_local_root())
            return RenderFrameHost::FrameIterationAction::kContinue;

          // At this point, |rfh| is a local root that is in the subtree of
          // |root|.
          FrameTreeNode* node = rfh->frame_tree_node();
          bool should_create =
              !node->IsMainFrame() || node->IsFencedFrameRoot();
          if (should_create) {
            scoped_refptr<DevToolsAgentHost> new_host =
                RenderFrameDevToolsAgentHost::GetOrCreateFor(node);
            new_hosts.insert(new_host);
          }

          // Note: We don't search through children of local roots as they will
          // be handled by a FrameAutoAttacher that is created for the local
          // root.
          return RenderFrameHost::FrameIterationAction::kSkipChildren;
        });

    AuctionWorkletDevToolsAgentHostManager::GetInstance().GetAllForFrame(
        render_frame_host_, &new_auction_worklet_hosts);

    SharedStorageWorkletDevToolsManager::GetInstance()->GetAllForFrame(
        render_frame_host_, &new_shared_storage_worklet_hosts);
  }

  DispatchSetAttachedTargetsOfType(new_hosts, DevToolsAgentHost::kTypeFrame);
  DispatchSetAttachedTargetsOfType(
      TargetAutoAttacher::Hosts(new_auction_worklet_hosts.begin(),
                                new_auction_worklet_hosts.end()),
      DevToolsAgentHost::kTypeAuctionWorklet);
  DispatchSetAttachedTargetsOfType(
      TargetAutoAttacher::Hosts(new_shared_storage_worklet_hosts.begin(),
                                new_shared_storage_worklet_hosts.end()),
      DevToolsAgentHost::kTypeSharedStorageWorklet);
}

}  // namespace content
