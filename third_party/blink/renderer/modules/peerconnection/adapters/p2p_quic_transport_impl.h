// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_IMPL_H_

#include "base/threading/thread_checker.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_packet_writer.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/tools/quic_simple_crypto_server_stream_helper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/webrtc/rtc_base/rtccertificate.h"

namespace blink {

// The P2PQuicTransportImpl subclasses the quic::QuicSession in order to expose
// QUIC as a P2P transport. This specific subclass implements the crypto
// handshake for a peer to peer connection, which requires verifying the remote
// certificate's fingerprint, but otherwise exposes a raw quic::QuicSession.
//
// At a high level, the quic::QuicConnection manages the actual connection
// between two endpoints, while the quic::QuicSession owns and manages the
// quic::QuicStreams. The quic::QuicSession also handles the negotiation between
// endpoints, session control (reset, window updates, control frames, etc.), and
// callbacks from either the connection (quic::QuicConnectionVisitorInterface),
// frames being acked or lost (quic::SessionNotifierInterface), or handshake
// state changes.
//
// This object should be run entirely on the webrtc worker thread.
class MODULES_EXPORT P2PQuicTransportImpl final
    : public quic::QuicSession,
      public P2PQuicTransport,
      public P2PQuicPacketTransport::ReceiveDelegate,
      public quic::QuicCryptoClientStream::ProofHandler {
 public:
  P2PQuicTransportImpl(
      P2PQuicTransportConfig p2p_transport_config,
      std::unique_ptr<net::QuicChromiumConnectionHelper> helper,
      std::unique_ptr<quic::QuicConnection> connection,
      const quic::QuicConfig& quic_config,
      quic::QuicClock* clock);

  ~P2PQuicTransportImpl() override;

  // P2PQuicTransport overrides.

  void Stop() override;

  // Sets the remote fingerprints, and if the the P2PQuicTransportImpl is a
  // client starts the QUIC handshake . This handshake is currently insecure,
  // meaning that the certificates used are fake and are not verified. It also
  // assumes a handshake for a server/client case. This must be called before
  // creating any streams.
  //
  // TODO(https://crbug.com/874300): Verify both the client and server
  // certificates with the signaled remote fingerprints. Until the TLS 1.3
  // handshake is supported in the QUIC core library we can only verify the
  // server's certificate, but not the client's. Note that this means
  // implementing the handshake for a P2P case, in which case verification
  // completes after both receiving the signaled remote fingerprint and getting
  // a client hello. Because either can come first, a synchronous call to verify
  // the remote fingerprint is not possible.
  void Start(std::vector<std::unique_ptr<rtc::SSLFingerprint>>
                 remote_fingerprints) override;

  // Creates an outgoing stream that is owned by the quic::QuicSession.
  P2PQuicStreamImpl* CreateStream() override;

  // P2PQuicPacketTransport::Delegate override.
  void OnPacketDataReceived(const char* data, size_t data_len) override;

  // quic::QuicCryptoClientStream::ProofHandler overrides used in a client
  // connection to get certificate verification details.

  // Called when the proof verification completes. This information is used
  // for 0 RTT handshakes, which isn't relevant for our P2P handshake.
  void OnProofValid(
      const quic::QuicCryptoClientConfig::CachedState& cached) override{};

  // Called when proof verification become available.
  void OnProofVerifyDetailsAvailable(
      const quic::ProofVerifyDetails& verify_details) override{};

  // quic::QuicConnectionVisitorInterface override.
  void OnConnectionClosed(quic::QuicErrorCode error,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source) override;

 protected:
  // quic::QuicSession overrides.
  // TODO(https://crbug.com/874296): Subclass QuicStream and implement these
  // functions.

  // Creates a new stream initiated from the remote side. The caller does not
  // own the stream, so the stream is activated and ownership is moved to the
  // quic::QuicSession.
  P2PQuicStreamImpl* CreateIncomingStream(
      quic::QuicStreamId id) override;

  // Creates a new outgoing stream. The caller does not own the
  // stream, so the stream is activated and ownership is moved to the
  // quic::QuicSession.
  P2PQuicStreamImpl* CreateOutgoingBidirectionalStream();

  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;

 private:
  // This is for testing connection failures and handshake failures.
  friend class P2PQuicTransportTest;

  // These functions are used for testing.
  //
  // Returns true if the quic::QuicConnection has been closed remotely or
  // locally.
  bool IsClosed();
  quic::QuicConnection* connection() { return connection_.get(); }
  // Allows the test to set its own proof verification.
  void set_crypto_client_config(
      std::unique_ptr<quic::QuicCryptoClientConfig> crypto_client_config);

  // quic::QuicSession overrides.
  const quic::QuicCryptoStream* GetCryptoStream() const override;
  quic::QuicCryptoStream* GetMutableCryptoStream() override;

  // Creates the crypto stream necessary for handshake negotiation, and
  // initializes the parent class (quic::QuicSession). This must be called on
  // both sides before communicating between endpoints (Start, Close, etc.).
  void InitializeCryptoStream();

  // Creates a new stream. This helper function is used when we need to create
  // a new incoming stream or outgoing stream.
  P2PQuicStreamImpl* CreateStreamInternal(quic::QuicStreamId id);

  // The server_config and client_config are used for setting up the crypto
  // connection. The ownership of these objects or the objects they own
  // (quic::ProofSource, quic::ProofVerifier, etc.), are not passed on to the
  // QUIC library for the handshake, so we must own them here. A client
  // |perspective_| will not have a crypto_server_config and vice versa.
  std::unique_ptr<quic::QuicCryptoServerConfig> crypto_server_config_;
  std::unique_ptr<quic::QuicCryptoClientConfig> crypto_client_config_;
  // Used by server |crypto_stream_| to track most recently compressed certs.
  std::unique_ptr<quic::QuicCompressedCertsCache> compressed_certs_cache_;
  std::unique_ptr<quic::QuicCryptoServerStream::Helper> server_stream_helper_;
  // Owned by the P2PQuicTransportImpl. |helper_| is placed before
  // |connection_| to ensure it outlives it.
  std::unique_ptr<net::QuicChromiumConnectionHelper> helper_;

  std::unique_ptr<quic::QuicConnection> connection_;

  std::unique_ptr<quic::QuicCryptoStream> crypto_stream_;
  // Crypto information. Note that currently the handshake is insecure and these
  // are not used...
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints_;

  quic::Perspective perspective_;
  // Outlives the P2PQuicTransport.
  P2PQuicPacketTransport* packet_transport_;
  P2PQuicTransport::Delegate* delegate_ = nullptr;
  // Owned by whatever creates the P2PQuicTransportImpl. The |clock_| needs to
  // outlive the P2PQuicTransportImpl.
  quic::QuicClock* clock_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_IMPL_H_
