// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/send_stream.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class OutgoingStreamClient final
    : public GarbageCollected<OutgoingStreamClient>,
      public OutgoingStream::Client {
 public:
  OutgoingStreamClient(WebTransport* transport, uint32_t stream_id)
      : transport_(transport), stream_id_(stream_id) {}

  // OutgoingStream::Client implementation
  void SendFin() override {
    transport_->SendFin(stream_id_);
  }

  void ForgetStream() override { transport_->ForgetOutgoingStream(stream_id_); }

  void Reset(uint8_t code) override {
    transport_->ResetStream(stream_id_, code);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(transport_);
    OutgoingStream::Client::Trace(visitor);
  }

 private:
  const Member<WebTransport> transport_;
  base::OnceClosure fin_callback_;
  const uint32_t stream_id_;
};

}  // namespace

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
