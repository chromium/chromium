// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_IMPL_H_

#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream.h"

namespace blink {

class MODULES_EXPORT P2PQuicStreamImpl final : public quic::QuicStream,
                                               public P2PQuicStream {
 public:
  P2PQuicStreamImpl(quic::QuicStreamId id,
                    quic::QuicSession* session,
                    uint32_t delegate_read_buffer_size,
                    uint32_t write_buffer_size);
  P2PQuicStreamImpl(quic::PendingStream* pending,
                    quic::QuicSession* session,
                    uint32_t delegate_read_buffer_size,
                    uint32_t write_buffer_size);
  ~P2PQuicStreamImpl() override;
  // P2PQuicStream overrides
  void SetDelegate(P2PQuicStream::Delegate* delegate) override;

  void Reset() override;

  void WriteData(Vector<uint8_t> data, bool fin) override;

  void MarkReceivedDataConsumed(uint32_t amount) override;

  // For testing purposes. This is returns true after quic::QuicStream::OnClose
  bool IsClosedForTesting();

  // For testing purposes. This exposes the amount of received data that the
  // P2PQuicStream is aware is buffered by the delegate.
  uint32_t DelegateReadBufferedAmountForTesting();

  // quic::QuicStream overrides.
  //
  // Right now this marks the data as consumed and drops it.
  // TODO(https://crbug.com/874296): We need to update this function for
  // reading and consuming data properly while the main JavaScript thread is
  // busy. See:
  // https://w3c.github.io/webrtc-quic/#dom-rtcquicstream-waitforreadable
  void OnDataAvailable() override;
  // Called by the quic::QuicSession when receiving a RST_STREAM frame from the
  // remote side. This closes the stream for reading & writing (if not already
  // closed), and sends a RST_STREAM frame if one has not been sent yet.
  void OnStreamReset(const quic::QuicRstStreamFrame& frame) override;
  //  Called by the quic::QuicSession. This means the stream is closed for
  //  reading
  // and writing, and can now be deleted by the quic::QuicSession.
  void OnClose() override;

 protected:
  // quic::QuicStream overrides.
  //
  // Called when written data (from WriteData()) is consumed by QUIC. This means
  // the data has either been sent across the wire, or it has been turned into a
  // packet and queued if the socket is unexpectedly blocked.
  void OnStreamDataConsumed(size_t bytes_consumed) override;

 private:
  using quic::QuicStream::Reset;

  // Must either outlive the P2PQuicStream or unset itself upon destruction.
  Delegate* delegate_;

  // The read buffer size of the delegate. The |delegate_read_buffered_amount_|
  // must never exceed this value (enforced by the P2PQuicStreamImpl).
  const uint32_t delegate_read_buffer_size_;
  // The maximum size allowed to be buffered write side. The
  // |write_buffered_amount_| must never exceed this value, and it is up
  // to the delegate to enforce this.
  const uint32_t write_buffer_size_;
  // How much total data has been received and given to the delegate,
  // but not yet consumed by the delegate. This value gets increased when data
  // is received from the QUIC library in OnDataAvailable() and and decreased
  // when the delegate updates that data has been read with
  // MarkReceivedDataConsumed().
  uint32_t delegate_read_buffered_amount_ = 0;
  // How much data is buffered by the QUIC library, but has not yet
  // been sent. This value gets increased when WriteData() is called
  // and decreased when OnDataConsumed() gets called by the QUIC library,
  // due to the data being sent.
  uint32_t write_buffered_amount_ = 0;

  // Set after OnClose gets called.
  bool closed_ = false;

  // This is set after the sequencer is closed due to the P2PQuicStream
  // consuming all of the sequencer's data up to the FIN bit.
  bool consumed_fin_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_IMPL_H_
