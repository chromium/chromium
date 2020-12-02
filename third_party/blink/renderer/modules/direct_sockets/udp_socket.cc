// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_socket.h"

#include "base/macros.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

UDPSocket::UDPSocket(ScriptPromiseResolver& resolver)
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

UDPSocket::~UDPSocket() = default;

mojo::PendingReceiver<network::mojom::blink::UDPSocket>
UDPSocket::GetUDPSocketReceiver() {
  DCHECK(resolver_);
  return udp_socket_.BindNewPipeAndPassReceiver();
}

mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
UDPSocket::GetUDPSocketListener() {
  DCHECK(resolver_);
  auto result = socket_listener_receiver_.BindNewPipeAndPassRemote();

  socket_listener_receiver_.set_disconnect_handler(WTF::Bind(
      &UDPSocket::OnSocketListenerConnectionError, WrapPersistent(this)));

  return result;
}

void UDPSocket::Init(int32_t result,
                     const base::Optional<net::IPEndPoint>& local_addr,
                     const base::Optional<net::IPEndPoint>& peer_addr) {
  DCHECK(resolver_);
  if (result == net::Error::OK) {
    // TODO(crbug.com/1119620): Finish initialization.
    NOTIMPLEMENTED();
    resolver_->Resolve(this);
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Permission denied"));
  }
  resolver_ = nullptr;
}

ScriptPromise UDPSocket::close(ScriptState*, ExceptionState&) {
  // TODO(crbug.com/905818): Implement close.
  NOTIMPLEMENTED();
  return ScriptPromise();
}

void UDPSocket::OnReceived(int32_t result,
                           const base::Optional<::net::IPEndPoint>& src_addr,
                           base::Optional<::base::span<const ::uint8_t>> data) {
  // TODO(crbug.com/1119620): Implement.
  NOTIMPLEMENTED();
}

void UDPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  ScriptWrappable::Trace(visitor);
}

void UDPSocket::OnSocketListenerConnectionError() {
  // TODO(crbug.com/1119620): Implement UDP error handling.
  NOTIMPLEMENTED();
}

}  // namespace blink
