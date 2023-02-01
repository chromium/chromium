// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_quic_spdy_stream.h"
#include "net/base/io_buffer.h"
#include "net/http/http_status_code.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/spdy_utils.h"

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

int WebSocketQuicSpdyStream::Read(IOBuffer* buf, int buf_len) {
  DCHECK_GT(buf_len, 0);
  DCHECK(buf->data());

  if (IsDoneReading()) {
    return 0;  // EOF
  }

  if (!HasBytesToRead()) {
    return ERR_IO_PENDING;
  }

  iovec iov;
  iov.iov_base = buf->data();
  iov.iov_len = buf_len;
  size_t bytes_read = Readv(&iov, 1);
  // Since HasBytesToRead is true, Readv() must have read some data.
  DCHECK_NE(0u, bytes_read);
  return bytes_read;
}

}  // namespace net
