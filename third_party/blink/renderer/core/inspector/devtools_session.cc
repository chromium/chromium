// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/inspector/devtools_session.h"

#include <string>
#include <utility>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/protocol/protocol.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
const char kSessionId[] = "sessionId";

bool ShouldInterruptForMethod(const String& method) {
  return method != "Debugger.evaluateOnCallFrame" &&
         method != "Runtime.evaluate" && method != "Runtime.callFunctionOn" &&
         method != "Runtime.getProperties" && method != "Runtime.runScript";
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
            CrossThreadWeakHandle<::blink::DevToolsSession> session,
            mojo::PendingReceiver<mojom::blink::DevToolsSession> receiver)
      : io_task_runner_(io_task_runner),
        inspector_task_runner_(inspector_task_runner),
        session_(std::move(session)) {
    PostCrossThreadTask(
        *io_task_runner, FROM_HERE,
        CrossThreadBindOnce(&IOSession::BindInterface,
                            CrossThreadUnretained(this), std::move(receiver)));
  }

  IOSession(const IOSession&) = delete;
  IOSession& operator=(const IOSession&) = delete;

  ~IOSession() override = default;

  void BindInterface(
      mojo::PendingReceiver<mojom::blink::DevToolsSession> receiver) {
    receiver_.Bind(std::move(receiver), io_task_runner_);

    // We set the disconnect handler for the IO session to detach the devtools
    // session from its V8 session. This is necessary to unpause and detach
    // the main thread session if the main thread is blocked in
    // an instrumentation pause.
    receiver_.set_disconnect_handler(WTF::BindOnce(
        [](scoped_refptr<InspectorTaskRunner> inspector_task_runner,
           CrossThreadWeakHandle<::blink::DevToolsSession> session) {
          inspector_task_runner->AppendTask(CrossThreadBindOnce(
              &::blink::DevToolsSession::DetachFromV8,
              MakeUnwrappingCrossThreadWeakHandle(session)));
        },
        inspector_task_runner_, session_));
  }

  void DeleteSoon() { io_task_runner_->DeleteSoon(FROM_HERE, this); }

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolCommand(int call_id,
                               const String& method,
                               base::span<const uint8_t> message) override {
    TRACE_EVENT_WITH_FLOW1("devtools", "IOSession::DispatchProtocolCommand",
                           call_id,
                           TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN,
                           "call_id", call_id);
    // Crash renderer.
    if (method == "Page.crash")
      CHECK(false);
    // Post a task to the worker or main renderer thread that will interrupt V8
    // and be run immediately. Only methods that do not run JS code are safe.
    Vector<uint8_t> message_copy;
    message_copy.AppendSpan(message);
    if (ShouldInterruptForMethod(method)) {
      inspector_task_runner_->AppendTask(CrossThreadBindOnce(
          &::blink::DevToolsSession::DispatchProtocolCommandImpl,
          MakeUnwrappingCrossThreadWeakHandle(session_), call_id, method,
          std::move(message_copy)));
    } else {
      inspector_task_runner_->AppendTaskDontInterrupt(CrossThreadBindOnce(
          &::blink::DevToolsSession::DispatchProtocolCommandImpl,
          MakeUnwrappingCrossThreadWeakHandle(session_), call_id, method,
          std::move(message_copy)));
    }
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  CrossThreadWeakHandle<::blink::DevToolsSession> session_;
  mojo::Receiver<mojom::blink::DevToolsSession> receiver_{this};
};

DevToolsSession::DevToolsSession(
    DevToolsAgent* agent,
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost>
        host_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
        main_receiver,
    mojo::PendingReceiver<mojom::blink::DevToolsSession> io_receiver,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses,
    bool client_is_trusted,
    const String& session_id,
    bool session_waits_for_debugger,
    scoped_refptr<base::SequencedTaskRunner> mojo_task_runner)
    : agent_(agent),
      inspector_backend_dispatcher_(new protocol::UberDispatcher(this)),
      session_state_(std::move(reattach_session_state)),
      client_expects_binary_responses_(client_expects_binary_responses),
      client_is_trusted_(client_is_trusted),
      v8_session_state_(kV8StateKey),
      v8_session_state_cbor_(&v8_session_state_, /*default_value=*/{}),
      session_id_(session_id),
      session_waits_for_debugger_(session_waits_for_debugger) {
  receiver_.Bind(std::move(main_receiver), mojo_task_runner);

  io_session_ =
      new IOSession(agent_->io_task_runner_, agent_->inspector_task_runner_,
                    MakeCrossThreadWeakHandle(this), std::move(io_receiver));

  host_remote_.Bind(std::move(host_remote), mojo_task_runner);
  host_remote_.set_disconnect_handler(
      WTF::BindOnce(&DevToolsSession::Detach, WrapWeakPersistent(this)));

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
  v8_session_ = inspector->connect(
      context_group_id, this,
      v8_inspector::StringView(cbor.data(), cbor.size()),
      client_is_trusted_ ? v8_inspector::V8Inspector::kFullyTrusted
                         : v8_inspector::V8Inspector::kUntrusted,
      session_waits_for_debugger_
          ? v8_inspector::V8Inspector::kWaitingForDebugger
          : v8_inspector::V8Inspector::kNotWaitingForDebugger);
}

bool DevToolsSession::IsDetached() {
  return !io_session_;
}

void DevToolsSession::Append(InspectorAgent* agent) {
  agents_.push_back(agent);
  agent->Init(agent_->probe_sink_.Get(), inspector_backend_dispatcher_.get(),
              &session_state_);
}

void DevToolsSession::Detach() {
  agent_->client_->DebuggerTaskStarted();
  agent_->client_->DetachSession(this);
  agent_->DetachDevToolsSession(this);
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

void DevToolsSession::DetachFromV8() {
  if (v8_session_) {
    v8_session_->stop();
  }
}

void DevToolsSession::DispatchProtocolCommand(
    int call_id,
    const String& method,
    base::span<const uint8_t> message) {
  TRACE_EVENT_WITH_FLOW1(
      "devtools", "DevToolsSession::DispatchProtocolCommand", call_id,
      TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN, "call_id", call_id);
  return DispatchProtocolCommandImpl(call_id, method, message);
}

void DevToolsSession::DispatchProtocolCommandImpl(
    int call_id,
    const String& method,
    base::span<const uint8_t> data) {
  DCHECK(crdtp::cbor::IsCBORMessage(
      crdtp::span<uint8_t>(data.data(), data.size())));
  TRACE_EVENT_WITH_FLOW1(
      "devtools", "DevToolsSession::DispatchProtocolCommandImpl", call_id,
      TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN, "call_id", call_id);
  TRACE_EVENT1("devtools", "api_call", "method_name", method);

  // IOSession does not provide ordering guarantees relative to
  // Session, so a command may come to IOSession after Session is detached,
  // and get posted to main thread to this method.
  //
  // At the same time, Session may not be garbage collected yet
  // (even though already detached), and CrossThreadWeakHandle<Session>
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
    crdtp::Dispatchable dispatchable(crdtp::SpanFrom(data));
    // This message has already been checked by content::DevToolsSession.
    DCHECK(dispatchable.ok());
    inspector_backend_dispatcher_->Dispatch(dispatchable).Run();
  }
  agent_->client_->DebuggerTaskFinished();
}

void DevToolsSession::DidStartProvisionalLoad(LocalFrame* frame) {
  if (v8_session_ && agent_->inspected_frames_->Root() == frame) {
    v8_session_->setSkipAllPauses(true);
    v8_session_->resume(true /* terminate on resume */);
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

void DevToolsSession::PaintTiming(Document* document,
                                  const char* name,
                                  double timestamp) {
  if (v8_session_ &&
      agent_->inspected_frames_->Root()->GetDocument() == document) {
    v8_session_->triggerPreciseCoverageDeltaUpdate(
        ToV8InspectorStringView(name));
  }
}

void DevToolsSession::DomContentLoadedEventFired(LocalFrame* local_frame) {
  if (v8_session_ && agent_->inspected_frames_->Root() == local_frame) {
    v8_session_->triggerPreciseCoverageDeltaUpdate(
        ToV8InspectorStringView("DomContentLoaded"));
  }
}

void DevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponse(call_id, message->Serialize());
}

void DevToolsSession::FallThrough(int call_id,
                                  crdtp::span<uint8_t> method,
                                  crdtp::span<uint8_t> message) {
  // There's no other layer to handle the command.
  NOTREACHED_IN_MIGRATION();
}

void DevToolsSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  SendProtocolResponse(call_id, Get8BitStringFrom(message.get()));
}

void DevToolsSession::SendProtocolResponse(int call_id,
                                           std::vector<uint8_t> message) {
  TRACE_EVENT_WITH_FLOW1(
      "devtools", "DevToolsSession::SendProtocolResponse", call_id,
      TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN, "call_id", call_id);
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

void DevToolsSession::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> notification) {
  if (IsDetached())
    return;
  notification_queue_.push_back(WTF::BindOnce(
      [](std::unique_ptr<protocol::Serializable> notification) {
        return notification->Serialize();
      },
      std::move(notification)));
}

void DevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> notification) {
  if (IsDetached())
    return;
  notification_queue_.push_back(WTF::BindOnce(
      [](std::unique_ptr<v8_inspector::StringBuffer> notification) {
        return Get8BitStringFrom(notification.get());
      },
      std::move(notification)));
}

void DevToolsSession::flushProtocolNotifications() {
  FlushProtocolNotifications();
}

void DevToolsSession::FlushProtocolNotifications() {
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

void DevToolsSession::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(host_remote_);
  visitor->Trace(agent_);
  visitor->Trace(agents_);
}

blink::mojom::blink::DevToolsMessagePtr DevToolsSession::FinalizeMessage(
    std::vector<uint8_t> message) const {
  std::vector<uint8_t> message_to_send = std::move(message);
  if (!session_id_.empty()) {
    crdtp::Status status = crdtp::cbor::AppendString8EntryToCBORMap(
        crdtp::SpanFrom(kSessionId), crdtp::SpanFrom(session_id_.Ascii()),
        &message_to_send);
    CHECK(status.ok()) << status.ToASCIIString();
  }
  if (!client_expects_binary_responses_) {
    std::vector<uint8_t> json;
    crdtp::Status status =
        crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(message_to_send), &json);
    CHECK(status.ok()) << status.ToASCIIString();
    message_to_send = std::move(json);
  }
  auto mojo_msg = mojom::blink::DevToolsMessage::New();
  mojo_msg->data = std::move(message_to_send);
  return mojo_msg;
}

}  // namespace blink
