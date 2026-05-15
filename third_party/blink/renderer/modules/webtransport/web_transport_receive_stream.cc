// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_receive_stream.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_receive_stream_stats.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Mirrors receive_stream.cc::ForgetStream. When the IncomingStream is done it
// asks WebTransport to forget the stream, optionally sending STOP_SENDING.
//
// `transport` is captured via WrapWeakPersistent below, so it can be nullptr
// here if the WebTransport was garbage-collected before the IncomingStream
// fired this callback. In that case there is nothing to forget — the
// WebTransport (and the incoming_stream_map_ entry plus its Mojo bindings) no
// longer exists — so we return early instead of dereferencing nullptr.
void ForgetStream(WebTransport* transport,
                  uint32_t stream_id,
                  std::optional<uint8_t> stop_sending_code,
                  bool has_received_close) {
  if (!transport) {
    return;
  }
  if (stop_sending_code) {
    transport->StopSending(stream_id, *stop_sending_code);
  }
  transport->ForgetIncomingStream(stream_id, has_received_close);
}

}  // namespace

WebTransportReceiveStream::WebTransportReceiveStream(
    ScriptState* script_state,
    WebTransport* web_transport,
    uint32_t stream_id,
    mojo::ScopedDataPipeConsumerHandle handle)
    : incoming_stream_(MakeGarbageCollected<IncomingStream>(
          script_state,
          BindOnce(ForgetStream, WrapWeakPersistent(web_transport), stream_id),
          std::move(handle))) {}

WebTransportReceiveStream::~WebTransportReceiveStream() = default;

ScriptPromise<WebTransportReceiveStreamStats>
WebTransportReceiveStream::getStats(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<WebTransportReceiveStreamStats>();
  }
  // TODO(crbug.com/510589920): Implement actual stats collection from the
  // network service via Mojo. Currently returns zeroed stats — this is a stub,
  // matching WebTransportSendStream::getStats(). The IDL defaults
  // (bytesReceived = 0, bytesRead = 0) are relied on here; do not add
  // setBytesReceived/setBytesRead calls without real Mojo data.
  auto* stats = MakeGarbageCollected<WebTransportReceiveStreamStats>();
  return ToResolvedPromise<WebTransportReceiveStreamStats>(script_state, stats);
}

void WebTransportReceiveStream::Trace(Visitor* visitor) const {
  visitor->Trace(incoming_stream_);
  ReadableStream::Trace(visitor);
}

}  // namespace blink
