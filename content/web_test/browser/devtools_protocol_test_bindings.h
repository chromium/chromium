// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_DEVTOOLS_PROTOCOL_TEST_BINDINGS_H_
#define CONTENT_WEB_TEST_BROWSER_DEVTOOLS_PROTOCOL_TEST_BINDINGS_H_

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHost;
class DevToolsFrontendHost;

class DevToolsProtocolTestBindings : public WebContentsObserver,
                                     public DevToolsAgentHostClient {
 public:
  explicit DevToolsProtocolTestBindings(WebContents* devtools, std::string log);

  DevToolsProtocolTestBindings(const DevToolsProtocolTestBindings&) = delete;
  DevToolsProtocolTestBindings& operator=(const DevToolsProtocolTestBindings&) =
      delete;

  ~DevToolsProtocolTestBindings() override;
  static GURL MapTestURLIfNeeded(const GURL& test_url, bool* is_protocol_test);

 private:
  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  bool AllowUnsafeOperations() override;

  // WebContentsObserver overrides
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  void ParseLog(std::string_view log);
  void HandleMessagesFromLog(std::string_view protocol_message_string);
  void HandleMessageFromTest(base::Value::Dict message);

  scoped_refptr<DevToolsAgentHost> agent_host_;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // DevToolsFrontendHost does not exist on Android and iOS, but we also don't
  // run web tests natively on Android.
  std::unique_ptr<DevToolsFrontendHost> frontend_host_;
#endif
  // Log of protocol messages, used to script the bindings behavior.
  std::vector<base::Value::Dict> log_;
  // The index of the next message in the log.
  size_t log_pos_ = 0;
  // If true, the binding is using the log instead of sending real messages.
  // The log is enabled if a non-empty log is provided via the constructor.
  bool log_enabled_ = false;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_DEVTOOLS_PROTOCOL_TEST_BINDINGS_H_
