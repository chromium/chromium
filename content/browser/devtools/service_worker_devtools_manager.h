// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/web/devtools_agent.mojom.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}

namespace content {

class BrowserContext;
class ServiceWorkerDevToolsAgentHost;
class ServiceWorkerContextCore;

// Manages ServiceWorkerDevToolsAgentHost's. This class lives on UI thread.
class CONTENT_EXPORT ServiceWorkerDevToolsManager {
 public:
  class Observer {
   public:
    virtual void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                               bool* should_pause_on_start) {}
    virtual void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {}

   protected:
    virtual ~Observer() {}
  };

  // Returns the ServiceWorkerDevToolsManager singleton.
  static ServiceWorkerDevToolsManager* GetInstance();

  ServiceWorkerDevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);
  void AddAllAgentHosts(
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result);
  void AddAllAgentHostsForBrowserContext(
      BrowserContext* browser_context,
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result);

  void WorkerCreated(int worker_process_id,
                     int worker_route_id,
                     const ServiceWorkerContextCore* context,
                     base::WeakPtr<ServiceWorkerContextCore> context_weak,
                     int64_t version_id,
                     const GURL& url,
                     const GURL& scope,
                     bool is_installed_version,
                     base::UnguessableToken* devtools_worker_token,
                     bool* pause_on_start);
  void WorkerReadyForInspection(
      int worker_process_id,
      int worker_route_id,
      blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
      blink::mojom::DevToolsAgentAssociatedPtrInfo devtools_agent_ptr_info);
  void WorkerVersionInstalled(int worker_process_id, int worker_route_id);
  void WorkerVersionDoomed(int worker_process_id, int worker_route_id);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);
  void NavigationPreloadRequestSent(int worker_process_id,
                                    int worker_route_id,
                                    const std::string& request_id,
                                    const network::ResourceRequest& request);
  void NavigationPreloadResponseReceived(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const GURL& url,
      const network::ResourceResponseHead& head);
  void NavigationPreloadCompleted(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void set_debug_service_worker_on_start(bool debug_on_start);
  bool debug_service_worker_on_start() const {
    return debug_service_worker_on_start_;
  }
  void AgentHostDestroyed(ServiceWorkerDevToolsAgentHost* agent_host);

 private:
  friend struct base::DefaultSingletonTraits<ServiceWorkerDevToolsManager>;
  friend class ServiceWorkerDevToolsAgentHost;

  using WorkerId = std::pair<int, int>;

  ServiceWorkerDevToolsManager();
  ~ServiceWorkerDevToolsManager();

  base::ObserverList<Observer>::Unchecked observer_list_;
  bool debug_service_worker_on_start_;

  // We retatin agent hosts as long as the service worker is alive.
  std::map<WorkerId, scoped_refptr<ServiceWorkerDevToolsAgentHost>> live_hosts_;

  // Clients may retain agent host for the terminated shared worker,
  // and we reconnect them when shared worker is restarted.
  base::flat_set<ServiceWorkerDevToolsAgentHost*> terminated_hosts_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_
