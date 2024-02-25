// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_PUBLIC_TEST_MOCK_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_DEVTOOLS_AGENT_HOST_H_

#include "content/public/browser/devtools_agent_host.h"

namespace content {

class MockDevToolsAgentHost : public content::DevToolsAgentHost {
 public:
  MockDevToolsAgentHost() = default;

  // content::DevToolsAgentHost
  std::string CreateIOStreamFromData(
      scoped_refptr<base::RefCountedMemory>) override;
  bool AttachClient(content::DevToolsAgentHostClient* client) override;
  bool AttachClientWithoutWakeLock(
      content::DevToolsAgentHostClient* client) override;
  bool DetachClient(content::DevToolsAgentHostClient* client) override;
  bool IsAttached() override;
  void DispatchProtocolMessage(content::DevToolsAgentHostClient* client,
                               base::span<const uint8_t> message) override;
  void InspectElement(content::RenderFrameHost* frame_host,
                      int x,
                      int y) override {}
  std::string GetId() override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  bool CanAccessOpener() override;
  std::string GetOpenerFrameId() override;
  content::WebContents* GetWebContents() override;
  content::BrowserContext* GetBrowserContext() override;
  void DisconnectWebContents() override {}
  void ConnectWebContents(content::WebContents* web_contents) override {}
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  bool Activate() override;
  void Reload() override {}
  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;
  content::RenderProcessHost* GetProcessHost() override;
  void ForceDetachAllSessions() override {}

 protected:
  ~MockDevToolsAgentHost() override = default;

  raw_ptr<content::DevToolsAgentHostClient> client_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_DEVTOOLS_AGENT_HOST_H_
