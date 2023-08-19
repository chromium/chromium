// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "content/public/browser/devtools_agent_host.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class SharedStorageWorkletDevToolsAgentHost;
class SharedStorageWorkletHost;

// Manages `SharedStorageWorkletDevToolsAgentHost`s for Shared Storage Worklets.
class SharedStorageWorkletDevToolsManager {
 public:
  // Returns the SharedStorageWorkletDevToolsManager singleton.
  static SharedStorageWorkletDevToolsManager* GetInstance();

  SharedStorageWorkletDevToolsManager(
      const SharedStorageWorkletDevToolsManager&) = delete;
  SharedStorageWorkletDevToolsManager& operator=(
      const SharedStorageWorkletDevToolsManager&) = delete;

  void AddAllAgentHosts(std::vector<scoped_refptr<DevToolsAgentHost>>* result);

  void WorkletCreated(SharedStorageWorkletHost& worklet_host,
                      const base::UnguessableToken& devtools_worklet_token);
  void WorkletReadyForInspection(
      SharedStorageWorkletHost& worklet_host,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver);
  void WorkletDestroyed(SharedStorageWorkletHost& worklet_host);

 private:
  friend struct base::DefaultSingletonTraits<
      SharedStorageWorkletDevToolsManager>;

  SharedStorageWorkletDevToolsManager();
  ~SharedStorageWorkletDevToolsManager();

  // We retatin agent hosts as long as the shared storage worklet is alive.
  std::map<SharedStorageWorkletHost*,
           scoped_refptr<SharedStorageWorkletDevToolsAgentHost>>
      hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_H_
