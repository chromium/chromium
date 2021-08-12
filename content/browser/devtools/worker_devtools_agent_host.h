// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"

namespace content {

class DedicatedWorkerHost;

// The WorkerDevToolsAgentHost is the devtools host class for dedicated workers,
// (but not shared or service workers), and worklets. It does not have a pointer
// to a DedicatedWorkerHost object, but in case the host is for a dedicated
// worker (and not a worklet) then the devtools_worker_token_ is identical to
// the DedicatedWorkerToken of the dedicated worker.
class WorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  WorkerDevToolsAgentHost(
      int process_id,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetParentId() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  absl::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

 private:
  ~WorkerDevToolsAgentHost() override;
  void Disconnected();
  DedicatedWorkerHost* GetDedicatedWorkerHost();

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;
  protocol::TargetAutoAttacher* auto_attacher() override;

  const int process_id_;
  const GURL url_;
  const std::string name_;
  const std::string parent_id_;
  std::unique_ptr<protocol::TargetAutoAttacher> auto_attacher_;
  base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback_;
  const base::UnguessableToken devtools_worker_token_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
