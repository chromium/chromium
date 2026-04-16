// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_send_stream.h"

#include <utility>

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
    : transport_(web_transport),
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
  // TODO(crbug.com/487117768): Send priority update via Mojo.
  send_group_ = group;
}

void WebTransportSendStream::setSendOrder(int64_t order) {
  // TODO(crbug.com/487117768): Send priority update via Mojo.
  send_order_ = order;
}

void WebTransportSendStream::ApplySendStreamOptions(
    WebTransportSendGroup* send_group,
    int64_t send_order,
    ExceptionState& exception_state) {
  if (send_group) {
    setSendGroup(send_group, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }
  setSendOrder(send_order);
}

ScriptPromise<WebTransportSendStreamStats> WebTransportSendStream::getStats(
    ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<WebTransportSendStreamStats>();
  }
  // TODO(crbug.com/487117768): Implement actual stats collection from the
  // network service via Mojo. Currently returns zeroed stats regardless of
  // stream state — this is a stub, matching WebTransportSendGroup::getStats().
  auto* stats = MakeGarbageCollected<WebTransportSendStreamStats>();
  stats->setBytesWritten(0);
  stats->setBytesSent(0);
  stats->setBytesAcknowledged(0);
  return ToResolvedPromise<WebTransportSendStreamStats>(script_state, stats);
}

void WebTransportSendStream::Trace(Visitor* visitor) const {
  visitor->Trace(transport_);
  visitor->Trace(outgoing_stream_);
  visitor->Trace(send_group_);
  WritableStream::Trace(visitor);
}

}  // namespace blink
