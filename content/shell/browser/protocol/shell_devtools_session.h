// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_PROTOCOL_SHELL_DEVTOOLS_SESSION_H_
#define CONTENT_SHELL_BROWSER_PROTOCOL_SHELL_DEVTOOLS_SESSION_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/shell/browser/protocol/protocol.h"

namespace content {
class BrowserContext;

namespace shell::protocol {

class DomainHandler;

class ShellDevToolsSession : public FrontendChannel {
 public:
  ShellDevToolsSession(raw_ref<BrowserContext> browser_context,
                       content::DevToolsAgentHostClientChannel* channel);

  ShellDevToolsSession(const ShellDevToolsSession&) = delete;
  ShellDevToolsSession& operator=(const ShellDevToolsSession&) = delete;

  ~ShellDevToolsSession() override;

  void HandleCommand(
      base::span<const uint8_t> message,
      content::DevToolsManagerDelegate::NotHandledCallback callback);

 private:
  void AddHandler(std::unique_ptr<DomainHandler> handler);

  // FrontendChannel:
  void SendProtocolResponse(int call_id,
                            std::unique_ptr<Serializable> message) override;
  void SendProtocolNotification(std::unique_ptr<Serializable> message) override;
  void FlushProtocolNotifications() override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  const raw_ref<const BrowserContext> browser_context_;
  UberDispatcher dispatcher_;
  std::vector<std::unique_ptr<DomainHandler>> handlers_;
  base::flat_map<int, content::DevToolsManagerDelegate::NotHandledCallback>
      pending_commands_;
  raw_ptr<content::DevToolsAgentHostClientChannel> client_channel_;
};

}  // namespace shell::protocol
}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_PROTOCOL_SHELL_DEVTOOLS_SESSION_H_
