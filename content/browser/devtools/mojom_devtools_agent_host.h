// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_MOJOM_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_MOJOM_DEVTOOLS_AGENT_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class MojomDevToolsAgentHostDelegate;

// Agent host that implements a basic blink::mojom::DevToolsAgent flow using a
// delegate.
class MojomDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  static void GetAll(DevToolsAgentHost::List* out_list);

  MojomDevToolsAgentHost(
      const std::string& id,
      std::unique_ptr<MojomDevToolsAgentHostDelegate> delegate);

  MojomDevToolsAgentHost(const MojomDevToolsAgentHost&) = delete;
  MojomDevToolsAgentHost& operator=(const MojomDevToolsAgentHost&) = delete;

 private:
  ~MojomDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides:
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;

  // DevToolsAgentHost overrides:
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  void Reload() override;

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> associated_agent_remote_;
  std::unique_ptr<MojomDevToolsAgentHostDelegate> delegate_;

  static std::vector<std::string>& host_ids();
};
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_MOJOM_DEVTOOLS_AGENT_HOST_H_
