// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"

#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// UDPWritableStreamWrapper::UnderlyingSink declaration

class UDPWritableStreamWrapper::UDPUnderlyingSink final
    : public WritableStreamWrapper::UnderlyingSink {
 public:
  explicit UDPUnderlyingSink(UDPWritableStreamWrapper* writable_stream_wrapper)
      : WritableStreamWrapper::UnderlyingSink(writable_stream_wrapper) {}

  ScriptPromise close(ScriptState* script_state, ExceptionState&) override {
    GetWritableStreamWrapper()->CloseStream(/*error=*/false);
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    WritableStreamWrapper::UnderlyingSink::Trace(visitor);
  }
};

// UDPWritableStreamWrapper definition

UDPWritableStreamWrapper::UDPWritableStreamWrapper(
    ScriptState* script_state,
    const Member<UDPSocketMojoRemote> udp_socket)
    : WritableStreamWrapper(script_state), udp_socket_(udp_socket) {
  InitSinkAndWritable(/*sink=*/MakeGarbageCollected<UDPUnderlyingSink>(this),
                      /*high_water_mark=*/1);
}

bool UDPWritableStreamWrapper::HasPendingWrite() const {
  return !!send_resolver_;
}

void UDPWritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(udp_socket_);
  visitor->Trace(send_resolver_);
  WritableStreamWrapper::Trace(visitor);
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

  DCHECK(!send_resolver_);
  send_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());

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

void UDPWritableStreamWrapper::CloseStream(bool error) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(error ? State::kAborted : State::kClosed);

  ScriptState::Scope scope(GetScriptState());

  ScriptValue exception =
      error ? CreateException(GetScriptState(), DOMExceptionCode::kNetworkError,
                              "Connection aborted by remote")
            : CreateException(GetScriptState(),
                              DOMExceptionCode::kInvalidStateError,
                              "Stream closed.");

  if (send_resolver_) {
    send_resolver_->Reject(exception);
    send_resolver_ = nullptr;
  }

  Controller()->error(GetScriptState(), exception);
}

}  // namespace blink
