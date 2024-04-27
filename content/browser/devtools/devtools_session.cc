// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace content {
namespace {
// Keep in sync with WebDevToolsAgent::ShouldInterruptForMethod.
// TODO(petermarshall): find a way to share this.
bool ShouldSendOnIO(crdtp::span<uint8_t> method) {
  static auto* kEntries = new std::vector<crdtp::span<uint8_t>>{
      crdtp::SpanFrom("Debugger.getPossibleBreakpoints"),
      crdtp::SpanFrom("Debugger.getScriptSource"),
      crdtp::SpanFrom("Debugger.getStackTrace"),
      crdtp::SpanFrom("Debugger.pause"),
      crdtp::SpanFrom("Debugger.removeBreakpoint"),
      crdtp::SpanFrom("Debugger.resume"),
      crdtp::SpanFrom("Debugger.setBreakpoint"),
      crdtp::SpanFrom("Debugger.setBreakpointByUrl"),
      crdtp::SpanFrom("Debugger.setBreakpointsActive"),
      crdtp::SpanFrom("Emulation.setScriptExecutionDisabled"),
      crdtp::SpanFrom("Page.crash"),
      crdtp::SpanFrom("Performance.getMetrics"),
      crdtp::SpanFrom("Runtime.terminateExecution"),
  };
  DCHECK(std::is_sorted(kEntries->begin(), kEntries->end(), crdtp::SpanLt()));
  return std::binary_search(kEntries->begin(), kEntries->end(), method,
                            crdtp::SpanLt());
}

// During navigation, we only suspend the main-thread messages. The IO thread
// messages should go through so that the renderer can be woken up
// via the IO thread even if the renderer does not process message loops.
//
// In particular, we are looking to deadlocking the renderer when
// reloading the page during the instrumentation pause (crbug.com/1354043):
//
// - If we are in the pause, there is no way to commit or fail the navigation
//   in the renderer because the instrumentation pause does not process message
//   loops.
// - At the same time, the instrumentation pause could not wake up if
//   the resume message was blocked by the suspension of message sending during
//   navigation.
//
// To give the renderer a chance to wake up, we always forward the messages
// for the IO thread to the renderer.
bool ShouldSuspendDuringNavigation(crdtp::span<uint8_t> method) {
  return !ShouldSendOnIO(method);
}

// Async control commands (such as CSS.enable) are idempotant and can
// be safely replayed in the new RenderFrameHost. We will always forward
// them to the new renderer on cross process navigation. Main rationale for
// it is that the client doesn't expect such calls to fail in normal
// circumstances.
//
// Ideally all non-control async commands shoulds be listed here but we
// conservatively start with Runtime domain where the decision is more
// clear.
bool TerminateOnCrossProcessNavigation(crdtp::span<uint8_t> method) {
  static auto* kEntries = new std::vector<crdtp::span<uint8_t>>{
      crdtp::SpanFrom("Runtime.awaitPromise"),
      crdtp::SpanFrom("Runtime.callFunctionOn"),
      crdtp::SpanFrom("Runtime.evaluate"),
      crdtp::SpanFrom("Runtime.runScript"),
      crdtp::SpanFrom("Runtime.terminateExecution"),
  };
  DCHECK(std::is_sorted(kEntries->begin(), kEntries->end(), crdtp::SpanLt()));
  return std::binary_search(kEntries->begin(), kEntries->end(), method,
                            crdtp::SpanLt());
}

const char kResumeMethod[] = "Runtime.runIfWaitingForDebugger";
const char kSessionId[] = "sessionId";

// Clients match against this error message verbatim (http://crbug.com/1001678).
const char kTargetClosedMessage[] = "Inspected target navigated or closed";
const char kTargetCrashedMessage[] = "Target crashed";
}  // namespace

DevToolsSession::PendingMessage::PendingMessage(PendingMessage&&) = default;
DevToolsSession::PendingMessage::PendingMessage(int call_id,
                                                crdtp::span<uint8_t> method,
                                                crdtp::span<uint8_t> payload)
    : call_id(call_id),
      method(method.begin(), method.end()),
      payload(payload.begin(), payload.end()) {}

DevToolsSession::PendingMessage::~PendingMessage() = default;

DevToolsSession::DevToolsSession(DevToolsAgentHostClient* client, Mode mode)
    : client_(client), mode_(mode) {}

DevToolsSession::DevToolsSession(DevToolsAgentHostClient* client,
                                 const std::string& session_id,
                                 DevToolsSession* parent,
                                 Mode mode)
    : client_(client),
      root_session_(parent->GetRootSession()),
      session_id_(session_id),
      mode_(mode) {
  DCHECK(root_session_);
  DCHECK(!session_id_.empty());
}

DevToolsSession::~DevToolsSession() {
  if (proxy_delegate_)
    proxy_delegate_->Detach(this);
  // It is Ok for session to be deleted without the dispose -
  // it can be kicked out by an extension connect / disconnect.
  if (dispatcher_)
    Dispose();
}

void DevToolsSession::SetAgentHost(DevToolsAgentHostImpl* agent_host) {
  DCHECK(!agent_host_);
  agent_host_ = agent_host;
}

void DevToolsSession::SetRuntimeResumeCallback(
    base::OnceClosure runtime_resume) {
  runtime_resume_ = std::move(runtime_resume);
}

bool DevToolsSession::IsWaitingForDebuggerOnStart() const {
  return !runtime_resume_.is_null();
}

void DevToolsSession::Dispose() {
  dispatcher_.reset();
  for (auto& pair : handlers_)
    pair.second->Disable();
  handlers_.clear();
}

DevToolsSession* DevToolsSession::GetRootSession() {
  return root_session_ ? root_session_.get() : this;
}

void DevToolsSession::AddHandler(
    std::unique_ptr<protocol::DevToolsDomainHandler> handler) {
  DCHECK(agent_host_);
  handler->Wire(dispatcher_.get());
  handler->SetSession(this);
  handlers_[handler->name()] = std::move(handler);
}

void DevToolsSession::SetBrowserOnly(bool browser_only) {
  browser_only_ = browser_only;
}

void DevToolsSession::TurnIntoExternalProxy(
    DevToolsExternalAgentProxyDelegate* proxy_delegate) {
  proxy_delegate_ = proxy_delegate;
  proxy_delegate_->Attach(this);
}

void DevToolsSession::AttachToAgent(blink::mojom::DevToolsAgent* agent,
                                    bool force_using_io_session) {
  DCHECK(agent_host_);
  if (!agent) {
    receiver_.reset();
    session_.reset();
    io_session_.reset();
    return;
  }

  // TODO(crbug.com/41467868): Consider a reset flow since new mojo types
  // checks is_bound strictly.
  if (receiver_.is_bound()) {
    receiver_.reset();
    session_.reset();
    io_session_.reset();
  }

  use_io_session_ = force_using_io_session;
  agent->AttachDevToolsSession(
      receiver_.BindNewEndpointAndPassRemote(),
      session_.BindNewEndpointAndPassReceiver(),
      io_session_.BindNewPipeAndPassReceiver(), session_state_cookie_.Clone(),
      client_->UsesBinaryProtocol(), client_->IsTrusted(), session_id_,
      IsWaitingForDebuggerOnStart());
  session_.set_disconnect_handler(base::BindOnce(
      &DevToolsSession::MojoConnectionDestroyed, base::Unretained(this)));

  // Set cookie to an empty struct to reattach next time instead of attaching.
  if (!session_state_cookie_)
    session_state_cookie_ = blink::mojom::DevToolsSessionState::New();

  // We're attaching to a new agent while suspended; therefore, messages that
  // have been sent previously either need to be terminated or re-sent once we
  // resume, as we will not get any responses from the old agent at this point.
  if (suspended_sending_messages_to_agent_) {
    for (auto it = pending_messages_.begin(); it != pending_messages_.end();) {
      const PendingMessage& message = *it;
      if (waiting_for_response_.count(message.call_id) &&
          TerminateOnCrossProcessNavigation(crdtp::SpanFrom(message.method))) {
        // Send error to the client and remove the message from pending.
        SendProtocolResponse(
            message.call_id,
            crdtp::CreateErrorResponse(
                message.call_id,
                crdtp::DispatchResponse::ServerError(kTargetClosedMessage)));
        it = pending_messages_.erase(it);
      } else {
        // We'll send or re-send the message in ResumeSendingMessagesToAgent.
        ++it;
      }
    }
    waiting_for_response_.clear();
    return;
  }

  // The session is not suspended but the RenderFrameHost may be updated
  // during navigation because:
  // - auto attached to a new OOPIF
  // - cross-process navigation in the main frame
  // Therefore, we re-send outstanding messages to the new host.
  for (const PendingMessage& message : pending_messages_) {
    if (waiting_for_response_.count(message.call_id))
      DispatchToAgent(message);
  }
}

void DevToolsSession::MojoConnectionDestroyed() {
  receiver_.reset();
  session_.reset();
  io_session_.reset();
}

// The client of the devtools session will call this method to send a message
// to handlers / agents that the session is connected with.
void DevToolsSession::DispatchProtocolMessage(
    base::span<const uint8_t> message) {
  if (client_->UsesBinaryProtocol()) {
    crdtp::Status status =
        crdtp::cbor::CheckCBORMessage(crdtp::SpanFrom(message));
    if (!status.ok()) {
      DispatchProtocolMessageToClient(
          crdtp::CreateErrorNotification(
              crdtp::DispatchResponse::ParseError(status.ToASCIIString()))
              ->Serialize());
      return;
    }
  }

  // If the session is in proxy mode, then |message| will be sent to
  // an external session, so it needs to be sent as JSON.
  // TODO(dgozman): revisit the proxy delegate.
  if (proxy_delegate_) {                   // External session wants JSON.
    if (!client_->UsesBinaryProtocol()) {  // Client sent JSON.
      proxy_delegate_->SendMessageToBackend(this, message);
      return;
    }
    // External session wants JSON, but client provided CBOR.
    std::vector<uint8_t> json;
    crdtp::Status status =
        crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(message), &json);
    if (status.ok()) {
      proxy_delegate_->SendMessageToBackend(this, json);
      return;
    }
    DispatchProtocolMessageToClient(
        crdtp::CreateErrorNotification(
            crdtp::DispatchResponse::ParseError(status.ToASCIIString()))
            ->Serialize());
    return;
  }
  // Before dispatching, convert the message to CBOR if needed.
  std::vector<uint8_t> converted_cbor_message;
  if (!client_->UsesBinaryProtocol()) {  // Client sent JSON.
    crdtp::Status status = crdtp::json::ConvertJSONToCBOR(
        crdtp::SpanFrom(message), &converted_cbor_message);
    if (!status.ok()) {
      DispatchProtocolMessageToClient(
          crdtp::CreateErrorNotification(
              crdtp::DispatchResponse::ParseError(status.ToASCIIString()))
              ->Serialize());
      return;
    }
    message = converted_cbor_message;
  }
  // At this point |message| is CBOR.
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  if (!dispatchable.ok()) {
    DispatchProtocolMessageToClient(
        (dispatchable.HasCallId()
             ? crdtp::CreateErrorResponse(dispatchable.CallId(),
                                          dispatchable.DispatchError())
             : crdtp::CreateErrorNotification(dispatchable.DispatchError()))
            ->Serialize());

    return;
  }
  if (dispatchable.SessionId().empty()) {
    DispatchProtocolMessageInternal(std::move(dispatchable), message);
    return;
  }
  std::string session_id(dispatchable.SessionId().begin(),
                         dispatchable.SessionId().end());
  auto it = child_sessions_.find(session_id);
  if (it == child_sessions_.end()) {
    auto error = crdtp::DispatchResponse::SessionNotFound(
        "Session with given id not found.");
    DispatchProtocolMessageToClient(
        (dispatchable.HasCallId()
             ? crdtp::CreateErrorResponse(dispatchable.CallId(),
                                          std::move(error))
             : crdtp::CreateErrorNotification(std::move(error)))
            ->Serialize());
    return;
  }
  DevToolsSession* session = it->second;
  DCHECK(!session->proxy_delegate_);
  session->DispatchProtocolMessageInternal(std::move(dispatchable), message);
}

void DevToolsSession::DispatchProtocolMessageInternal(
    crdtp::Dispatchable dispatchable,
    base::span<const uint8_t> message) {
  if ((browser_only_ || runtime_resume_) &&
      crdtp::SpanEquals(crdtp::SpanFrom(kResumeMethod),
                        dispatchable.Method())) {
    if (runtime_resume_) {
      std::move(runtime_resume_).Run();
    }
    if (browser_only_) {
      DispatchProtocolMessageToClient(
          crdtp::CreateResponse(dispatchable.CallId(), nullptr)->Serialize());
      return;
    }
  }

  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (delegate && !dispatchable.Method().empty()) {
    delegate->HandleCommand(this, message,
                            base::BindOnce(&DevToolsSession::HandleCommand,
                                           weak_factory_.GetWeakPtr()));
  } else {
    HandleCommandInternal(std::move(dispatchable), message);
  }
}

void DevToolsSession::HandleCommand(base::span<const uint8_t> message) {
  HandleCommandInternal(crdtp::Dispatchable(crdtp::SpanFrom(message)), message);
}

void DevToolsSession::HandleCommandInternal(crdtp::Dispatchable dispatchable,
                                            base::span<const uint8_t> message) {
  DCHECK(dispatchable.ok());
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_->Dispatch(dispatchable);
  if (browser_only_ || dispatched.MethodFound()) {
    TRACE_EVENT_WITH_FLOW2(
        "devtools", "DevToolsSession::HandleCommand in Browser",
        dispatchable.CallId(), TRACE_EVENT_FLAG_FLOW_OUT, "method",
        std::string(dispatchable.Method().begin(), dispatchable.Method().end()),
        "call_id", dispatchable.CallId());
    dispatched.Run();
  } else {
    FallThrough(dispatchable.CallId(), dispatchable.Method(),
                crdtp::SpanFrom(message));
  }
}

void DevToolsSession::FallThrough(int call_id,
                                  crdtp::span<uint8_t> method,
                                  crdtp::span<uint8_t> message) {
  // In browser-only mode, we should've handled everything in dispatcher.
  DCHECK(!browser_only_);

  if (base::Contains(waiting_for_response_, call_id)) {
    DispatchProtocolMessageToClient(
        crdtp::CreateErrorResponse(call_id,
                                   crdtp::DispatchResponse::InvalidRequest(
                                       "Duplicate `id` in protocol request"))
            ->Serialize());
  }

  auto it = pending_messages_.emplace(pending_messages_.end(), call_id, method,
                                      message);
  if (suspended_sending_messages_to_agent_ &&
      ShouldSuspendDuringNavigation(method))
    return;

  DispatchToAgent(pending_messages_.back());
  waiting_for_response_[call_id] = it;
}

// This method implements DevtoolsAgentHostClientChannel and
// sends messages coming from the browser to the client.
void DevToolsSession::DispatchProtocolMessageToClient(
    std::vector<uint8_t> message) {
  DCHECK(crdtp::cbor::IsCBORMessage(crdtp::SpanFrom(message)));

  if (!session_id_.empty()) {
    crdtp::Status status = crdtp::cbor::AppendString8EntryToCBORMap(
        crdtp::SpanFrom(kSessionId), crdtp::SpanFrom(session_id_), &message);
    DCHECK(status.ok()) << status.ToASCIIString();
  }
  if (!client_->UsesBinaryProtocol()) {
    std::vector<uint8_t> json;
    crdtp::Status status =
        crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(message), &json);
    DCHECK(status.ok()) << status.ToASCIIString();
    message = std::move(json);
  }
  client_->DispatchProtocolMessage(agent_host_, message);
}

content::DevToolsAgentHost* DevToolsSession::GetAgentHost() {
  return agent_host_;
}

content::DevToolsAgentHostClient* DevToolsSession::GetClient() {
  return client_;
}

void DevToolsSession::DispatchToAgent(const PendingMessage& message) {
  DCHECK(!browser_only_);
  // We send all messages on the IO channel for workers so that messages like
  // Debugger.pause don't get stuck behind other blocking messages.
  if (ShouldSendOnIO(crdtp::SpanFrom(message.method)) || use_io_session_) {
    if (io_session_) {
      TRACE_EVENT_WITH_FLOW2(
          "devtools", "DevToolsSession::DispatchToAgent on IO", message.call_id,
          TRACE_EVENT_FLAG_FLOW_OUT, "method", message.method, "call_id",
          message.call_id);
      io_session_->DispatchProtocolCommand(message.call_id, message.method,
                                           message.payload);
    }
  } else {
    if (session_) {
      TRACE_EVENT_WITH_FLOW2("devtools", "DevToolsSession::DispatchToAgent",
                             message.call_id, TRACE_EVENT_FLAG_FLOW_OUT,
                             "method", message.method, "call_id",
                             message.call_id);
      session_->DispatchProtocolCommand(message.call_id, message.method,
                                        message.payload);
    }
  }
}

void DevToolsSession::SuspendSendingMessagesToAgent() {
  DCHECK(!browser_only_);
  suspended_sending_messages_to_agent_ = true;
}

void DevToolsSession::ResumeSendingMessagesToAgent() {
  DCHECK(!browser_only_);
  suspended_sending_messages_to_agent_ = false;
  for (auto it = pending_messages_.begin(); it != pending_messages_.end();
       ++it) {
    const PendingMessage& message = *it;
    if (waiting_for_response_.count(message.call_id))
      continue;
    DispatchToAgent(message);
    waiting_for_response_[message.call_id] = it;
  }
}

void DevToolsSession::ClearPendingMessages(bool did_crash) {
  for (auto it = pending_messages_.begin(); it != pending_messages_.end();) {
    const PendingMessage& message = *it;
    if (SpanEquals(crdtp::SpanFrom("Page.reload"),
                   crdtp::SpanFrom(message.method))) {
      ++it;
      continue;
    }
    // Send error to the client and remove the message from pending.
    std::string error_message =
        did_crash ? kTargetCrashedMessage : kTargetClosedMessage;
    SendProtocolResponse(
        message.call_id,
        crdtp::CreateErrorResponse(
            message.call_id,
            crdtp::DispatchResponse::ServerError(error_message)));
    waiting_for_response_.erase(message.call_id);
    it = pending_messages_.erase(it);
  }
}

// The following methods handle responses or notifications coming from
// the browser to the client.
void DevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  DispatchProtocolMessageToClient(message->Serialize());
  // |this| may be deleted at this point.
}

void DevToolsSession::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  DispatchProtocolMessageToClient(message->Serialize());
  // |this| may be deleted at this point.
}

void DevToolsSession::FlushProtocolNotifications() {}

// The following methods handle responses or notifications coming from the
// renderer (blink) to the client. It is important that these messages not be
// parsed and sent as is, since a renderer may be compromised; so therefore,
// we're not sending them via the DevToolsAgentHostClientChannel interface
// (::DispatchProtocolMessageToClient) but directly to the client instead.
static void DispatchProtocolResponseOrNotification(
    DevToolsAgentHostClient* client,
    DevToolsAgentHostImpl* agent_host,
    blink::mojom::DevToolsMessagePtr message) {
  client->DispatchProtocolMessage(agent_host, message->data);
}

void DevToolsSession::DispatchProtocolResponse(
    blink::mojom::DevToolsMessagePtr message,
    int call_id,
    blink::mojom::DevToolsSessionStatePtr updates) {
  TRACE_EVENT_WITH_FLOW1("devtools",
                         "DevToolsSession::DispatchProtocolResponse", call_id,
                         TRACE_EVENT_FLAG_FLOW_IN, "call_id", call_id);
  ApplySessionStateUpdates(std::move(updates));
  auto it = waiting_for_response_.find(call_id);
  // TODO(johannes): Consider shutting down renderer instead of just
  // dropping the message. See shutdownForBadMessage().
  if (it == waiting_for_response_.end())
    return;
  pending_messages_.erase(it->second);
  waiting_for_response_.erase(it);
  DispatchProtocolResponseOrNotification(client_, agent_host_,
                                         std::move(message));
  // |this| may be deleted at this point.
}

void DevToolsSession::DispatchProtocolNotification(
    blink::mojom::DevToolsMessagePtr message,
    blink::mojom::DevToolsSessionStatePtr updates) {
  ApplySessionStateUpdates(std::move(updates));
  DispatchProtocolResponseOrNotification(client_, agent_host_,
                                         std::move(message));
  // |this| may be deleted at this point.
}

void DevToolsSession::DispatchOnClientHost(base::span<const uint8_t> message) {
  // |message| either comes from a web socket, in which case it's JSON.
  // Or it comes from another devtools_session, in which case it may be CBOR
  // already. We auto-detect and convert to what the client wants as needed.
  bool is_cbor_message = crdtp::cbor::IsCBORMessage(crdtp::SpanFrom(message));
  if (client_->UsesBinaryProtocol() == is_cbor_message) {
    client_->DispatchProtocolMessage(agent_host_, message);
    return;
  }
  std::vector<uint8_t> converted;
  crdtp::Status status =
      client_->UsesBinaryProtocol()
          ? crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(message), &converted)
          : crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(message),
                                           &converted);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  client_->DispatchProtocolMessage(agent_host_, converted);
  // |this| may be deleted at this point.
}

void DevToolsSession::ConnectionClosed() {
  DevToolsAgentHostClient* client = client_;
  DevToolsAgentHostImpl* agent_host = agent_host_;
  agent_host->DetachInternal(this);
  // |this| is deleted here, do not use any fields below.
  client->AgentHostClosed(agent_host);
}

void DevToolsSession::ApplySessionStateUpdates(
    blink::mojom::DevToolsSessionStatePtr updates) {
  if (!updates)
    return;
  if (!session_state_cookie_)
    session_state_cookie_ = blink::mojom::DevToolsSessionState::New();
  for (auto& entry : updates->entries) {
    if (entry.second.has_value())
      session_state_cookie_->entries[entry.first] = std::move(*entry.second);
    else
      session_state_cookie_->entries.erase(entry.first);
  }
}

DevToolsSession* DevToolsSession::AttachChildSession(
    const std::string& session_id,
    DevToolsAgentHostImpl* agent_host,
    DevToolsAgentHostClient* client,
    Mode mode,
    base::OnceClosure resume_callback) {
  DCHECK(!agent_host->SessionByClient(client));
  DCHECK(!root_session_);
  std::unique_ptr<DevToolsSession> session(
      new DevToolsSession(client, session_id, this, mode));
  session->SetRuntimeResumeCallback(std::move(resume_callback));
  DevToolsSession* session_ptr = session.get();
  // If attach did not succeed, |session| is already destroyed.
  if (!agent_host->AttachInternal(std::move(session)))
    return nullptr;
  child_sessions_[session_id] = session_ptr;
  for (auto& observer : child_observers_) {
    observer.SessionAttached(*session_ptr);
  }
  return session_ptr;
}

void DevToolsSession::DetachChildSession(const std::string& session_id) {
  child_sessions_.erase(session_id);
}

bool DevToolsSession::HasChildSession(const std::string& session_id) {
  return base::Contains(child_sessions_, session_id);
}

void DevToolsSession::AddObserver(ChildObserver* obs) {
  child_observers_.AddObserver(obs);
  for (auto& entry : child_sessions_) {
    obs->SessionAttached(*entry.second);
  }
}

void DevToolsSession::RemoveObserver(ChildObserver* obs) {
  child_observers_.RemoveObserver(obs);
}

}  // namespace content
