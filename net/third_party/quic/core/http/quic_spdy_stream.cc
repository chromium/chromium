// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_spdy_stream.h"

#include <utility>

#include "net/third_party/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/quic_write_blocked_list.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

using spdy::SpdyHeaderBlock;
using spdy::SpdyPriority;

namespace quic {
#define ENDPOINT                                                   \
  (session()->perspective() == Perspective::IS_SERVER ? "Server: " \
                                                      : "Client:"  \
                                                        " ")

QuicSpdyStream::QuicSpdyStream(QuicStreamId id,
                               QuicSpdySession* spdy_session,
                               StreamType type)
    : QuicStream(id, spdy_session, /*is_static=*/false, type),
      spdy_session_(spdy_session),
      visitor_(nullptr),
      headers_decompressed_(false),
      trailers_decompressed_(false),
      trailers_consumed_(false) {
  DCHECK_NE(QuicUtils::GetCryptoStreamId(
                spdy_session->connection()->transport_version()),
            id);
  // Don't receive any callbacks from the sequencer until headers
  // are complete.
  sequencer()->SetBlockedUntilFlush();
}

QuicSpdyStream::~QuicSpdyStream() {}

size_t QuicSpdyStream::WriteHeaders(
    SpdyHeaderBlock header_block,
    bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  size_t bytes_written = spdy_session_->WriteHeaders(
      id(), std::move(header_block), fin, priority(), std::move(ack_listener));
  if (fin) {
    // TODO(rch): Add test to ensure fin_sent_ is set whenever a fin is sent.
    set_fin_sent(true);
    CloseWriteSide();
  }
  return bytes_written;
}

void QuicSpdyStream::WriteOrBufferBody(
    const QuicString& data,
    bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  WriteOrBufferData(data, fin, std::move(ack_listener));
}

size_t QuicSpdyStream::WriteTrailers(
    SpdyHeaderBlock trailer_block,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (fin_sent()) {
    QUIC_BUG << "Trailers cannot be sent after a FIN, on stream " << id();
    return 0;
  }

  // The header block must contain the final offset for this stream, as the
  // trailers may be processed out of order at the peer.
  QUIC_DLOG(INFO) << "Inserting trailer: (" << kFinalOffsetHeaderKey << ", "
                  << stream_bytes_written() + BufferedDataBytes() << ")";
  trailer_block.insert(
      std::make_pair(kFinalOffsetHeaderKey,
                     QuicTextUtils::Uint64ToString(stream_bytes_written() +
                                                   BufferedDataBytes())));

  // Write the trailing headers with a FIN, and close stream for writing:
  // trailers are the last thing to be sent on a stream.
  const bool kFin = true;
  size_t bytes_written =
      spdy_session_->WriteHeaders(id(), std::move(trailer_block), kFin,
                                  priority(), std::move(ack_listener));
  set_fin_sent(kFin);

  // Trailers are the last thing to be sent on a stream, but if there is still
  // queued data then CloseWriteSide() will cause it never to be sent.
  if (BufferedDataBytes() == 0) {
    CloseWriteSide();
  }

  return bytes_written;
}

size_t QuicSpdyStream::Readv(const struct iovec* iov, size_t iov_len) {
  DCHECK(FinishedReadingHeaders());
  return sequencer()->Readv(iov, iov_len);
}

int QuicSpdyStream::GetReadableRegions(iovec* iov, size_t iov_len) const {
  DCHECK(FinishedReadingHeaders());
  return sequencer()->GetReadableRegions(iov, iov_len);
}

void QuicSpdyStream::MarkConsumed(size_t num_bytes) {
  DCHECK(FinishedReadingHeaders());
  return sequencer()->MarkConsumed(num_bytes);
}

bool QuicSpdyStream::IsDoneReading() const {
  bool done_reading_headers = FinishedReadingHeaders();
  bool done_reading_body = sequencer()->IsClosed();
  bool done_reading_trailers = FinishedReadingTrailers();
  return done_reading_headers && done_reading_body && done_reading_trailers;
}

bool QuicSpdyStream::HasBytesToRead() const {
  return sequencer()->HasBytesToRead();
}

void QuicSpdyStream::MarkTrailersConsumed() {
  trailers_consumed_ = true;
}

void QuicSpdyStream::ConsumeHeaderList() {
  header_list_.Clear();
  if (FinishedReadingHeaders()) {
    sequencer()->SetUnblocked();
  }
}

void QuicSpdyStream::OnStreamHeadersPriority(SpdyPriority priority) {
  DCHECK_EQ(Perspective::IS_SERVER, session()->connection()->perspective());
  SetPriority(priority);
}

void QuicSpdyStream::OnStreamHeaderList(bool fin,
                                        size_t frame_len,
                                        const QuicHeaderList& header_list) {
  // The headers list avoid infinite buffering by clearing the headers list
  // if the current headers are too large. So if the list is empty here
  // then the headers list must have been too large, and the stream should
  // be reset.
  // TODO(rch): Use an explicit "headers too large" signal. An empty header list
  // might be acceptable if it corresponds to a trailing header frame.
  if (header_list.empty()) {
    OnHeadersTooLarge();
    if (IsDoneReading()) {
      return;
    }
  }
  if (!headers_decompressed_) {
    OnInitialHeadersComplete(fin, frame_len, header_list);
  } else {
    OnTrailingHeadersComplete(fin, frame_len, header_list);
  }
}

void QuicSpdyStream::OnHeadersTooLarge() {
  Reset(QUIC_HEADERS_TOO_LARGE);
}

void QuicSpdyStream::OnInitialHeadersComplete(
    bool fin,
    size_t /*frame_len*/,
    const QuicHeaderList& header_list) {
  headers_decompressed_ = true;
  header_list_ = header_list;
  if (fin) {
    OnStreamFrame(QuicStreamFrame(id(), fin, 0, QuicStringPiece()));
  }
  if (FinishedReadingHeaders()) {
    sequencer()->SetUnblocked();
  }
}

void QuicSpdyStream::OnPromiseHeaderList(
    QuicStreamId /* promised_id */,
    size_t /* frame_len */,
    const QuicHeaderList& /*header_list */) {
  // To be overridden in QuicSpdyClientStream.  Not supported on
  // server side.
  session()->connection()->CloseConnection(
      QUIC_INVALID_HEADERS_STREAM_DATA, "Promise headers received by server",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

void QuicSpdyStream::OnTrailingHeadersComplete(
    bool fin,
    size_t /*frame_len*/,
    const QuicHeaderList& header_list) {
  DCHECK(!trailers_decompressed_);
  if (fin_received()) {
    QUIC_DLOG(ERROR) << "Received Trailers after FIN, on stream: " << id();
    session()->connection()->CloseConnection(
        QUIC_INVALID_HEADERS_STREAM_DATA, "Trailers after fin",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  if (!fin) {
    QUIC_DLOG(ERROR) << "Trailers must have FIN set, on stream: " << id();
    session()->connection()->CloseConnection(
        QUIC_INVALID_HEADERS_STREAM_DATA, "Fin missing from trailers",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }

  size_t final_byte_offset = 0;
  if (!SpdyUtils::CopyAndValidateTrailers(header_list, &final_byte_offset,
                                          &received_trailers_)) {
    QUIC_DLOG(ERROR) << "Trailers for stream " << id() << " are malformed.";
    session()->connection()->CloseConnection(
        QUIC_INVALID_HEADERS_STREAM_DATA, "Trailers are malformed",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  trailers_decompressed_ = true;
  OnStreamFrame(
      QuicStreamFrame(id(), fin, final_byte_offset, QuicStringPiece()));
}

void QuicSpdyStream::OnPriorityFrame(SpdyPriority priority) {
  DCHECK_EQ(Perspective::IS_SERVER, session()->connection()->perspective());
  SetPriority(priority);
}

void QuicSpdyStream::OnStreamReset(const QuicRstStreamFrame& frame) {
  if (frame.error_code != QUIC_STREAM_NO_ERROR) {
    QuicStream::OnStreamReset(frame);
    return;
  }
  QUIC_DVLOG(1) << "Received QUIC_STREAM_NO_ERROR, not discarding response";
  set_rst_received(true);
  MaybeIncreaseHighestReceivedOffset(frame.byte_offset);
  set_stream_error(frame.error_code);
  CloseWriteSide();
}

void QuicSpdyStream::OnClose() {
  QuicStream::OnClose();

  if (visitor_) {
    Visitor* visitor = visitor_;
    // Calling Visitor::OnClose() may result the destruction of the visitor,
    // so we need to ensure we don't call it again.
    visitor_ = nullptr;
    visitor->OnClose(this);
  }
}

void QuicSpdyStream::OnCanWrite() {
  QuicStream::OnCanWrite();

  // Trailers (and hence a FIN) may have been sent ahead of queued body bytes.
  if (!HasBufferedData() && fin_sent()) {
    CloseWriteSide();
  }
}

bool QuicSpdyStream::FinishedReadingHeaders() const {
  return headers_decompressed_ && header_list_.empty();
}

bool QuicSpdyStream::ParseHeaderStatusCode(const SpdyHeaderBlock& header,
                                           int* status_code) const {
  SpdyHeaderBlock::const_iterator it = header.find(spdy::kHttp2StatusHeader);
  if (it == header.end()) {
    return false;
  }
  const QuicStringPiece status(it->second);
  if (status.size() != 3) {
    return false;
  }
  // First character must be an integer in range [1,5].
  if (status[0] < '1' || status[0] > '5') {
    return false;
  }
  // The remaining two characters must be integers.
  if (!isdigit(status[1]) || !isdigit(status[2])) {
    return false;
  }
  return QuicTextUtils::StringToInt(status, status_code);
}

bool QuicSpdyStream::FinishedReadingTrailers() const {
  // If no further trailing headers are expected, and the decompressed trailers
  // (if any) have been consumed, then reading of trailers is finished.
  if (!fin_received()) {
    return false;
  } else if (!trailers_decompressed_) {
    return true;
  } else {
    return trailers_consumed_;
  }
}

void QuicSpdyStream::ClearSession() {
  spdy_session_ = nullptr;
}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
