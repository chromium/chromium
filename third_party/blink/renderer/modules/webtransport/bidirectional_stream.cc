// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"

#include <utility>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webtransport/send_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

BidirectionalStream::BidirectionalStream(
    ScriptState* script_state,
    WebTransport* web_transport,
    uint32_t stream_id,
    mojo::ScopedDataPipeProducerHandle outgoing_producer,
    mojo::ScopedDataPipeConsumerHandle incoming_consumer)
    : receive_stream_(
          MakeGarbageCollected<ReceiveStream>(script_state,
                                              web_transport,
                                              stream_id,
                                              std::move(incoming_consumer))) {
  // TODO(crbug.com/487117768): Remove old SendStream path when
  // WebTransportSendGroup ships.
  if (RuntimeEnabledFeatures::WebTransportSendGroupEnabled(
          ExecutionContext::From(script_state))) {
    send_stream_ = MakeGarbageCollected<WebTransportSendStream>(
        script_state, web_transport, stream_id, std::move(outgoing_producer));
  } else {
    send_stream_ = MakeGarbageCollected<SendStream>(
        script_state, web_transport, stream_id, std::move(outgoing_producer));
  }
}

void BidirectionalStream::Init(ExceptionState& exception_state) {
  if (auto* send_stream =
          DynamicTo<WebTransportSendStream>(send_stream_.Get())) {
    send_stream->Init(exception_state);
  } else {
    // The constructor guarantees send_stream_ is either WebTransportSendStream
    // or SendStream. SendStream lacks DowncastTraits (no IDL binding), so we
    // can't DynamicTo<SendStream>; we CHECK non-null and rely on the
    // constructor-established type invariant for the static_cast.
    auto* writable = send_stream_.Get();
    CHECK(writable);
    static_cast<SendStream*>(writable)->Init(exception_state);
  }
  if (exception_state.HadException())
    return;

  receive_stream_->Init(exception_state);
}

OutgoingStream* BidirectionalStream::GetOutgoingStream() {
  if (auto* send_stream =
          DynamicTo<WebTransportSendStream>(send_stream_.Get())) {
    return send_stream->GetOutgoingStream();
  }
  // The constructor guarantees send_stream_ is either WebTransportSendStream
  // or SendStream. SendStream lacks DowncastTraits (no IDL binding), so we
  // can't DynamicTo<SendStream>; we CHECK non-null and rely on the
  // constructor-established type invariant for the static_cast.
  auto* writable = send_stream_.Get();
  CHECK(writable);
  return static_cast<SendStream*>(writable)->GetOutgoingStream();
}

void BidirectionalStream::Trace(Visitor* visitor) const {
  visitor->Trace(send_stream_);
  visitor->Trace(receive_stream_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
