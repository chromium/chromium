// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_H_

#include "base/logging.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_stats.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/rtc_base/ssl_fingerprint.h"

namespace blink {

class P2PQuicStream;

// Includes any relevant parameters that were negotiated in the QUIC handshake.
// Used in the P2PQuicTransport::Delegate::OnConnected callback.
struct P2PQuicNegotiatedParams final {
  // If datagrams were negotiated to be supported.
  bool datagrams_supported() const { return max_datagram_length_.has_value(); }

  void set_max_datagram_length(uint16_t max_datagram_length) {
    DCHECK_GT(max_datagram_length, 0);
    max_datagram_length_ = max_datagram_length;
  }

  uint16_t max_datagram_length() const {
    DCHECK(datagrams_supported());
    return max_datagram_length_.value_or(0);
  }

 private:
  // According to draft-pauly-quic-datagram-02#section-3, this should be
  // negotiated as a transport parameter, although in the QUICHE implementation
  // it is based upon the QUIC version (determines packet header size) and
  // default max packet size (1350 bytes).
  base::Optional<uint16_t> max_datagram_length_;
};

// Used by the RTCQuicTransport Web API. This transport creates and manages
// streams, handles negotiation, state changes and errors. Every
// P2PQuicTransport function maps directly to a method in the RTCQuicTransport
// Web API, i.e. RTCQuicTransport::stop() -->
// P2PQuicTransport::Stop(). This allows posting just one task across
// thread boundaries to execute a function.
//
// This object should be run entirely on the webrtc worker thread.
class P2PQuicTransport {
  USING_FAST_MALLOC(P2PQuicTransport);

 public:
  // A config used when starting the QUIC handshake.
  struct StartConfig final {
    explicit StartConfig(
        Vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints_in)
        : remote_fingerprints(std::move(remote_fingerprints_in)) {
      DCHECK_GT(remote_fingerprints.size(), 0u);
    }

    explicit StartConfig(const std::string pre_shared_key_in)
        : pre_shared_key(pre_shared_key_in) {
      DCHECK(!pre_shared_key.empty());
    }

    // These fingerprints are used to verify the self signed remote certificate
    // used in the QUIC handshake. See:
    // https://w3c.github.io/webrtc-quic/#quic-transport*
    Vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints;

    // The pre shared key to be used in the handshake.
    const std::string pre_shared_key;
  };

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
    virtual void OnConnected(P2PQuicNegotiatedParams negotiated_params) {}

    // Called when the datagram has been consumed by the QUIC library and sent
    // on the network.
    virtual void OnDatagramSent() {}

    // Called when we receive a datagram from the remote side.
    virtual void OnDatagramReceived(Vector<uint8_t> datagram) {}

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
  // remote side. This must not be called before Start().
  virtual void Stop() = 0;

  // Starts the QUIC handshake negotiation. If this is a client perspective
  // this means initiating the QUIC handshake with a CHLO, while for
  // a server perspective this means now listening and responding to a CHLO.
  virtual void Start(StartConfig config) = 0;

  // Creates a new outgoing stream. This stream is owned by the
  // P2PQuicTransport. Its lifetime is managed by the P2PQuicTransport,
  // and can be deleted when:
  // - The P2PQuicStream becomes closed for reading and writing.
  // - Stop() is called.
  // - The P2PQuicTransport is deleted.
  virtual P2PQuicStream* CreateStream() = 0;

  // Gets the associated stats. These are QUIC connection level stats.
  virtual P2PQuicTransportStats GetStats() const = 0;

  // Sends datagram. If the QUIC connection is write blocked due to congestion
  // control the message will be buffered and sent once it is unblocked. Once
  // the datagram has been sent, Delegate::OnDatagramSent will be called.
  virtual void SendDatagram(Vector<uint8_t> datagram) = 0;

  // TODO(https://crbug.com/874296): Consider adding a getter for the
  // local fingerprints of the certificate(s) set in the constructor.
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_H_
