// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "content/public/browser/devtools_agent_host.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class SharedWorkerDevToolsAgentHost;
class SharedWorkerHost;

// Manages WorkerDevToolsAgentHost's for Shared Workers.
// This class lives on UI thread.
class SharedWorkerDevToolsManager {
 public:
  // Returns the SharedWorkerDevToolsManager singleton.
  static SharedWorkerDevToolsManager* GetInstance();

  SharedWorkerDevToolsManager(const SharedWorkerDevToolsManager&) = delete;
  SharedWorkerDevToolsManager& operator=(const SharedWorkerDevToolsManager&) =
      delete;

  void AddAllAgentHosts(
      std::vector<scoped_refptr<SharedWorkerDevToolsAgentHost>>* result);
  void AgentHostDestroyed(SharedWorkerDevToolsAgentHost* agent_host);

  void WorkerCreated(SharedWorkerHost* worker_host,
                     bool* pause_on_start,
                     base::UnguessableToken* devtools_worker_token);
  void WorkerReadyForInspection(
      SharedWorkerHost* worker_host,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver);
  void WorkerDestroyed(SharedWorkerHost* worker_host);

  SharedWorkerDevToolsAgentHost* GetDevToolsHost(SharedWorkerHost* host);

 private:
  friend struct base::DefaultSingletonTraits<SharedWorkerDevToolsManager>;

  SharedWorkerDevToolsManager();
  ~SharedWorkerDevToolsManager();

  // We retatin agent hosts as long as the shared worker is alive.
  std::map<SharedWorkerHost*, scoped_refptr<SharedWorkerDevToolsAgentHost>>
      live_hosts_;
  // Clients may retain agent host for the terminated shared worker,
  // and we reconnect them when shared worker is restarted.
  base::flat_set<raw_ptr<SharedWorkerDevToolsAgentHost>> terminated_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
