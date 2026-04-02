// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

BidirectionalStream::BidirectionalStream(
    ScriptState* script_state,
    WebTransport* web_transport,
    uint32_t stream_id,
    mojo::ScopedDataPipeProducerHandle outgoing_producer,
    mojo::ScopedDataPipeConsumerHandle incoming_consumer)
    : send_stream_(
          MakeGarbageCollected<SendStream>(script_state,
                                           web_transport,
                                           stream_id,
                                           std::move(outgoing_producer))),
      receive_stream_(
          MakeGarbageCollected<ReceiveStream>(script_state,
                                              web_transport,
                                              stream_id,
                                              std::move(incoming_consumer))) {}

void BidirectionalStream::Init(ExceptionState& exception_state) {
  send_stream_->Init(exception_state);
  if (exception_state.HadException())
    return;

  receive_stream_->Init(exception_state);
}

void BidirectionalStream::Trace(Visitor* visitor) const {
  visitor->Trace(send_stream_);
  visitor->Trace(receive_stream_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
