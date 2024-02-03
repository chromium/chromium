// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worklet_devtools_agent_host.h"

namespace content {

WorkletDevToolsAgentHost::WorkletDevToolsAgentHost(
    int process_id,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& devtools_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback)
    : WorkerOrWorkletDevToolsAgentHost(process_id,
                                       url,
                                       name,
                                       devtools_worker_token,
                                       parent_id,
                                       std::move(destroyed_callback)) {
  NotifyCreated();
}

WorkletDevToolsAgentHost::~WorkletDevToolsAgentHost() = default;

std::string WorkletDevToolsAgentHost::GetType() {
  return kTypeWorklet;
}

bool WorkletDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                             bool acquire_wake_lock) {
  // Default implementation returns false, blocking the session -- hence
  // the override.
  return true;
}

}  // namespace content
