// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include "base/barrier_callback.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_dns_query_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kTCPNetworkFailuresHistogramName[] =
    "DirectSockets.TCPNetworkFailures";

bool CheckSendReceiveBufferSize(const TCPSocketOptions* options,
                                ExceptionState& exception_state) {
  if (options->hasSendBufferSize() && options->sendBufferSize() == 0) {
    exception_state.ThrowTypeError("sendBufferSize must be greater than zero.");
    return false;
  }
  if (options->hasReceiveBufferSize() && options->receiveBufferSize() == 0) {
    exception_state.ThrowTypeError(
        "receiverBufferSize must be greater than zero.");
    return false;
  }
  return true;
}

mojom::blink::DirectTCPSocketOptionsPtr CreateTCPSocketOptions(
    const String& remote_address,
    const uint16_t remote_port,
    const TCPSocketOptions* options,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectTCPSocketOptions::New();

  socket_options->remote_addr =
      net::HostPortPair(remote_address.Utf8(), remote_port);

  if (!CheckSendReceiveBufferSize(options, exception_state)) {
    return {};
  }

  if (options->hasKeepAliveDelay() &&
      base::Milliseconds(options->keepAliveDelay()) < base::Seconds(1)) {
    exception_state.ThrowTypeError(
        "keepAliveDelay must be no less than 1,000 milliseconds.");
    return {};
  }

  // noDelay has a default value specified, therefore it's safe to call
  // ->noDelay() without checking ->hasNoDelay() first.
  socket_options->no_delay = options->noDelay();

  socket_options->keep_alive_options =
      network::mojom::blink::TCPKeepAliveOptions::New(
          /*enable=*/options->hasKeepAliveDelay() ? true : false,
          /*delay=*/options->hasKeepAliveDelay()
              ? base::Milliseconds(options->keepAliveDelay()).InSeconds()
              : 0);

  if (options->hasSendBufferSize()) {
    socket_options->send_buffer_size = options->sendBufferSize();
  }
  if (options->hasReceiveBufferSize()) {
    socket_options->receive_buffer_size = options->receiveBufferSize();
  }

  if (options->hasDnsQueryType()) {
    switch (options->dnsQueryType().AsEnum()) {
      case V8SocketDnsQueryType::Enum::kIpv4:
        socket_options->dns_query_type = net::DnsQueryType::A;
        break;
      case V8SocketDnsQueryType::Enum::kIpv6:
        socket_options->dns_query_type = net::DnsQueryType::AAAA;
        break;
    }
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

// static
TCPSocket* TCPSocket::CreateFromAcceptedConnection(
    ScriptState* script_state,
    mojo::PendingRemote<network::mojom::blink::TCPConnectedSocket> tcp_socket,
    mojo::PendingReceiver<network::mojom::blink::SocketObserver>
        socket_observer,
    const net::IPEndPoint& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  auto* socket = MakeGarbageCollected<TCPSocket>(script_state);
  // TODO(crbug.com/1417998): support local_addr for accepted sockets.
  socket->FinishOpenOrAccept(std::move(tcp_socket), std::move(socket_observer),
                             peer_addr, /*local_addr=*/std::nullopt,
                             std::move(receive_stream), std::move(send_stream));
  DCHECK_EQ(socket->GetState(), State::kOpen);
  return socket;
}

TCPSocket::TCPSocket(ScriptState* script_state)
    : Socket(script_state),
      ActiveScriptWrappable<TCPSocket>({}),
      tcp_socket_{GetExecutionContext()},
      socket_observer_{this, GetExecutionContext()},
      opened_(MakeGarbageCollected<
              ScriptPromiseProperty<TCPSocketOpenInfo, DOMException>>(
          GetExecutionContext())) {}

TCPSocket::~TCPSocket() = default;

ScriptPromise<TCPSocketOpenInfo> TCPSocket::opened(
    ScriptState* script_state) const {
  return opened_->Promise(script_state->World());
}

ScriptPromise<IDLUndefined> TCPSocket::close(ScriptState*,
                                             ExceptionState& exception_state) {
  if (GetState() == State::kOpening) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is not properly initialized.");
    return EmptyPromise();
  }

  auto* script_state = GetScriptState();
  if (GetState() != State::kOpen) {
    return closed(script_state);
  }

  if (readable_stream_wrapper_->Locked() ||
      writable_stream_wrapper_->Locked()) {
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

  auto writable_abort = writable_stream_wrapper_->Writable()->abort(
      script_state, ScriptValue::From(script_state, reason), exception_state);
  DCHECK(!exception_state.HadException()) << exception_state.Message();
  writable_abort.MarkAsHandled();

  return closed(script_state);
}

bool TCPSocket::Open(const String& remote_address,
                     const uint16_t remote_port,
                     const TCPSocketOptions* options,
                     ExceptionState& exception_state) {
  auto open_tcp_socket_options = CreateTCPSocketOptions(
      remote_address, remote_port, options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
      socket_receiver;
  mojo::PendingRemote<network::mojom::blink::SocketObserver> observer_remote;

  auto callback =
      WTF::BindOnce(&TCPSocket::OnTCPSocketOpened, WrapPersistent(this),
                    socket_receiver.InitWithNewPipeAndPassRemote(),
                    observer_remote.InitWithNewPipeAndPassReceiver());
  GetServiceRemote()->OpenTCPSocket(
      std::move(open_tcp_socket_options), std::move(socket_receiver),
      std::move(observer_remote), std::move(callback));
  return true;
}

void TCPSocket::OnTCPSocketOpened(
    mojo::PendingRemote<network::mojom::blink::TCPConnectedSocket> tcp_socket,
    mojo::PendingReceiver<network::mojom::blink::SocketObserver>
        socket_observer,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  if (result == net::OK) {
    DCHECK(peer_addr);
    FinishOpenOrAccept(std::move(tcp_socket), std::move(socket_observer),
                       *peer_addr, local_addr, std::move(receive_stream),
                       std::move(send_stream));
  } else {
    // Error codes are negative.
    base::UmaHistogramSparse(kTCPNetworkFailuresHistogramName, -result);
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

void TCPSocket::FinishOpenOrAccept(
    mojo::PendingRemote<network::mojom::blink::TCPConnectedSocket> tcp_socket,
    mojo::PendingReceiver<network::mojom::blink::SocketObserver>
        socket_observer,
    const net::IPEndPoint& peer_addr,
    const std::optional<net::IPEndPoint>& local_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  tcp_socket_.Bind(std::move(tcp_socket),
                   GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  socket_observer_.Bind(
      std::move(socket_observer),
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  socket_observer_.set_disconnect_handler(
      WTF::BindOnce(&TCPSocket::OnSocketConnectionError, WrapPersistent(this)));

  auto close_callback = base::BarrierCallback<ScriptValue>(
      /*num_callbacks=*/2,
      WTF::BindOnce(&TCPSocket::OnBothStreamsClosed, WrapWeakPersistent(this)));

  readable_stream_wrapper_ = MakeGarbageCollected<TCPReadableStreamWrapper>(
      GetScriptState(), close_callback, std::move(receive_stream));
  writable_stream_wrapper_ = MakeGarbageCollected<TCPWritableStreamWrapper>(
      GetScriptState(), close_callback, std::move(send_stream));

  auto* open_info = TCPSocketOpenInfo::Create();

  open_info->setReadable(readable_stream_wrapper_->Readable());
  open_info->setWritable(writable_stream_wrapper_->Writable());

  open_info->setRemoteAddress(String{peer_addr.ToStringWithoutPort()});
  open_info->setRemotePort(peer_addr.port());

  if (local_addr) {
    open_info->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    open_info->setLocalPort(local_addr->port());
  }

  opened_->Resolve(open_info);
  SetState(State::kOpen);
}

void TCPSocket::OnSocketConnectionError() {
  DCHECK_EQ(GetState(), State::kOpen);
  readable_stream_wrapper_->ErrorStream(net::ERR_CONNECTION_ABORTED);
  writable_stream_wrapper_->ErrorStream(net::ERR_CONNECTION_ABORTED);
}

void TCPSocket::OnServiceConnectionError() {
  if (GetState() == State::kOpening) {
    OnTCPSocketOpened(mojo::NullRemote(), mojo::NullReceiver(),
                      net::ERR_CONTEXT_SHUT_DOWN, std::nullopt, std::nullopt,
                      mojo::ScopedDataPipeConsumerHandle(),
                      mojo::ScopedDataPipeProducerHandle());
  }
}

void TCPSocket::ReleaseResources() {
  ResetServiceAndFeatureHandle();
  tcp_socket_.reset();
  socket_observer_.reset();
}

void TCPSocket::OnReadError(int32_t net_error) {
  // |net_error| equal to net::OK means EOF -- in this case the
  // stream is not really errored but rather closed gracefully.
  DCHECK_EQ(GetState(), State::kOpen);
  readable_stream_wrapper_->ErrorStream(net_error);
}

void TCPSocket::OnWriteError(int32_t net_error) {
  DCHECK_EQ(GetState(), State::kOpen);
  writable_stream_wrapper_->ErrorStream(net_error);
}

void TCPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(tcp_socket_);
  visitor->Trace(socket_observer_);
  visitor->Trace(opened_);
  visitor->Trace(readable_stream_wrapper_);
  visitor->Trace(writable_stream_wrapper_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

bool TCPSocket::HasPendingActivity() const {
  if (GetState() != State::kOpen) {
    return false;
  }
  return writable_stream_wrapper_->HasPendingWrite();
}

void TCPSocket::ContextDestroyed() {
  ReleaseResources();
}

void TCPSocket::OnBothStreamsClosed(std::vector<ScriptValue> args) {
  DCHECK_EQ(GetState(), State::kOpen);
  DCHECK_EQ(args.size(), 2U);

  // Finds first actual exception and rejects |closed| with it.
  // If neither stream was errored, resolves |closed|.
  if (auto it = base::ranges::find_if_not(args, &ScriptValue::IsEmpty);
      it != args.end()) {
    GetClosedProperty().Reject(*it);
    SetState(State::kAborted);
  } else {
    GetClosedProperty().ResolveWithUndefined();
    SetState(State::kClosed);
  }
  ReleaseResources();

  DCHECK_NE(GetState(), State::kOpen);
}

}  // namespace blink
