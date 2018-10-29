// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_session.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/Protocol.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
}

// static
bool InspectorSession::ShouldInterruptForMethod(const String& method) {
  // Keep in sync with DevToolsSession::ShouldSendOnIO.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics" || method == "Page.crash" ||
         method == "Runtime.terminateExecution" ||
         method == "Emulation.setScriptExecutionDisabled";
}

InspectorSession::InspectorSession(
    Client* client,
    CoreProbeSink* instrumenting_agents,
    InspectedFrames* inspected_frames,
    int session_id,
    v8_inspector::V8Inspector* inspector,
    int context_group_id,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state)
    : client_(client),
      v8_session_(nullptr),
      session_id_(session_id),
      disposed_(false),
      instrumenting_agents_(instrumenting_agents),
      inspected_frames_(inspected_frames),
      inspector_backend_dispatcher_(new protocol::UberDispatcher(this)),
      session_state_(std::move(reattach_session_state)),
      v8_session_state_(kV8StateKey),
      v8_session_state_json_(&v8_session_state_, /*default_value=*/String()) {
  v8_session_state_.InitFrom(&session_state_);

  // inspector->connect may result in calls to |this| against the
  // V8Inspector::Channel interface for receiving responses / notifications,
  // while v8_session_ is still nullptr.
  v8_session_ =
      inspector->connect(context_group_id, /*channel*/ this,
                         ToV8InspectorStringView(v8_session_state_json_.Get()));

  instrumenting_agents_->addInspectorSession(this);
}

InspectorSession::~InspectorSession() {
  DCHECK(disposed_);
}

void InspectorSession::Append(InspectorAgent* agent) {
  agents_.push_back(agent);
  agent->Init(instrumenting_agents_.Get(), inspector_backend_dispatcher_.get(),
              &session_state_);
}

void InspectorSession::Restore() {
  DCHECK(!disposed_);
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->Restore();
}

void InspectorSession::Dispose() {
  DCHECK(!disposed_);
  disposed_ = true;
  instrumenting_agents_->removeInspectorSession(this);
  inspector_backend_dispatcher_.reset();
  for (wtf_size_t i = agents_.size(); i > 0; i--)
    agents_[i - 1]->Dispose();
  agents_.clear();
  v8_session_.reset();
}

void InspectorSession::DispatchProtocolMessage(int call_id,
                                               const String& method,
                                               const String& message) {
  DCHECK(!disposed_);
  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          ToV8InspectorStringView(method))) {
    v8_session_->dispatchProtocolMessage(ToV8InspectorStringView(message));
  } else {
    inspector_backend_dispatcher_->dispatch(
        call_id, method, protocol::StringUtil::parseJSON(message), message);
  }
}

void InspectorSession::DispatchProtocolMessage(const String& message) {
  DCHECK(!disposed_);
  int call_id;
  String method;
  std::unique_ptr<protocol::Value> parsed_message =
      protocol::StringUtil::parseJSON(message);
  if (!inspector_backend_dispatcher_->parseCommand(parsed_message.get(),
                                                   &call_id, &method)) {
    return;
  }
  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          ToV8InspectorStringView(method))) {
    v8_session_->dispatchProtocolMessage(ToV8InspectorStringView(message));
  } else {
    inspector_backend_dispatcher_->dispatch(call_id, method,
                                            std::move(parsed_message), message);
  }
}

void InspectorSession::DidStartProvisionalLoad(LocalFrame* frame) {
  if (inspected_frames_->Root() == frame) {
    v8_session_->setSkipAllPauses(true);
    v8_session_->resume();
  }
}

void InspectorSession::DidFailProvisionalLoad(LocalFrame* frame) {
  if (inspected_frames_->Root() == frame)
    v8_session_->setSkipAllPauses(false);
}

void InspectorSession::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->DidCommitLoadForLocalFrame(frame);
  if (inspected_frames_->Root() == frame)
    v8_session_->setSkipAllPauses(false);
}

void InspectorSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponse(call_id, message->serialize());
}

void InspectorSession::fallThrough(int call_id,
                                   const String& method,
                                   const String& message) {
  // There's no other layer to handle the command.
  NOTREACHED();
}

void InspectorSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  // We can potentially avoid copies if WebString would convert to utf8 right
  // from StringView, but it uses StringImpl itself, so we don't create any
  // extra copies here.
  SendProtocolResponse(call_id, ToCoreString(message->string()));
}

void InspectorSession::SendProtocolResponse(int call_id,
                                            const String& message) {
  if (disposed_)
    return;
  flushProtocolNotifications();
  if (v8_session_)
    v8_session_state_json_.Set(ToCoreString(v8_session_->stateJSON()));
  client_->SendProtocolResponse(session_id_, call_id, message,
                                session_state_.TakeUpdates());
}

class InspectorSession::Notification {
 public:
  static std::unique_ptr<Notification> CreateForBlink(
      std::unique_ptr<protocol::Serializable> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  static std::unique_ptr<Notification> CreateForV8(
      std::unique_ptr<v8_inspector::StringBuffer> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  String Serialize() {
    if (blink_notification_) {
      serialized_ = blink_notification_->serialize();
      blink_notification_.reset();
    } else if (v8_notification_) {
      serialized_ = ToCoreString(v8_notification_->string());
      v8_notification_.reset();
    }
    return serialized_;
  }

 private:
  explicit Notification(std::unique_ptr<protocol::Serializable> notification)
      : blink_notification_(std::move(notification)) {}

  explicit Notification(
      std::unique_ptr<v8_inspector::StringBuffer> notification)
      : v8_notification_(std::move(notification)) {}

  std::unique_ptr<protocol::Serializable> blink_notification_;
  std::unique_ptr<v8_inspector::StringBuffer> v8_notification_;
  String serialized_;
};

void InspectorSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> notification) {
  if (disposed_)
    return;
  notification_queue_.push_back(
      Notification::CreateForBlink(std::move(notification)));
}

void InspectorSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> notification) {
  if (disposed_)
    return;
  notification_queue_.push_back(
      Notification::CreateForV8(std::move(notification)));
}

void InspectorSession::flushProtocolNotifications() {
  if (disposed_)
    return;
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->FlushPendingProtocolNotifications();
  if (!notification_queue_.size())
    return;
  if (v8_session_)
    v8_session_state_json_.Set(ToCoreString(v8_session_->stateJSON()));
  for (wtf_size_t i = 0; i < notification_queue_.size(); ++i) {
    client_->SendProtocolNotification(session_id_,
                                      notification_queue_[i]->Serialize(),
                                      session_state_.TakeUpdates());
  }
  notification_queue_.clear();
}

void InspectorSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(instrumenting_agents_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(agents_);
}

}  // namespace blink
