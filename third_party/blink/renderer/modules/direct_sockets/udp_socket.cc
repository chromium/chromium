// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_socket.h"

#include "base/barrier_callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_dns_query_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kUDPNetworkFailuresHistogramName[] =
    "DirectSockets.UDPNetworkFailures";

bool CheckSendReceiveBufferSize(const UDPSocketOptions* options,
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

std::optional<network::mojom::blink::RestrictedUDPSocketMode>
InferUDPSocketMode(const UDPSocketOptions* options,
                   ExceptionState& exception_state) {
  std::optional<network::mojom::blink::RestrictedUDPSocketMode> mode;
  if (options->hasRemoteAddress() && options->hasRemotePort()) {
    mode = network::mojom::RestrictedUDPSocketMode::CONNECTED;
  } else if (options->hasRemoteAddress() || options->hasRemotePort()) {
    exception_state.ThrowTypeError(
        "remoteAddress and remotePort should either be specified together or "
        "not specified at all.");
    return {};
  }

  if (options->hasLocalAddress()) {
    if (mode) {
      exception_state.ThrowTypeError(
          "remoteAddress and localAddress cannot be specified at the same "
          "time.");
      return {};
    }

    mode = network::mojom::blink::RestrictedUDPSocketMode::BOUND;
  } else if (options->hasLocalPort()) {
    exception_state.ThrowTypeError(
        "localPort cannot be specified without localAddress.");
    return {};
  }

  if (!mode) {
    exception_state.ThrowTypeError(
        "neither remoteAddress nor localAddress specified.");
    return {};
  }

  return mode;
}

mojom::blink::DirectConnectedUDPSocketOptionsPtr
CreateConnectedUDPSocketOptions(const UDPSocketOptions* options,
                                ExceptionState& exception_state) {
  DCHECK(options->hasRemoteAddress() && options->hasRemotePort());

  if (options->hasIpv6Only()) {
    exception_state.ThrowTypeError(
        "ipv6Only can only be specified with localAddress.");
    return {};
  }

  if (!CheckSendReceiveBufferSize(options, exception_state)) {
    return {};
  }

  auto socket_options = mojom::blink::DirectConnectedUDPSocketOptions::New();

  socket_options->remote_addr =
      net::HostPortPair(options->remoteAddress().Utf8(), options->remotePort());
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

  if (options->hasReceiveBufferSize()) {
    socket_options->receive_buffer_size = options->receiveBufferSize();
  }
  if (options->hasSendBufferSize()) {
    socket_options->send_buffer_size = options->sendBufferSize();
  }

  return socket_options;
}

mojom::blink::DirectBoundUDPSocketOptionsPtr CreateBoundUDPSocketOptions(
    const UDPSocketOptions* options,
    ExceptionState& exception_state) {
  DCHECK(options->hasLocalAddress());
  auto socket_options = mojom::blink::DirectBoundUDPSocketOptions::New();

  auto local_ip = net::IPAddress::FromIPLiteral(options->localAddress().Utf8());
  if (!local_ip) {
    exception_state.ThrowTypeError("localAddress must be a valid IP address.");
    return {};
  }

  if (options->hasLocalPort() && options->localPort() == 0) {
    exception_state.ThrowTypeError(
        "localPort must be greater than zero. Leave this field unassigned to "
        "allow the OS to pick a port on its own.");
    return {};
  }

  if (options->hasDnsQueryType()) {
    exception_state.ThrowTypeError(
        "dnsQueryType is only relevant when remoteAddress is specified.");
    return {};
  }

  if (!CheckSendReceiveBufferSize(options, exception_state)) {
    return {};
  }

  if (options->hasIpv6Only()) {
    if (local_ip != net::IPAddress::IPv6AllZeros()) {
      exception_state.ThrowTypeError(
          "ipv6Only can only be specified when localAddress is [::] or "
          "equivalent.");
      return {};
    }
    socket_options->ipv6_only = options->ipv6Only();
  }

  socket_options->local_addr =
      net::IPEndPoint(std::move(*local_ip),
                      options->hasLocalPort() ? options->localPort() : 0U);

  if (options->hasReceiveBufferSize()) {
    socket_options->receive_buffer_size = options->receiveBufferSize();
  }
  if (options->hasSendBufferSize()) {
    socket_options->send_buffer_size = options->sendBufferSize();
  }

  return socket_options;
}

}  // namespace

// static
UDPSocket* UDPSocket::Create(ScriptState* script_state,
                             const UDPSocketOptions* options,
                             ExceptionState& exception_state) {
  if (!Socket::CheckContextAndPermissions(script_state, exception_state)) {
    return nullptr;
  }

  auto* socket = MakeGarbageCollected<UDPSocket>(script_state);
  if (!socket->Open(options, exception_state)) {
    return nullptr;
  }
  return socket;
}

UDPSocket::UDPSocket(ScriptState* script_state)
    : Socket(script_state),
      ActiveScriptWrappable<UDPSocket>({}),
      udp_socket_(
          MakeGarbageCollected<UDPSocketMojoRemote>(GetExecutionContext())),
      opened_(MakeGarbageCollected<
              ScriptPromiseProperty<UDPSocketOpenInfo, DOMException>>(
          GetExecutionContext())) {}

UDPSocket::~UDPSocket() = default;

ScriptPromise<UDPSocketOpenInfo> UDPSocket::opened(
    ScriptState* script_state) const {
  return opened_->Promise(script_state->World());
}

ScriptPromise<IDLUndefined> UDPSocket::close(ScriptState*,
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

bool UDPSocket::Open(const UDPSocketOptions* options,
                     ExceptionState& exception_state) {
  auto mode = InferUDPSocketMode(options, exception_state);
  if (!mode) {
    return false;
  }

  mojo::PendingReceiver<network::mojom::blink::UDPSocketListener>
      socket_listener;
  auto socket_listener_remote = socket_listener.InitWithNewPipeAndPassRemote();

  switch (*mode) {
    case network::mojom::blink::RestrictedUDPSocketMode::CONNECTED: {
      auto connected_options =
          CreateConnectedUDPSocketOptions(options, exception_state);
      if (exception_state.HadException()) {
        return false;
      }
      GetServiceRemote()->OpenConnectedUDPSocket(
          std::move(connected_options), GetUDPSocketReceiver(),
          std::move(socket_listener_remote),
          WTF::BindOnce(&UDPSocket::OnConnectedUDPSocketOpened,
                        WrapPersistent(this), std::move(socket_listener)));
      return true;
    }
    case network::mojom::blink::RestrictedUDPSocketMode::BOUND: {
      auto bound_options =
          CreateBoundUDPSocketOptions(options, exception_state);
      if (exception_state.HadException()) {
        return false;
      }
      GetServiceRemote()->OpenBoundUDPSocket(
          std::move(bound_options), GetUDPSocketReceiver(),
          std::move(socket_listener_remote),
          WTF::BindOnce(&UDPSocket::OnBoundUDPSocketOpened,
                        WrapPersistent(this), std::move(socket_listener)));
      return true;
    }
  }
}

void UDPSocket::FinishOpen(
    network::mojom::RestrictedUDPSocketMode mode,
    mojo::PendingReceiver<network::mojom::blink::UDPSocketListener>
        socket_listener,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr) {
  if (result == net::OK) {
    auto close_callback = base::BarrierCallback<ScriptValue>(
        /*num_callbacks=*/2, WTF::BindOnce(&UDPSocket::OnBothStreamsClosed,
                                           WrapWeakPersistent(this)));

    auto* script_state = GetScriptState();
    readable_stream_wrapper_ = MakeGarbageCollected<UDPReadableStreamWrapper>(
        script_state, close_callback, udp_socket_, std::move(socket_listener));
    // |peer_addr| is populated only in CONNECTED mode.
    writable_stream_wrapper_ = MakeGarbageCollected<UDPWritableStreamWrapper>(
        script_state, close_callback, udp_socket_, mode);

    auto* open_info = UDPSocketOpenInfo::Create();

    open_info->setReadable(readable_stream_wrapper_->Readable());
    open_info->setWritable(writable_stream_wrapper_->Writable());

    if (peer_addr) {
      open_info->setRemoteAddress(String{peer_addr->ToStringWithoutPort()});
      open_info->setRemotePort(peer_addr->port());
    }

    open_info->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    open_info->setLocalPort(local_addr->port());

    opened_->Resolve(open_info);

    SetState(State::kOpen);
  } else {
    FailOpenWith(result);
    SetState(State::kAborted);
  }

  DCHECK_NE(GetState(), State::kOpening);
}

void UDPSocket::OnConnectedUDPSocketOpened(
    mojo::PendingReceiver<network::mojom::blink::UDPSocketListener>
        socket_listener,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr) {
  FinishOpen(network::mojom::RestrictedUDPSocketMode::CONNECTED,
             std::move(socket_listener), result, local_addr, peer_addr);
}

void UDPSocket::OnBoundUDPSocketOpened(
    mojo::PendingReceiver<network::mojom::blink::UDPSocketListener>
        socket_listener,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  FinishOpen(network::mojom::RestrictedUDPSocketMode::BOUND,
             std::move(socket_listener), result, local_addr,
             /*peer_addr=*/std::nullopt);
}

void UDPSocket::FailOpenWith(int32_t error) {
  // Error codes are negative.
  base::UmaHistogramSparse(kUDPNetworkFailuresHistogramName, -error);
  ReleaseResources();

  ScriptState::Scope scope(GetScriptState());
  auto* exception = CreateDOMExceptionFromNetErrorCode(error);
  opened_->Reject(exception);
  GetClosedProperty().Reject(ScriptValue(GetScriptState()->GetIsolate(),
                                         exception->ToV8(GetScriptState())));
}

mojo::PendingReceiver<network::mojom::blink::RestrictedUDPSocket>
UDPSocket::GetUDPSocketReceiver() {
  auto pending_receiver = udp_socket_->get().BindNewPipeAndPassReceiver(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  udp_socket_->get().set_disconnect_handler(
      WTF::BindOnce(&UDPSocket::CloseOnError, WrapWeakPersistent(this)));
  return pending_receiver;
}

bool UDPSocket::HasPendingActivity() const {
  if (GetState() != State::kOpen) {
    return false;
  }
  return writable_stream_wrapper_->HasPendingWrite();
}

void UDPSocket::ContextDestroyed() {
  // Release resources as quickly as possible.
  ReleaseResources();
}

void UDPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(udp_socket_);
  visitor->Trace(opened_);
  visitor->Trace(readable_stream_wrapper_);
  visitor->Trace(writable_stream_wrapper_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

void UDPSocket::OnServiceConnectionError() {
  if (GetState() == State::kOpening) {
    FailOpenWith(net::ERR_CONNECTION_FAILED);
    SetState(State::kAborted);
  }
}

void UDPSocket::CloseOnError() {
  DCHECK_EQ(GetState(), State::kOpen);
  readable_stream_wrapper_->ErrorStream(net::ERR_CONNECTION_ABORTED);
  writable_stream_wrapper_->ErrorStream(net::ERR_CONNECTION_ABORTED);
}

void UDPSocket::ReleaseResources() {
  ResetServiceAndFeatureHandle();
  udp_socket_->Close();
}

void UDPSocket::OnBothStreamsClosed(std::vector<ScriptValue> args) {
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
