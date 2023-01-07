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
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
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

mojom::blink::DirectSocketOptionsPtr CreateUDPSocketOptions(
    const UDPSocketOptions* options,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectSocketOptions::New();

  socket_options->remote_hostname = options->remoteAddress();
  socket_options->remote_port = options->remotePort();

  if (!CheckSendReceiveBufferSize(options, exception_state)) {
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
      udp_socket_(
          MakeGarbageCollected<UDPSocketMojoRemote>(GetExecutionContext())),
      socket_listener_{this, GetExecutionContext()} {}

UDPSocket::~UDPSocket() = default;

bool UDPSocket::Open(const UDPSocketOptions* options,
                     ExceptionState& exception_state) {
  auto open_udp_socket_options =
      CreateUDPSocketOptions(options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  ConnectService();

  service_->get()->OpenUdpSocket(
      std::move(open_udp_socket_options), GetUDPSocketReceiver(),
      GetUDPSocketListener(),
      WTF::BindOnce(&UDPSocket::Init, WrapPersistent(this)));

  return true;
}

void UDPSocket::Init(int32_t result,
                     const absl::optional<net::IPEndPoint>& local_addr,
                     const absl::optional<net::IPEndPoint>& peer_addr) {
  if (result == net::OK && peer_addr) {
    auto close_callback = base::BarrierCallback<ScriptValue>(
        /*num_callbacks=*/2, WTF::BindOnce(&UDPSocket::OnBothStreamsClosed,
                                           WrapWeakPersistent(this)));

    readable_stream_wrapper_ = MakeGarbageCollected<UDPReadableStreamWrapper>(
        script_state_, close_callback, udp_socket_);
    writable_stream_wrapper_ = MakeGarbageCollected<UDPWritableStreamWrapper>(
        script_state_, close_callback, udp_socket_);

    auto* open_info = UDPSocketOpenInfo::Create();

    open_info->setReadable(readable_stream_wrapper_->Readable());
    open_info->setWritable(writable_stream_wrapper_->Writable());

    open_info->setRemoteAddress(String{peer_addr->ToStringWithoutPort()});
    open_info->setRemotePort(peer_addr->port());

    open_info->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    open_info->setLocalPort(local_addr->port());

    opened_resolver_->Resolve(open_info);
  } else {
    if (result != net::OK) {
      // Error codes are negative.
      base::UmaHistogramSparse(kUDPNetworkFailuresHistogramName, -result);
    }
    opened_resolver_->Reject(CreateDOMExceptionFromNetErrorCode(result));
    CloseServiceAndResetFeatureHandle();

    closed_resolver_->Reject();
  }

  opened_resolver_ = nullptr;
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
      WTF::BindOnce(&UDPSocket::OnSocketConnectionError, WrapPersistent(this)));

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
  if (opened_resolver_) {
    Init(net::ERR_UNEXPECTED, absl::nullopt, absl::nullopt);
  }
}

void UDPSocket::OnSocketConnectionError() {
  CloseOnError();
}

void UDPSocket::CloseOnError() {
  if (!Initialized()) {
    return;
  }

  readable_stream_wrapper_->ErrorStream(net::ERR_CONNECTION_ABORTED);
  writable_stream_wrapper_->ErrorStream(net::ERR_CONNECTION_ABORTED);
}

void UDPSocket::OnBothStreamsClosed(std::vector<ScriptValue> args) {
  DCHECK_EQ(args.size(), 2U);

  // Finds first actual exception and rejects |closed| with it.
  // If neither of the streams was errored, resolves |closed|.
  if (auto it = base::ranges::find_if_not(args, &ScriptValue::IsEmpty);
      it != args.end()) {
    RejectClosed(*it);
  } else {
    ResolveClosed();
  }
  CloseServiceAndResetFeatureHandle();

  socket_listener_.reset();

  // Close the socket.
  udp_socket_->Close();
}

}  // namespace blink
