// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_OUTGOING_STREAM_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_OUTGOING_STREAM_CLIENT_H_

#include <stdint.h>

#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// Shared OutgoingStream::Client implementation used by both SendStream and
// WebTransportSendStream. Delegates SendFin/ForgetStream/Reset to the
// WebTransport object that owns the stream.
class OutgoingStreamClient final
    : public GarbageCollected<OutgoingStreamClient>,
      public OutgoingStream::Client {
 public:
  OutgoingStreamClient(WebTransport* transport, uint32_t stream_id)
      : transport_(transport), stream_id_(stream_id) {}

  // OutgoingStream::Client implementation:
  void SendFin() override { transport_->SendFin(stream_id_); }

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
  const uint32_t stream_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_OUTGOING_STREAM_CLIENT_H_
