// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/headless_devtools_session.h"

#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "headless/lib/browser/protocol/browser_handler.h"
#include "headless/lib/browser/protocol/headless_handler.h"
#include "headless/lib/browser/protocol/page_handler.h"
#include "headless/lib/browser/protocol/target_handler.h"

namespace headless {
namespace protocol {

HeadlessDevToolsSession::HeadlessDevToolsSession(
    base::WeakPtr<HeadlessBrowserImpl> browser,
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client)
    : browser_(browser),
      agent_host_(agent_host),
      client_(client),
      dispatcher_(std::make_unique<UberDispatcher>(this)) {
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    AddHandler(std::make_unique<HeadlessHandler>(browser_,
                                                 agent_host->GetWebContents()));
    AddHandler(
        std::make_unique<PageHandler>(browser_, agent_host->GetWebContents()));
  }
  if (agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser)
    AddHandler(std::make_unique<BrowserHandler>(browser_));
  AddHandler(std::make_unique<TargetHandler>(browser_));
}

HeadlessDevToolsSession::~HeadlessDevToolsSession() {
  dispatcher_.reset();
  for (auto& pair : handlers_)
    pair.second->Disable();
  handlers_.clear();
}

void HeadlessDevToolsSession::HandleCommand(
    std::unique_ptr<base::DictionaryValue> command,
    const std::string& message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  if (!browser_) {
    std::move(callback).Run(std::move(command), message);
    return;
  }
  int call_id;
  std::string method;
  std::unique_ptr<protocol::Value> protocolCommand =
      protocol::toProtocolValue(command.get(), 1000);
  if (!dispatcher_->parseCommand(protocolCommand.get(), &call_id, &method)) {
    return;
  }
  if (dispatcher_->canDispatch(method)) {
    pending_commands_[call_id] =
        std::make_pair(std::move(callback), std::move(command));
    dispatcher_->dispatch(call_id, method, std::move(protocolCommand), message);
    return;
  }
  std::move(callback).Run(std::move(command), message);
}
void HeadlessDevToolsSession::AddHandler(
    std::unique_ptr<protocol::DomainHandler> handler) {
  handler->Wire(dispatcher_.get());
  handlers_[handler->name()] = std::move(handler);
}

void HeadlessDevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<Serializable> message) {
  pending_commands_.erase(call_id);
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
}

void HeadlessDevToolsSession::fallThrough(int call_id,
                                          const std::string& method,
                                          const std::string& message) {
  PendingCommand command = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(command.first).Run(std::move(command.second), message);
}

void HeadlessDevToolsSession::sendProtocolNotification(
    std::unique_ptr<Serializable> message) {
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
}

void HeadlessDevToolsSession::flushProtocolNotifications() {}

}  // namespace protocol
}  // namespace headless
