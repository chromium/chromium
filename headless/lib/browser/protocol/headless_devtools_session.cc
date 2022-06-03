// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/headless_devtools_session.h"

#include "base/command_line.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "headless/lib/browser/protocol/browser_handler.h"
#include "headless/lib/browser/protocol/headless_handler.h"
#include "headless/lib/browser/protocol/page_handler.h"
#include "headless/lib/browser/protocol/target_handler.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace headless {
namespace protocol {
HeadlessDevToolsSession::HeadlessDevToolsSession(
    base::WeakPtr<HeadlessBrowserImpl> browser,
    content::DevToolsAgentHostClientChannel* channel)
    : browser_(browser), dispatcher_(this), client_channel_(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    AddHandler(std::make_unique<HeadlessHandler>(browser_.get(),
                                                 agent_host->GetWebContents()));
    AddHandler(std::make_unique<PageHandler>(agent_host,
                                             agent_host->GetWebContents()));
  }
  if (channel->GetClient()->MayAttachToBrowser()) {
    AddHandler(
        std::make_unique<BrowserHandler>(browser_.get(), agent_host->GetId()));
  }
  AddHandler(std::make_unique<TargetHandler>(browser_.get()));
}

HeadlessDevToolsSession::~HeadlessDevToolsSession() {
  for (auto& handler : handlers_)
    handler->Disable();
}

void HeadlessDevToolsSession::HandleCommand(
    base::span<const uint8_t> message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  if (!browser_) {
    std::move(callback).Run(message);
    return;
  }
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  // content::DevToolsSession receives this message first, so we may
  // assume it's ok.
  DCHECK(dispatchable.ok());
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_.Dispatch(dispatchable);
  if (!dispatched.MethodFound()) {
    std::move(callback).Run(message);
    return;
  }
  pending_commands_[dispatchable.CallId()] = std::move(callback);
  dispatched.Run();
}

void HeadlessDevToolsSession::AddHandler(
    std::unique_ptr<protocol::DomainHandler> handler) {
  handler->Wire(&dispatcher_);
  handlers_.push_back(std::move(handler));
}

// The following methods handle responses or notifications coming from
// the browser to the client.

void HeadlessDevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<Serializable> message) {
  pending_commands_.erase(call_id);

  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void HeadlessDevToolsSession::SendProtocolNotification(
    std::unique_ptr<Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void HeadlessDevToolsSession::FlushProtocolNotifications() {}

void HeadlessDevToolsSession::FallThrough(int call_id,
                                          crdtp::span<uint8_t> method,
                                          crdtp::span<uint8_t> message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
}  // namespace protocol
}  // namespace headless
