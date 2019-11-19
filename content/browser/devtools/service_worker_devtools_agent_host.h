// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class BrowserContext;

class ServiceWorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  using List = std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using Map = std::map<std::string,
                       scoped_refptr<ServiceWorkerDevToolsAgentHost>>;

  ServiceWorkerDevToolsAgentHost(
      int worker_process_id,
      int worker_route_id,
      const ServiceWorkerContextCore* context,
      base::WeakPtr<ServiceWorkerContextCore> context_weak,
      int64_t version_id,
      const GURL& url,
      const GURL& scope,
      bool is_installed_version,
      const base::UnguessableToken& devtools_worker_token);

  // DevToolsAgentHost overrides.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  void WorkerRestarted(int worker_process_id, int worker_route_id);
  void WorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver);
  void WorkerDestroyed();
  void WorkerVersionInstalled();
  void WorkerVersionDoomed();

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

  bool Matches(const ServiceWorkerContextCore* context, int64_t version_id);

 private:
  ~ServiceWorkerDevToolsAgentHost() override;
  void UpdateIsAttached(bool attached);

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session) override;
  void DetachSession(DevToolsSession* session) override;

  void UpdateLoaderFactories(base::OnceClosure callback);

  enum WorkerState {
    WORKER_NOT_READY,
    WORKER_READY,
    WORKER_TERMINATED,
  };
  WorkerState state_;
  base::UnguessableToken devtools_worker_token_;
  int worker_process_id_;
  int worker_route_id_;
  const ServiceWorkerContextCore* context_;
  base::WeakPtr<ServiceWorkerContextCore> context_weak_;
  int64_t version_id_;
  GURL url_;
  GURL scope_;
  base::Time version_installed_time_;
  base::Time version_doomed_time_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
