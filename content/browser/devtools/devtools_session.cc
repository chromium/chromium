// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

namespace {

bool ShouldSendOnIO(const std::string& method) {
  // Keep in sync with WebDevToolsAgent::ShouldInterruptForMethod.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics" || method == "Page.crash" ||
         method == "Runtime.terminateExecution" ||
         method == "Emulation.setScriptExecutionDisabled";
}

}  // namespace

DevToolsSession::DevToolsSession(DevToolsAgentHostImpl* agent_host,
                                 DevToolsAgentHostClient* client)
    : binding_(this),
      agent_host_(agent_host),
      client_(client),
      dispatcher_(new protocol::UberDispatcher(this)),
      weak_factory_(this) {}

DevToolsSession::~DevToolsSession() {
  // It is Ok for session to be deleted without the dispose -
  // it can be kicked out by an extension connect / disconnect.
  if (dispatcher_)
    Dispose();
}

void DevToolsSession::Dispose() {
  dispatcher_.reset();
  for (auto& pair : handlers_)
    pair.second->Disable();
  handlers_.clear();
}

void DevToolsSession::AddHandler(
    std::unique_ptr<protocol::DevToolsDomainHandler> handler) {
  handler->Wire(dispatcher_.get());
  handlers_[handler->name()] = std::move(handler);
}

void DevToolsSession::SetBrowserOnly(bool browser_only) {
  browser_only_ = browser_only;
}

void DevToolsSession::AttachToAgent(blink::mojom::DevToolsAgent* agent) {
  if (!agent) {
    binding_.Close();
    session_ptr_.reset();
    io_session_ptr_.reset();
    return;
  }

  blink::mojom::DevToolsSessionHostAssociatedPtrInfo host_ptr_info;
  binding_.Bind(mojo::MakeRequest(&host_ptr_info));
  agent->AttachDevToolsSession(
      std::move(host_ptr_info), mojo::MakeRequest(&session_ptr_),
      mojo::MakeRequest(&io_session_ptr_), session_state_cookie_.Clone());
  session_ptr_.set_connection_error_handler(base::BindOnce(
      &DevToolsSession::MojoConnectionDestroyed, base::Unretained(this)));

  if (!suspended_sending_messages_to_agent_) {
    for (const auto& pair : waiting_for_response_messages_) {
      int call_id = pair.first;
      const WaitingMessage& message = pair.second;
      DispatchProtocolMessageToAgent(call_id, message.method, message.message);
    }
  } else {
    std::vector<SuspendedMessage> temp;
    for (const auto& pair : waiting_for_response_messages_)
      temp.push_back({pair.first, pair.second.method, pair.second.message});
    suspended_messages_.insert(suspended_messages_.begin(), temp.begin(),
                               temp.end());
    waiting_for_response_messages_.clear();
  }

  // Set cookie to an empty struct to reattach next time instead of attaching.
  if (!session_state_cookie_)
    session_state_cookie_ = blink::mojom::DevToolsSessionState::New();
}

void DevToolsSession::SendResponse(
    std::unique_ptr<base::DictionaryValue> response) {
  std::string json;
  base::JSONWriter::Write(*response.get(), &json);
  client_->DispatchProtocolMessage(agent_host_, json);
  // |this| may be deleted at this point.
}

void DevToolsSession::MojoConnectionDestroyed() {
  binding_.Close();
  session_ptr_.reset();
  io_session_ptr_.reset();
}

void DevToolsSession::DispatchProtocolMessage(
    const std::string& message,
    std::unique_ptr<base::DictionaryValue> parsed_message) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (delegate && parsed_message) {
    delegate->HandleCommand(agent_host_, client_, std::move(parsed_message),
                            message,
                            base::BindOnce(&DevToolsSession::HandleCommand,
                                           weak_factory_.GetWeakPtr()));
  } else {
    HandleCommand(std::move(parsed_message), message);
  }
}

void DevToolsSession::HandleCommand(
    std::unique_ptr<base::DictionaryValue> parsed_message,
    const std::string& message) {
  std::unique_ptr<protocol::Value> protocol_command =
      protocol::toProtocolValue(parsed_message.get(), 1000);
  int call_id;
  std::string method;
  if (!dispatcher_->parseCommand(protocol_command.get(), &call_id, &method))
    return;
  if (browser_only_ || dispatcher_->canDispatch(method)) {
    dispatcher_->dispatch(call_id, method, std::move(protocol_command),
                          message);
  } else {
    fallThrough(call_id, method, message);
  }
}

void DevToolsSession::fallThrough(int call_id,
                                  const std::string& method,
                                  const std::string& message) {
  // In browser-only mode, we should've handled everything in dispatcher.
  DCHECK(!browser_only_);

  if (suspended_sending_messages_to_agent_) {
    suspended_messages_.push_back({call_id, method, message});
    return;
  }

  DispatchProtocolMessageToAgent(call_id, method, message);
  waiting_for_response_messages_[call_id] = {method, message};
}

void DevToolsSession::DispatchProtocolMessageToAgent(
    int call_id,
    const std::string& method,
    const std::string& message) {
  DCHECK(!browser_only_);
  if (ShouldSendOnIO(method)) {
    if (io_session_ptr_)
      io_session_ptr_->DispatchProtocolCommand(call_id, method, message);
  } else {
    if (session_ptr_)
      session_ptr_->DispatchProtocolCommand(call_id, method, message);
  }
}

void DevToolsSession::SuspendSendingMessagesToAgent() {
  DCHECK(!browser_only_);
  suspended_sending_messages_to_agent_ = true;
}

void DevToolsSession::ResumeSendingMessagesToAgent() {
  DCHECK(!browser_only_);
  suspended_sending_messages_to_agent_ = false;
  for (const SuspendedMessage& message : suspended_messages_) {
    DispatchProtocolMessageToAgent(message.call_id, message.method,
                                   message.message);
    waiting_for_response_messages_[message.call_id] = {message.method,
                                                       message.message};
  }
  suspended_messages_.clear();
}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
  // |this| may be deleted at this point.
}

void DevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
  // |this| may be deleted at this point.
}

void DevToolsSession::flushProtocolNotifications() {
}

void DevToolsSession::DispatchProtocolResponse(
    const std::string& message,
    int call_id,
    blink::mojom::DevToolsSessionStatePtr updates) {
  ApplySessionStateUpdates(std::move(updates));
  waiting_for_response_messages_.erase(call_id);
  client_->DispatchProtocolMessage(agent_host_, message);
  // |this| may be deleted at this point.
}

void DevToolsSession::DispatchProtocolNotification(
    const std::string& message,
    blink::mojom::DevToolsSessionStatePtr updates) {
  ApplySessionStateUpdates(std::move(updates));
  client_->DispatchProtocolMessage(agent_host_, message);
  // |this| may be deleted at this point.
}

void DevToolsSession::ApplySessionStateUpdates(
    blink::mojom::DevToolsSessionStatePtr updates) {
  if (!updates)
    return;
  if (!session_state_cookie_)
    session_state_cookie_ = blink::mojom::DevToolsSessionState::New();
  for (auto& entry : updates->entries) {
    if (entry.second.has_value())
      session_state_cookie_->entries[entry.first] = std::move(entry.second);
    else
      session_state_cookie_->entries.erase(entry.first);
  }
}
}  // namespace content
