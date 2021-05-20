// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/send_stream.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SendStream::SendStream(ScriptState* script_state,
                       WebTransport* web_transport,
                       uint32_t stream_id,
                       mojo::ScopedDataPipeProducerHandle handle)
    : outgoing_stream_(MakeGarbageCollected<OutgoingStream>(script_state,
                                                            this,
                                                            std::move(handle))),
      web_transport_(web_transport),
      stream_id_(stream_id) {}

SendStream::~SendStream() = default;

void SendStream::OnIncomingStreamClosed(bool fin_received) {
  // SendStream is not an IncomingStream, so this shouldn't be called.
  NOTREACHED();
}

void SendStream::Reset() {
  outgoing_stream_->Reset();
}

void SendStream::ContextDestroyed() {
  outgoing_stream_->ContextDestroyed();
}

void SendStream::SendFin() {
  web_transport_->SendFin(stream_id_);
  web_transport_->ForgetStream(stream_id_);
}

void SendStream::OnOutgoingStreamAbort() {
  web_transport_->AbortStream(stream_id_);
  web_transport_->ForgetStream(stream_id_);
}

void SendStream::Trace(Visitor* visitor) const {
  visitor->Trace(outgoing_stream_);
  visitor->Trace(web_transport_);
  WritableStream::Trace(visitor);
  WebTransportStream::Trace(visitor);
  OutgoingStream::Client::Trace(visitor);
}

}  // namespace blink
