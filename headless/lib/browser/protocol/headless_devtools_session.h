// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_DEVTOOLS_SESSION_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_DEVTOOLS_SESSION_H_

#include <memory>

#include "base/values.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "headless/lib/browser/protocol/forward.h"
#include "headless/lib/browser/protocol/protocol.h"

namespace headless {
class HeadlessBrowserImpl;
class UberDispatcher;

namespace protocol {

class DomainHandler;

class HeadlessDevToolsSession : public FrontendChannel {
 public:
  HeadlessDevToolsSession(base::WeakPtr<HeadlessBrowserImpl> browser,
                          content::DevToolsAgentHostClientChannel* channel);
  ~HeadlessDevToolsSession() override;

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

  base::WeakPtr<HeadlessBrowserImpl> browser_;
  UberDispatcher dispatcher_;
  std::vector<std::unique_ptr<DomainHandler>> handlers_;
  base::flat_map<int, content::DevToolsManagerDelegate::NotHandledCallback>
      pending_commands_;
  content::DevToolsAgentHostClientChannel* client_channel_;
  DISALLOW_COPY_AND_ASSIGN(HeadlessDevToolsSession);
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_DEVTOOLS_SESSION_H_
