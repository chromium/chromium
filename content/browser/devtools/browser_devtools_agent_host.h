// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include "base/containers/flat_map.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class BrowserDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  // TODO(caseq,dgozman): this should probably be a singleton.
  static const std::set<BrowserDevToolsAgentHost*>& Instances();

 private:
  friend class DevToolsAgentHost;
  BrowserDevToolsAgentHost(
      scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
      const CreateServerSocketCallback& socket_callback,
      bool only_discovery);
  ~BrowserDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;

  // DevToolsAgentHost implementation.
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner_;
  CreateServerSocketCallback socket_callback_;
  bool only_discovery_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
