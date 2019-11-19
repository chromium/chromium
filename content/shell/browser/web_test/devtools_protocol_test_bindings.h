// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_DEVTOOLS_PROTOCOL_TEST_BINDINGS_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_DEVTOOLS_PROTOCOL_TEST_BINDINGS_H_

#include <memory>
#include <string>

#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHost;
class DevToolsFrontendHost;

class DevToolsProtocolTestBindings : public WebContentsObserver,
                                     public DevToolsAgentHostClient {
 public:
  explicit DevToolsProtocolTestBindings(WebContents* devtools);
  ~DevToolsProtocolTestBindings() override;
  static GURL MapTestURLIfNeeded(const GURL& test_url, bool* is_protocol_test);

 private:
  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;

  // WebContentsObserver overrides
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  void HandleMessageFromTest(const std::string& message);

  scoped_refptr<DevToolsAgentHost> agent_host_;
#if !defined(OS_ANDROID)
  // DevToolsFrontendHost does not exist on Android, but we also don't run web
  // tests natively on Android.
  std::unique_ptr<DevToolsFrontendHost> frontend_host_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DevToolsProtocolTestBindings);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_DEVTOOLS_PROTOCOL_TEST_BINDINGS_H_
