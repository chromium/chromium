// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_quic_spdy_stream.h"

namespace net {

WebSocketQuicSpdyStream::WebSocketQuicSpdyStream(
    quic::QuicStreamId id,
    quic::QuicSpdyClientSessionBase* session,
    quic::StreamType type)
    : quic::QuicSpdyStream(id, session, type) {}

WebSocketQuicSpdyStream::~WebSocketQuicSpdyStream() {
  if (delegate_) {
    delegate_->ClearStream();
  }
}

void WebSocketQuicSpdyStream::OnBodyAvailable() {
  if (delegate_) {
    delegate_->OnBodyAvailable();
  }
}

void WebSocketQuicSpdyStream::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const quic::QuicHeaderList& header_list) {
  QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len, header_list);
  if (delegate_) {
    delegate_->OnInitialHeadersComplete(fin, frame_len, header_list);
  }
}

}  // namespace net
