// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_readable_stream_wrapper.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

TCPServerReadableStreamWrapper::TCPServerReadableStreamWrapper(
    ScriptState* script_state,
    CloseOnceCallback on_close,
    mojo::PendingRemote<network::mojom::blink::TCPServerSocket>
        tcp_server_socket)
    : ReadableStreamDefaultWrapper(script_state),
      on_close_(std::move(on_close)),
      tcp_server_socket_(ExecutionContext::From(script_state)) {
  tcp_server_socket_.Bind(std::move(tcp_server_socket),
                          ExecutionContext::From(script_state)
                              ->GetTaskRunner(TaskType::kNetworking));
  tcp_server_socket_.set_disconnect_handler(
      WTF::BindOnce(&TCPServerReadableStreamWrapper::ErrorStream,
                    WrapWeakPersistent(this), net::ERR_CONNECTION_ABORTED));

  ScriptState::Scope scope(script_state);

  auto* source =
      ReadableStreamDefaultWrapper::MakeForwardingUnderlyingSource(this);
  SetSource(source);

  auto* readable = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, source, /*high_water_mark=*/0);
  SetReadable(readable);
}

void TCPServerReadableStreamWrapper::Pull() {
  DCHECK(tcp_server_socket_.is_bound());

  mojo::PendingReceiver<network::mojom::blink::SocketObserver> socket_observer;
  mojo::PendingRemote<network::mojom::blink::SocketObserver>
      socket_observer_remote = socket_observer.InitWithNewPipeAndPassRemote();

  tcp_server_socket_->Accept(
      std::move(socket_observer_remote),
      WTF::BindOnce(&TCPServerReadableStreamWrapper::OnAccept,
                    WrapPersistent(this), std::move(socket_observer)));
}

void TCPServerReadableStreamWrapper::CloseStream() {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kClosed);

  tcp_server_socket_.reset();

  std::move(on_close_).Run(/*exception=*/ScriptValue());
}

void TCPServerReadableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kAborted);

  tcp_server_socket_.reset();

  auto* script_state = GetScriptState();
  // Scope is needed because there's no ScriptState* on the call stack for
  // ScriptValue.
  ScriptState::Scope scope{script_state};

  auto exception = ScriptValue(
      script_state->GetIsolate(),
      V8ThrowDOMException::CreateOrDie(
          script_state->GetIsolate(), DOMExceptionCode::kNetworkError,
          String{"Server socket closed: " + net::ErrorToString(error_code)}));
  Controller()->Error(exception.V8Value());
  std::move(on_close_).Run(exception);
}

void TCPServerReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(tcp_server_socket_);
  ReadableStreamDefaultWrapper::Trace(visitor);
}

void TCPServerReadableStreamWrapper::OnAccept(
    mojo::PendingReceiver<network::mojom::blink::SocketObserver>
        socket_observer,
    int result,
    const std::optional<net::IPEndPoint>& remote_addr,
    mojo::PendingRemote<network::mojom::blink::TCPConnectedSocket>
        tcp_socket_remote,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  if (result != net::OK) {
    ErrorStream(result);
    return;
  }

  auto* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);
  Controller()->Enqueue(TCPSocket::CreateFromAcceptedConnection(
      script_state, std::move(tcp_socket_remote), std::move(socket_observer),
      *remote_addr, std::move(receive_stream), std::move(send_stream)));
}

}  // namespace blink
