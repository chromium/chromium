// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_devtools_session.h"

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_inspector_util.h"
#include "content/services/auction_worklet/debug_command_queue.h"
#include "content/services/auction_worklet/protocol/event_breakpoints.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/frontend_channel.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/protocol_core.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace auction_worklet {

// BreakpointHandler implements the
// EventBreakpoints.setInstrumentationBreakpoint and
// EventBreakpoints.removeInstrumentationBreakpoint messages.
class AuctionV8DevToolsSession::BreakpointHandler
    : public auction_worklet::protocol::EventBreakpoints::Backend {
 public:
  // `v8_session` is expected to outlast invocation of any methods other than
  // the destructor on `this`.
  explicit BreakpointHandler(v8_inspector::V8InspectorSession* v8_session)
      : v8_session_(v8_session) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  }

  ~BreakpointHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  }

  void MaybeTriggerInstrumentationBreakpoint(const std::string& name) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
    if (instrumentation_breakpoints_.find(name) !=
        instrumentation_breakpoints_.end()) {
      std::string category("EventListener");
      std::string aux_json = base::StringPrintf(
          R"({"eventName":"instrumentation:%s"})", name.c_str());
      v8_session_->schedulePauseOnNextStatement(ToStringView(category),
                                                ToStringView(aux_json));
    }
  }

  void DetachFromV8Session() { v8_session_ = nullptr; }

 private:
  crdtp::DispatchResponse SetInstrumentationBreakpoint(
      const std::string& event_name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
    instrumentation_breakpoints_.insert(event_name);
    return crdtp::DispatchResponse::Success();
  }

  crdtp::DispatchResponse RemoveInstrumentationBreakpoint(
      const std::string& event_name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
    instrumentation_breakpoints_.erase(event_name);
    return crdtp::DispatchResponse::Success();
  }

  raw_ptr<v8_inspector::V8InspectorSession> v8_session_;
  std::set<std::string> instrumentation_breakpoints_;
  SEQUENCE_CHECKER(v8_sequence_checker_);
};

// IOSession, which handles the pipe passed to the `io_session` parameter of
// DevToolsAgent::AttachDevToolsSession(), runs on a non-V8 sequence (except
// creation happens on the V8 thread). It's owned by the corresponding pipe.
//
// Its task is to forward messages to the v8 thread via DebugCommandQueue, with
// the `v8_thread_dispatch` callback being asked to run there to execute the
// command. The callback is responsible for dealing with possibility of the main
// session object being deleted.
class AuctionV8DevToolsSession::IOSession
    : public blink::mojom::DevToolsSession {
 public:
  using RunDispatch =
      base::RepeatingCallback<void(int32_t call_id,
                                   const std::string& method,
                                   std::vector<uint8_t> message)>;

  ~IOSession() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(io_session_receiver_sequence_checker_);
  }

  static void Create(
      mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
      scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
      scoped_refptr<DebugCommandQueue> debug_command_queue,
      RunDispatch v8_thread_dispatch) {
    auto instance = base::WrapUnique(new IOSession(
        std::move(debug_command_queue), std::move(v8_thread_dispatch)));
    io_session_receiver_sequence->PostTask(
        FROM_HERE,
        base::BindOnce(&IOSession::ConnectReceiver, std::move(instance),
                       std::move(io_session_receiver)));
  }

  // DevToolsSession implementation.
  void DispatchProtocolCommand(int32_t call_id,
                               const std::string& method,
                               base::span<const uint8_t> message) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(io_session_receiver_sequence_checker_);

    // TODO(morlovich): We probably need to do v8 interrupt on some commands
    // here. Discuss in review.
    debug_command_queue_->QueueTaskForV8Thread(
        base::BindOnce(v8_thread_dispatch_, call_id, method,
                       std::vector<uint8_t>(message.begin(), message.end())));
  }

 private:
  IOSession(scoped_refptr<DebugCommandQueue> debug_command_queue,
            RunDispatch v8_thread_dispatch)
      : debug_command_queue_(std::move(debug_command_queue)),
        v8_thread_dispatch_(v8_thread_dispatch) {
    DETACH_FROM_SEQUENCE(io_session_receiver_sequence_checker_);
  }

  static void ConnectReceiver(
      std::unique_ptr<IOSession> instance,
      mojo::PendingReceiver<blink::mojom::DevToolsSession>
          io_session_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        instance->io_session_receiver_sequence_checker_);
    mojo::MakeSelfOwnedReceiver(std::move(instance),
                                std::move(io_session_receiver));
  }

  const scoped_refptr<DebugCommandQueue> debug_command_queue_;
  RunDispatch v8_thread_dispatch_;

  SEQUENCE_CHECKER(io_session_receiver_sequence_checker_);
};

AuctionV8DevToolsSession::AuctionV8DevToolsSession(
    AuctionV8Helper* v8_helper,
    scoped_refptr<DebugCommandQueue> debug_command_queue,
    int context_group_id,
    const std::string& session_id,
    bool client_expects_binary_responses,
    bool session_waits_for_debugger,
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
    scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
    mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
    SessionDestroyedCallback on_delete_callback)
    : v8_helper_(v8_helper),
      debug_command_queue_(debug_command_queue.get()),
      context_group_id_(context_group_id),
      session_id_(session_id),
      client_expects_binary_responses_(client_expects_binary_responses),
      host_(std::move(host)),
      on_delete_callback_(std::move(on_delete_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_session_ = v8_helper_->inspector()->connect(
      context_group_id_, this /* as V8Inspector::Channel */,
      v8_inspector::StringView(), v8_inspector::V8Inspector::kFullyTrusted,
      session_waits_for_debugger
          ? v8_inspector::V8Inspector::kWaitingForDebugger
          : v8_inspector::V8Inspector::kNotWaitingForDebugger);
  IOSession::Create(
      std::move(io_session_receiver), std::move(io_session_receiver_sequence),
      std::move(debug_command_queue),
      base::BindRepeating(
          &AuctionV8DevToolsSession::DispatchProtocolCommandFromIO,
          weak_ptr_factory_.GetWeakPtr()));

  breakpoint_handler_ = std::make_unique<BreakpointHandler>(v8_session_.get());
  auction_worklet::protocol::EventBreakpoints::Dispatcher::wire(
      &fallback_dispatcher_, breakpoint_handler_.get());
}

AuctionV8DevToolsSession::~AuctionV8DevToolsSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  std::move(on_delete_callback_).Run(this);
  breakpoint_handler_->DetachFromV8Session();
  v8_session_.reset();
}

base::OnceClosure AuctionV8DevToolsSession::MakeAbortPauseCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // Note that this can be cancelled by the weak pointer only if the session
  // got unpaused by other means, since if it's paused it's not returning
  // control to the event loop, so Mojo won't get a chance to delete `this`.
  return base::BindOnce(&AuctionV8DevToolsSession::AbortDebuggerPause,
                        weak_ptr_factory_.GetWeakPtr());
}

void AuctionV8DevToolsSession::MaybeTriggerInstrumentationBreakpoint(
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  breakpoint_handler_->MaybeTriggerInstrumentationBreakpoint(name);
}

void AuctionV8DevToolsSession::DispatchProtocolCommandFromIO(
    int32_t call_id,
    const std::string& method,
    std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  DispatchProtocolCommand(call_id, method, std::move(message));
}

void AuctionV8DevToolsSession::DispatchProtocolCommand(
    int32_t call_id,
    const std::string& method,
    base::span<const uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // We always talk binary to V8 so it talks it back to us, making it easier
  // to append session ID. That's also useful for crdtp::Dispatchable.
  crdtp::span<uint8_t> message_span(message.data(), message.size());
  v8_inspector::StringView cbor_message;
  std::vector<uint8_t> converted_cbor_out;
  if (crdtp::cbor::IsCBORMessage(message_span)) {
    cbor_message = v8_inspector::StringView(message.data(), message.size());
  } else {
    crdtp::Status status =
        crdtp::json::ConvertJSONToCBOR(message_span, &converted_cbor_out);
    CHECK(status.ok()) << status.ToASCIIString();
    cbor_message = v8_inspector::StringView(converted_cbor_out.data(),
                                            converted_cbor_out.size());
  }

  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          v8_inspector::StringView(
              reinterpret_cast<const uint8_t*>(method.data()),
              method.size()))) {
    // Need v8 isolate access.
    AuctionV8Helper::FullIsolateScope v8_scope(v8_helper_);

    v8_session_->dispatchProtocolMessage(cbor_message);
  } else {
    crdtp::Dispatchable dispatchable(crdtp::span<uint8_t>(
        cbor_message.characters8(), cbor_message.length()));
    fallback_dispatcher_.Dispatch(dispatchable).Run();
  }
}

void AuctionV8DevToolsSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendProtocolResponseImpl(call_id, GetStringBytes(message.get()));
}

void AuctionV8DevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendNotificationImpl(GetStringBytes(message.get()));
}

void AuctionV8DevToolsSession::flushProtocolNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // TODO(morlovich): Consider buffering of notifications.
}

void AuctionV8DevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<crdtp::Serializable> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendProtocolResponseImpl(call_id, message->Serialize());
}

void AuctionV8DevToolsSession::SendProtocolNotification(
    std::unique_ptr<crdtp::Serializable> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();
}

void AuctionV8DevToolsSession::FallThrough(int call_id,
                                           crdtp::span<uint8_t> method,
                                           crdtp::span<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();
}

void AuctionV8DevToolsSession::FlushProtocolNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();
}

void AuctionV8DevToolsSession::AbortDebuggerPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // Note that if the session got resumed by other means before execution got
  // here V8 will simply ignore this call.
  v8_session_->resume(/*setTerminateOnResume=*/true);
}

void AuctionV8DevToolsSession::SendProtocolResponseImpl(
    int call_id,
    std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  host_->DispatchProtocolResponse(FinalizeMessage(std::move(message)), call_id,
                                  nullptr);
}

void AuctionV8DevToolsSession::SendNotificationImpl(
    std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  host_->DispatchProtocolNotification(FinalizeMessage(std::move(message)),
                                      nullptr);
}

// Add session ID and maybe convert the message into JSON format, as
// documented as requirements in DevToolsAgent.AttachDevToolsSession mojo
// method documentation, and then encapsulate it inside a mojo
// DevToolsMessage.
//
// This is pretty much a copy-paste job from
// third_party/blink/renderer/core/inspector/devtools_session.cc, just with
// content types.
blink::mojom::DevToolsMessagePtr AuctionV8DevToolsSession::FinalizeMessage(
    std::vector<uint8_t> message) const {
  std::vector<uint8_t> message_to_send = std::move(message);
  if (!session_id_.empty()) {
    crdtp::Status status = crdtp::cbor::AppendString8EntryToCBORMap(
        crdtp::SpanFrom("sessionId"), crdtp::SpanFrom(session_id_),
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
  auto mojo_msg = blink::mojom::DevToolsMessage::New();
  mojo_msg->data = std::move(message_to_send);
  return mojo_msg;
}

}  // namespace auction_worklet
