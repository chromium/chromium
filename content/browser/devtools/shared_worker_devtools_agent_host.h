// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class SharedWorkerInstance;
class SharedWorkerHost;

class SharedWorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  using List = std::vector<scoped_refptr<SharedWorkerDevToolsAgentHost>>;

  SharedWorkerDevToolsAgentHost(
      SharedWorkerHost* worker_host,
      const base::UnguessableToken& devtools_worker_token);

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  bool Matches(SharedWorkerHost* worker_host);
  void WorkerReadyForInspection();
  void WorkerRestarted(SharedWorkerHost* worker_host);
  void WorkerDestroyed();

  const base::UnguessableToken& devtools_worker_token() const {
    return devtools_worker_token_;
  }

 private:
  ~SharedWorkerDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session,
                     TargetRegistry* registry) override;
  void DetachSession(DevToolsSession* session) override;
  void UpdateRendererChannel(bool force) override;

  enum WorkerState {
    WORKER_NOT_READY,
    WORKER_READY,
    WORKER_TERMINATED,
  };
  WorkerState state_;
  SharedWorkerHost* worker_host_;
  base::UnguessableToken devtools_worker_token_;
  std::unique_ptr<SharedWorkerInstance> instance_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_
