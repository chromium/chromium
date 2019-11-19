// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_auto_attacher.h"

#include "base/containers/queue.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
namespace protocol {

namespace {

using ScopeAgentsMap =
    std::map<GURL, std::unique_ptr<ServiceWorkerDevToolsAgentHost::List>>;

void GetMatchingHostsByScopeMap(
    const ServiceWorkerDevToolsAgentHost::List& agent_hosts,
    const base::flat_set<GURL>& urls,
    ScopeAgentsMap* scope_agents_map) {
  base::flat_set<GURL> host_name_set;
  for (const GURL& url : urls)
    host_name_set.insert(url.GetOrigin());
  for (const auto& host : agent_hosts) {
    if (host_name_set.find(host->scope().GetOrigin()) == host_name_set.end())
      continue;
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
  // 4. Frame is trying to navigate and we _should_ pick up an existing SW but we don't.
  //    We _could_ do this, but since we are not pausing the navigation, there
  //    is no principal difference between picking up SW earlier or later.
  //
  // We also try to detach from SW picked up for [3] if navigation has failed
  // (from DidFinishNavigation).

  base::flat_set<GURL> frame_urls;
  if (render_frame_host) {
    for (FrameTreeNode* node :
         render_frame_host->frame_tree_node()->frame_tree()->Nodes()) {
      frame_urls.insert(node->current_url());
      // We use both old and new frame urls to support [3], where we attach while
      // navigation is still ongoing.
      if (node->navigation_request()) {
        frame_urls.insert(node->navigation_request()->common_params().url);
      }
    }
  }
  return frame_urls;
}

}  // namespace

TargetAutoAttacher::TargetAutoAttacher(
    AttachCallback attach_callback,
    DetachCallback detach_callback,
    DevToolsRendererChannel* renderer_channel)
    : attach_callback_(attach_callback),
      detach_callback_(detach_callback),
      renderer_channel_(renderer_channel),
      render_frame_host_(nullptr),
      auto_attach_(false),
      wait_for_debugger_on_start_(false) {}

TargetAutoAttacher::~TargetAutoAttacher() {}

void TargetAutoAttacher::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  UpdateFrames();
  UpdatePortals();
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::UpdatePortals() {
  if (!auto_attach_)
    return;

  Hosts new_hosts;
  if (render_frame_host_ &&
      render_frame_host_->frame_tree_node()->IsMainFrame()) {
    WebContentsImpl* outer_web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host_));
    for (WebContents* web_contents :
         outer_web_contents->GetInnerWebContents()) {
      WebContentsImpl* web_contents_impl =
          static_cast<WebContentsImpl*>(web_contents);
      if (!web_contents_impl->IsPortal())
        continue;

      scoped_refptr<DevToolsAgentHost> new_host =
          RenderFrameDevToolsAgentHost::GetOrCreateFor(
              web_contents_impl->GetFrameTree()->root());
      new_hosts.insert(new_host);
    }
  }

  // TODO(dgozman): support wait_for_debugger_on_start_.
  ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypePage, false);
}

void TargetAutoAttacher::UpdateServiceWorkers() {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::UpdateFrames() {
  if (!auto_attach_)
    return;

  Hosts new_hosts;
  if (render_frame_host_) {
    FrameTreeNode* root = render_frame_host_->frame_tree_node();
    base::queue<FrameTreeNode*> queue;
    queue.push(root);
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();
      bool cross_process = node->current_frame_host()->IsCrossProcessSubframe();
      if (node != root && cross_process) {
        scoped_refptr<DevToolsAgentHost> new_host =
            RenderFrameDevToolsAgentHost::GetOrCreateFor(node);
        new_hosts.insert(new_host);
      } else {
        for (size_t i = 0; i < node->child_count(); ++i)
          queue.push(node->child_at(i));
      }
    }
  }

  // TODO(dgozman): support wait_for_debugger_on_start_.
  ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypeFrame, false);
}

void TargetAutoAttacher::AgentHostClosed(DevToolsAgentHost* host) {
  auto_attached_hosts_.erase(base::WrapRefCounted(host));
}

bool TargetAutoAttacher::ShouldThrottleFramesNavigation() {
  return auto_attach_;
}

DevToolsAgentHost* TargetAutoAttacher::AutoAttachToFrame(
    NavigationRequest* navigation_request) {
  if (!ShouldThrottleFramesNavigation())
    return nullptr;

  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  RenderFrameHostImpl* new_host = navigation_request->GetRenderFrameHost();

  // |new_host| can be nullptr for navigation that doesn't commmit
  // (e.g. download). Skip possibly detaching the old agent host so the DevTools
  // message logged via the old RFH can be seen.
  if (!new_host)
    return nullptr;

  scoped_refptr<DevToolsAgentHost> agent_host =
      RenderFrameDevToolsAgentHost::FindForDangling(frame_tree_node);

  // Process the window.open auto-attaches for new targets.
  if (frame_tree_node->original_opener()) {
    if (!agent_host) {
      agent_host =
          RenderFrameDevToolsAgentHost::CreateForCrossProcessNavigation(
              navigation_request);
    }
    if (auto_attached_hosts_.find(agent_host) != auto_attached_hosts_.end())
      return nullptr;
    attach_callback_.Run(agent_host.get(), wait_for_debugger_on_start_);
    auto_attached_hosts_.insert(agent_host);
    return wait_for_debugger_on_start_ ? agent_host.get() : nullptr;
  }

  bool old_cross_process = !!agent_host;
  bool is_portal_main_frame =
      frame_tree_node->IsMainFrame() &&
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(new_host))
          ->IsPortal();
  bool new_cross_process =
      new_host->IsCrossProcessSubframe() || is_portal_main_frame;

  if (old_cross_process == new_cross_process)
    return nullptr;

  if (new_cross_process) {
    agent_host = RenderFrameDevToolsAgentHost::CreateForCrossProcessNavigation(
        navigation_request);
    attach_callback_.Run(agent_host.get(), wait_for_debugger_on_start_);
    auto_attached_hosts_.insert(agent_host);
    return wait_for_debugger_on_start_ ? agent_host.get() : nullptr;
  }

  DCHECK(old_cross_process);
  auto it = auto_attached_hosts_.find(agent_host);
  // This should not happen in theory, but error pages are sometimes not
  // picked up. See https://crbug.com/836511 and https://crbug.com/817881.
  if (it == auto_attached_hosts_.end())
    return nullptr;
  auto_attached_hosts_.erase(it);
  detach_callback_.Run(agent_host.get());
  return nullptr;
}

void TargetAutoAttacher::ReattachServiceWorkers(bool waiting_for_debugger) {
  if (!auto_attaching_service_workers_)
    return;

  BrowserContext* browser_context = nullptr;
  if (render_frame_host_)
    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();

  auto matching = GetMatchingServiceWorkers(browser_context,
                                            GetFrameUrls(render_frame_host_));
  Hosts new_hosts;
  for (const auto& pair : matching)
    new_hosts.insert(pair.second);
  ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypeServiceWorker,
                        waiting_for_debugger);
}

void TargetAutoAttacher::ReattachTargetsOfType(const Hosts& new_hosts,
                                               const std::string& type,
                                               bool waiting_for_debugger) {
  Hosts old_hosts = auto_attached_hosts_;
  for (auto& host : old_hosts) {
    if (host->GetType() == type && new_hosts.find(host) == new_hosts.end()) {
      auto_attached_hosts_.erase(host);
      detach_callback_.Run(host.get());
    }
  }
  for (auto& host : new_hosts) {
    if (old_hosts.find(host) == old_hosts.end()) {
      attach_callback_.Run(host.get(), waiting_for_debugger);
      auto_attached_hosts_.insert(host);
    }
  }
}

void TargetAutoAttacher::SetAutoAttach(bool auto_attach,
                                       bool wait_for_debugger_on_start,
                                       base::OnceClosure callback) {
  wait_for_debugger_on_start_ = wait_for_debugger_on_start;
  if (auto_attach && !auto_attach_) {
    auto_attach_ = true;
    auto_attaching_service_workers_ =
        render_frame_host_ && !render_frame_host_->GetParent();
    if (auto_attaching_service_workers_) {
      ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
      ReattachServiceWorkers(false);
    }
    UpdateFrames();
    UpdatePortals();
  } else if (!auto_attach && auto_attach_) {
    auto_attach_ = false;
    Hosts empty;
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeFrame, false);
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypePage, false);
    if (auto_attaching_service_workers_) {
      ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
      ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeServiceWorker,
                            false);
      auto_attaching_service_workers_ = false;
    }
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeDedicatedWorker,
                          false);
    DCHECK(auto_attached_hosts_.empty());
  }
  renderer_channel_->SetReportChildWorkers(
      this, auto_attach, wait_for_debugger_on_start, std::move(callback));
}

// -------- ServiceWorkerDevToolsManager::Observer ----------

void TargetAutoAttacher::WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                                       bool* should_pause_on_start) {
  BrowserContext* browser_context = nullptr;
  if (render_frame_host_)
    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();

  auto hosts = GetMatchingServiceWorkers(browser_context,
                                         GetFrameUrls(render_frame_host_));
  if (hosts.find(host->GetId()) != hosts.end()) {
    *should_pause_on_start = wait_for_debugger_on_start_;
    Hosts new_hosts;
    for (const auto& pair : hosts)
      new_hosts.insert(pair.second);
    ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypeServiceWorker,
                          wait_for_debugger_on_start_);
  }
}

void TargetAutoAttacher::WorkerVersionInstalled(
    ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::WorkerVersionDoomed(
    ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::ChildWorkerCreated(DevToolsAgentHostImpl* agent_host,
                                            bool waiting_for_debugger) {
  attach_callback_.Run(agent_host, waiting_for_debugger);
  auto_attached_hosts_.insert(scoped_refptr<DevToolsAgentHost>(agent_host));
}

}  // namespace protocol
}  // namespace content
