// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"

namespace content {

class WorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  WorkerDevToolsAgentHost(
      int process_id,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetParentId() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

 private:
  ~WorkerDevToolsAgentHost() override;
  void Disconnected();

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session) override;
  void DetachSession(DevToolsSession* session) override;

  const int process_id_;
  const GURL url_;
  const std::string name_;
  const std::string parent_id_;
  base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_
