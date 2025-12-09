// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_quic_spdy_stream.h"

#include <sys/types.h>  // for struct iovec

#include "base/check.h"
#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"

namespace quic {
class QuicHeaderList;
}  // namespace quic

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

void WebSocketQuicSpdyStream::OnClose() {
  quic::QuicSpdyStream::OnClose();
  if (delegate_) {
    delegate_->OnClose(MapQuicErrorToNetError());
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

void WebSocketQuicSpdyStream::OnCanWriteNewData() {
  quic::QuicSpdyStream::OnCanWriteNewData();
  if (delegate_) {
    delegate_->OnCanWriteNewData();
  }
}

int WebSocketQuicSpdyStream::MapQuicErrorToNetError() {
  // Map Connection QUIC errors to net errors.
  if (connection_error() != quic::QUIC_NO_ERROR) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }

  // Map Stream QUIC errors to net errors.
  switch (stream_error()) {
    case quic::QUIC_STREAM_NO_ERROR:
      return OK;
    case quic::QUIC_STREAM_GENERAL_PROTOCOL_ERROR:
      return ERR_QUIC_PROTOCOL_ERROR;
    case quic::QUIC_STREAM_INTERNAL_ERROR:
      return ERR_FAILED;
    case quic::QUIC_STREAM_CANCELLED:
      return ERR_ABORTED;
    default:
      return ERR_CONNECTION_RESET;
  }
}

}  // namespace net
