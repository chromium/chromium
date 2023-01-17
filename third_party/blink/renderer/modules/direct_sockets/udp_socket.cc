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
          MakeGarbageCollected<UDPSocketMojoRemote>(GetExecutionContext())) {}

UDPSocket::~UDPSocket() = default;

ScriptPromise UDPSocket::close(ScriptState*, ExceptionState& exception_state) {
  if (GetState() == State::kOpening) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is not properly initialized.");
    return ScriptPromise();
  }

  auto* script_state = GetScriptState();
  if (GetState() != State::kOpen) {
    return closed(script_state);
  }

  if (readable_stream_wrapper_->Locked() ||
      writable_stream_wrapper_->Locked()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Close called on locked streams.");
    return ScriptPromise();
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
  auto open_udp_socket_options =
      CreateUDPSocketOptions(options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  mojo::PendingReceiver<network::mojom::blink::UDPSocketListener>
      socket_listener;
  mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
      socket_listener_remote = socket_listener.InitWithNewPipeAndPassRemote();

  GetServiceRemote()->OpenUdpSocket(
      std::move(open_udp_socket_options), GetUDPSocketReceiver(),
      std::move(socket_listener_remote),
      WTF::BindOnce(&UDPSocket::Init, WrapPersistent(this),
                    std::move(socket_listener)));

  return true;
}

void UDPSocket::Init(
    mojo::PendingReceiver<network::mojom::blink::UDPSocketListener>
        socket_listener,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr,
    const absl::optional<net::IPEndPoint>& peer_addr) {
  if (result == net::OK) {
    DCHECK(peer_addr);
    auto close_callback = base::BarrierCallback<ScriptValue>(
        /*num_callbacks=*/2, WTF::BindOnce(&UDPSocket::OnBothStreamsClosed,
                                           WrapWeakPersistent(this)));

    auto* script_state = GetScriptState();
    readable_stream_wrapper_ = MakeGarbageCollected<UDPReadableStreamWrapper>(
        script_state, close_callback, udp_socket_, std::move(socket_listener));
    writable_stream_wrapper_ = MakeGarbageCollected<UDPWritableStreamWrapper>(
        script_state, close_callback, udp_socket_);

    auto* open_info = UDPSocketOpenInfo::Create();

    open_info->setReadable(readable_stream_wrapper_->Readable());
    open_info->setWritable(writable_stream_wrapper_->Writable());

    open_info->setRemoteAddress(String{peer_addr->ToStringWithoutPort()});
    open_info->setRemotePort(peer_addr->port());

    open_info->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    open_info->setLocalPort(local_addr->port());

    GetOpenedPromiseResolver()->Resolve(open_info);

    SetState(State::kOpen);
  } else {
    // Error codes are negative.
    base::UmaHistogramSparse(kUDPNetworkFailuresHistogramName, -result);
    ReleaseResources();

    GetOpenedPromiseResolver()->Reject(
        CreateDOMExceptionFromNetErrorCode(result));
    GetClosedPromiseResolver()->Reject();

    SetState(State::kAborted);
  }

  DCHECK_NE(GetState(), State::kOpening);
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

  visitor->Trace(readable_stream_wrapper_);
  visitor->Trace(writable_stream_wrapper_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

void UDPSocket::OnServiceConnectionError() {
  if (GetState() == State::kOpening) {
    Init(mojo::NullReceiver(), net::ERR_UNEXPECTED, absl::nullopt,
         absl::nullopt);
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
    GetClosedPromiseResolver()->Reject(*it);
    SetState(State::kAborted);
  } else {
    GetClosedPromiseResolver()->Resolve();
    SetState(State::kClosed);
  }
  ReleaseResources();

  DCHECK_NE(GetState(), State::kOpen);
}

}  // namespace blink
