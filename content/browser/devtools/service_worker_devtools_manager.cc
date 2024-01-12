// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_manager.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_listener.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"

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

ServiceWorkerDevToolsAgentHost*
ServiceWorkerDevToolsManager::GetDevToolsAgentHostForNewInstallingWorker(
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id) {
  auto it = base::ranges::find_if(
      new_installing_hosts_,
      [&context_wrapper, &version_id](
          const scoped_refptr<ServiceWorkerDevToolsAgentHost>& agent_host) {
        return agent_host->context_wrapper() == context_wrapper &&
               agent_host->version_id() == version_id;
      });
  if (it == new_installing_hosts_.end())
    return nullptr;
  return it->get();
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
  for (auto& it : new_installing_hosts_) {
    if (it->GetBrowserContext() == browser_context)
      result->push_back(it.get());
  }
}

void ServiceWorkerDevToolsManager::WorkerMainScriptFetchingStarting(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    const GlobalRenderFrameHostId& requesting_frame_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Verify that we are not getting a similar host that's already in a stopped
  // state. This should never happen, we are installing a new SW, we cannot
  // have the same one that was started and stopped.
  ServiceWorkerContextWrapper* context_wrapper_ptr = context_wrapper.get();
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host =
      TakeStoppedHost(context_wrapper_ptr, version_id);
  DCHECK(!agent_host);

  scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
      base::MakeRefCounted<ServiceWorkerDevToolsAgentHost>(
          -1, -1, std::move(context_wrapper), version_id, url, scope,
          /*is_installed_version=*/false,
          /*client_security_state=*/nullptr,
          /*coep_reporter=*/mojo::NullRemote(),
          base::UnguessableToken::Create());

  ServiceWorkerDevToolsAgentHost* host_ptr = host.get();
  new_installing_hosts_.insert(std::move(host));

  for (auto& observer : observer_list_) {
    bool should_pause_on_start = false;
    observer.WorkerCreated(host_ptr, &should_pause_on_start);
    if (should_pause_on_start) {
      host_ptr->set_should_pause_on_start(true);
    }
  }

  // Now that we have a devtools target, we need to give devtools the
  // opportunity to attach to it before we do the actual fetch. We pass it a
  // callback that will be called once we have all the handlers ready.
  if (host_ptr->should_pause_on_start()) {
    devtools_instrumentation::ThrottleServiceWorkerMainScriptFetch(
        context_wrapper_ptr, version_id, requesting_frame_id, throttle_handle);
  }
}

void ServiceWorkerDevToolsManager::WorkerMainScriptFetchingFailed(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
      TakeNewInstallingHost(context_wrapper.get(), version_id);

  // While not strictly required, some WPTs expect all messages to be answered
  // before finishing and will loop until they get an answer. This call makes
  // sure all pending messages are answered with an error when we fail the
  // main script fetch.
  host->WorkerMainScriptFetchingFailed();

  // This observer call should trigger the destruction of the
  // ServiceWorkerDevToolsAgentHost by removing the scoped_ptr references held
  // by auto-attachers.
  for (auto& observer : observer_list_)
    observer.WorkerDestroyed(host.get());
}

void ServiceWorkerDevToolsManager::WorkerStarting(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed_version,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    base::UnguessableToken* devtools_worker_token,
    bool* pause_on_start) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId worker_id(worker_process_id, worker_route_id);
  DCHECK(!base::Contains(live_hosts_, worker_id));

  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host =
      TakeStoppedHost(context_wrapper.get(), version_id);
  if (agent_host) {
    live_hosts_[worker_id] = agent_host;
    agent_host->WorkerStarted(worker_process_id, worker_route_id);
    *pause_on_start =
        agent_host->IsAttached() && agent_host->should_pause_on_start();
    *devtools_worker_token = agent_host->devtools_worker_token();
    return;
  }

  agent_host = TakeNewInstallingHost(context_wrapper.get(), version_id);
  if (agent_host) {
    live_hosts_[worker_id] = agent_host;
    agent_host->WorkerStarted(worker_process_id, worker_route_id);
    *pause_on_start = agent_host->should_pause_on_start();
    *devtools_worker_token = agent_host->devtools_worker_token();

    if (client_security_state) {
      agent_host->UpdateClientSecurityState(std::move(client_security_state),
                                            std::move(coep_reporter));
    }

    return;
  }

  *devtools_worker_token = base::UnguessableToken::Create();
  auto host = base::MakeRefCounted<ServiceWorkerDevToolsAgentHost>(
      worker_process_id, worker_route_id, std::move(context_wrapper),
      version_id, url, scope, is_installed_version,
      std::move(client_security_state), std::move(coep_reporter),
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
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);
  for (auto* network :
       protocol::NetworkHandler::ForAgentHost(it->second.get())) {
    network->RequestSent(request_id, std::string(), request.headers,
                         *request_info,
                         protocol::Network::Initiator::TypeEnum::Preload,
                         /*initiator_url=*/std::nullopt,
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

  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  for (auto* network : protocol::NetworkHandler::ForAgentHost(it->second.get()))
    network->ResponseReceived(request_id, std::string(), url,
                              protocol::Network::ResourceTypeEnum::Other,
                              *head_info, protocol::Maybe<std::string>());
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
  auto it = base::ranges::find_if(
      stopped_hosts_, [&context_wrapper, &version_id](
                          ServiceWorkerDevToolsAgentHost* agent_host) {
        return agent_host->context_wrapper() == context_wrapper &&
               agent_host->version_id() == version_id;
      });
  if (it == stopped_hosts_.end())
    return nullptr;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(*it);
  stopped_hosts_.erase(it);
  return agent_host;
}

scoped_refptr<ServiceWorkerDevToolsAgentHost>
ServiceWorkerDevToolsManager::TakeNewInstallingHost(
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id) {
  auto it = base::ranges::find_if(
      new_installing_hosts_,
      [&context_wrapper, &version_id](
          const scoped_refptr<ServiceWorkerDevToolsAgentHost>& agent_host) {
        return agent_host->context_wrapper() == context_wrapper &&
               agent_host->version_id() == version_id;
      });
  if (it == new_installing_hosts_.end())
    return nullptr;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(std::move(*it));
  new_installing_hosts_.erase(it);
  return agent_host;
}

}  // namespace content
