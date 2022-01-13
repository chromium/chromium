// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_socket.h"

#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kUDPNetworkFailuresHistogramName[] =
    "DirectSockets.UDPNetworkFailures";

}

UDPSocket::UDPSocket(ExecutionContext* execution_context,
                     ScriptPromiseResolver& resolver)
    : ExecutionContextClient(execution_context),
      init_resolver_(&resolver),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kOutstandingNetworkRequestDirectSocket,
              {SchedulingPolicy::DisableBackForwardCache()})) {
  DCHECK(init_resolver_);
}

UDPSocket::~UDPSocket() = default;

mojo::PendingReceiver<blink::mojom::blink::DirectUDPSocket>
UDPSocket::GetUDPSocketReceiver() {
  DCHECK(init_resolver_);
  return udp_socket_.BindNewPipeAndPassReceiver();
}

mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
UDPSocket::GetUDPSocketListener() {
  DCHECK(init_resolver_);
  auto result = socket_listener_receiver_.BindNewPipeAndPassRemote();

  socket_listener_receiver_.set_disconnect_handler(WTF::Bind(
      &UDPSocket::OnSocketListenerConnectionError, WrapPersistent(this)));

  return result;
}

void UDPSocket::Init(int32_t result,
                     const absl::optional<net::IPEndPoint>& local_addr,
                     const absl::optional<net::IPEndPoint>& peer_addr) {
  DCHECK(init_resolver_);
  if (result == net::Error::OK && peer_addr.has_value()) {
    peer_addr_ = peer_addr;
    init_resolver_->Resolve(this);
  } else {
    if (result != net::Error::OK) {
      // Error codes are negative.
      base::UmaHistogramSparse(kUDPNetworkFailuresHistogramName, -result);
    }
    // TODO(crbug/1282199): Create specific exception based on error code.
    init_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Network error."));
  }
  init_resolver_ = nullptr;
}

ScriptPromise UDPSocket::close(ScriptState* script_state, ExceptionState&) {
  DoClose(/*is_local_close=*/true);

  return ScriptPromise::CastUndefined(script_state);
}

String UDPSocket::remoteAddress() const {
  return String::FromUTF8(peer_addr_->ToStringWithoutPort());
}

uint16_t UDPSocket::remotePort() const {
  return peer_addr_->port();
}

void UDPSocket::OnReceived(int32_t result,
                           const absl::optional<::net::IPEndPoint>& src_addr,
                           absl::optional<::base::span<const ::uint8_t>> data) {
  // TODO(crbug.com/1119620): Implement.
  NOTIMPLEMENTED();
}

bool UDPSocket::HasPendingActivity() const {
  return !!send_resolver_;
}

void UDPSocket::Trace(Visitor* visitor) const {
  visitor->Trace(init_resolver_);
  visitor->Trace(send_resolver_);
  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void UDPSocket::OnSocketListenerConnectionError() {
  DoClose(/*is_local_close=*/false);
}

void UDPSocket::DoClose(bool is_local_close) {
  init_resolver_ = nullptr;
  socket_listener_receiver_.reset();
  if (is_local_close && udp_socket_.is_bound())
    udp_socket_->Close();
  udp_socket_.reset();
  feature_handle_for_scheduler_.reset();
}

}  // namespace blink
