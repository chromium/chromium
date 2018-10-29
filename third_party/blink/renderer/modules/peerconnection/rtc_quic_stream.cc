// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

namespace blink {

RTCQuicStream::RTCQuicStream(ExecutionContext* context,
                             RTCQuicTransport* transport,
                             QuicStreamProxy* stream_proxy)
    : EventTargetWithInlineData(),
      ContextClient(context),
      transport_(transport),
      proxy_(stream_proxy) {
  DCHECK(transport_);
  DCHECK(proxy_);
}

RTCQuicStream::~RTCQuicStream() = default;

RTCQuicTransport* RTCQuicStream::transport() const {
  return transport_;
}

String RTCQuicStream::state() const {
  switch (state_) {
    case RTCQuicStreamState::kNew:
      return "new";
    case RTCQuicStreamState::kOpening:
      return "opening";
    case RTCQuicStreamState::kOpen:
      return "open";
    case RTCQuicStreamState::kClosing:
      return "closing";
    case RTCQuicStreamState::kClosed:
      return "closed";
  }
  return String();
}

uint32_t RTCQuicStream::readBufferedAmount() const {
  return read_buffered_amount_;
}

uint32_t RTCQuicStream::writeBufferedAmount() const {
  return write_buffered_amount_;
}

void RTCQuicStream::finish() {
  if (!writeable_) {
    return;
  }
  proxy_->Finish();
  writeable_ = false;
  if (readable_) {
    DCHECK_EQ(state_, RTCQuicStreamState::kOpen);
    state_ = RTCQuicStreamState::kClosing;
  } else {
    DCHECK_EQ(state_, RTCQuicStreamState::kClosing);
    Close();
  }
}

void RTCQuicStream::reset() {
  if (IsClosed()) {
    return;
  }
  proxy_->Reset();
  writeable_ = false;
  readable_ = false;
  Close();
}

void RTCQuicStream::Stop() {
  readable_ = false;
  writeable_ = false;
  state_ = RTCQuicStreamState::kClosed;
  proxy_ = nullptr;
}

void RTCQuicStream::Close() {
  Stop();
  transport_->RemoveStream(this);
}

void RTCQuicStream::OnRemoteReset() {
  DCHECK_NE(state_, RTCQuicStreamState::kClosed);
  Close();
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

void RTCQuicStream::OnRemoteFinish() {
  DCHECK_NE(state_, RTCQuicStreamState::kClosed);
  DCHECK(readable_);
  readable_ = false;
  if (writeable_) {
    DCHECK_EQ(state_, RTCQuicStreamState::kOpen);
    state_ = RTCQuicStreamState::kClosing;
  } else {
    DCHECK_EQ(state_, RTCQuicStreamState::kClosing);
    Close();
  }
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

const AtomicString& RTCQuicStream::InterfaceName() const {
  return EventTargetNames::RTCQuicStream;
}

ExecutionContext* RTCQuicStream::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void RTCQuicStream::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
