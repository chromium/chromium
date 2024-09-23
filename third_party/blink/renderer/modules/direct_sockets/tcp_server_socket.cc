// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_socket.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_server_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_server_socket_options.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

mojom::blink::DirectTCPServerSocketOptionsPtr CreateTCPServerSocketOptions(
    const String& local_address,
    const TCPServerSocketOptions* options,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectTCPServerSocketOptions::New();

  net::IPAddress address;
  if (!address.AssignFromIPLiteral(local_address.Utf8())) {
    exception_state.ThrowTypeError("localAddress must be a valid IP address.");
    return {};
  }

  if (options->hasLocalPort() && options->localPort() == 0) {
    exception_state.ThrowTypeError(
        "localPort must be greater than zero. Leave this field unassigned to "
        "allow the OS to pick a port on its own.");
    return {};
  }

  // Port 0 allows the OS to pick an available port on its own.
  net::IPEndPoint local_addr = net::IPEndPoint(
      std::move(address), options->hasLocalPort() ? options->localPort() : 0U);

  if (options->hasBacklog()) {
    if (options->backlog() == 0) {
      exception_state.ThrowTypeError("backlog must be greater than zero.");
      return {};
    }
    socket_options->backlog = options->backlog();
  }

  if (options->hasIpv6Only()) {
    if (local_addr.address() != net::IPAddress::IPv6AllZeros()) {
      exception_state.ThrowTypeError(
          "ipv6Only can only be specified when localAddress is [::] or "
          "equivalent.");
      return {};
    }
    socket_options->ipv6_only = options->ipv6Only();
  }

  socket_options->local_addr = std::move(local_addr);
  return socket_options;
}

}  // namespace

TCPServerSocket::TCPServerSocket(ScriptState* script_state)
    : Socket(script_state),
      opened_(MakeGarbageCollected<
              ScriptPromiseProperty<TCPServerSocketOpenInfo, DOMException>>(
          GetExecutionContext())) {}

TCPServerSocket::~TCPServerSocket() = default;

// static
TCPServerSocket* TCPServerSocket::Create(ScriptState* script_state,
                                         const String& local_address,
                                         const TCPServerSocketOptions* options,
                                         ExceptionState& exception_state) {
  if (!Socket::CheckContextAndPermissions(script_state, exception_state)) {
    return nullptr;
  }

  auto* socket = MakeGarbageCollected<TCPServerSocket>(script_state);
  if (!socket->Open(local_address, options, exception_state)) {
    return nullptr;
  }
  return socket;
}

ScriptPromise<TCPServerSocketOpenInfo> TCPServerSocket::opened(
    ScriptState* script_state) const {
  return opened_->Promise(script_state->World());
}

ScriptPromise<IDLUndefined> TCPServerSocket::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (GetState() == State::kOpening) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is not properly initialized.");
    return EmptyPromise();
  }

  if (GetState() != State::kOpen) {
    return closed(script_state);
  }

  if (readable_stream_wrapper_->Locked()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Close called on locked streams.");
    return EmptyPromise();
  }

  auto* reason = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Stream closed.");

  auto readable_cancel = readable_stream_wrapper_->Readable()->cancel(
      script_state, ScriptValue::From(script_state, reason), exception_state);
  DCHECK(!exception_state.HadException()) << exception_state.Message();
  readable_cancel.MarkAsHandled();

  return closed(script_state);
}

bool TCPServerSocket::Open(const String& local_addr,
                           const TCPServerSocketOptions* options,
                           ExceptionState& exception_state) {
  auto open_tcp_server_socket_options =
      CreateTCPServerSocketOptions(local_addr, options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  mojo::PendingRemote<network::mojom::blink::TCPServerSocket> tcp_server_remote;
  mojo::PendingReceiver<network::mojom::blink::TCPServerSocket>
      tcp_server_receiver = tcp_server_remote.InitWithNewPipeAndPassReceiver();

  GetServiceRemote()->OpenTCPServerSocket(
      std::move(open_tcp_server_socket_options), std::move(tcp_server_receiver),
      WTF::BindOnce(&TCPServerSocket::OnTCPServerSocketOpened,
                    WrapPersistent(this), std::move(tcp_server_remote)));
  return true;
}

void TCPServerSocket::OnTCPServerSocketOpened(
    mojo::PendingRemote<network::mojom::blink::TCPServerSocket>
        tcp_server_remote,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  if (result == net::OK) {
    DCHECK(local_addr);
    readable_stream_wrapper_ =
        MakeGarbageCollected<TCPServerReadableStreamWrapper>(
            GetScriptState(),
            WTF::BindOnce(&TCPServerSocket::OnReadableStreamClosed,
                          WrapPersistent(this)),
            std::move(tcp_server_remote));

    auto* open_info = TCPServerSocketOpenInfo::Create();
    open_info->setReadable(readable_stream_wrapper_->Readable());
    open_info->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    open_info->setLocalPort(local_addr->port());

    opened_->Resolve(open_info);

    SetState(State::kOpen);
  } else {
    // Error codes are negative.
    base::UmaHistogramSparse("DirectSockets.TCPServerNetworkFailures", -result);
    ReleaseResources();

    ScriptState::Scope scope(GetScriptState());
    auto* exception = CreateDOMExceptionFromNetErrorCode(result);
    opened_->Reject(exception);
    GetClosedProperty().Reject(ScriptValue(GetScriptState()->GetIsolate(),
                                           exception->ToV8(GetScriptState())));

    SetState(State::kAborted);
  }

  DCHECK_NE(GetState(), State::kOpening);
}

void TCPServerSocket::Trace(Visitor* visitor) const {
  visitor->Trace(opened_);
  visitor->Trace(readable_stream_wrapper_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
}

void TCPServerSocket::ContextDestroyed() {
  // Release resources as quickly as possible.
  ReleaseResources();
}

void TCPServerSocket::ReleaseResources() {
  ResetServiceAndFeatureHandle();
  readable_stream_wrapper_.Clear();
}

void TCPServerSocket::OnReadableStreamClosed(ScriptValue exception) {
  DCHECK_EQ(GetState(), State::kOpen);

  if (!exception.IsEmpty()) {
    GetClosedProperty().Reject(exception);
    SetState(State::kAborted);
  } else {
    GetClosedProperty().ResolveWithUndefined();
    SetState(State::kClosed);
  }
  ReleaseResources();

  DCHECK_NE(GetState(), State::kOpen);
}

}  // namespace blink
