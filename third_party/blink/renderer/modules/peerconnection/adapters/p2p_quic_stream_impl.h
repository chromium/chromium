// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_IMPL_H_

#include "net/third_party/quic/core/quic_session.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream.h"

namespace blink {

class MODULES_EXPORT P2PQuicStreamImpl final : public P2PQuicStream,
                                               public quic::QuicStream {
 public:
  P2PQuicStreamImpl(quic::QuicStreamId id, quic::QuicSession* session);
  ~P2PQuicStreamImpl() override;

  // QuicStream overrides.
  //
  // Right now this marks the data as consumed and drops it.
  // TODO(https://crbug.com/874296): We need to update this function for
  // reading and consuming data properly while the main JavaScript thread is
  // busy. See:
  // https://w3c.github.io/webrtc-quic/#dom-rtcquicstream-waitforreadable
  void OnDataAvailable() override;

  // P2PQuicStream overrides
  void SetDelegate(P2PQuicStream::Delegate* delegate) override;

  void Reset() override;

  void Finish() override;

  // quic::QuicStream overrides
  //
  // Called by the quic::QuicSession when receiving a RST_STREAM frame from the
  // remote side. This closes the stream for reading & writing (if not already
  // closed), and sends a RST_STREAM frame if one has not been sent yet.
  void OnStreamReset(const quic::QuicRstStreamFrame& frame) override;

  // Called when the stream has finished consumed data up to the FIN bit from
  // the quic::QuicStreamSequencer. This will close the underlying QuicStream
  // for reading. This can be called either by the P2PQuicStreamImpl when
  // reading data, or by the quic::QuicStreamSequencer if we're done reading &
  // receive a stream frame with the FIN bit.
  void OnFinRead() override;

 private:
  using quic::QuicStream::Reset;
  Delegate* delegate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_IMPL_H_
