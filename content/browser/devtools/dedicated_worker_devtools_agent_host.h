// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/worker_or_worklet_devtools_agent_host.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace content {

namespace protocol {
class TargetAutoAttacher;
}  // namespace protocol

class DedicatedWorkerHost;

class DedicatedWorkerDevToolsAgentHost final
    : public WorkerOrWorkletDevToolsAgentHost {
 public:
  static DedicatedWorkerDevToolsAgentHost* GetFor(
      const DedicatedWorkerHost* host);

  DedicatedWorkerDevToolsAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

 private:
  ~DedicatedWorkerDevToolsAgentHost() override;

  // DevToolsAgentHost overrides
  std::string GetType() override;

  // DevToolsAgentHostImpl overrides
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  protocol::TargetAutoAttacher* auto_attacher() override;
  std::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

  DedicatedWorkerHost* GetDedicatedWorkerHost();

  std::unique_ptr<protocol::TargetAutoAttacher> const auto_attacher_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_
