// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/navigator_socket.h"

#include "base/macros.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/direct_sockets/socket_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class NavigatorSocket::PendingRequest final
    : public GarbageCollected<PendingRequest> {
 public:
  PendingRequest(NavigatorSocket&, ScriptPromiseResolver&);

  // TODO(crbug.com/905818): Resolve Promise<TCPSocket>
  void TcpCallback(int32_t result);

  // TODO(crbug.com/1119620): Resolve Promise<UDPSocket>
  void UdpCallback(int32_t result);

  void OnConnectionError();

  void Trace(Visitor* visitor) const {
    visitor->Trace(navigator_);
    visitor->Trace(resolver_);
  }

 private:
  WeakMember<NavigatorSocket> navigator_;
  Member<ScriptPromiseResolver> resolver_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

NavigatorSocket::PendingRequest::PendingRequest(
    NavigatorSocket& navigator_socket,
    ScriptPromiseResolver& resolver)
    : navigator_(&navigator_socket),
      resolver_(&resolver),
      feature_handle_for_scheduler_(
          ExecutionContext::From(resolver_->GetScriptState())
              ->GetScheduler()
              ->RegisterFeature(
                  SchedulingPolicy::Feature::
                      kOutstandingNetworkRequestDirectSocket,
                  {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {}

void NavigatorSocket::PendingRequest::TcpCallback(int32_t result) {
  if (navigator_)
    navigator_->pending_requests_.erase(this);

  // TODO(crbug.com/905818): Compare with net::OK
  if (result == 0) {
    // TODO(crbug.com/905818): Resolve TCPSocket
    NOTIMPLEMENTED();
    resolver_->Resolve();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Permission denied"));
  }
}

void NavigatorSocket::PendingRequest::UdpCallback(int32_t result) {
  if (navigator_)
    navigator_->pending_requests_.erase(this);

  // TODO(crbug.com/1119620): Compare with net::OK
  if (result == 0) {
    // TODO(crbug.com/1119620): Resolve UDPSocket
    NOTIMPLEMENTED();
    resolver_->Resolve();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Permission denied"));
  }
}

void NavigatorSocket::PendingRequest::OnConnectionError() {
  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError,
      "Internal error: could not connect to DirectSocketsService interface."));
}

const char NavigatorSocket::kSupplementName[] = "NavigatorSocket";

NavigatorSocket::NavigatorSocket(ExecutionContext* context)
    : ExecutionContextLifecycleStateObserver(context) {}

// static
NavigatorSocket& NavigatorSocket::From(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  NavigatorSocket* supplement =
      Supplement<ExecutionContext>::From<NavigatorSocket>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorSocket>(context);
    ProvideTo(*context, supplement);
  }
  return *supplement;
}

// static
ScriptPromise NavigatorSocket::openTCPSocket(ScriptState* script_state,
                                             Navigator& navigator,
                                             const SocketOptions* options,
                                             ExceptionState& exception_state) {
  return From(script_state)
      .openTCPSocket(script_state, options, exception_state);
}

// static
ScriptPromise NavigatorSocket::openUDPSocket(ScriptState* script_state,
                                             Navigator& navigator,
                                             const SocketOptions* options,
                                             ExceptionState& exception_state) {
  return From(script_state)
      .openUDPSocket(script_state, options, exception_state);
}

void NavigatorSocket::ContextDestroyed() {}

void NavigatorSocket::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  // TODO(crbug.com/1120868) close connections when the lifecycle state is not
  // "active".
  NOTIMPLEMENTED();
}

void NavigatorSocket::Trace(Visitor* visitor) const {
  visitor->Trace(service_remote_);
  visitor->Trace(pending_requests_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

void NavigatorSocket::EnsureServiceConnected(LocalDOMWindow& window) {
  DCHECK(RuntimeEnabledFeatures::DirectSocketsEnabled());

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
    const SocketOptions& options) {
  auto socket_options = mojom::blink::DirectSocketOptions::New();

  if (options.hasLocalAddress())
    socket_options->local_hostname = options.localAddress();
  if (options.hasLocalPort())
    socket_options->local_port = options.localPort();

  if (options.hasRemoteAddress())
    socket_options->remote_hostname = options.remoteAddress();
  if (options.hasRemotePort())
    socket_options->remote_port = options.remotePort();

  if (options.hasSendBufferSize())
    socket_options->send_buffer_size = options.sendBufferSize();
  if (options.hasReceiveBufferSize())
    socket_options->receive_buffer_size = options.receiveBufferSize();

  if (options.hasKeepAlive())
    socket_options->keep_alive = options.keepAlive();
  if (options.hasNoDelay())
    socket_options->no_delay = options.noDelay();

  return socket_options;
}

ScriptPromise NavigatorSocket::openTCPSocket(ScriptState* script_state,
                                             const SocketOptions* options,
                                             ExceptionState& exception_state) {
  if (!OpenSocketPermitted(script_state, options, exception_state))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  PendingRequest* pending =
      MakeGarbageCollected<PendingRequest>(*this, *resolver);
  pending_requests_.insert(pending);
  ScriptPromise promise = resolver->Promise();

  service_remote_->OpenTcpSocket(
      CreateSocketOptions(*options),
      WTF::Bind(&PendingRequest::TcpCallback, WrapPersistent(pending)));
  return promise;
}

ScriptPromise NavigatorSocket::openUDPSocket(ScriptState* script_state,
                                             const SocketOptions* options,
                                             ExceptionState& exception_state) {
  if (!OpenSocketPermitted(script_state, options, exception_state))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  PendingRequest* pending =
      MakeGarbageCollected<PendingRequest>(*this, *resolver);
  pending_requests_.insert(pending);
  ScriptPromise promise = resolver->Promise();

  service_remote_->OpenUdpSocket(
      CreateSocketOptions(*options),
      WTF::Bind(&PendingRequest::UdpCallback, WrapPersistent(pending)));
  return promise;
}

bool NavigatorSocket::OpenSocketPermitted(ScriptState* script_state,
                                          const SocketOptions* options,
                                          ExceptionState& exception_state) {
  LocalDOMWindow* const window = script_state->ContextIsValid()
                                     ? LocalDOMWindow::From(script_state)
                                     : nullptr;
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    return false;
  }

  // TODO(crbug.com/1119600): Do not consume (or check) transient activation
  // for reconnection attempts.
  if (!LocalFrame::ConsumeTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Must be handling a user gesture to open a socket.");
    return false;
  }

  DCHECK(options);
  if (!options->hasRemotePort()) {
    exception_state.ThrowTypeError("remotePort was not specified.");
    return false;
  }

  EnsureServiceConnected(*window);
  return true;
}

void NavigatorSocket::OnConnectionError() {
  for (auto& pending : pending_requests_) {
    pending->OnConnectionError();
  }
  pending_requests_.clear();
  service_remote_.reset();
}

}  // namespace blink
