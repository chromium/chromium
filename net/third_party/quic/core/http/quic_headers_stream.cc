// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_headers_stream.h"

#include "net/third_party/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"

namespace quic {

QuicHeadersStream::CompressedHeaderInfo::CompressedHeaderInfo(
    QuicStreamOffset headers_stream_offset,
    QuicStreamOffset full_length,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener)
    : headers_stream_offset(headers_stream_offset),
      full_length(full_length),
      unacked_length(full_length),
      ack_listener(std::move(ack_listener)) {}

QuicHeadersStream::CompressedHeaderInfo::CompressedHeaderInfo(
    const CompressedHeaderInfo& other) = default;

QuicHeadersStream::CompressedHeaderInfo::~CompressedHeaderInfo() {}

QuicHeadersStream::QuicHeadersStream(QuicSpdySession* session)
    : QuicStream(QuicUtils::GetHeadersStreamId(
                     session->connection()->transport_version()),
                 session,
                 /*is_static=*/true,
                 BIDIRECTIONAL),
      spdy_session_(session) {
  // The headers stream is exempt from connection level flow control.
  DisableConnectionFlowControlForThisStream();
}

QuicHeadersStream::~QuicHeadersStream() {}

void QuicHeadersStream::OnDataAvailable() {
  struct iovec iov;
  while (sequencer()->GetReadableRegion(&iov)) {
    if (spdy_session_->ProcessHeaderData(iov) != iov.iov_len) {
      // Error processing data.
      return;
    }
    sequencer()->MarkConsumed(iov.iov_len);
    MaybeReleaseSequencerBuffer();
  }
}

void QuicHeadersStream::MaybeReleaseSequencerBuffer() {
  if (spdy_session_->ShouldReleaseHeadersStreamSequencerBuffer()) {
    sequencer()->ReleaseBufferIfEmpty();
  }
}

bool QuicHeadersStream::OnStreamFrameAcked(QuicStreamOffset offset,
                                           QuicByteCount data_length,
                                           bool fin_acked,
                                           QuicTime::Delta ack_delay_time) {
  QuicIntervalSet<QuicStreamOffset> newly_acked(offset, offset + data_length);
  newly_acked.Difference(bytes_acked());
  for (const auto& acked : newly_acked) {
    QuicStreamOffset acked_offset = acked.min();
    QuicByteCount acked_length = acked.max() - acked.min();
    for (CompressedHeaderInfo& header : unacked_headers_) {
      if (acked_offset < header.headers_stream_offset) {
        // This header frame offset belongs to headers with smaller offset, stop
        // processing.
        break;
      }

      if (acked_offset >= header.headers_stream_offset + header.full_length) {
        // This header frame belongs to headers with larger offset.
        continue;
      }

      QuicByteCount header_offset = acked_offset - header.headers_stream_offset;
      QuicByteCount header_length =
          std::min(acked_length, header.full_length - header_offset);

      if (header.unacked_length < header_length) {
        QUIC_BUG << "Unsent stream data is acked. unacked_length: "
                 << header.unacked_length << " acked_length: " << header_length;
        CloseConnectionWithDetails(QUIC_INTERNAL_ERROR,
                                   "Unsent stream data is acked");
        return false;
      }
      if (header.ack_listener != nullptr && header_length > 0) {
        header.ack_listener->OnPacketAcked(header_length, ack_delay_time);
      }
      header.unacked_length -= header_length;
      acked_offset += header_length;
      acked_length -= header_length;
    }
  }
  // Remove headers which are fully acked. Please note, header frames can be
  // acked out of order, but unacked_headers_ is cleaned up in order.
  while (!unacked_headers_.empty() &&
         unacked_headers_.front().unacked_length == 0) {
    unacked_headers_.pop_front();
  }
  return QuicStream::OnStreamFrameAcked(offset, data_length, fin_acked,
                                        ack_delay_time);
}

void QuicHeadersStream::OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                                   QuicByteCount data_length,
                                                   bool /*fin_retransmitted*/) {
  QuicStream::OnStreamFrameRetransmitted(offset, data_length, false);
  for (CompressedHeaderInfo& header : unacked_headers_) {
    if (offset < header.headers_stream_offset) {
      // This header frame offset belongs to headers with smaller offset, stop
      // processing.
      break;
    }

    if (offset >= header.headers_stream_offset + header.full_length) {
      // This header frame belongs to headers with larger offset.
      continue;
    }

    QuicByteCount header_offset = offset - header.headers_stream_offset;
    QuicByteCount retransmitted_length =
        std::min(data_length, header.full_length - header_offset);
    if (header.ack_listener != nullptr && retransmitted_length > 0) {
      header.ack_listener->OnPacketRetransmitted(retransmitted_length);
    }
    offset += retransmitted_length;
    data_length -= retransmitted_length;
  }
}

void QuicHeadersStream::OnDataBuffered(
    QuicStreamOffset offset,
    QuicByteCount data_length,
    const QuicReferenceCountedPointer<QuicAckListenerInterface>& ack_listener) {
  // Populate unacked_headers_.
  if (!unacked_headers_.empty() &&
      (offset == unacked_headers_.back().headers_stream_offset +
                     unacked_headers_.back().full_length) &&
      ack_listener == unacked_headers_.back().ack_listener) {
    // Try to combine with latest inserted entry if they belong to the same
    // header (i.e., having contiguous offset and the same ack listener).
    unacked_headers_.back().full_length += data_length;
    unacked_headers_.back().unacked_length += data_length;
  } else {
    unacked_headers_.push_back(
        CompressedHeaderInfo(offset, data_length, ack_listener));
  }
}

}  // namespace quic
