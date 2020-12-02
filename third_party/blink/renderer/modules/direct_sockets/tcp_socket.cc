// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include "base/macros.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

TCPSocket::TCPSocket(ScriptPromiseResolver& resolver)
    : resolver_(&resolver),
      feature_handle_for_scheduler_(
          ExecutionContext::From(resolver_->GetScriptState())
              ->GetScheduler()
              ->RegisterFeature(
                  SchedulingPolicy::Feature::
                      kOutstandingNetworkRequestDirectSocket,
                  {SchedulingPolicy::DisableBackForwardCache()})) {
  DCHECK(resolver_);
}

TCPSocket::~TCPSocket() = default;

mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
TCPSocket::GetTCPSocketReceiver() {
  DCHECK(resolver_);
  return tcp_socket_.BindNewPipeAndPassReceiver();
}

mojo::PendingRemote<network::mojom::blink::SocketObserver>
TCPSocket::GetTCPSocketObserver() {
  DCHECK(resolver_);
  auto result = socket_observer_receiver_.BindNewPipeAndPassRemote();

  socket_observer_receiver_.set_disconnect_handler(WTF::Bind(
      &TCPSocket::OnSocketObserverConnectionError, WrapPersistent(this)));

  return result;
}

void TCPSocket::Init(int32_t result,
                     const base::Optional<net::IPEndPoint>& local_addr,
                     const base::Optional<net::IPEndPoint>& peer_addr,
                     mojo::ScopedDataPipeConsumerHandle receive_stream,
                     mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(resolver_);
  if (result == net::Error::OK) {
    // TODO(crbug.com/905818): Finish initialization.
    NOTIMPLEMENTED();
    resolver_->Resolve(this);
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Permission denied"));
  }
  resolver_ = nullptr;
}

ScriptPromise TCPSocket::close(ScriptState*, ExceptionState&) {
  // TODO(crbug.com/905818): Implement close.
  NOTIMPLEMENTED();
  return ScriptPromise();
}

void TCPSocket::OnReadError(int32_t net_error) {
  // TODO(crbug.com/905818): Implement error handling.
  NOTIMPLEMENTED();
}

void TCPSocket::OnWriteError(int32_t net_error) {
  // TODO(crbug.com/905818): Implement error handling.
  NOTIMPLEMENTED();
}

void TCPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  ScriptWrappable::Trace(visitor);
}

void TCPSocket::OnSocketObserverConnectionError() {
  // TODO(crbug.com/905818): Implement error handling.
  NOTIMPLEMENTED();
}

}  // namespace blink
