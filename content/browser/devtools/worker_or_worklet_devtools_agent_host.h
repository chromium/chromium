// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_OR_WORKLET_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_OR_WORKLET_DEVTOOLS_AGENT_HOST_H_

#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"

namespace content {

// This is a base class for dedicated (but not shared or service) workers and
// for common worklets. See DedicatedWorkerDevToolsAgentHost and
// WorkletDevToolsAgentHost for concrete implementation.
class WorkerOrWorkletDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  WorkerOrWorkletDevToolsAgentHost(
      const WorkerOrWorkletDevToolsAgentHost&) = delete;
  WorkerOrWorkletDevToolsAgentHost& operator=(
      const WorkerOrWorkletDevToolsAgentHost&) = delete;

  // DevToolsAgentHost overrides.
  BrowserContext* GetBrowserContext() override;
  RenderProcessHost* GetProcessHost() override;
  std::string GetTitle() override;
  std::string GetParentId() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

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

 protected:
  WorkerOrWorkletDevToolsAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

  ~WorkerOrWorkletDevToolsAgentHost() override;

 private:
  void Disconnected();

  const base::UnguessableToken devtools_worker_token_;
  const std::string parent_id_;
  const int process_id_;

  GURL url_;
  std::string name_;
  base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_OR_WORKLET_DEVTOOLS_AGENT_HOST_H_
