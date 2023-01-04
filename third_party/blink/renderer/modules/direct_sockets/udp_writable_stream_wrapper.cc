// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"

#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// UDPWritableStreamWrapper definition

UDPWritableStreamWrapper::UDPWritableStreamWrapper(
    ScriptState* script_state,
    CloseOnceCallback on_close,
    const Member<UDPSocketMojoRemote> udp_socket)
    : WritableStreamWrapper(script_state),
      on_close_(std::move(on_close)),
      udp_socket_(udp_socket) {
  ScriptState::Scope scope(script_state);

  auto* sink = WritableStreamWrapper::MakeForwardingUnderlyingSink(this);
  SetSink(sink);

  auto* writable = WritableStream::CreateWithCountQueueingStrategy(
      script_state, sink, /*high_water_mark=*/1);
  SetWritable(writable);
}

bool UDPWritableStreamWrapper::HasPendingWrite() const {
  return !!write_promise_resolver_;
}

void UDPWritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(udp_socket_);
  visitor->Trace(write_promise_resolver_);
  WritableStreamWrapper::Trace(visitor);
}

void UDPWritableStreamWrapper::OnAbortSignal() {
  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(
        Controller()->signal()->reason(GetScriptState()));
    write_promise_resolver_ = nullptr;
  }
}

ScriptPromise UDPWritableStreamWrapper::Write(ScriptValue chunk,
                                              ExceptionState& exception_state) {
  DCHECK(udp_socket_->get().is_bound());

  UDPMessage* message = UDPMessage::Create(GetScriptState()->GetIsolate(),
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

  DCHECK(!write_promise_resolver_);
  write_promise_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(
      GetScriptState(), exception_state.GetContext());

  // Why not just return write_promise_resolver_->Promise()?
  // In view of the async nature of the write handler, the callback might get
  // executed earlier than the function return statement. There are two
  // concerns related to that behavior:
  // -- write_promise_resolver_ will be set to nullptr and the above call with
  // crash;
  // -- write_promise_resolver_->Reject() will be called earlier than
  // write_promise_resolver_->Promise(), and the resulting promise will be dummy
  // (i.e. fulfilled by default).
  ScriptPromise promise = write_promise_resolver_->Promise();
  udp_socket_->get()->Send(data,
                           WTF::BindOnce(&UDPWritableStreamWrapper::OnSend,
                                         WrapWeakPersistent(this)));
  return promise;
}

void UDPWritableStreamWrapper::OnSend(int32_t result) {
  if (write_promise_resolver_) {
    if (result == net::Error::OK) {
      write_promise_resolver_->Resolve();
      write_promise_resolver_ = nullptr;
    } else {
      ErrorStream(result);
    }
    DCHECK(!write_promise_resolver_);
  }
}

void UDPWritableStreamWrapper::CloseStream() {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kClosed);
  DCHECK(!write_promise_resolver_);

  std::move(on_close_).Run(/*exception=*/ScriptValue());
}

void UDPWritableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kAborted);

  auto* script_state = write_promise_resolver_
                           ? write_promise_resolver_->GetScriptState()
                           : GetScriptState();
  // Scope is needed because there's no ScriptState* on the call stack for
  // ScriptValue::From.
  ScriptState::Scope scope{script_state};

  auto exception = ScriptValue::From(
      script_state,
      V8ThrowDOMException::CreateOrDie(script_state->GetIsolate(),
                                       DOMExceptionCode::kNetworkError,
                                       String{"Stream aborted by the remote: " +
                                              net::ErrorToString(error_code)}));

  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(exception);
    write_promise_resolver_ = nullptr;
  } else {
    Controller()->error(script_state, exception);
  }

  std::move(on_close_).Run(exception);
}

}  // namespace blink
