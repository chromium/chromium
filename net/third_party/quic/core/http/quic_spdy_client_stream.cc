// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_spdy_client_stream.h"

#include <utility>

#include "net/third_party/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quic/core/http/quic_spdy_client_session.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_alarm.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

using spdy::SpdyHeaderBlock;

namespace quic {

QuicSpdyClientStream::QuicSpdyClientStream(QuicStreamId id,
                                           QuicSpdyClientSession* session,
                                           StreamType type)
    : QuicSpdyStream(id, session, type),
      content_length_(-1),
      response_code_(0),
      header_bytes_read_(0),
      header_bytes_written_(0),
      session_(session),
      has_preliminary_headers_(false) {}

QuicSpdyClientStream::~QuicSpdyClientStream() = default;

void QuicSpdyClientStream::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len, header_list);

  DCHECK(headers_decompressed());
  header_bytes_read_ += frame_len;
  if (!SpdyUtils::CopyAndValidateHeaders(header_list, &content_length_,
                                         &response_headers_)) {
    QUIC_DLOG(ERROR) << "Failed to parse header list: "
                     << header_list.DebugString();
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  if (!ParseHeaderStatusCode(response_headers_, &response_code_)) {
    QUIC_DLOG(ERROR) << "Received invalid response code: "
                     << response_headers_[":status"].as_string();
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  if (response_code_ == 100 && !has_preliminary_headers_) {
    // These are preliminary 100 Continue headers, not the actual response
    // headers.
    set_headers_decompressed(false);
    has_preliminary_headers_ = true;
    preliminary_headers_ = std::move(response_headers_);
  }

  ConsumeHeaderList();
  QUIC_DVLOG(1) << "headers complete for stream " << id();

  session_->OnInitialHeadersComplete(id(), response_headers_);
}

void QuicSpdyClientStream::OnTrailingHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QuicSpdyStream::OnTrailingHeadersComplete(fin, frame_len, header_list);
  MarkTrailersConsumed();
}

void QuicSpdyClientStream::OnPromiseHeaderList(
    QuicStreamId promised_id,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  header_bytes_read_ += frame_len;
  int64_t content_length = -1;
  SpdyHeaderBlock promise_headers;
  if (!SpdyUtils::CopyAndValidateHeaders(header_list, &content_length,
                                         &promise_headers)) {
    QUIC_DLOG(ERROR) << "Failed to parse promise headers: "
                     << header_list.DebugString();
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  session_->HandlePromised(id(), promised_id, promise_headers);
  if (visitor() != nullptr) {
    visitor()->OnPromiseHeadersComplete(promised_id, frame_len);
  }
}

void QuicSpdyClientStream::OnDataAvailable() {
  // For push streams, visitor will not be set until the rendezvous
  // between server promise and client request is complete.
  if (visitor() == nullptr)
    return;

  while (HasBytesToRead()) {
    struct iovec iov;
    if (GetReadableRegions(&iov, 1) == 0) {
      // No more data to read.
      break;
    }
    QUIC_DVLOG(1) << "Client processed " << iov.iov_len << " bytes for stream "
                  << id();
    data_.append(static_cast<char*>(iov.iov_base), iov.iov_len);

    if (content_length_ >= 0 &&
        data_.size() > static_cast<uint64_t>(content_length_)) {
      QUIC_DLOG(ERROR) << "Invalid content length (" << content_length_
                       << ") with data of size " << data_.size();
      Reset(QUIC_BAD_APPLICATION_PAYLOAD);
      return;
    }
    MarkConsumed(iov.iov_len);
  }
  if (sequencer()->IsClosed()) {
    OnFinRead();
  } else {
    sequencer()->SetUnblocked();
  }
}

size_t QuicSpdyClientStream::SendRequest(SpdyHeaderBlock headers,
                                         QuicStringPiece body,
                                         bool fin) {
  QuicConnection::ScopedPacketFlusher flusher(
      session_->connection(), QuicConnection::SEND_ACK_IF_QUEUED);
  bool send_fin_with_headers = fin && body.empty();
  size_t bytes_sent = body.size();
  header_bytes_written_ =
      WriteHeaders(std::move(headers), send_fin_with_headers, nullptr);
  bytes_sent += header_bytes_written_;

  if (!body.empty()) {
    WriteOrBufferData(body, fin, nullptr);
  }

  return bytes_sent;
}

}  // namespace quic
