// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/memory/singleton.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_throttle_handle.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class DedicatedWorkerDevToolsAgentHost;
class DedicatedWorkerHost;

// Manages WorkerDevToolsAgentHost's for Dedicated Workers. This class lives on
// UI thread. This is only used for PlzDedicatedWorker.
class WorkerDevToolsManager {
 public:
  // Returns the WorkerDevToolsManager singleton.
  static WorkerDevToolsManager& GetInstance();

  DedicatedWorkerDevToolsAgentHost* GetDevToolsHost(
      const DedicatedWorkerHost* host);
  DedicatedWorkerDevToolsAgentHost* GetDevToolsHostFromToken(
      const base::UnguessableToken& token);
  void WorkerCreated(
      const DedicatedWorkerHost* host,
      int process_id,
      const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
      scoped_refptr<DevToolsThrottleHandle> throttle_handle);
  void WorkerDestroyed(const DedicatedWorkerHost* host);

 private:
  friend struct base::DefaultSingletonTraits<WorkerDevToolsManager>;

  WorkerDevToolsManager();
  ~WorkerDevToolsManager();

  // Retains agent hosts as long as the dedicated worker is alive.
  std::map<const DedicatedWorkerHost*,
           scoped_refptr<DedicatedWorkerDevToolsAgentHost>>
      hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_
