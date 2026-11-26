// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_OR_WORKLET_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_OR_WORKLET_DEVTOOLS_AGENT_HOST_H_

#include <map>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"

namespace content {

// This is a base class for dedicated (but not shared or service) workers and
// for common worklets. See DedicatedWorkerDevToolsAgentHost and
// WorkletDevToolsAgentHost for concrete implementation.
class CONTENT_EXPORT WorkerOrWorkletDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      public RenderProcessHostObserver {
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

  // This is only ever used for the DedicatedWorkerToolsAgentHost (the subclass)
  // and it should be moved to the subclass.
  void ChildWorkerCreated(
      const GURL& url,
      const std::string& name,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> callback);

  const base::UnguessableToken& devtools_worker_token() const {
    return devtools_worker_token_;
  }

  // Keeps the network conditions that the attached sessions emulate after the
  // sessions detach. Call this before a forced detach of the sessions of a
  // worker that continues to run. The conditions are cleared when the worker
  // disconnects, when a new session attaches, or when the client that set them
  // disconnects. Only dedicated workers have sessions with network conditions
  // (see DedicatedWorkerDevToolsAgentHost::AttachSession()), so this does
  // nothing for worklets.
  void PreserveNetworkConditions();
  // Clears the network conditions that PreserveNetworkConditions() kept.
  void ClearPreservedNetworkConditions();
  // Clears the preserved network conditions that belong to `root_session`.
  void ClearPreservedNetworkConditionsForRootSession(
      DevToolsSession* root_session);

 protected:
  WorkerOrWorkletDevToolsAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

  ~WorkerOrWorkletDevToolsAgentHost() override;

  void Disconnected();

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

 private:
  const base::UnguessableToken devtools_worker_token_;
  const std::string parent_id_;
  const int process_id_;

  GURL url_;
  std::string name_;
  base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback_;
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      process_observation_{this};
  scoped_refptr<WorkerOrWorkletDevToolsAgentHost> self_keepalive_;

  // Network conditions that outlive the session that set them.
  struct PreservedNetworkConditions {
    // The root session of the client that set the conditions.
    raw_ptr<DevToolsSession> root_session;
    // Clears the conditions.
    base::OnceClosure cleanup;
    // Clears the conditions when the client that set them disconnects.
    base::CallbackListSubscription client_subscription;
  };
  void ClearPreservedNetworkConditionsOfClient(int id);

  std::map<int, PreservedNetworkConditions> preserved_network_conditions_;
  int next_preserved_network_conditions_id_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_OR_WORKLET_DEVTOOLS_AGENT_HOST_H_
