// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_send_stream.h"

#include <optional>
#include <utility>

#include "services/network/public/mojom/web_transport.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_stream_stats.h"
#include "third_party/blink/renderer/modules/webtransport/outgoing_stream_client.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_group.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

WebTransportSendStream::WebTransportSendStream(
    ScriptState* script_state,
    WebTransport* web_transport,
    uint32_t stream_id,
    mojo::ScopedDataPipeProducerHandle handle)
    : WritableStream(script_state),
      transport_(web_transport),
      stream_id_(stream_id),
      outgoing_stream_(MakeGarbageCollected<OutgoingStream>(
          script_state,
          MakeGarbageCollected<OutgoingStreamClient>(web_transport, stream_id),
          std::move(handle))) {}

WebTransportSendStream::~WebTransportSendStream() = default;

void WebTransportSendStream::setSendGroup(WebTransportSendGroup* group,
                                          ExceptionState& exception_state) {
  // Per spec, the sendGroup must belong to the same WebTransport instance.
  // https://w3c.github.io/webtransport/#dom-webtransportsendstream-sendgroup
  if (group && group->GetTransport() != transport_.Get()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The sendGroup belongs to a different WebTransport instance.");
    return;
  }
  if (send_group_ != group) {
    send_group_ = group;
    SendPriorityUpdate();
  }
}

void WebTransportSendStream::setSendOrder(int64_t order) {
  if (send_order_ != order) {
    send_order_ = order;
    SendPriorityUpdate();
  }
}

void WebTransportSendStream::ApplySendStreamOptions(
    WebTransportSendGroup* send_group,
    int64_t send_order) {
  send_group_ = send_group;
  send_order_ = send_order;
}

void WebTransportSendStream::SendPriorityUpdate() {
  transport_->SetStreamPriority(
      stream_id_,
      network::mojom::blink::WebTransportStreamPriority::New(
          send_group_ ? std::make_optional<uint32_t>(send_group_->group_id())
                      : std::nullopt,
          send_order_));
}

ScriptPromise<WebTransportSendStreamStats> WebTransportSendStream::getStats(
    ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<WebTransportSendStreamStats>();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebTransportSendStreamStats>>(
          script_state);
  auto promise = resolver->Promise();
  if (outgoing_stream_->AreSendStatsFinalized()) {
    ResolveGetStats(resolver);
    return promise;
  }
  transport_->GetSendStreamStats(
      stream_id_, BindOnce(&WebTransportSendStream::OnGetStatsResponse,
                           WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void WebTransportSendStream::OnGetStatsResponse(
    ScriptPromiseResolver<WebTransportSendStreamStats>* resolver,
    network::mojom::blink::WebTransportSendStreamStatsPtr stats) {
  outgoing_stream_->UpdateSendStats(std::move(stats));
  ResolveGetStats(resolver);
}

void WebTransportSendStream::ResolveGetStats(
    ScriptPromiseResolver<WebTransportSendStreamStats>* resolver) const {
  if (!resolver->GetExecutionContext()) {
    return;
  }
  auto* stats = MakeGarbageCollected<WebTransportSendStreamStats>();
  stats->setBytesWritten(outgoing_stream_->BytesWritten());
  stats->setBytesSent(outgoing_stream_->BytesSent());
  stats->setBytesAcknowledged(outgoing_stream_->BytesAcknowledged());
  resolver->Resolve(stats);
}

void WebTransportSendStream::Trace(Visitor* visitor) const {
  visitor->Trace(transport_);
  visitor->Trace(outgoing_stream_);
  visitor->Trace(send_group_);
  WritableStream::Trace(visitor);
}

}  // namespace blink
