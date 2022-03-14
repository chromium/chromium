// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kTCPNetworkFailuresHistogramName[] =
    "DirectSockets.TCPNetworkFailures";

std::pair<DOMExceptionCode, String>
CreateDOMExceptionCodeAndMessageFromNetErrorCode(int32_t net_error) {
  switch (net_error) {
    case net::ERR_NAME_NOT_RESOLVED:
      return {DOMExceptionCode::kNetworkError,
              "Hostname couldn't be resolved."};
    case net::ERR_INVALID_URL:
      return {DOMExceptionCode::kDataError, "Supplied url is not valid."};
    case net::ERR_UNEXPECTED:
      return {DOMExceptionCode::kUnknownError, "Unexpected error occured."};
    case net::ERR_ACCESS_DENIED:
      return {DOMExceptionCode::kInvalidAccessError,
              "Access to the requested host is blocked."};
    case net::ERR_BLOCKED_BY_RESPONSE:
      return {
          DOMExceptionCode::kInvalidAccessError,
          "Access to the requested host is blocked by cross-origin policy."};
    default:
      return {DOMExceptionCode::kNetworkError, "Network Error."};
  }
}

DOMException* CreateDOMExceptionFromNetErrorCode(int32_t net_error) {
  auto [code, message] =
      CreateDOMExceptionCodeAndMessageFromNetErrorCode(net_error);
  return MakeGarbageCollected<DOMException>(code, std::move(message));
}

}  // namespace

TCPSocket::TCPSocket(ExecutionContext* execution_context,
                     ScriptPromiseResolver& resolver)
    : ExecutionContextClient(execution_context),
      resolver_(&resolver),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kOutstandingNetworkRequestDirectSocket,
              {SchedulingPolicy::DisableBackForwardCache()})),
      tcp_socket_{execution_context},
      socket_observer_receiver_{this, execution_context} {
  DCHECK(resolver_);
}

TCPSocket::~TCPSocket() = default;

mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
TCPSocket::GetTCPSocketReceiver() {
  DCHECK(resolver_);
  return tcp_socket_.BindNewPipeAndPassReceiver(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
}

mojo::PendingRemote<network::mojom::blink::SocketObserver>
TCPSocket::GetTCPSocketObserver() {
  DCHECK(resolver_);
  auto result = socket_observer_receiver_.BindNewPipeAndPassRemote(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

  socket_observer_receiver_.set_disconnect_handler(WTF::Bind(
      &TCPSocket::OnSocketObserverConnectionError, WrapPersistent(this)));

  return result;
}

void TCPSocket::Init(int32_t result,
                     const absl::optional<net::IPEndPoint>& local_addr,
                     const absl::optional<net::IPEndPoint>& peer_addr,
                     mojo::ScopedDataPipeConsumerHandle receive_stream,
                     mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(resolver_);
  DCHECK(!tcp_readable_stream_wrapper_);
  DCHECK(!tcp_writable_stream_wrapper_);
  if (result == net::Error::OK && peer_addr.has_value()) {
    local_addr_ = local_addr;
    peer_addr_ = peer_addr;
    tcp_readable_stream_wrapper_ =
        MakeGarbageCollected<TCPReadableStreamWrapper>(
            resolver_->GetScriptState(),
            WTF::Bind(&TCPSocket::OnReadableStreamAbort,
                      WrapWeakPersistent(this)),
            std::move(receive_stream));
    tcp_writable_stream_wrapper_ =
        MakeGarbageCollected<TCPWritableStreamWrapper>(
            resolver_->GetScriptState(),
            WTF::Bind(&TCPSocket::OnWritableStreamAbort,
                      WrapWeakPersistent(this)),
            std::move(send_stream));
    resolver_->Resolve(this);
  } else {
    if (result != net::Error::OK) {
      // Error codes are negative.
      base::UmaHistogramSparse(kTCPNetworkFailuresHistogramName, -result);
    }
    resolver_->Reject(CreateDOMExceptionFromNetErrorCode(result));
    socket_observer_receiver_.reset();
  }
  resolver_ = nullptr;
}

ScriptPromise TCPSocket::close(ScriptState* script_state, ExceptionState&) {
  DoClose(/*is_local_close=*/true);

  return ScriptPromise::CastUndefined(script_state);
}

ReadableStream* TCPSocket::readable() const {
  DCHECK(tcp_readable_stream_wrapper_);
  return tcp_readable_stream_wrapper_->Readable();
}

WritableStream* TCPSocket::writable() const {
  DCHECK(tcp_writable_stream_wrapper_);
  return tcp_writable_stream_wrapper_->Writable();
}

String TCPSocket::remoteAddress() const {
  DCHECK(peer_addr_);
  return String::FromUTF8(peer_addr_->ToStringWithoutPort());
}

uint16_t TCPSocket::remotePort() const {
  DCHECK(peer_addr_);
  return peer_addr_->port();
}

void TCPSocket::OnReadError(int32_t net_error) {
  if (net_error > 0 || net_error == net::Error::ERR_IO_PENDING) {
    return;
  }

  ResetReadableStream();
}

void TCPSocket::OnWriteError(int32_t net_error) {
  if (net_error > 0 || net_error == net::Error::ERR_IO_PENDING) {
    return;
  }

  ResetWritableStream();
}

void TCPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(tcp_readable_stream_wrapper_);
  visitor->Trace(tcp_writable_stream_wrapper_);
  visitor->Trace(tcp_socket_);
  visitor->Trace(socket_observer_receiver_);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void TCPSocket::OnSocketObserverConnectionError() {
  DoClose(/*is_local_close=*/false);
}

void TCPSocket::OnReadableStreamAbort() {
  ResetWritableStream();
}

void TCPSocket::OnWritableStreamAbort() {
  ResetReadableStream();
}

void TCPSocket::DoClose(bool is_local_close) {
  local_addr_ = absl::nullopt;
  peer_addr_ = absl::nullopt;
  tcp_socket_.reset();
  socket_observer_receiver_.reset();
  feature_handle_for_scheduler_.reset();

  if (resolver_) {
    DOMExceptionCode code = is_local_close ? DOMExceptionCode::kAbortError
                                           : DOMExceptionCode::kNetworkError;
    String message =
        String::Format("The request was aborted %s",
                       is_local_close ? "locally" : "due to connection error");
    resolver_->Reject(MakeGarbageCollected<DOMException>(code, message));
    resolver_ = nullptr;

    DCHECK(!tcp_readable_stream_wrapper_);
    DCHECK(!tcp_writable_stream_wrapper_);

    return;
  }

  ResetReadableStream();
  ResetWritableStream();
}

void TCPSocket::ResetReadableStream() {
  if (!tcp_readable_stream_wrapper_)
    return;

  if (tcp_readable_stream_wrapper_->GetState() ==
      TCPReadableStreamWrapper::State::kAborted) {
    return;
  }
  tcp_readable_stream_wrapper_->Reset();
  tcp_readable_stream_wrapper_ = nullptr;
}

void TCPSocket::ResetWritableStream() {
  if (!tcp_writable_stream_wrapper_)
    return;

  if (tcp_writable_stream_wrapper_->GetState() ==
      TCPWritableStreamWrapper::State::kAborted) {
    return;
  }
  tcp_writable_stream_wrapper_->Reset();
  tcp_writable_stream_wrapper_ = nullptr;
}

}  // namespace blink
