// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_

#include <stdint.h>

#include <map>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class BrowserContext;

class ServiceWorkerDevToolsAgentHost : public DevToolsAgentHostImpl,
                                       RenderProcessHostObserver {
 public:
  using List = std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using Map = std::map<std::string,
                       scoped_refptr<ServiceWorkerDevToolsAgentHost>>;

  // Instantiates an agent host for the service worker identified by
  // `worker_process_id` and `worker_route_id`.
  //
  // `client_security_state` may be nullptr if the worker script headers have
  // not been fetched yet. In that case, `UpdateClientSecurityState()` should be
  // called once the headers have been fetched.
  ServiceWorkerDevToolsAgentHost(
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
      const base::UnguessableToken& devtools_worker_token);

  ServiceWorkerDevToolsAgentHost(const ServiceWorkerDevToolsAgentHost&) =
      delete;
  ServiceWorkerDevToolsAgentHost& operator=(
      const ServiceWorkerDevToolsAgentHost&) = delete;

  // DevToolsAgentHost overrides.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  NetworkLoaderFactoryParamsAndInfo CreateNetworkFactoryParamsForDevTools()
      override;
  RenderProcessHost* GetProcessHost() override;
  std::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

  void WorkerStarted(int worker_process_id, int worker_route_id);
  void WorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver);

  // Sets the client security state of the linked service worker.
  // Called when the worker starts, once the client security state is known.
  // `client_security_state` must not be nullptr.
  void UpdateClientSecurityState(
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter);

  void WorkerStopped();
  void WorkerVersionInstalled();
  void WorkerVersionDoomed();

  // This a niche function used when failing a ServiceWorker main script fetch
  // with PlzServiceWorker. Since the worker did not have the opportunity to
  // boot up, some messages will be left unanswered. This makes sure they are
  // answered with an error message, avoid time outs in WPTs.
  void WorkerMainScriptFetchingFailed();

  const GURL& scope() const { return scope_; }
  const base::UnguessableToken& devtools_worker_token() const {
    return devtools_worker_token_;
  }

  // If the ServiceWorker has been installed before the worker instance started,
  // it returns the time when the instance started. Otherwise returns the time
  // when the ServiceWorker was installed.
  base::Time version_installed_time() const { return version_installed_time_; }

  // Returns the time when the ServiceWorker was doomed.
  base::Time version_doomed_time() const { return version_doomed_time_; }

  int64_t version_id() const { return version_id_; }
  ServiceWorkerContextWrapper* context_wrapper() {
    return context_wrapper_.get();
  }

  bool should_pause_on_start() { return should_pause_on_start_; }
  void set_should_pause_on_start(bool should_pause_on_start);

 private:
  ~ServiceWorkerDevToolsAgentHost() override;
  void UpdateIsAttached(bool attached);
  void UpdateProcessHost();

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;
  protocol::TargetAutoAttacher* auto_attacher() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  void UpdateLoaderFactories(base::OnceClosure callback);

  std::unique_ptr<protocol::TargetAutoAttacher> auto_attacher_;

  enum WorkerState {
    WORKER_NOT_READY,
    WORKER_READY,
    WORKER_TERMINATED,
  };
  WorkerState state_;
  base::UnguessableToken devtools_worker_token_;
  int worker_process_id_;
  int worker_route_id_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  int64_t version_id_;
  GURL url_;
  GURL scope_;
  base::Time version_installed_time_;
  base::Time version_doomed_time_;

  // `should_pause_on_start_` is set by DevTools auto-attachers if any that
  // asked for execution to be paused so that they could attach asynchronously
  // to the new ServiceWorker target. If true, we throttle the main script fetch
  // and pause the renderer when starting.
  // Note: This is only used with PlzServiceWorker. If PlzServiceWorker is off,
  // this state is not stored but passed directly into the starting parameters
  // of the ServiceWorker as `should_wait_for_debugger`.
  bool should_pause_on_start_ = false;

  // The client security state of the linked service worker.
  // This is passed to network URL loader factories used for fetches initiated
  // by the service worker.
  network::mojom::ClientSecurityStatePtr client_security_state_;

  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;

  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      process_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
