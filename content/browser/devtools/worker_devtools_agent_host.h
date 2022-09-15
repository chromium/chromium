// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_

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
  static WorkerDevToolsAgentHost* GetFor(DedicatedWorkerHost* host);

  WorkerDevToolsAgentHost(
      int process_id,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

  WorkerDevToolsAgentHost(const WorkerDevToolsAgentHost&) = delete;
  WorkerDevToolsAgentHost& operator=(const WorkerDevToolsAgentHost&) = delete;

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;
  RenderProcessHost* GetProcessHost() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetParentId() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  absl::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

  void SetRenderer(
      int process_id,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver);

  void ChildWorkerCreated(
      const GURL& url,
      const std::string& name,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> callback);

  const base::UnguessableToken& devtools_worker_token() const {
    return devtools_worker_token_;
  }

 private:
  ~WorkerDevToolsAgentHost() override;
  void Disconnected();
  DedicatedWorkerHost* GetDedicatedWorkerHost();

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;
  protocol::TargetAutoAttacher* auto_attacher() override;

  const int process_id_;
  GURL url_;
  std::string name_;
  const std::string parent_id_;
  std::unique_ptr<protocol::TargetAutoAttacher> auto_attacher_;
  base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback_;
  const base::UnguessableToken devtools_worker_token_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
