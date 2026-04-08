// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/send_stream.h"

#include <utility>

#include "third_party/blink/renderer/modules/webtransport/outgoing_stream_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SendStream::SendStream(ScriptState* script_state,
                       WebTransport* web_transport,
                       uint32_t stream_id,
                       mojo::ScopedDataPipeProducerHandle handle)
    : outgoing_stream_(MakeGarbageCollected<OutgoingStream>(
          script_state,
          MakeGarbageCollected<OutgoingStreamClient>(web_transport, stream_id),
          std::move(handle))) {}

SendStream::~SendStream() = default;

void SendStream::Trace(Visitor* visitor) const {
  visitor->Trace(outgoing_stream_);
  WritableStream::Trace(visitor);
}

}  // namespace blink
