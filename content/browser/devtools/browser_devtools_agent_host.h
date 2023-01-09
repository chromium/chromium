// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include "base/task/single_thread_task_runner.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class BrowserDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  // TODO(caseq,dgozman): this should probably be a singleton.
  static const std::set<BrowserDevToolsAgentHost*>& Instances();

 private:
  friend class DevToolsAgentHost;
  class BrowserAutoAttacher;

  BrowserDevToolsAgentHost(
      scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
      const CreateServerSocketCallback& socket_callback,
      bool only_discovery);
  ~BrowserDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;
  protocol::TargetAutoAttacher* auto_attacher() override;

  // DevToolsAgentHost implementation.
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  std::unique_ptr<BrowserAutoAttacher> auto_attacher_;
  scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner_;
  CreateServerSocketCallback socket_callback_;
  bool only_discovery_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
