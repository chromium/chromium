// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_H_

namespace blink {

// The bidirectional QUIC stream object to be used by the RTCQuicStream Web
// API. See: https://w3c.github.io/webrtc-quic/#quicstream*
//
// Lifetime: The P2PQuicStream is owned by the P2PQuicTransport, and can be
// deleted after the stream is closed for reading and writing. This can happen
// in 3 ways: 1) OnRemoteReset has been fired. 2) Calling Reset(). 3) Both
// Finish() has been called and OnRemoteFinish has been fired.
class P2PQuicStream {
 public:
  // Receives callbacks for receiving RST_STREAM frames or a STREAM_FRAME with
  // the FIN bit set. The Delegate should be subclassed by an object that can
  // post the task to the main JS thread. The delegate's lifetime should outlive
  // this P2PQuicStream.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the stream receives a RST_STREAM frame from the remote side.
    // This means the stream is closed and can no longer read or write, and is
    // deleted by the quic::QuicSession.
    virtual void OnRemoteReset() {}

    // Called when the P2PQuicStream has consumed all incoming data from the
    // remote side up to the FIN bit. Consuming means that the data is marked
    // as consumed by quic::QuicStreamSequencer, but the data has not
    // necessarily been read by the application. If the stream has already
    // finished writing, then upon consuming the FIN bit the stream can no
    // longer read or write and is deleted by the quic::QuicSession.
    virtual void OnRemoteFinish() {}
  };

  virtual ~P2PQuicStream() = default;

  // Sends a RST_STREAM frame to the remote side. This closes the P2PQuicStream
  // for reading & writing and it will be deleted by the quic::QuicSession. When
  // the remote side receives the RST_STREAM frame it will close the stream for
  // reading and writing and send a RST_STREAM frame back. Calling Reset() will
  // not trigger OnRemoteReset to be called locally when the RST_STREAM frame is
  // received from the remote side, because the local stream is already closed.
  virtual void Reset() = 0;

  // Sends a stream frame with the FIN bit set, which notifies the remote side
  // that this stream is done writing. The stream can no longer write after
  // calling this function. If the stream has already received a FIN bit, this
  // will close the stream for reading & writing and it will be deleted by the
  // quic::QuicSession.
  virtual void Finish() = 0;

  // Sets the delegate object, which must outlive the P2PQuicStream.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // TODO:(https://crbug.com/874296): Create functions for reading and writing,
  // specifically for waitForReadable/waitForWriteable.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_STREAM_H_
