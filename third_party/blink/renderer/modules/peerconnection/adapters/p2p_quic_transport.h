// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_H_

#include "third_party/webrtc/rtc_base/sslfingerprint.h"

namespace blink {

class P2PQuicStream;

// Used by the RTCQuicTransport Web API. This transport creates and manages
// streams, handles negotiation, state changes and errors. Every
// P2PQuicTransport function maps directly to a method in the RTCQuicTransport
// Web API, i.e. RTCQuicTransport::stop() -->
// P2PQuicTransport::Stop(). This allows posting just one task across
// thread boundaries to execute a function.
//
// This object should be run entirely on the webrtc worker thread.
class P2PQuicTransport {
 public:
  // Used for receiving callbacks from the P2PQuicTransport regarding QUIC
  // connection changes, handshake success/failures and new QuicStreams being
  // added from the remote side.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Called when receiving a close frame from the remote side, due to
    // calling P2PQuicTransport::Stop().
    virtual void OnRemoteStopped() {}
    // Called when the connection is closed due to a QUIC error. This can happen
    // locally by the framer, or remotely by the peer.
    virtual void OnConnectionFailed(const std::string& error_details,
                                    bool from_remote) {}
    // Called when the crypto handshake has completed and fingerprints have been
    // verified.
    virtual void OnConnected() {}

    // Called when an incoming stream is received from the remote side. This
    // stream is owned by the P2PQuicTransport. Its lifetime is managed by the
    // P2PQuicTransport, and can be deleted when:
    // - The P2PQuicStream becomes closed for reading and writing.
    // - Stop() is called.
    // - The P2PQuicTransport is deleted.
    virtual void OnStream(P2PQuicStream* stream) {}
  };

  virtual ~P2PQuicTransport() = default;

  // Closes the QuicConnection and sends a close frame to the remote side.
  // This will trigger P2PQuicTransport::Delegate::OnRemoteClosed() on the
  // remote side.
  virtual void Stop() = 0;

  // Starts the QUIC handshake negotiation and sets the remote fingerprints
  // that were signaled through a secure channel. These fingerprints are used to
  // verify the self signed remote certificate used in the QUIC handshake. See:
  // https://w3c.github.io/webrtc-quic/#quic-transport*
  virtual void Start(std::vector<std::unique_ptr<rtc::SSLFingerprint>>
                         remote_fingerprints) = 0;

  // Creates a new outgoing stream. This stream is owned by the
  // P2PQuicTransport. Its lifetime is managed by the P2PQuicTransport,
  // and can be deleted when:
  // - The P2PQuicStream becomes closed for reading and writing.
  // - Stop() is called.
  // - The P2PQuicTransport is deleted.
  virtual P2PQuicStream* CreateStream() = 0;

  // TODO(https://crbug.com/874296): Consider adding a getter for the
  // local fingerprints of the certificate(s) set in the constructor.
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_H_
