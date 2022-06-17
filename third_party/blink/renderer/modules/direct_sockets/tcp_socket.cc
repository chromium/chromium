// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_close_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_connection.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/direct_sockets_service_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kTCPNetworkFailuresHistogramName[] =
    "DirectSockets.TCPNetworkFailures";

bool CheckKeepAliveOptionsValidity(const TCPSocketOptions* options,
                                   ExceptionState& exception_state) {
  if (options->hasKeepAlive() && options->keepAlive()) {
    if (!options->hasKeepAliveDelay()) {
      exception_state.ThrowTypeError(
          "keepAliveDelay must be set when keepAlive = true.");
      return false;
    }
    if (base::Milliseconds(options->keepAliveDelay()) < base::Seconds(1)) {
      exception_state.ThrowTypeError(
          "keepAliveDelay must be no less than one second.");
      return false;
    }
  } else {
    if (options->hasKeepAliveDelay()) {
      exception_state.ThrowTypeError(
          "keepAliveDelay must not be set when keepAlive = "
          "false or missing.");
      return false;
    }
  }
  return true;
}

mojom::blink::DirectSocketOptionsPtr CreateTCPSocketOptions(
    const String& remote_address,
    const uint16_t remote_port,
    const TCPSocketOptions* options,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectSocketOptions::New();

  socket_options->remote_hostname = remote_address;
  socket_options->remote_port = remote_port;

  const bool has_full_local_address =
      options->hasLocalAddress() && options->hasLocalPort();

  if (const bool has_partial_local_address =
          options->hasLocalAddress() || options->hasLocalPort();
      has_partial_local_address && !has_full_local_address) {
    exception_state.ThrowTypeError("Incomplete local address specified.");
    return {};
  }

  if (!CheckKeepAliveOptionsValidity(options, exception_state)) {
    return {};
  }

  if (options->hasNoDelay()) {
    socket_options->no_delay = options->noDelay();
  }
  if (options->hasKeepAlive()) {
    socket_options->keep_alive_options =
        network::mojom::blink::TCPKeepAliveOptions::New(
            /*enable=*/options->keepAlive(),
            /*delay=*/base::Milliseconds(options->getKeepAliveDelayOr(0))
                .InSeconds());
  }

  if (has_full_local_address) {
    socket_options->local_hostname = options->localAddress();
    socket_options->local_port = options->localPort();
  }

  if (options->hasSendBufferSize()) {
    socket_options->send_buffer_size = options->sendBufferSize();
  }
  if (options->hasReceiveBufferSize()) {
    socket_options->receive_buffer_size = options->receiveBufferSize();
  }

  return socket_options;
}

}  // namespace

// static
TCPSocket* TCPSocket::Create(ScriptState* script_state,
                             const String& remoteAddress,
                             const uint16_t remotePort,
                             const TCPSocketOptions* options,
                             ExceptionState& exception_state) {
  if (!Socket::CheckContextAndPermissions(script_state, exception_state)) {
    return nullptr;
  }

  auto* socket = MakeGarbageCollected<TCPSocket>(script_state);
  if (!socket->Open(remoteAddress, remotePort, options, exception_state)) {
    return nullptr;
  }
  return socket;
}

TCPSocket::TCPSocket(ScriptState* script_state)
    : Socket(script_state),
      tcp_socket_{GetExecutionContext()},
      socket_observer_{this, GetExecutionContext()} {}

TCPSocket::~TCPSocket() = default;

bool TCPSocket::Open(const String& remote_address,
                     const uint16_t remote_port,
                     const TCPSocketOptions* options,
                     ExceptionState& exception_state) {
  auto open_tcp_socket_options = CreateTCPSocketOptions(
      remote_address, remote_port, options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  ConnectService();

  service_->get()->OpenTcpSocket(
      std::move(open_tcp_socket_options), GetTCPSocketReceiver(),
      GetTCPSocketObserver(),
      WTF::Bind(&TCPSocket::Init, WrapPersistent(this)));

  return true;
}

void TCPSocket::Init(int32_t result,
                     const absl::optional<net::IPEndPoint>& local_addr,
                     const absl::optional<net::IPEndPoint>& peer_addr,
                     mojo::ScopedDataPipeConsumerHandle receive_stream,
                     mojo::ScopedDataPipeProducerHandle send_stream) {
  if (result == net::OK && peer_addr) {
    readable_stream_wrapper_ = MakeGarbageCollected<TCPReadableStreamWrapper>(
        script_state_,
        WTF::Bind(&TCPSocket::CloseInternal, WrapWeakPersistent(this)),
        std::move(receive_stream));
    writable_stream_wrapper_ = MakeGarbageCollected<TCPWritableStreamWrapper>(
        script_state_,
        WTF::Bind(&TCPSocket::CloseInternal, WrapWeakPersistent(this)),
        std::move(send_stream));

    auto* connection = TCPSocketConnection::Create();

    connection->setReadable(readable_stream_wrapper_->Readable());
    connection->setWritable(writable_stream_wrapper_->Writable());

    connection->setRemoteAddress(String{peer_addr->ToStringWithoutPort()});
    connection->setRemotePort(peer_addr->port());

    connection->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    connection->setLocalPort(local_addr->port());

    connection_resolver_->Resolve(connection);
  } else {
    if (result != net::OK) {
      // Error codes are negative.
      base::UmaHistogramSparse(kTCPNetworkFailuresHistogramName, -result);
    }
    connection_resolver_->Reject(CreateDOMExceptionFromNetErrorCode(result));
    CloseServiceAndResetFeatureHandle();

    closed_resolver_->Reject();
  }

  connection_resolver_ = nullptr;
}

mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
TCPSocket::GetTCPSocketReceiver() {
  return tcp_socket_.BindNewPipeAndPassReceiver(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
}

mojo::PendingRemote<network::mojom::blink::SocketObserver>
TCPSocket::GetTCPSocketObserver() {
  auto pending_remote = socket_observer_.BindNewPipeAndPassRemote(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

  socket_observer_.set_disconnect_handler(
      WTF::Bind(&TCPSocket::OnSocketConnectionError, WrapPersistent(this)));

  return pending_remote;
}

void TCPSocket::OnSocketConnectionError() {
  if (Initialized()) {
    CloseInternal(/*error=*/true);
  }
}

void TCPSocket::OnServiceConnectionError() {
  if (connection_resolver_) {
    Init(net::ERR_CONTEXT_SHUT_DOWN, absl::nullopt, absl::nullopt,
         mojo::ScopedDataPipeConsumerHandle(),
         mojo::ScopedDataPipeProducerHandle());
  }
}

void TCPSocket::OnReadError(int32_t net_error) {
  if (net_error > 0 || net_error == net::Error::ERR_IO_PENDING) {
    return;
  }

  readable_stream_wrapper_->CloseStream(/*error=*/true);
}

void TCPSocket::OnWriteError(int32_t net_error) {
  if (net_error > 0 || net_error == net::Error::ERR_IO_PENDING) {
    return;
  }

  writable_stream_wrapper_->CloseStream(/*error=*/true);
}

void TCPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(tcp_socket_);
  visitor->Trace(socket_observer_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

bool TCPSocket::HasPendingActivity() const {
  return Socket::HasPendingActivity();
}

void TCPSocket::Close(const SocketCloseOptions* options,
                      ExceptionState& exception_state) {
  if (!Initialized()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is not properly initialized.");
    return;
  }

  if (Closed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is already closed or errored.");
    return;
  }

  if (!options->hasForce() || !options->force()) {
    if (readable_stream_wrapper_->Locked() ||
        writable_stream_wrapper_->Locked()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Close called on locked streams.");
      return;
    }
  }

  CloseInternal(/*error=*/false);
  DCHECK(Closed());
}

void TCPSocket::CloseInternal(bool error) {
  tcp_socket_.reset();
  socket_observer_.reset();

  CloseServiceAndResetFeatureHandle();
  ResolveOrRejectClosed(error);

  readable_stream_wrapper_->CloseStream(error);
  writable_stream_wrapper_->CloseStream(error);
}

}  // namespace blink
