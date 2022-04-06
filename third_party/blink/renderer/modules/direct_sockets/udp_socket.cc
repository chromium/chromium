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
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_close_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_connection.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
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

constexpr uint32_t readableStreamDefaultBufferSize = 32;

mojom::blink::DirectSocketOptionsPtr CreateUDPSocketOptions(
    const String& address,
    const uint16_t port,
    const UDPSocketOptions* options,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectSocketOptions::New();

  socket_options->remote_hostname = address;
  socket_options->remote_port = port;

  if (options->hasReadableStreamBufferSize() &&
      options->readableStreamBufferSize() == 0) {
    exception_state.ThrowTypeError(
        "readableStreamBufferSize must be greater than zero.");
    return {};
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
UDPSocket* UDPSocket::Create(ScriptState* script_state,
                             const String& address,
                             const uint16_t port,
                             const UDPSocketOptions* options,
                             ExceptionState& exception_state) {
  if (!Socket::CheckContextAndPermissions(script_state, exception_state)) {
    return nullptr;
  }

  auto* socket = MakeGarbageCollected<UDPSocket>(script_state);
  if (!socket->Open(address, port, options, exception_state)) {
    return nullptr;
  }
  return socket;
}

UDPSocket::UDPSocket(ScriptState* script_state)
    : Socket(script_state),
      udp_socket_(
          MakeGarbageCollected<UDPSocketMojoRemote>(GetExecutionContext())),
      socket_listener_{this, GetExecutionContext()} {}

UDPSocket::~UDPSocket() = default;

bool UDPSocket::Open(const String& address,
                     const uint16_t port,
                     const UDPSocketOptions* options,
                     ExceptionState& exception_state) {
  auto open_udp_socket_options =
      CreateUDPSocketOptions(address, port, options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  if (options->hasReadableStreamBufferSize()) {
    readable_stream_buffer_size_ = options->readableStreamBufferSize();
  }

  ConnectService();

  service_->get()->OpenUdpSocket(
      std::move(open_udp_socket_options), GetUDPSocketReceiver(),
      GetUDPSocketListener(),
      WTF::Bind(&UDPSocket::Init, WrapPersistent(this)));

  return true;
}

void UDPSocket::Init(int32_t result,
                     const absl::optional<net::IPEndPoint>& local_addr,
                     const absl::optional<net::IPEndPoint>& peer_addr) {
  if (result == net::OK && peer_addr) {
    readable_stream_wrapper_ = MakeGarbageCollected<UDPReadableStreamWrapper>(
        script_state_, udp_socket_,
        WTF::Bind(&UDPSocket::CloseInternal, WrapWeakPersistent(this)),
        readable_stream_buffer_size_.value_or(readableStreamDefaultBufferSize));
    writable_stream_wrapper_ = MakeGarbageCollected<UDPWritableStreamWrapper>(
        script_state_, udp_socket_);

    auto* connection = UDPSocketConnection::Create();

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
      base::UmaHistogramSparse(kUDPNetworkFailuresHistogramName, -result);
    }
    connection_resolver_->Reject(CreateDOMExceptionFromNetErrorCode(result));
    CloseServiceAndResetFeatureHandle();
  }

  connection_resolver_ = nullptr;
}

mojo::PendingReceiver<blink::mojom::blink::DirectUDPSocket>
UDPSocket::GetUDPSocketReceiver() {
  return udp_socket_->get().BindNewPipeAndPassReceiver(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
}

mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
UDPSocket::GetUDPSocketListener() {
  auto pending_remote = socket_listener_.BindNewPipeAndPassRemote(
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

  socket_listener_.set_disconnect_handler(
      WTF::Bind(&UDPSocket::OnSocketConnectionError, WrapPersistent(this)));

  return pending_remote;
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
    CloseOnError();
    return;
  }

  readable_stream_wrapper_->Push(*data, src_addr);
}

bool UDPSocket::HasPendingActivity() const {
  return Socket::HasPendingActivity();
}

void UDPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(udp_socket_);
  visitor->Trace(socket_listener_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

void UDPSocket::OnServiceConnectionError() {
  if (connection_resolver_) {
    Init(net::ERR_UNEXPECTED, absl::nullopt, absl::nullopt);
  }
}

void UDPSocket::OnSocketConnectionError() {
  CloseOnError();
}

void UDPSocket::CloseOnError() {
  if (Initialized()) {
    CloseInternal(/*error=*/true);
    DCHECK(Closed());
  }
}

void UDPSocket::Close(const SocketCloseOptions* options,
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

void UDPSocket::CloseInternal(bool error) {
  CloseServiceAndResetFeatureHandle();
  ResolveOrRejectClosed(error);

  socket_listener_.reset();

  // Reject pending read/write promises.
  readable_stream_wrapper_->CloseStream(error);
  writable_stream_wrapper_->CloseStream(error);

  // Close the socket.
  udp_socket_->Close();
}

}  // namespace blink
