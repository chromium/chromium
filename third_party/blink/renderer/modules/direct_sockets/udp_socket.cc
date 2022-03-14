// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_socket.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kUDPNetworkFailuresHistogramName[] =
    "DirectSockets.UDPNetworkFailures";

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

UDPSocket::UDPSocket(ExecutionContext* execution_context,
                     ScriptPromiseResolver& resolver)
    : ExecutionContextClient(execution_context),
      init_resolver_(&resolver),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kOutstandingNetworkRequestDirectSocket,
              {SchedulingPolicy::DisableBackForwardCache()})),
      udp_socket_(MakeGarbageCollected<UDPSocketMojoRemote>(execution_context)),
      socket_listener_receiver_(this, execution_context) {
  DCHECK(init_resolver_);
}

UDPSocket::~UDPSocket() = default;

mojo::PendingReceiver<blink::mojom::blink::DirectUDPSocket>
UDPSocket::GetUDPSocketReceiver() {
  DCHECK(init_resolver_);
  return udp_socket_->get().BindNewPipeAndPassReceiver(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
}

mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
UDPSocket::GetUDPSocketListener() {
  DCHECK(init_resolver_);
  auto result = socket_listener_receiver_.BindNewPipeAndPassRemote(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

  socket_listener_receiver_.set_disconnect_handler(WTF::Bind(
      &UDPSocket::OnSocketListenerConnectionError, WrapPersistent(this)));

  return result;
}

void UDPSocket::Init(int32_t result,
                     const absl::optional<net::IPEndPoint>& local_addr,
                     const absl::optional<net::IPEndPoint>& peer_addr) {
  DCHECK(init_resolver_);
  if (result == net::Error::OK && peer_addr.has_value()) {
    peer_addr_ = peer_addr;
    local_addr_ = local_addr;
    udp_readable_stream_wrapper_ =
        MakeGarbageCollected<UDPReadableStreamWrapper>(
            init_resolver_->GetScriptState(), udp_socket_,
            WTF::Bind(&UDPSocket::CloseInternal, WrapWeakPersistent(this)));
    udp_writable_stream_wrapper_ =
        MakeGarbageCollected<UDPWritableStreamWrapper>(
            init_resolver_->GetScriptState(), udp_socket_);
    init_resolver_->Resolve(this);
  } else {
    if (result != net::Error::OK) {
      // Error codes are negative.
      base::UmaHistogramSparse(kUDPNetworkFailuresHistogramName, -result);
    }
    init_resolver_->Reject(CreateDOMExceptionFromNetErrorCode(result));
  }
  init_resolver_ = nullptr;
}

ScriptPromise UDPSocket::close(ScriptState* script_state, ExceptionState&) {
  DoClose();

  return ScriptPromise::CastUndefined(script_state);
}

ReadableStream* UDPSocket::readable() const {
  DCHECK(udp_readable_stream_wrapper_);
  return udp_readable_stream_wrapper_->Readable();
}

WritableStream* UDPSocket::writable() const {
  DCHECK(udp_writable_stream_wrapper_);
  return udp_writable_stream_wrapper_->Writable();
}

String UDPSocket::remoteAddress() const {
  return String::FromUTF8(peer_addr_->ToStringWithoutPort());
}

uint16_t UDPSocket::remotePort() const {
  return peer_addr_->port();
}

uint16_t UDPSocket::localPort() const {
  return local_addr_->port();
}

// Invoked when data is received.
// - When UDPSocket is used with Bind() (i.e. when localAddress/localPort in
// options)
//   On success, |result| is net::OK. |src_addr| indicates the address of the
//   sender. |data| contains the received data.
//   On failure, |result| is a negative network error code. |data| is null.
//   |src_addr| might be null.
// - When UDPSocket is used with Connect():
//   |src_addr| is always null. Data are always received from the remote
//   address specified in Connect().
//   On success, |result| is net::OK. |data| contains the received data.
//   On failure, |result| is a negative network error code. |data| is null.
//
// Note that in both cases, |data| can be an empty buffer when |result| is
// net::OK, which indicates a zero-byte payload.
// For further details please refer to the
// services/network/public/mojom/udp_socket.mojom file.
void UDPSocket::OnReceived(int32_t result,
                           const absl::optional<::net::IPEndPoint>& src_addr,
                           absl::optional<::base::span<const ::uint8_t>> data) {
  if (result != net::Error::OK) {
    DoClose();
    return;
  }

  udp_readable_stream_wrapper_->AcceptDatagram(
      *data, src_addr ? *src_addr : *peer_addr_);
}

bool UDPSocket::HasPendingActivity() const {
  if (!udp_writable_stream_wrapper_) {
    return false;
  }
  return udp_writable_stream_wrapper_->HasPendingActivity();
}

void UDPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(init_resolver_);
  visitor->Trace(udp_readable_stream_wrapper_);
  visitor->Trace(udp_writable_stream_wrapper_);
  visitor->Trace(udp_socket_);
  visitor->Trace(socket_listener_receiver_);
  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void UDPSocket::OnSocketListenerConnectionError() {
  DoClose();
}

void UDPSocket::DoClose() {
  if (closed_) {
    return;
  }

  if (udp_readable_stream_wrapper_) {
    // Closes the readable stream wrapper and executes a callback that performs
    // CloseInternal(). The goal is to support the closing logic from both
    // sides -- i.e. to make udpSocket.close() and
    // udpSocket.readable.getReader().cancel() achieve the same result.
    // §9.2.12: https://www.w3.org/TR/tcp-udp-sockets/#widl-UDPSocket-readable
    udp_readable_stream_wrapper_->Close();
    DCHECK(closed_);  // CloseInternal() is called by UDPReadableStreamWrapper.
  } else {
    CloseInternal();
  }
}

void UDPSocket::CloseInternal() {
  closed_ = true;

  init_resolver_ = nullptr;
  socket_listener_receiver_.reset();

  // Reject pending write promises.
  if (udp_writable_stream_wrapper_) {
    udp_writable_stream_wrapper_->Close();
  }
  // Close the socket.
  udp_socket_->Close();

  feature_handle_for_scheduler_.reset();
}

}  // namespace blink
