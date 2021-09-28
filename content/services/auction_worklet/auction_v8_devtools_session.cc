// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_devtools_session.h"

#include <stdint.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/debug_command_queue.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/frontend_channel.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace auction_worklet {

namespace {

std::vector<uint8_t> Get8BitStringFrom(v8_inspector::StringBuffer* msg) {
  const v8_inspector::StringView& s = msg->string();
  if (s.is8Bit()) {
    return std::vector<uint8_t>(s.characters8(), s.characters8() + s.length());
  } else {
    std::string converted = base::UTF16ToUTF8(base::StringPiece16(
        reinterpret_cast<const char16_t*>(s.characters16()), s.length()));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(converted.data());
    return std::vector<uint8_t>(data, data + converted.size());
  }
}

}  // namespace

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
      : debug_command_queue_(debug_command_queue),
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

  scoped_refptr<DebugCommandQueue> debug_command_queue_;
  RunDispatch v8_thread_dispatch_;

  SEQUENCE_CHECKER(io_session_receiver_sequence_checker_);
};

AuctionV8DevToolsSession::AuctionV8DevToolsSession(
    AuctionV8Helper* v8_helper,
    scoped_refptr<DebugCommandQueue> debug_command_queue,
    int context_group_id,
    const std::string& session_id,
    bool client_expects_binary_responses,
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
    scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
    mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
    SessionDestroyedCallback on_delete_callback)
    : v8_helper_(v8_helper),
      debug_command_queue_(std::move(debug_command_queue)),
      context_group_id_(context_group_id),
      session_id_(session_id),
      client_expects_binary_responses_(client_expects_binary_responses),
      host_(std::move(host)),
      on_delete_callback_(std::move(on_delete_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_session_ = v8_helper_->inspector()->connect(
      context_group_id_, this /* as V8Inspector::Channel */,
      v8_inspector::StringView());
  IOSession::Create(
      std::move(io_session_receiver), std::move(io_session_receiver_sequence),
      debug_command_queue_,
      base::BindRepeating(
          &AuctionV8DevToolsSession::DispatchProtocolCommandFromIO,
          weak_ptr_factory_.GetWeakPtr()));
}

AuctionV8DevToolsSession::~AuctionV8DevToolsSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  std::move(on_delete_callback_).Run(this);
  v8::Locker locker(v8_helper_->isolate());
  v8_session_.reset();
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
    // Need v8 locker, isolate access.
    AuctionV8Helper::FullIsolateScope v8_scope(v8_helper_);

    v8_session_->dispatchProtocolMessage(cbor_message);
  } else {
    crdtp::Dispatchable dispatchable(crdtp::span<uint8_t>(
        cbor_message.characters8(), cbor_message.length()));
    // For now, this should just produce the proper error.
    fallback_dispatcher_.Dispatch(dispatchable).Run();
  }
}

void AuctionV8DevToolsSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendProtocolResponseImpl(call_id, Get8BitStringFrom(message.get()));
}

void AuctionV8DevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  SendNotificationImpl(Get8BitStringFrom(message.get()));
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
