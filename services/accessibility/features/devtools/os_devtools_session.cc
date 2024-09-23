// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/accessibility/features/devtools/os_devtools_session.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/accessibility/features/devtools/debug_command_queue.h"
#include "services/accessibility/features/devtools/os_devtools_agent.h"
#include "services/accessibility/features/v8_manager.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "v8/include/v8-microtask-queue.h"

namespace {
std::vector<uint8_t> GetStringBytes(const v8_inspector::StringView& s) {
  if (s.is8Bit()) {
    return std::vector<uint8_t>(s.characters8(), s.characters8() + s.length());
  }
  std::string converted = base::UTF16ToUTF8(std::u16string_view(
      reinterpret_cast<const char16_t*>(s.characters16()), s.length()));
  const uint8_t* data = reinterpret_cast<const uint8_t*>(converted.data());
  return std::vector<uint8_t>(data, data + converted.size());
}
}  // namespace

namespace ax {

// OSDevToolsSession::IOSession, which handles the pipe passed to the
// `io_session` parameter of DevToolsAgent::AttachDevToolsSession(), runs on a
// non-V8 sequence (except creation happens on the V8 thread). It's owned by the
// corresponding pipe.
//
// Its task is to forward messages to the v8 thread via DebugCommandQueue, with
// the `v8_thread_dispatch` callback being asked to run there to execute the
// command. The callback is responsible for dealing with possibility of the main
// session object being deleted.
class OSDevToolsSession::IOSession : public blink::mojom::DevToolsSession {
 public:
  using DispatchCallback =
      base::RepeatingCallback<void(int32_t call_id,
                                   const std::string& method,
                                   std::vector<uint8_t> message)>;

  ~IOSession() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(io_session_receiver_sequence_checker_);
  }

  static void Create(
      mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
      scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
      const scoped_refptr<DebugCommandQueue> debug_command_queue,
      DispatchCallback v8_thread_dispatch) {
    auto instance = base::WrapUnique(
        new IOSession(debug_command_queue, std::move(v8_thread_dispatch)));
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
    debug_command_queue_->QueueTaskForV8Thread(
        base::BindOnce(v8_thread_dispatch_, call_id, method,
                       std::vector<uint8_t>(message.begin(), message.end())));
  }

 private:
  IOSession(const scoped_refptr<DebugCommandQueue> debug_command_queue,
            DispatchCallback v8_thread_dispatch)
      : debug_command_queue_(debug_command_queue),
        v8_thread_dispatch_(std::move(v8_thread_dispatch)) {
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
  DispatchCallback v8_thread_dispatch_;

  SEQUENCE_CHECKER(io_session_receiver_sequence_checker_);
};

OSDevToolsSession::OSDevToolsSession(
    V8Environment& v8_env,
    OSDevToolsAgent& agent,
    const scoped_refptr<DebugCommandQueue> debug_command_queue,
    const std::string& session_id,
    bool client_expects_binary_responses,
    bool session_waits_for_debugger,
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
    scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
    mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
    SessionDestroyedCallback on_delete_callback)
    : v8_env_(v8_env),
      debug_command_queue_(*debug_command_queue),
      session_id_(session_id),
      client_expects_binary_responses_(client_expects_binary_responses),
      on_delete_callback_(std::move(on_delete_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  host_.Bind(std::move(host));
  // Connect v8 session.
  v8_session_ = agent.ConnectSession(this, session_waits_for_debugger);
  IOSession::Create(
      std::move(io_session_receiver), io_session_receiver_sequence,
      debug_command_queue,
      base::BindRepeating(&OSDevToolsSession::DispatchProtocolCommandFromIO,
                          weak_ptr_factory_.GetWeakPtr()));
}

OSDevToolsSession::~OSDevToolsSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  std::move(on_delete_callback_).Run(this);
  v8_session_->stop();
}

base::OnceClosure OSDevToolsSession::MakeAbortPauseCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // Note that this can be cancelled by the weak pointer only if the session
  // got unpaused by other means, since if it's paused it's not returning
  // control to the event loop, so Mojo won't get a chance to delete `this`.
  return base::BindOnce(&OSDevToolsSession::AbortDebuggerPause,
                        weak_ptr_factory_.GetWeakPtr());
}

void OSDevToolsSession::MaybeTriggerInstrumentationBreakpoint(
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // TODO(290815208)
}

void OSDevToolsSession::DispatchProtocolCommandFromIO(
    int32_t call_id,
    const std::string& method,
    std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  DispatchProtocolCommand(call_id, method, message);
}

void OSDevToolsSession::DispatchProtocolCommand(
    int32_t call_id,
    const std::string& method,
    base::span<const uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  // Binary is always used to talk to V8 so it also only talks binary back,
  // making it easier to append session ID. That's also useful for
  // crdtp::Dispatchable.
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
    auto* isolate = v8_env_->GetIsolate();
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8_session_->dispatchProtocolMessage(cbor_message);
    // Run microtasks.
    v8_env_->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(isolate);
  } else {
    crdtp::Dispatchable dispatchable(crdtp::span<uint8_t>(
        cbor_message.characters8(), cbor_message.length()));
    fallback_dispatcher_.Dispatch(dispatchable).Run();
  }
}

void OSDevToolsSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendProtocolResponseImpl(call_id, ::GetStringBytes(message.get()->string()));
}

void OSDevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendNotificationImpl(::GetStringBytes(message.get()->string()));
}

void OSDevToolsSession::flushProtocolNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();
}

void OSDevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<crdtp::Serializable> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendProtocolResponseImpl(call_id, message->Serialize());
}

void OSDevToolsSession::SendProtocolNotification(
    std::unique_ptr<crdtp::Serializable> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendNotificationImpl(message->Serialize());
}

void OSDevToolsSession::FallThrough(int call_id,
                                    crdtp::span<uint8_t> method,
                                    crdtp::span<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();
}

void OSDevToolsSession::FlushProtocolNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();
}

void OSDevToolsSession::AbortDebuggerPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // Note that if the session got resumed by other means before execution got
  // here V8 will simply ignore this call.
  v8_session_->resume(/*setTerminateOnResume=*/true);
}

void OSDevToolsSession::SendProtocolResponseImpl(int call_id,
                                                 std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  crdtp::span<uint8_t> msg_span(message.data(), message.size());
  host_->DispatchProtocolResponse(FinalizeMessage(std::move(message)), call_id,
                                  nullptr);
}

void OSDevToolsSession::SendNotificationImpl(std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  crdtp::span<uint8_t> msg_span(message.data(), message.size());
  host_->DispatchProtocolNotification(FinalizeMessage(std::move(message)),
                                      nullptr);
}

// Add session ID and maybe convert the message into JSON format, as
// documented as requirements in DevToolsAgent.AttachDevToolsSession mojo
// method documentation, and then encapsulate it inside a mojo
// DevToolsMessage.
//
// This is pretty much a copy-paste job from
// third_party/blink/renderer/core/inspector/devtools_session.cc.
// The primary difference is that namespacing of DevToolsMessage.
blink::mojom::DevToolsMessagePtr OSDevToolsSession::FinalizeMessage(
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

}  // namespace ax
