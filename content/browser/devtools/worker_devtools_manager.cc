// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_manager.h"

#include "base/containers/contains.h"
#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
WorkerDevToolsManager& WorkerDevToolsManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  return *base::Singleton<WorkerDevToolsManager>::get();
}

WorkerDevToolsManager::WorkerDevToolsManager() = default;
WorkerDevToolsManager::~WorkerDevToolsManager() = default;

DedicatedWorkerDevToolsAgentHost* WorkerDevToolsManager::GetDevToolsHost(
    const DedicatedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = hosts_.find(host);
  return it == hosts_.end() ? nullptr : it->second.get();
}

DedicatedWorkerDevToolsAgentHost*
WorkerDevToolsManager::GetDevToolsHostFromToken(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& it : hosts_) {
    if (it.second->devtools_worker_token() == token) {
      return it.second.get();
    }
  }

  return nullptr;
}

void WorkerDevToolsManager::WorkerCreated(
    const DedicatedWorkerHost* host,
    int process_id,
    const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::Contains(hosts_, host));

  hosts_[host] = base::MakeRefCounted<DedicatedWorkerDevToolsAgentHost>(
      process_id,
      /*url=*/GURL(), /*name=*/"", host->GetToken().value(), /*parent_id=*/"",
      /*destroyed_callback=*/base::DoNothing());

  devtools_instrumentation::ThrottleWorkerMainScriptFetch(
      host->GetToken().value(), ancestor_render_frame_host_id,
      std::move(throttle_handle));
}

void WorkerDevToolsManager::WorkerDestroyed(const DedicatedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  hosts_.erase(host);
}

}  // namespace content
