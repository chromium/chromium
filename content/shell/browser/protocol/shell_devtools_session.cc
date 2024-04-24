// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/protocol/shell_devtools_session.h"

#include "base/command_line.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/shell/browser/protocol/browser_handler.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace content::shell::protocol {
ShellDevToolsSession::ShellDevToolsSession(
    raw_ref<BrowserContext> browser_context,
    content::DevToolsAgentHostClientChannel* channel)
    : browser_context_(browser_context),
      dispatcher_(this),
      client_channel_(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  AddHandler(
      std::make_unique<BrowserHandler>(browser_context_, agent_host->GetId()));
}

ShellDevToolsSession::~ShellDevToolsSession() {
  for (std::unique_ptr<DomainHandler>& handler : handlers_) {
    handler->Disable();
  }
}

void ShellDevToolsSession::HandleCommand(
    base::span<const uint8_t> message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  // content::DevToolsSession receives this message first, so we may
  // assume it's ok.
  CHECK(dispatchable.ok());
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_.Dispatch(dispatchable);
  if (!dispatched.MethodFound()) {
    std::move(callback).Run(message);
    return;
  }
  pending_commands_[dispatchable.CallId()] = std::move(callback);
  dispatched.Run();
}

void ShellDevToolsSession::AddHandler(
    std::unique_ptr<protocol::DomainHandler> handler) {
  handler->Wire(&dispatcher_);
  handlers_.push_back(std::move(handler));
}

// The following methods handle responses or notifications coming from
// the browser to the client.

void ShellDevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<Serializable> message) {
  pending_commands_.erase(call_id);

  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ShellDevToolsSession::SendProtocolNotification(
    std::unique_ptr<Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ShellDevToolsSession::FlushProtocolNotifications() {}

void ShellDevToolsSession::FallThrough(int call_id,
                                       crdtp::span<uint8_t> method,
                                       crdtp::span<uint8_t> message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
}  // namespace content::shell::protocol
