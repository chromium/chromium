// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_datagrams_writable.h"

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_group.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

// Avoid queueing datagrams outside the transport, where expiration and
// connection-level buffer limits cannot apply.
constexpr size_t kHighWaterMark = 1;

}  // namespace

WebTransportDatagramsWritable::WebTransportDatagramsWritable(
    ScriptState* script_state,
    WebTransport* transport,
    WebTransportSendGroup* send_group,
    int64_t send_order)
    : WritableStream(script_state),
      transport_(transport),
      send_group_(send_group),
      send_order_(send_order) {}

WebTransportDatagramsWritable::~WebTransportDatagramsWritable() = default;

void WebTransportDatagramsWritable::Init(ScriptState* script_state,
                                         UnderlyingSinkBase* sink,
                                         ExceptionState& exception_state) {
  InitWithCountQueueingStrategy(script_state, sink, kHighWaterMark,
                                /*optimizer=*/nullptr, exception_state);
}

void WebTransportDatagramsWritable::setSendGroup(
    WebTransportSendGroup* group,
    ExceptionState& exception_state) {
  if (group && group->GetTransport() != transport_.Get()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The sendGroup belongs to a different WebTransport instance.");
    return;
  }
  send_group_ = group;
}

void WebTransportDatagramsWritable::Trace(Visitor* visitor) const {
  visitor->Trace(transport_);
  visitor->Trace(send_group_);
  WritableStream::Trace(visitor);
}

}  // namespace blink
