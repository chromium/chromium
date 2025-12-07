// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include <memory>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_dns_query_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_options.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/socket.h"
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

std::unique_ptr<protocol::Network::DirectTCPSocketOptions> MapProbeTCPOptions(
    const TCPSocketOptions* options) {
  auto probe_options_builder =
      protocol::Network::DirectTCPSocketOptions::create();
  if (options->hasKeepAliveDelay()) {
    probe_options_builder.setKeepAliveDelay(options->keepAliveDelay());
  }
  if (options->hasSendBufferSize()) {
    probe_options_builder.setSendBufferSize(options->sendBufferSize());
  }
  if (options->hasReceiveBufferSize()) {
    probe_options_builder.setReceiveBufferSize(options->receiveBufferSize());
  }
  if (options->hasDnsQueryType()) {
    probe_options_builder.setDnsQueryType(
        Socket::MapProbeDnsQueryType(options->dnsQueryType()));
  }

  return probe_options_builder.setNoDelay(options->noDelay()).build();
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
      script_state, ScriptValue::From(script_state, reason),
      ASSERT_NO_EXCEPTION);
  readable_cancel.MarkAsHandled();

  auto writable_abort = writable_stream_wrapper_->Writable()->abort(
      script_state, ScriptValue::From(script_state, reason),
      ASSERT_NO_EXCEPTION);
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

  auto callback = BindOnce(&TCPSocket::OnTCPSocketOpened, WrapPersistent(this),
                           socket_receiver.InitWithNewPipeAndPassRemote(),
                           observer_remote.InitWithNewPipeAndPassReceiver());
  GetServiceRemote()->OpenTCPSocket(
      std::move(open_tcp_socket_options), std::move(socket_receiver),
      std::move(observer_remote), std::move(callback));

  std::unique_ptr<protocol::Network::DirectTCPSocketOptions> proble_options =
      MapProbeTCPOptions(options);
  probe::DirectTCPSocketCreated(GetExecutionContext(), inspector_id_,
                                remote_address, remote_port, *proble_options);

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
    abort_net_error_ = result;
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
      BindOnce(&TCPSocket::OnSocketConnectionError, WrapPersistent(this)));

  readable_stream_wrapper_ = MakeGarbageCollected<TCPReadableStreamWrapper>(
      GetScriptState(),
      BindOnce(&TCPSocket::OnStreamClosed, WrapWeakPersistent(this)),
      std::move(receive_stream), inspector_id_);
  writable_stream_wrapper_ = MakeGarbageCollected<TCPWritableStreamWrapper>(
      GetScriptState(),
      BindOnce(&TCPSocket::OnStreamClosed, WrapWeakPersistent(this)),
      std::move(send_stream), inspector_id_);

  auto* open_info = TCPSocketOpenInfo::Create();

  open_info->setReadable(readable_stream_wrapper_->Readable());
  open_info->setWritable(writable_stream_wrapper_->Writable());

  String remote_address(peer_addr.ToStringWithoutPort());

  open_info->setRemoteAddress(remote_address);
  open_info->setRemotePort(peer_addr.port());

  std::optional<String> opt_local_address;
  std::optional<uint16_t> opt_local_port;
  if (local_addr) {
    opt_local_address = String{local_addr->ToStringWithoutPort()};
    opt_local_port = local_addr->port();

    open_info->setLocalAddress(*opt_local_address);
    open_info->setLocalPort(local_addr->port());
  }

  opened_->Resolve(open_info);
  SetState(State::kOpen);

  probe::DirectTCPSocketOpened(GetExecutionContext(), inspector_id_,
                               remote_address, peer_addr.port(),
                               opt_local_address, opt_local_port);
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
  visitor->Trace(stream_error_);

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

void TCPSocket::SetState(State state) {
  Socket::SetState(state);
  switch (state) {
    case Socket::State::kOpening:
    case Socket::State::kOpen:
      break;
    case Socket::State::kClosed:
      probe::DirectTCPSocketClosed(GetExecutionContext(), inspector_id_);
      break;
    case Socket::State::kAborted:
      probe::DirectTCPSocketAborted(GetExecutionContext(), inspector_id_,
                                    abort_net_error_);
      break;
  }
}

void TCPSocket::OnStreamClosed(v8::Local<v8::Value> exception, int net_error) {
  DCHECK_EQ(GetState(), State::kOpen);
  DCHECK_LE(streams_closed_count_, 1);

  if (stream_error_.IsEmpty() && !exception.IsEmpty()) {
    stream_error_.Reset(GetScriptState()->GetIsolate(), exception);
    abort_net_error_ = net_error;
  }

  if (++streams_closed_count_ == 2) {
    OnBothStreamsClosed();
  }
}

void TCPSocket::OnBothStreamsClosed() {
  // If one of the streams was errored, rejects |closed| with the first
  // exception.
  // If neither stream was errored, resolves |closed|.
  if (!stream_error_.IsEmpty()) {
    auto* isolate = GetScriptState()->GetIsolate();
    GetClosedProperty().Reject(
        ScriptValue(isolate, stream_error_.Get(isolate)));
    SetState(State::kAborted);
    stream_error_.Reset();
  } else {
    GetClosedProperty().ResolveWithUndefined();
    SetState(State::kClosed);
  }
  ReleaseResources();

  DCHECK_NE(GetState(), State::kOpen);
}

}  // namespace blink
