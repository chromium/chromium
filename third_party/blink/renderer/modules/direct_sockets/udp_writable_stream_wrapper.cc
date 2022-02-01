// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"

#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// UDPWritableStreamWrapper::UnderlyingSink declaration

class UDPWritableStreamWrapper::UnderlyingSink final
    : public UnderlyingSinkBase {
 public:
  explicit UnderlyingSink(UDPWritableStreamWrapper* udp_writable_stream_wrapper)
      : udp_writable_stream_wrapper_(udp_writable_stream_wrapper) {}

  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController* controller,
                      ExceptionState&) override;
  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState& exception_state) override;
  ScriptPromise close(ScriptState* script_state, ExceptionState&) override;
  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(udp_writable_stream_wrapper_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  const Member<UDPWritableStreamWrapper> udp_writable_stream_wrapper_;
};

// UDPWritableStreamWrapper::UnderlyingSink definition

ScriptPromise UDPWritableStreamWrapper::UnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState&) {
  udp_writable_stream_wrapper_->controller_ = controller;
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UDPWritableStreamWrapper::UnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController*,
    ExceptionState& exception_state) {
  return udp_writable_stream_wrapper_->SinkWrite(script_state, chunk,
                                                 exception_state);
}

ScriptPromise UDPWritableStreamWrapper::UnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState&) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!udp_writable_stream_wrapper_->send_resolver_);

  // It's not possible to close the writable side of the UDP socket, therefore
  // no action is taken.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UDPWritableStreamWrapper::UnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!udp_writable_stream_wrapper_->send_resolver_);

  // It's not possible to close the writable side of the UDP socket, therefore
  // no action is taken.
  return ScriptPromise::CastUndefined(script_state);
}

// UDPWritableStreamWrapper definition

UDPWritableStreamWrapper::UDPWritableStreamWrapper(
    ScriptState* script_state,
    const Member<UDPSocketMojoRemote> udp_socket)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      script_state_(script_state),
      udp_socket_(udp_socket) {
  ScriptState::Scope scope(script_state);
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state_,
      MakeGarbageCollected<UDPWritableStreamWrapper::UnderlyingSink>(this), 1);
}

UDPWritableStreamWrapper::~UDPWritableStreamWrapper() = default;

bool UDPWritableStreamWrapper::HasPendingActivity() const {
  return !!send_resolver_;
}

void UDPWritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(udp_socket_);
  visitor->Trace(send_resolver_);
  visitor->Trace(writable_);
  visitor->Trace(controller_);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

ScriptPromise UDPWritableStreamWrapper::SinkWrite(
    ScriptState* script_state,
    ScriptValue chunk,
    ExceptionState& exception_state) {
  // If socket has been closed.
  if (!udp_socket_->get().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is disconnected.");
    return ScriptPromise();
  }

  UDPMessage* message = UDPMessage::Create(script_state->GetIsolate(),
                                           chunk.V8Value(), exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  if (!message->hasData()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "UDPMessage: missing 'data' field.");
    return ScriptPromise();
  }

  DOMArrayPiece array_piece(message->data());
  base::span<const uint8_t> data{array_piece.Bytes(), array_piece.ByteLength()};

  DCHECK(!send_resolver_);
  send_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // Why not just return send_resolver_->Promise()?
  // In view of the async nature of the write handler, the callback might get
  // executed earlier than the function return statement. There are two
  // concerns related to that behavior:
  // -- send_resolver_ will be set to nullptr and the above call with crash;
  // -- send_resover_->Reject() will be called earlier than
  // send_resolver_->Promise(), and the resulting promise will be dummy
  // (i.e. fulfilled by default).
  ScriptPromise promise = send_resolver_->Promise();
  udp_socket_->get()->Send(data, WTF::Bind(&UDPWritableStreamWrapper::OnSend,
                                           WrapWeakPersistent(this)));
  return promise;
}

void UDPWritableStreamWrapper::OnSend(int32_t result) {
  if (send_resolver_) {
    if (result == net::Error::OK) {
      send_resolver_->Resolve();
    } else {
      send_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "Failed to send."));
    }
    send_resolver_ = nullptr;
  }
}

void UDPWritableStreamWrapper::Close() {
  if (send_resolver_) {
    send_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Failed to send data."));
    send_resolver_ = nullptr;
  }
}

}  // namespace blink
