// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/devtools_session.h"

#include <string>
#include <utility>
#include <vector>

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/protocol/Protocol.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
bool ShouldInterruptForMethod(const String& method) {
  // Keep in sync with DevToolsSession::ShouldSendOnIO.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics" || method == "Page.crash" ||
         method == "Runtime.terminateExecution" ||
         method == "Debugger.getStackTrace" ||
         method == "Emulation.setScriptExecutionDisabled";
}

Vector<uint8_t> UnwrapMessage(const mojom::blink::DevToolsMessagePtr& message) {
  Vector<uint8_t> unwrap_message;
  unwrap_message.Append(message->data.data(), message->data.size());
  return unwrap_message;
}

// Platform allows us to inject the string<->double conversion
// routines from Blink into the inspector_protocol JSON parser / serializer.
class JsonPlatform : public crdtp::json::Platform {
 public:
  bool StrToD(const char* str, double* result) const override {
    bool ok;
    *result = String(str).ToDouble(&ok);
    return ok;
  }

  // Prints |value| in a format suitable for JSON.
  std::unique_ptr<char[]> DToStr(double value) const override {
    String str = String::NumberToStringECMAScript(value);
    DCHECK(str.Is8Bit());
    std::unique_ptr<char[]> result(new char[str.length() + 1]);
    memcpy(result.get(), str.Characters8(), str.length());
    result.get()[str.length()] = '\0';
    return result;
  }
};

crdtp::Status ConvertCBORToJSON(crdtp::span<uint8_t> cbor,
                                std::vector<uint8_t>* json) {
  DCHECK(crdtp::cbor::IsCBORMessage(cbor));
  JsonPlatform platform;
  return ConvertCBORToJSON(platform, cbor, json);
}

std::vector<uint8_t> Get8BitStringFrom(v8_inspector::StringBuffer* msg) {
  const v8_inspector::StringView& s = msg->string();
  DCHECK(s.is8Bit());
  return std::vector<uint8_t>(s.characters8(), s.characters8() + s.length());
}
}  // namespace

// Created and stored in unique_ptr on UI.
// Binds request, receives messages and destroys on IO.
class DevToolsSession::IOSession : public mojom::blink::DevToolsSession {
 public:
  IOSession(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
            scoped_refptr<InspectorTaskRunner> inspector_task_runner,
            CrossThreadWeakPersistent<::blink::DevToolsSession> session,
            mojo::PendingReceiver<mojom::blink::DevToolsSession> receiver)
      : io_task_runner_(io_task_runner),
        inspector_task_runner_(inspector_task_runner),
        session_(std::move(session)) {
    PostCrossThreadTask(*io_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&IOSession::BindInterface,
                                            CrossThreadUnretained(this),
                                            WTF::Passed(std::move(receiver))));
  }

  ~IOSession() override {}

  void BindInterface(
      mojo::PendingReceiver<mojom::blink::DevToolsSession> receiver) {
    receiver_.Bind(std::move(receiver), io_task_runner_);
  }

  void DeleteSoon() { io_task_runner_->DeleteSoon(FROM_HERE, this); }

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolCommand(
      int call_id,
      const String& method,
      mojom::blink::DevToolsMessagePtr message) override {
    DCHECK(ShouldInterruptForMethod(method));
    // Crash renderer.
    if (method == "Page.crash")
      CHECK(false);
    inspector_task_runner_->AppendTask(CrossThreadBindOnce(
        &::blink::DevToolsSession::DispatchProtocolCommandImpl, session_,
        call_id, method, UnwrapMessage(message)));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  CrossThreadWeakPersistent<::blink::DevToolsSession> session_;
  mojo::Receiver<mojom::blink::DevToolsSession> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(IOSession);
};

DevToolsSession::DevToolsSession(
    DevToolsAgent* agent,
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost>
        host_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
        main_receiver,
    mojo::PendingReceiver<mojom::blink::DevToolsSession> io_receiver,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses)
    : agent_(agent),
      receiver_(this, std::move(main_receiver)),
      inspector_backend_dispatcher_(new protocol::UberDispatcher(this)),
      session_state_(std::move(reattach_session_state)),
      client_expects_binary_responses_(client_expects_binary_responses),
      v8_session_state_(kV8StateKey),
      v8_session_state_cbor_(&v8_session_state_,
                             /*default_value=*/{}) {
  io_session_ = new IOSession(
      agent_->io_task_runner_, agent_->inspector_task_runner_,
      WrapCrossThreadWeakPersistent(this), std::move(io_receiver));

  host_remote_.Bind(std::move(host_remote));
  host_remote_.set_disconnect_handler(
      WTF::Bind(&DevToolsSession::Detach, WrapWeakPersistent(this)));

  bool restore = !!session_state_.ReattachState();
  v8_session_state_.InitFrom(&session_state_);
  agent_->client_->AttachSession(this, restore);
  agent_->probe_sink_->AddDevToolsSession(this);
  if (restore) {
    for (wtf_size_t i = 0; i < agents_.size(); i++)
      agents_[i]->Restore();
  }
}

DevToolsSession::~DevToolsSession() {
  DCHECK(IsDetached());
}

void DevToolsSession::ConnectToV8(v8_inspector::V8Inspector* inspector,
                                  int context_group_id) {
  const auto& cbor = v8_session_state_cbor_.Get();
  v8_session_ =
      inspector->connect(context_group_id, this,
                         v8_inspector::StringView(cbor.data(), cbor.size()));
}

bool DevToolsSession::IsDetached() {
  return !host_remote_.is_bound();
}

void DevToolsSession::Append(InspectorAgent* agent) {
  agents_.push_back(agent);
  agent->Init(agent_->probe_sink_.Get(), inspector_backend_dispatcher_.get(),
              &session_state_);
}

void DevToolsSession::Detach() {
  agent_->client_->DebuggerTaskStarted();
  agent_->client_->DetachSession(this);
  agent_->sessions_.erase(this);
  receiver_.reset();
  host_remote_.reset();
  CHECK(io_session_);
  io_session_->DeleteSoon();
  io_session_ = nullptr;
  agent_->probe_sink_->RemoveDevToolsSession(this);
  inspector_backend_dispatcher_.reset();
  for (wtf_size_t i = agents_.size(); i > 0; i--)
    agents_[i - 1]->Dispose();
  agents_.clear();
  v8_session_.reset();
  agent_->client_->DebuggerTaskFinished();
}

void DevToolsSession::FlushProtocolNotifications() {
  flushProtocolNotifications();
}

void DevToolsSession::DispatchProtocolCommand(
    int call_id,
    const String& method,
    blink::mojom::blink::DevToolsMessagePtr message_ptr) {
  return DispatchProtocolCommandImpl(call_id, method,
                                     UnwrapMessage(message_ptr));
}

void DevToolsSession::DispatchProtocolCommandImpl(int call_id,
                                                  const String& method,
                                                  Vector<uint8_t> data) {
  DCHECK(crdtp::cbor::IsCBORMessage(
      crdtp::span<uint8_t>(data.data(), data.size())));

  // IOSession does not provide ordering guarantees relative to
  // Session, so a command may come to IOSession after Session is detached,
  // and get posted to main thread to this method.
  //
  // At the same time, Session may not be garbage collected yet
  // (even though already detached), and CrossThreadWeakPersistent<Session>
  // will still be valid.
  //
  // Both these factors combined may lead to this method being called after
  // detach, so we have to check it here.
  if (IsDetached())
    return;
  agent_->client_->DebuggerTaskStarted();
  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          ToV8InspectorStringView(method))) {
    // Binary protocol messages are passed using 8-bit StringView.
    v8_session_->dispatchProtocolMessage(
        v8_inspector::StringView(data.data(), data.size()));
  } else {
    std::unique_ptr<protocol::Value> value =
        protocol::Value::parseBinary(data.data(), data.size());
    // Don't pass protocol message further - there is no passthrough.
    inspector_backend_dispatcher_->dispatch(call_id, method, std::move(value),
                                            protocol::ProtocolMessage());
  }
  agent_->client_->DebuggerTaskFinished();
}

void DevToolsSession::DidStartProvisionalLoad(LocalFrame* frame) {
  if (v8_session_ && agent_->inspected_frames_->Root() == frame) {
    v8_session_->setSkipAllPauses(true);
    v8_session_->resume();
  }
}

void DevToolsSession::DidFailProvisionalLoad(LocalFrame* frame) {
  if (v8_session_ && agent_->inspected_frames_->Root() == frame)
    v8_session_->setSkipAllPauses(false);
}

void DevToolsSession::DidCommitLoad(LocalFrame* frame, DocumentLoader*) {
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->DidCommitLoadForLocalFrame(frame);
  if (v8_session_ && agent_->inspected_frames_->Root() == frame)
    v8_session_->setSkipAllPauses(false);
}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponse(call_id, std::move(*message).TakeSerialized());
}

void DevToolsSession::fallThrough(int call_id,
                                  const String& method,
                                  const protocol::ProtocolMessage& message) {
  // There's no other layer to handle the command.
  NOTREACHED();
}

void DevToolsSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  SendProtocolResponse(call_id, Get8BitStringFrom(message.get()));
}

void DevToolsSession::SendProtocolResponse(int call_id,
                                           std::vector<uint8_t> message) {
  if (IsDetached())
    return;
  flushProtocolNotifications();
  if (v8_session_)
    v8_session_state_cbor_.Set(v8_session_->state());
  // Make tests more predictable by flushing all sessions before sending
  // protocol response in any of them.
  if (WebTestSupport::IsRunningWebTest())
    agent_->FlushProtocolNotifications();

  host_remote_->DispatchProtocolResponse(FinalizeMessage(std::move(message)),
                                         call_id, session_state_.TakeUpdates());
}

void DevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> notification) {
  if (IsDetached())
    return;
  notification_queue_.push_back(WTF::Bind(
      [](std::unique_ptr<protocol::Serializable> notification) {
        return std::move(*notification).TakeSerialized();
      },
      std::move(notification)));
}

void DevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> notification) {
  if (IsDetached())
    return;
  notification_queue_.push_back(WTF::Bind(
      [](std::unique_ptr<v8_inspector::StringBuffer> notification) {
        return Get8BitStringFrom(notification.get());
      },
      std::move(notification)));
}

void DevToolsSession::flushProtocolNotifications() {
  if (IsDetached())
    return;
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->FlushPendingProtocolNotifications();
  if (!notification_queue_.size())
    return;
  if (v8_session_)
    v8_session_state_cbor_.Set(v8_session_->state());
  for (wtf_size_t i = 0; i < notification_queue_.size(); ++i) {
    host_remote_->DispatchProtocolNotification(
        FinalizeMessage(std::move(notification_queue_[i]).Run()),
        session_state_.TakeUpdates());
  }
  notification_queue_.clear();
}

void DevToolsSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_);
  visitor->Trace(agents_);
}

blink::mojom::blink::DevToolsMessagePtr DevToolsSession::FinalizeMessage(
    std::vector<uint8_t> message) const {
  std::vector<uint8_t> message_to_send = std::move(message);
  if (!client_expects_binary_responses_) {
    std::vector<uint8_t> json;
    crdtp::Status status =
        ConvertCBORToJSON(crdtp::SpanFrom(message_to_send), &json);
    CHECK(status.ok()) << status.ToASCIIString();
    message_to_send = std::move(json);
  }
  auto mojo_msg = mojom::blink::DevToolsMessage::New();
  mojo_msg->data = std::move(message_to_send);
  return mojo_msg;
}

}  // namespace blink
