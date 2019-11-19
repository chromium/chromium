// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"

#include <utility>
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

P2PQuicStreamImpl::P2PQuicStreamImpl(quic::QuicStreamId id,
                                     quic::QuicSession* session,
                                     uint32_t delegate_read_buffer_size,
                                     uint32_t write_buffer_size)
    : quic::QuicStream(id, session, /*is_static=*/false, quic::BIDIRECTIONAL),
      delegate_read_buffer_size_(delegate_read_buffer_size),
      write_buffer_size_(write_buffer_size) {
  DCHECK_GT(delegate_read_buffer_size_, 0u);
  DCHECK_GT(write_buffer_size_, 0u);
}

P2PQuicStreamImpl::P2PQuicStreamImpl(quic::PendingStream* pending,
                                     quic::QuicSession* session,
                                     uint32_t delegate_read_buffer_size,
                                     uint32_t write_buffer_size)
    : quic::QuicStream(pending,
                       quic::BIDIRECTIONAL,
                       /*is_static=*/false),
      delegate_read_buffer_size_(delegate_read_buffer_size),
      write_buffer_size_(write_buffer_size) {
  DCHECK_GT(delegate_read_buffer_size_, 0u);
  DCHECK_GT(write_buffer_size_, 0u);
}

P2PQuicStreamImpl::~P2PQuicStreamImpl() {}

void P2PQuicStreamImpl::OnDataAvailable() {
  if (!sequencer()->HasBytesToRead() && sequencer()->IsClosed()) {
    // We have consumed all data from the sequencer up to the FIN bit. This can
    // only occur by receiving an empty STREAM frame with the FIN bit set.
    quic::QuicStream::OnFinRead();
    consumed_fin_ = true;
    if (delegate_) {
      delegate_->OnDataReceived({}, /*fin=*/true);
    }
  }

  uint32_t delegate_read_buffer_available =
      delegate_read_buffer_size_ - delegate_read_buffered_amount_;
  uint32_t total_read_amount =
      std::min(static_cast<uint32_t>(sequencer()->ReadableBytes()),
               delegate_read_buffer_available);
  // Nothing to do if the delegate's read buffer can't fit anymore data,
  // or the sequencer doesn't have any data available to be read.
  if (total_read_amount == 0 || consumed_fin_) {
    return;
  }
  Vector<uint8_t> data(total_read_amount);
  uint32_t current_data_offset = 0;
  struct iovec iov;

  // Read data from the quic::QuicStreamSequencer until we have exhausted the
  // data, or have read at least the amount of the delegate's read buffer size.
  while (sequencer()->GetReadableRegion(&iov)) {
    uint32_t read_amount = static_cast<uint32_t>(iov.iov_len);
    if (read_amount == 0) {
      // Read everything available from the quic::QuicStreamSequencer.
      DCHECK_EQ(current_data_offset, total_read_amount);
      break;
    }
    // Limit the |consume_amount| by the amount available in the delegate's read
    // buffer.
    uint32_t consume_amount = std::min(
        read_amount, delegate_read_buffer_available - current_data_offset);
    memcpy(data.data() + current_data_offset, iov.iov_base, consume_amount);
    sequencer()->MarkConsumed(consume_amount);
    current_data_offset += consume_amount;
    if (read_amount > consume_amount) {
      // The delegate cannot store more data in its read buffer.
      DCHECK_EQ(current_data_offset, total_read_amount);
      break;
    }
  }

  bool fin = !sequencer()->HasBytesToRead() && sequencer()->IsClosed();
  delegate_read_buffered_amount_ += data.size();
  DCHECK(delegate_read_buffer_size_ >= delegate_read_buffered_amount_);
  if (fin) {
    quic::QuicStream::OnFinRead();
    consumed_fin_ = fin;
  }
  if (delegate_) {
    delegate_->OnDataReceived(std::move(data), fin);
  }
}

void P2PQuicStreamImpl::OnStreamDataConsumed(size_t bytes_consumed) {
  // We should never consume more than has been written.
  DCHECK_GE(write_buffered_amount_, bytes_consumed);
  QuicStream::OnStreamDataConsumed(bytes_consumed);
  if (bytes_consumed > 0) {
    write_buffered_amount_ -= bytes_consumed;
    if (delegate_) {
      delegate_->OnWriteDataConsumed(SafeCast<uint32_t>(bytes_consumed));
    }
  }
}

void P2PQuicStreamImpl::Reset() {
  if (rst_sent()) {
    // No need to reset twice. This could have already been sent as consequence
    // of receiving a RST_STREAM frame.
    return;
  }
  quic::QuicStream::Reset(quic::QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
}

void P2PQuicStreamImpl::MarkReceivedDataConsumed(uint32_t amount) {
  DCHECK_GE(delegate_read_buffered_amount_, amount);
  delegate_read_buffered_amount_ -= amount;
  if (sequencer()->HasBytesToRead() || !consumed_fin_) {
    OnDataAvailable();
  }
}

void P2PQuicStreamImpl::WriteData(Vector<uint8_t> data, bool fin) {
  // It is up to the delegate to not write more data than the
  // |write_buffer_size_|.
  DCHECK_GE(write_buffer_size_, data.size() + write_buffered_amount_);
  write_buffered_amount_ += data.size();
  QuicStream::WriteOrBufferData(
      quic::QuicStringPiece(reinterpret_cast<const char*>(data.data()),
                            data.size()),
      fin, nullptr);
}

void P2PQuicStreamImpl::SetDelegate(P2PQuicStream::Delegate* delegate) {
  delegate_ = delegate;
}

void P2PQuicStreamImpl::OnStreamReset(const quic::QuicRstStreamFrame& frame) {
  // Calling this on the QuicStream will ensure that the stream is closed
  // for reading and writing and we send a RST frame to the remote side if
  // we have not already.
  quic::QuicStream::OnStreamReset(frame);
  if (delegate_) {
    delegate_->OnRemoteReset();
  }
}

void P2PQuicStreamImpl::OnClose() {
  closed_ = true;
  quic::QuicStream::OnClose();
}

bool P2PQuicStreamImpl::IsClosedForTesting() {
  return closed_;
}

uint32_t P2PQuicStreamImpl::DelegateReadBufferedAmountForTesting() {
  return delegate_read_buffered_amount_;
}

}  // namespace blink
