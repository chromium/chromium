// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"

#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_dns_query_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/report.h"
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
    const Member<UDPSocketMojoRemote> udp_socket,
    network::mojom::blink::RestrictedUDPSocketMode mode,
    uint64_t inspector_id)
    : WritableStreamWrapper(script_state),
      on_close_(std::move(on_close)),
      udp_socket_(udp_socket),
      mode_(mode),
      inspector_id_(inspector_id) {
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

ScriptPromise<IDLUndefined> UDPWritableStreamWrapper::Write(
    ScriptValue chunk,
    ExceptionState& exception_state) {
  DCHECK(udp_socket_->get().is_bound());

  UDPMessage* message = UDPMessage::Create(GetScriptState()->GetIsolate(),
                                           chunk.V8Value(), exception_state);
  if (exception_state.HadException()) {
    return EmptyPromise();
  }

  if (!message->hasData()) {
    exception_state.ThrowTypeError("UDPMessage: missing 'data' field.");
    return EmptyPromise();
  }

  std::optional<net::HostPortPair> dest_addr;
  if (message->hasRemoteAddress() && message->hasRemotePort()) {
    if (mode_ == network::mojom::RestrictedUDPSocketMode::CONNECTED) {
      exception_state.ThrowTypeError(
          "UDPMessage: 'remoteAddress' and 'remotePort' must not be specified "
          "in 'connected' mode.");
      return EmptyPromise();
    }
    dest_addr = net::HostPortPair(message->remoteAddress().Utf8(),
                                  message->remotePort());
  } else if (message->hasRemoteAddress() || message->hasRemotePort()) {
    exception_state.ThrowTypeError(
        "UDPMessage: either none or both 'remoteAddress' and 'remotePort' "
        "fields must be specified.");
    return EmptyPromise();
  } else if (mode_ == network::mojom::RestrictedUDPSocketMode::BOUND) {
    exception_state.ThrowTypeError(
        "UDPMessage: 'remoteAddress' and 'remotePort' must be specified "
        "in 'bound' mode.");
    return EmptyPromise();
  }

  auto dns_query_type = net::DnsQueryType::UNSPECIFIED;
  if (message->hasDnsQueryType()) {
    if (mode_ == network::mojom::RestrictedUDPSocketMode::CONNECTED) {
      exception_state.ThrowTypeError(
          "UDPMessage: 'dnsQueryType' must not be specified "
          "in 'connected' mode.");
      return EmptyPromise();
    }
    switch (message->dnsQueryType().AsEnum()) {
      case V8SocketDnsQueryType::Enum::kIpv4:
        dns_query_type = net::DnsQueryType::A;
        break;
      case V8SocketDnsQueryType::Enum::kIpv6:
        dns_query_type = net::DnsQueryType::AAAA;
        break;
    }
  }

  DOMArrayPiece array_piece(message->data());
  base::span<const uint8_t> data = array_piece.ByteSpan();
  if (data.empty()) {
    exception_state.ThrowTypeError(
        "UDPMessage: 'data' field must not be empty.");
    return EmptyPromise();
  }

  DCHECK(!write_promise_resolver_);
  write_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          GetScriptState(), exception_state.GetContext());

  auto callback =
      BindOnce(&UDPWritableStreamWrapper::OnSend, WrapWeakPersistent(this));
  if (dest_addr) {
    udp_socket_->get()->SendTo(data, *dest_addr, dns_query_type,
                               std::move(callback));
  } else {
    udp_socket_->get()->Send(data, std::move(callback));
  }

  // report to CDP
  {
    std::optional<String> probe_remote_addr;
    std::optional<uint16_t> probe_remote_port;
    if (message->hasRemoteAddress()) {
      probe_remote_addr = message->remoteAddress();
    }
    if (message->hasRemotePort()) {
      probe_remote_port = message->remotePort();
    }
    probe::DirectUDPSocketChunkSent(*GetScriptState(), inspector_id_, data,
                                    std::move(probe_remote_addr),
                                    std::move(probe_remote_port));
  }

  return write_promise_resolver_->Promise();
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

  std::move(on_close_).Run(/*exception=*/v8::Local<v8::Value>(),
                           /*net_error=*/net::OK);
}

void UDPWritableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kAborted);

  // Error codes are negative.
  base::UmaHistogramSparse("DirectSockets.UDPWritableStreamError", -error_code);

  auto* script_state = write_promise_resolver_
                           ? write_promise_resolver_->GetScriptState()
                           : GetScriptState();
  // Scope is needed because there's no ScriptState* on the call stack for
  // ScriptValue.
  ScriptState::Scope scope{script_state};

  auto message =
      String{"Stream aborted by the remote: " + net::ErrorToString(error_code)};

  auto exception = V8ThrowDOMException::CreateOrDie(
      script_state->GetIsolate(), DOMExceptionCode::kNetworkError, message);

  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(exception);
    write_promise_resolver_ = nullptr;
  } else {
    Controller()->error(script_state,
                        ScriptValue(script_state->GetIsolate(), exception));
  }

  std::move(on_close_).Run(exception, error_code);
}

}  // namespace blink
