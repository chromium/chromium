// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/navigator_socket.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/tcp_socket.mojom-blink.h"
#include "services/network/public/mojom/udp_socket.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_socket_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_socket_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kPermissionDeniedHistogramName[] =
    "DirectSockets.PermissionDeniedFailures";

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

}  // namespace

const char NavigatorSocket::kSupplementName[] = "NavigatorSocket";

NavigatorSocket::NavigatorSocket(ExecutionContext* context)
    : Supplement(*context), ExecutionContextLifecycleStateObserver(context) {}

// static
NavigatorSocket& NavigatorSocket::From(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  NavigatorSocket* supplement =
      Supplement<ExecutionContext>::From<NavigatorSocket>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorSocket>(context);
    supplement->UpdateStateIfNeeded();
    ProvideTo(*context, supplement);
  }
  return *supplement;
}

// static
ScriptPromise NavigatorSocket::openTCPSocket(ScriptState* script_state,
                                             Navigator& navigator,
                                             const TCPSocketOptions* options,
                                             ExceptionState& exception_state) {
  return From(script_state)
      .openTCPSocket(script_state, options, exception_state);
}

// static
ScriptPromise NavigatorSocket::openUDPSocket(ScriptState* script_state,
                                             Navigator& navigator,
                                             const UDPSocketOptions* options,
                                             ExceptionState& exception_state) {
  return From(script_state)
      .openUDPSocket(script_state, options, exception_state);
}

void NavigatorSocket::ContextDestroyed() {}

void NavigatorSocket::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  if (state == mojom::blink::FrameLifecycleState::kFrozen) {
    // Clear service_remote_ and pending connections.
    OnConnectionError();
  }
}

void NavigatorSocket::Trace(Visitor* visitor) const {
  visitor->Trace(service_remote_);
  visitor->Trace(pending_tcp_);
  visitor->Trace(pending_udp_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

void NavigatorSocket::EnsureServiceConnected(LocalDOMWindow& window) {
  if (!service_remote_.is_bound()) {
    window.GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        service_remote_.BindNewPipeAndPassReceiver(
            window.GetTaskRunner(TaskType::kMiscPlatformAPI)));
    service_remote_.set_disconnect_handler(WTF::Bind(
        &NavigatorSocket::OnConnectionError, WrapWeakPersistent(this)));
    DCHECK(service_remote_.is_bound());
  }
}

// static
mojom::blink::DirectSocketOptionsPtr NavigatorSocket::CreateSocketOptions(
    const SocketOptions* options,
    NavigatorSocket::ProtocolType socket_type,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectSocketOptions::New();

  const bool has_full_local_address =
      options->hasLocalAddress() && options->hasLocalPort();

  if (const bool has_partial_local_address =
          options->hasLocalAddress() || options->hasLocalPort();
      has_partial_local_address && !has_full_local_address) {
    exception_state.ThrowTypeError("Incomplete local address specified.");
    return {};
  }

  const bool has_full_remote_address =
      options->hasRemoteAddress() && options->hasRemotePort();

  if (const bool has_partial_remote_address =
          options->hasRemoteAddress() || options->hasRemotePort();
      has_partial_remote_address && !has_full_remote_address) {
    exception_state.ThrowTypeError("Incomplete remote address specified.");
    return {};
  }

  // Socket-specific options & checks.
  switch (socket_type) {
    case NavigatorSocket::ProtocolType::kTcp: {
      if (!has_full_remote_address) {
        exception_state.ThrowTypeError(
            "Complete remote address is always required for TCP.");
        return {};
      }
      const TCPSocketOptions* tcp_options =
          static_cast<const TCPSocketOptions*>(options);
      if (tcp_options->hasNoDelay()) {
        socket_options->no_delay = tcp_options->noDelay();
      }
      if (!CheckKeepAliveOptionsValidity(tcp_options, exception_state)) {
        return {};
      }
      if (tcp_options->hasKeepAlive()) {
        socket_options->keep_alive_options =
            network::mojom::blink::TCPKeepAliveOptions::New(
                /*enable=*/tcp_options->keepAlive(),
                /*delay=*/base::Milliseconds(
                    tcp_options->getKeepAliveDelayOr(0))
                    .InSeconds());
      }
      break;
    }
    case NavigatorSocket::ProtocolType::kUdp: {
      if (!has_full_remote_address && !has_full_local_address) {
        exception_state.ThrowTypeError(
            "Neither complete remote address nor "
            "complete local address specified.");
        return {};
      }
      if (has_full_remote_address && has_full_local_address) {
        exception_state.ThrowTypeError(
            "Both remote address and local address specified -- please "
            "choose only one.");
        return {};
      }
    }
  }

  if (has_full_local_address) {
    socket_options->local_hostname = options->localAddress();
    socket_options->local_port = options->localPort();
  }

  if (has_full_remote_address) {
    socket_options->remote_hostname = options->remoteAddress();
    socket_options->remote_port = options->remotePort();
  }

  if (options->hasSendBufferSize()) {
    socket_options->send_buffer_size = options->sendBufferSize();
  }
  if (options->hasReceiveBufferSize()) {
    socket_options->receive_buffer_size = options->receiveBufferSize();
  }

  return socket_options;
}

ScriptPromise NavigatorSocket::openTCPSocket(ScriptState* script_state,
                                             const TCPSocketOptions* options,
                                             ExceptionState& exception_state) {
  if (!OpenSocketPermitted(script_state, options, exception_state))
    return ScriptPromise();

  mojom::blink::DirectSocketOptionsPtr open_tcp_socket_options =
      CreateSocketOptions(options, NavigatorSocket::ProtocolType::kTcp,
                          exception_state);
  if (!open_tcp_socket_options) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  TCPSocket* pending = MakeGarbageCollected<TCPSocket>(
      ExecutionContext::From(script_state), *resolver);
  pending_tcp_.insert(pending);
  ScriptPromise promise = resolver->Promise();

  service_remote_->OpenTcpSocket(
      std::move(open_tcp_socket_options), pending->GetTCPSocketReceiver(),
      pending->GetTCPSocketObserver(),
      WTF::Bind(&NavigatorSocket::OnTcpOpen, WrapPersistent(this),
                WrapPersistent(pending)));
  return promise;
}

ScriptPromise NavigatorSocket::openUDPSocket(ScriptState* script_state,
                                             const UDPSocketOptions* options,
                                             ExceptionState& exception_state) {
  if (!OpenSocketPermitted(script_state, options, exception_state))
    return ScriptPromise();

  mojom::blink::DirectSocketOptionsPtr open_udp_socket_options =
      CreateSocketOptions(options, NavigatorSocket::ProtocolType::kUdp,
                          exception_state);
  if (!open_udp_socket_options) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  UDPSocket* pending = MakeGarbageCollected<UDPSocket>(
      ExecutionContext::From(script_state), *resolver);
  pending_udp_.insert(pending);
  ScriptPromise promise = resolver->Promise();

  service_remote_->OpenUdpSocket(
      std::move(open_udp_socket_options), pending->GetUDPSocketReceiver(),
      pending->GetUDPSocketListener(),
      WTF::Bind(&NavigatorSocket::OnUdpOpen, WrapPersistent(this),
                WrapPersistent(pending)));
  return promise;
}

bool NavigatorSocket::OpenSocketPermitted(ScriptState* script_state,
                                          const SocketOptions* options,
                                          ExceptionState& exception_state) {
  DCHECK_EQ(ExecutionContext::From(script_state), GetExecutionContext());
  LocalDOMWindow* const window = script_state->ContextIsValid()
                                     ? LocalDOMWindow::From(script_state)
                                     : nullptr;
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    return false;
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               mojom::blink::PermissionsPolicyFeature::kDirectSockets)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Permissions-Policy: direct-sockets are disabled.");
    return false;
  }

  // TODO(crbug.com/1119600): Do not consume (or check) transient activation
  // for reconnection attempts.
  if (!LocalFrame::ConsumeTransientUserActivation(window->GetFrame())) {
    base::UmaHistogramEnumeration(
        kPermissionDeniedHistogramName,
        blink::mojom::blink::DirectSocketFailureType::kTransientActivation);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Must be handling a user gesture to open a socket.");
    return false;
  }

  EnsureServiceConnected(*window);
  return true;
}

void NavigatorSocket::OnTcpOpen(
    TCPSocket* socket,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr,
    const absl::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  pending_tcp_.erase(socket);
  socket->Init(result, local_addr, peer_addr, std::move(receive_stream),
               std::move(send_stream));
}

void NavigatorSocket::OnUdpOpen(
    UDPSocket* socket,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr,
    const absl::optional<net::IPEndPoint>& peer_addr) {
  pending_udp_.erase(socket);
  socket->Init(result, local_addr, peer_addr);
}

void NavigatorSocket::OnConnectionError() {
  for (auto& pending : pending_tcp_) {
    pending->Init(net::Error::ERR_CONTEXT_SHUT_DOWN, absl::nullopt,
                  absl::nullopt, mojo::ScopedDataPipeConsumerHandle(),
                  mojo::ScopedDataPipeProducerHandle());
  }
  for (auto& pending : pending_udp_) {
    pending->Init(net::Error::ERR_CONTEXT_SHUT_DOWN, absl::nullopt,
                  absl::nullopt);
  }
  pending_tcp_.clear();
  pending_udp_.clear();
  service_remote_.reset();
}

}  // namespace blink
