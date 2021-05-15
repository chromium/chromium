// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_manager.h"

#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_listener.h"

namespace content {

// static
ServiceWorkerDevToolsManager* ServiceWorkerDevToolsManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<ServiceWorkerDevToolsManager> instance;
  return &*instance;
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

void ServiceWorkerDevToolsManager::WorkerStarting(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed_version,
    absl::optional<network::CrossOriginEmbedderPolicy>
        cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    base::UnguessableToken* devtools_worker_token,
    bool* pause_on_start) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  DCHECK(live_hosts_.find(worker_id) == live_hosts_.end());

  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host =
      TakeStoppedHost(context_wrapper.get(), version_id);
  if (agent_host) {
    live_hosts_[worker_id] = agent_host;
    agent_host->WorkerRestarted(worker_process_id, worker_route_id);
    *pause_on_start = agent_host->IsAttached();
    *devtools_worker_token = agent_host->devtools_worker_token();
    return;
  }

  *devtools_worker_token = base::UnguessableToken::Create();
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
      new ServiceWorkerDevToolsAgentHost(
          worker_process_id, worker_route_id, std::move(context_wrapper),
          version_id, url, scope, is_installed_version,
          cross_origin_embedder_policy, std::move(coep_reporter),
          *devtools_worker_token);
  live_hosts_[worker_id] = host;
  *pause_on_start = debug_service_worker_on_start_;
  for (auto& observer : observer_list_) {
    bool should_pause_on_start = false;
    observer.WorkerCreated(host.get(), &should_pause_on_start);
    if (should_pause_on_start)
      *pause_on_start = true;
  }
}

void ServiceWorkerDevToolsManager::WorkerReadyForInspection(
    int worker_process_id,
    int worker_route_id,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerReadyForInspection(std::move(agent_remote),
                                 std::move(host_receiver));
  // Bring up UI for the ones not picked by other clients.
  if (debug_service_worker_on_start_ && !host->IsAttached())
    host->Inspect();
}

void ServiceWorkerDevToolsManager::UpdateCrossOriginEmbedderPolicy(
    int worker_process_id,
    int worker_route_id,
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  it->second->UpdateCrossOriginEmbedderPolicy(
      std::move(cross_origin_embedder_policy), std::move(coep_reporter));
}

void ServiceWorkerDevToolsManager::WorkerVersionInstalled(int worker_process_id,
                                                          int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  it->second->WorkerVersionInstalled();
}

void ServiceWorkerDevToolsManager::WorkerVersionDoomed(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it != live_hosts_.end()) {
    it->second->WorkerVersionDoomed();
    return;
  }
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
      TakeStoppedHost(context_wrapper.get(), version_id);
  if (!host)
    return;
  host->WorkerVersionDoomed();
  // The worker has already been stopped and since it's doomed it will never
  // restart.
  for (auto& observer : observer_list_)
    observer.WorkerDestroyed(host.get());
}

void ServiceWorkerDevToolsManager::WorkerStopped(int worker_process_id,
                                                 int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(it->second);
  live_hosts_.erase(it);
  agent_host->WorkerStopped();
  if (agent_host->version_doomed_time().is_null()) {
    stopped_hosts_.insert(agent_host.get());
  } else {
    // The worker version has been doomed, it will never restart.
    for (auto& observer : observer_list_)
      observer.WorkerDestroyed(agent_host.get());
  }
}

void ServiceWorkerDevToolsManager::AgentHostDestroyed(
    ServiceWorkerDevToolsAgentHost* agent_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Might be missing during shutdown due to different
  // destruction order of this manager, service workers
  // and their agent hosts.
  stopped_hosts_.erase(agent_host);
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

ServiceWorkerDevToolsManager::~ServiceWorkerDevToolsManager() = default;

void ServiceWorkerDevToolsManager::NavigationPreloadRequestSent(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const network::ResourceRequest& request) {
  const WorkerId worker_id(worker_process_id, worker_route_id);
  auto it = live_hosts_.find(worker_id);
  if (it == live_hosts_.end())
    return;
  auto timestamp = base::TimeTicks::Now();
  for (auto* network :
       protocol::NetworkHandler::ForAgentHost(it->second.get())) {
    network->RequestSent(request_id, std::string(), request,
                         protocol::Network::Initiator::TypeEnum::Preload,
                         /*initiator_url=*/absl::nullopt,
                         /*initiator_devtools_request_id=*/"", timestamp);
  }
}

void ServiceWorkerDevToolsManager::NavigationPreloadResponseReceived(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
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

scoped_refptr<ServiceWorkerDevToolsAgentHost>
ServiceWorkerDevToolsManager::TakeStoppedHost(
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id) {
  auto it =
      std::find_if(stopped_hosts_.begin(), stopped_hosts_.end(),
                   [&context_wrapper,
                    &version_id](ServiceWorkerDevToolsAgentHost* agent_host) {
                     return agent_host->context_wrapper() == context_wrapper &&
                            agent_host->version_id() == version_id;
                   });
  if (it == stopped_hosts_.end())
    return nullptr;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(*it);
  stopped_hosts_.erase(it);
  return agent_host;
}

}  // namespace content
