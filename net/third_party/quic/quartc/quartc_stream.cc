// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_stream.h"

#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

QuartcStream::QuartcStream(QuicStreamId id, QuicSession* session)
    : QuicStream(id, session, /*is_static=*/false, BIDIRECTIONAL) {
  sequencer()->set_level_triggered(true);
}

QuartcStream::~QuartcStream() {}

void QuartcStream::OnDataAvailable() {
  // Do not deliver data until the entire stream's data is available.
  if (deliver_on_complete_ &&
      sequencer()->ReadableBytes() + sequencer()->NumBytesConsumed() <
          sequencer()->close_offset()) {
    return;
  }

  struct iovec iov;
  while (sequencer()->GetReadableRegion(&iov)) {
    DCHECK(delegate_);
    delegate_->OnReceived(this, reinterpret_cast<const char*>(iov.iov_base),
                          iov.iov_len);
    sequencer()->MarkConsumed(iov.iov_len);
  }
  // All the data has been received if the sequencer is closed.
  // Notify the delegate by calling the callback function one more time with
  // iov_len = 0.
  if (sequencer()->IsClosed()) {
    OnFinRead();
    delegate_->OnReceived(this, reinterpret_cast<const char*>(iov.iov_base), 0);
  }
}

void QuartcStream::OnClose() {
  QuicStream::OnClose();
  DCHECK(delegate_);
  delegate_->OnClose(this);
}

void QuartcStream::OnStreamDataConsumed(size_t bytes_consumed) {
  QuicStream::OnStreamDataConsumed(bytes_consumed);

  DCHECK(delegate_);
  delegate_->OnBufferChanged(this);
}

void QuartcStream::OnDataBuffered(
    QuicStreamOffset offset,
    QuicByteCount data_length,
    const QuicReferenceCountedPointer<QuicAckListenerInterface>& ack_listener) {
  DCHECK(delegate_);
  delegate_->OnBufferChanged(this);
}

void QuartcStream::OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                              QuicByteCount data_length,
                                              bool fin_retransmitted) {
  QuicStream::OnStreamFrameRetransmitted(offset, data_length,
                                         fin_retransmitted);

  DCHECK(delegate_);
  delegate_->OnBufferChanged(this);
}

void QuartcStream::OnStreamFrameLost(QuicStreamOffset offset,
                                     QuicByteCount data_length,
                                     bool fin_lost) {
  QuicStream::OnStreamFrameLost(offset, data_length, fin_lost);

  DCHECK(delegate_);
  delegate_->OnBufferChanged(this);
}

void QuartcStream::OnCanWrite() {
  if (cancel_on_loss_ && HasPendingRetransmission()) {
    Reset(QUIC_STREAM_CANCELLED);
    return;
  }
  QuicStream::OnCanWrite();
}

bool QuartcStream::cancel_on_loss() {
  return cancel_on_loss_;
}

void QuartcStream::set_cancel_on_loss(bool cancel_on_loss) {
  cancel_on_loss_ = cancel_on_loss;
}

bool QuartcStream::deliver_on_complete() {
  return deliver_on_complete_;
}

void QuartcStream::set_deliver_on_complete(bool deliver_on_complete) {
  deliver_on_complete_ = deliver_on_complete;
}

QuicByteCount QuartcStream::BytesPendingRetransmission() {
  if (cancel_on_loss_) {
    return 0;  // Lost bytes will never be retransmitted.
  }
  QuicByteCount bytes = 0;
  for (const auto& interval : send_buffer().pending_retransmissions()) {
    bytes += interval.Length();
  }
  return bytes;
}

void QuartcStream::FinishWriting() {
  WriteOrBufferData(QuicStringPiece(nullptr, 0), true, nullptr);
}

void QuartcStream::SetDelegate(Delegate* delegate) {
  if (delegate_) {
    LOG(WARNING) << "The delegate for Stream " << id()
                 << " has already been set.";
  }
  delegate_ = delegate;
  DCHECK(delegate_);
}

}  // namespace quic
