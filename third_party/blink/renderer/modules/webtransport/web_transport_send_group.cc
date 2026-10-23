// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_send_group.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_stream_stats.h"
#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class SendGroupStatsAggregator final
    : public GarbageCollected<SendGroupStatsAggregator> {
 public:
  SendGroupStatsAggregator(
      ScriptPromiseResolver<WebTransportSendStreamStats>* resolver,
      uint64_t bytes_written,
      uint64_t bytes_sent,
      uint64_t bytes_acknowledged)
      : resolver_(resolver),
        bytes_written_(bytes_written),
        bytes_sent_(bytes_sent),
        bytes_acknowledged_(bytes_acknowledged) {}

  void AddStream(OutgoingStream* stream) { streams_.push_back(stream); }

  void Resolve() {
    if (!resolver_->GetExecutionContext()) {
      return;
    }

    for (OutgoingStream* stream : streams_) {
      bytes_written_ += stream->BytesWritten();
      bytes_sent_ += stream->BytesSent();
      bytes_acknowledged_ += stream->BytesAcknowledged();
    }

    auto* stats = MakeGarbageCollected<WebTransportSendStreamStats>();
    stats->setBytesWritten(bytes_written_);
    stats->setBytesSent(bytes_sent_);
    stats->setBytesAcknowledged(bytes_acknowledged_);
    resolver_->Resolve(stats);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(resolver_);
    visitor->Trace(streams_);
  }

 private:
  const Member<ScriptPromiseResolver<WebTransportSendStreamStats>> resolver_;
  HeapVector<Member<OutgoingStream>> streams_;
  uint64_t bytes_written_ = 0;
  uint64_t bytes_sent_ = 0;
  uint64_t bytes_acknowledged_ = 0;
};

}  // namespace

WebTransportSendGroup::WebTransportSendGroup(WebTransport* transport,
                                             uint32_t group_id)
    : transport_(transport), group_id_(group_id) {}

WebTransportSendGroup::~WebTransportSendGroup() = default;

ScriptPromise<WebTransportSendStreamStats> WebTransportSendGroup::getStats(
    ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<WebTransportSendStreamStats>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebTransportSendStreamStats>>(
          script_state);
  auto promise = resolver->Promise();

  auto* aggregator = MakeGarbageCollected<SendGroupStatsAggregator>(
      resolver, finalized_bytes_written_, finalized_bytes_sent_,
      finalized_bytes_acknowledged_);

  Vector<uint32_t> active_stream_ids;
  active_stream_ids.ReserveInitialCapacity(streams_.size());
  for (WebTransportSendStream* stream : streams_) {
    OutgoingStream* outgoing_stream = stream->GetOutgoingStream();
    aggregator->AddStream(outgoing_stream);
    active_stream_ids.push_back(stream->stream_id());
  }
  if (active_stream_ids.empty()) {
    aggregator->Resolve();
    return promise;
  }

  transport_->RequestSendStreamStats(
      std::move(active_stream_ids),
      blink::BindOnce(&SendGroupStatsAggregator::Resolve,
                      WrapPersistent(aggregator)));
  return promise;
}

void WebTransportSendGroup::AddStream(WebTransportSendStream& stream) {
  if (stream.GetOutgoingStream()->AreSendStatsFinalized()) {
    AddFinalizedStreamStats(stream);
  } else {
    streams_.insert(&stream);
  }
}

void WebTransportSendGroup::RemoveStream(WebTransportSendStream& stream) {
  if (stream.GetOutgoingStream()->AreSendStatsFinalized()) {
    // TODO(https://github.com/w3c/webtransport/issues/802): Revisit whether
    // reassigning a stream should subtract its entire lifetime stats from its
    // previous group.
    RemoveFinalizedStreamStats(stream);
  } else {
    streams_.erase(&stream);
  }
}

void WebTransportSendGroup::OnStreamStatsFinalized(
    WebTransportSendStream& stream) {
  streams_.erase(&stream);
  AddFinalizedStreamStats(stream);
}

void WebTransportSendGroup::AddFinalizedStreamStats(
    const WebTransportSendStream& stream) {
  finalized_bytes_written_ += stream.GetOutgoingStream()->BytesWritten();
  finalized_bytes_sent_ += stream.GetOutgoingStream()->BytesSent();
  finalized_bytes_acknowledged_ +=
      stream.GetOutgoingStream()->BytesAcknowledged();
}

void WebTransportSendGroup::RemoveFinalizedStreamStats(
    const WebTransportSendStream& stream) {
  finalized_bytes_written_ -= stream.GetOutgoingStream()->BytesWritten();
  finalized_bytes_sent_ -= stream.GetOutgoingStream()->BytesSent();
  finalized_bytes_acknowledged_ -=
      stream.GetOutgoingStream()->BytesAcknowledged();
}

void WebTransportSendGroup::Trace(Visitor* visitor) const {
  visitor->Trace(transport_);
  visitor->Trace(streams_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
