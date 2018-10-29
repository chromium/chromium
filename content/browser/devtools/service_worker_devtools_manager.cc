// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_manager.h"

#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_listener.h"

namespace content {

// static
ServiceWorkerDevToolsManager* ServiceWorkerDevToolsManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<ServiceWorkerDevToolsManager>::get();
}

ServiceWorkerDevToolsAgentHost*
ServiceWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  auto it = live_hosts_.find(WorkerId(worker_process_id, worker_route_id));
  return it == live_hosts_.end() ? nullptr : it->second.get();
}

void ServiceWorkerDevToolsManager::AddAllAgentHosts(
    ServiceWorkerDevToolsAgentHost::List* result) {
  for (auto& it : live_hosts_)
    result->push_back(it.second.get());
}

void ServiceWorkerDevToolsManager::AddAllAgentHostsForBrowserContext(
    BrowserContext* browser_context,
    ServiceWorkerDevToolsAgentHost::List* result) {
  for (auto& it : live_hosts_) {
    if (it.second->GetBrowserContext() == browser_context)
      result->push_back(it.second.get());
  }
}

void ServiceWorkerDevToolsManager::WorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerContextCore* context,
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed_version,
    base::UnguessableToken* devtools_worker_token,
    bool* pause_on_start) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  DCHECK(live_hosts_.find(worker_id) == live_hosts_.end());

  auto it = std::find_if(
      terminated_hosts_.begin(), terminated_hosts_.end(),
      [&context, &version_id](ServiceWorkerDevToolsAgentHost* agent_host) {
        return agent_host->Matches(context, version_id);
      });
  if (it == terminated_hosts_.end()) {
    *devtools_worker_token = base::UnguessableToken::Create();
    scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
        new ServiceWorkerDevToolsAgentHost(worker_process_id, worker_route_id,
                                           context, context_weak, version_id,
                                           url, scope, is_installed_version,
                                           *devtools_worker_token);
    live_hosts_[worker_id] = host;
    *pause_on_start = debug_service_worker_on_start_;
    for (auto& observer : observer_list_) {
      bool should_pause_on_start = false;
      observer.WorkerCreated(host.get(), &should_pause_on_start);
      if (should_pause_on_start)
        *pause_on_start = true;
    }
    return;
  }

  ServiceWorkerDevToolsAgentHost* agent_host = *it;
  terminated_hosts_.erase(it);
  live_hosts_[worker_id] = agent_host;
  agent_host->WorkerRestarted(worker_process_id, worker_route_id);
  *pause_on_start = agent_host->IsAttached();
  *devtools_worker_token = agent_host->devtools_worker_token();
}

void ServiceWorkerDevToolsManager::WorkerReadyForInspection(
    int worker_process_id,
    int worker_route_id,
    blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
    blink::mojom::DevToolsAgentAssociatedPtrInfo devtools_agent_ptr_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerReadyForInspection(std::move(host_request),
                                 std::move(devtools_agent_ptr_info));
  // Bring up UI for the ones not picked by other clients.
  if (debug_service_worker_on_start_ && !host->IsAttached())
    host->Inspect();
}

void ServiceWorkerDevToolsManager::WorkerVersionInstalled(int worker_process_id,
                                                          int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerVersionInstalled();
  for (auto& observer : observer_list_)
    observer.WorkerVersionInstalled(host.get());
}

void ServiceWorkerDevToolsManager::WorkerVersionDoomed(int worker_process_id,
                                                       int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerVersionDoomed();
  for (auto& observer : observer_list_)
    observer.WorkerVersionDoomed(host.get());
}

void ServiceWorkerDevToolsManager::WorkerDestroyed(int worker_process_id,
                                                   int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(it->second);
  live_hosts_.erase(it);
  terminated_hosts_.insert(agent_host.get());
  agent_host->WorkerDestroyed();
  for (auto& observer : observer_list_)
    observer.WorkerDestroyed(agent_host.get());
}

void ServiceWorkerDevToolsManager::AgentHostDestroyed(
    ServiceWorkerDevToolsAgentHost* agent_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = terminated_hosts_.find(agent_host);
  // Might be missing during shutdown due to different
  // destruction order of this manager, service workers
  // and their agent hosts.
  if (it != terminated_hosts_.end())
    terminated_hosts_.erase(it);
}

void ServiceWorkerDevToolsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ServiceWorkerDevToolsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ServiceWorkerDevToolsManager::set_debug_service_worker_on_start(
    bool debug_on_start) {
  debug_service_worker_on_start_ = debug_on_start;
}

ServiceWorkerDevToolsManager::ServiceWorkerDevToolsManager()
    : debug_service_worker_on_start_(false) {
}

ServiceWorkerDevToolsManager::~ServiceWorkerDevToolsManager() {
}

void ServiceWorkerDevToolsManager::NavigationPreloadRequestSent(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const network::ResourceRequest& request) {
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  for (auto* network :
       protocol::NetworkHandler::ForAgentHost(it->second.get())) {
    network->RequestSent(request_id, std::string(), request,
                         protocol::Network::Initiator::TypeEnum::Preload,
                         base::nullopt /* initiator_url */);
  }
}

void ServiceWorkerDevToolsManager::NavigationPreloadResponseReceived(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const GURL& url,
    const network::ResourceResponseHead& head) {
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(it->second.get()))
    network->ResponseReceived(request_id, std::string(), url,
                              protocol::Network::ResourceTypeEnum::Other, head,
                              protocol::Maybe<std::string>());
}

void ServiceWorkerDevToolsManager::NavigationPreloadCompleted(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(it->second.get()))
    network->LoadingComplete(
        request_id, protocol::Network::ResourceTypeEnum::Other, status);
}

}  // namespace content
