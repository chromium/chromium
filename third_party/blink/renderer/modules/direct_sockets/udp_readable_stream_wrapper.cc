// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"

#include "base/callback_forward.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

constexpr uint32_t kReadableStreamBufferSize = 32;

}

// UDPReadableStreamWrapper definition

UDPReadableStreamWrapper::UDPReadableStreamWrapper(
    ScriptState* script_state,
    CloseOnceCallback on_close,
    const Member<UDPSocketMojoRemote> udp_socket)
    : ReadableStreamWrapper(script_state),
      on_close_(std::move(on_close)),
      udp_socket_(udp_socket) {
  InitSourceAndReadable(
      /*source=*/MakeGarbageCollected<UnderlyingSource>(GetScriptState(), this),
      kReadableStreamBufferSize);
}

void UDPReadableStreamWrapper::Pull() {
  // Keep pending_receive_requests_ equal to desired_size.
  DCHECK(udp_socket_->get().is_bound());
  int32_t desired_size = static_cast<int32_t>(Controller()->DesiredSize());
  if (desired_size > pending_receive_requests_) {
    uint32_t receive_more = desired_size - pending_receive_requests_;
    udp_socket_->get()->ReceiveMore(receive_more);
    pending_receive_requests_ += receive_more;
  }
}

bool UDPReadableStreamWrapper::Push(
    base::span<const uint8_t> data,
    const absl::optional<net::IPEndPoint>& src_addr) {
  DCHECK_GT(pending_receive_requests_, 0);
  pending_receive_requests_--;

  auto* buffer = DOMUint8Array::Create(data.data(), data.size_bytes());

  auto* message = UDPMessage::Create();
  message->setData(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(buffer)));
  if (src_addr) {
    message->setRemoteAddress(String{src_addr->ToStringWithoutPort()});
    message->setRemotePort(src_addr->port());
  }

  Controller()->Enqueue(message);

  return true;
}

void UDPReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(udp_socket_);
  ReadableStreamWrapper::Trace(visitor);
}

void UDPReadableStreamWrapper::CloseStream() {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kClosed);

  std::move(on_close_).Run(/*exception=*/ScriptValue());
}

void UDPReadableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kAborted);

  auto* script_state = GetScriptState();
  // Scope is needed because there's no ScriptState* on the call stack for
  // ScriptValue::From.
  ScriptState::Scope scope{script_state};

  auto exception = ScriptValue::From(
      script_state,
      V8ThrowDOMException::CreateOrDie(script_state->GetIsolate(),
                                       DOMExceptionCode::kNetworkError,
                                       String{"Stream aborted by the remote: " +
                                              net::ErrorToString(error_code)}));

  Controller()->Error(exception);

  std::move(on_close_).Run(exception);
}

}  // namespace blink
