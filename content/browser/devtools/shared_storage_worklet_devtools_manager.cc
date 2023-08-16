// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_storage_worklet_devtools_manager.h"

#include "base/ranges/algorithm.h"
#include "content/browser/devtools/shared_storage_worklet_devtools_agent_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
SharedStorageWorkletDevToolsManager*
SharedStorageWorkletDevToolsManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<SharedStorageWorkletDevToolsManager>::get();
}

void SharedStorageWorkletDevToolsManager::AddAllAgentHosts(
    std::vector<scoped_refptr<DevToolsAgentHost>>* result) {
  for (auto& it : hosts_) {
    result->push_back(it.second.get());
  }
}

void SharedStorageWorkletDevToolsManager::WorkletCreated(
    SharedStorageWorkletHost& worklet_host,
    const base::UnguessableToken& devtools_worklet_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(hosts_.find(&worklet_host) == hosts_.end());

  hosts_[&worklet_host] = MakeRefCounted<SharedStorageWorkletDevToolsAgentHost>(
      worklet_host, devtools_worklet_token);
}

void SharedStorageWorkletDevToolsManager::WorkletReadyForInspection(
    SharedStorageWorkletHost& worklet_host,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
        agent_host_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  hosts_.at(&worklet_host)
      ->WorkletReadyForInspection(std::move(agent_remote),
                                  std::move(agent_host_receiver));
}

void SharedStorageWorkletDevToolsManager::WorkletDestroyed(
    SharedStorageWorkletHost& worklet_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = hosts_.find(&worklet_host);
  CHECK(it != hosts_.end());

  it->second->WorkletDestroyed();
  hosts_.erase(it);
}

SharedStorageWorkletDevToolsManager::SharedStorageWorkletDevToolsManager() =
    default;
SharedStorageWorkletDevToolsManager::~SharedStorageWorkletDevToolsManager() =
    default;

}  // namespace content
