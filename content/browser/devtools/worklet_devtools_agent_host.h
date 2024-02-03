// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKLET_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKLET_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/worker_or_worklet_devtools_agent_host.h"

namespace content {

class WorkletDevToolsAgentHost final : public WorkerOrWorkletDevToolsAgentHost {
 public:
  WorkletDevToolsAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

 private:
  ~WorkletDevToolsAgentHost() override;

  // DevToolsAgentHost overrides
  std::string GetType() override;

  // DevToolsAgentHostImpl overrides
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKLET_DEVTOOLS_AGENT_HOST_H_
