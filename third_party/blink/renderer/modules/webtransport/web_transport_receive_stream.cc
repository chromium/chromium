// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_receive_stream.h"

#include <utility>

#include "services/network/public/mojom/web_transport.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
// But it also asks for the last receive stream stats to get final network byte
// count.
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
    transport->MaybeGetReceiveStreamStats(stream_id);
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
    : ReadableStream(script_state),
      web_transport_(web_transport),
      stream_id_(stream_id),
      incoming_stream_(MakeGarbageCollected<IncomingStream>(
          script_state,
          BindOnce(ForgetStream, WrapWeakPersistent(web_transport), stream_id),
          std::move(handle))) {}

WebTransportReceiveStream::~WebTransportReceiveStream() = default;

void WebTransportReceiveStream::OnGetStatsResponse(
    ScriptPromiseResolver<WebTransportReceiveStreamStats>* resolver,
    network::mojom::blink::WebTransportReceiveStreamStatsPtr mojo_stats) {
  if (mojo_stats) {
    incoming_stream_->UpdateNetworkBytesReceived(mojo_stats->bytes_received);
  }

  const uint64_t bytes_received = incoming_stream_->BytesReceived();
  auto* stats = MakeGarbageCollected<WebTransportReceiveStreamStats>();
  DCHECK_LE(bytes_read_, bytes_received);
  stats->setBytesReceived(bytes_received);
  stats->setBytesRead(bytes_read_);
  resolver->Resolve(stats);
}

ScriptPromise<WebTransportReceiveStreamStats>
WebTransportReceiveStream::getStats(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<WebTransportReceiveStreamStats>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<WebTransportReceiveStreamStats>>(script_state);
  auto promise = resolver->Promise();

  if (!web_transport_) {
    OnGetStatsResponse(resolver, nullptr);
    return promise;
  }

  web_transport_->GetReceiveStreamStats(
      stream_id_,
      blink::BindOnce(&WebTransportReceiveStream::OnGetStatsResponse,
                      WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void WebTransportReceiveStream::Trace(Visitor* visitor) const {
  visitor->Trace(web_transport_);
  visitor->Trace(incoming_stream_);
  ReadableStream::Trace(visitor);
}

}  // namespace blink
