// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/public/browser/shared_worker_instance.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class SharedWorkerHost;

class SharedWorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  using List = std::vector<scoped_refptr<SharedWorkerDevToolsAgentHost>>;

  static SharedWorkerDevToolsAgentHost* GetFor(SharedWorkerHost* worker_host);

  SharedWorkerDevToolsAgentHost(
      SharedWorkerHost* worker_host,
      const base::UnguessableToken& devtools_worker_token);

  SharedWorkerDevToolsAgentHost(const SharedWorkerDevToolsAgentHost&) = delete;
  SharedWorkerDevToolsAgentHost& operator=(
      const SharedWorkerDevToolsAgentHost&) = delete;

  // DevToolsAgentHost override.
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
  protocol::TargetAutoAttacher* auto_attacher() override;

  blink::StorageKey GetStorageKey() const;

  bool Matches(SharedWorkerHost* worker_host);
  void WorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver);
  void WorkerRestarted(SharedWorkerHost* worker_host);
  void WorkerDestroyed();

  const base::UnguessableToken& devtools_worker_token() const {
    return devtools_worker_token_;
  }

 private:
  ~SharedWorkerDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;

  std::unique_ptr<protocol::TargetAutoAttacher> auto_attacher_;

  enum WorkerState {
    WORKER_NOT_READY,
    WORKER_READY,
    WORKER_TERMINATED,
  };
  WorkerState state_;
  raw_ptr<SharedWorkerHost> worker_host_;
  base::UnguessableToken devtools_worker_token_;
  SharedWorkerInstance instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_
