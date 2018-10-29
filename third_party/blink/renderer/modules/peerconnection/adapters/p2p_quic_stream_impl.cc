// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"

#include "net/third_party/quic/core/quic_error_codes.h"

namespace blink {

P2PQuicStreamImpl::P2PQuicStreamImpl(quic::QuicStreamId id,
                                     quic::QuicSession* session)
    : quic::QuicStream(id, session, /*is_static=*/false, quic::BIDIRECTIONAL) {}

P2PQuicStreamImpl::~P2PQuicStreamImpl() {}

void P2PQuicStreamImpl::OnDataAvailable() {
  // We just drop the data by marking all data as immediately consumed.
  sequencer()->MarkConsumed(sequencer()->ReadableBytes());
  if (sequencer()->IsClosed()) {
    // This means all data has been consumed up to the FIN bit.
    OnFinRead();
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

void P2PQuicStreamImpl::Finish() {
  // Should never call Finish twice.
  DCHECK(!fin_sent());
  quic::QuicStream::WriteOrBufferData("", /*fin=*/true, nullptr);
}

void P2PQuicStreamImpl::SetDelegate(P2PQuicStream::Delegate* delegate) {
  delegate_ = delegate;
}

void P2PQuicStreamImpl::OnStreamReset(const quic::QuicRstStreamFrame& frame) {
  // TODO(https://crbug.com/874296): If we get an incoming stream we need to
  // make sure that the delegate is set before we have incoming data.
  DCHECK(delegate_);
  // Calling this on the QuicStream will ensure that the stream is closed
  // for reading and writing and we send a RST frame to the remote side if
  // we have not already.
  quic::QuicStream::OnStreamReset(frame);
  delegate_->OnRemoteReset();
}

void P2PQuicStreamImpl::OnFinRead() {
  // TODO(https://crbug.com/874296): If we get an incoming stream we need to
  // make sure that the delegate is set before we have incoming data.
  DCHECK(delegate_);
  // Calling this on the QuicStream ensures that the stream is closed
  // for reading.
  quic::QuicStream::OnFinRead();
  delegate_->OnRemoteFinish();
}

}  // namespace blink
