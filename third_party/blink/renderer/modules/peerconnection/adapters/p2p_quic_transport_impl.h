// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_IMPL_H_

#include <queue>
#include "base/threading/thread_checker.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_crypto_server_stream_helper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_config_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"

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
  // Creates the necessary QUIC and Chromium specific objects before
  // creating P2PQuicTransportImpl.
  static std::unique_ptr<P2PQuicTransportImpl> Create(
      quic::QuicClock* clock,
      quic::QuicAlarmFactory* alarm_factory,
      quic::QuicRandom* quic_random,
      Delegate* delegate,
      P2PQuicPacketTransport* packet_transport,
      const P2PQuicTransportConfig& config,
      std::unique_ptr<P2PQuicCryptoConfigFactory> crypto_config_factory,
      std::unique_ptr<P2PQuicCryptoStreamFactory> crypto_stream_factory);

  P2PQuicTransportImpl(
      Delegate* delegate,
      P2PQuicPacketTransport* packet_transport,
      const P2PQuicTransportConfig& p2p_transport_config,
      std::unique_ptr<quic::QuicConnectionHelperInterface> helper,
      std::unique_ptr<quic::QuicConnection> connection,
      const quic::QuicConfig& quic_config,
      std::unique_ptr<P2PQuicCryptoConfigFactory> crypto_config_factory,
      std::unique_ptr<P2PQuicCryptoStreamFactory> crypto_stream_factory,
      quic::QuicClock* clock);

  ~P2PQuicTransportImpl() override;

  // P2PQuicTransport overrides.
  void Stop() override;
  // This handshake is currently insecure in the case of using remote
  // fingerprints to verify the remote certificate. For a secure handshake, set
  // the pre_shared_key attribute of the |config| before calling this. This
  // function must be called before creating any streams.
  //
  // TODO(https://crbug.com/874300): Verify both the client and server
  // certificates with the signaled remote fingerprints. Until the TLS 1.3
  // handshake is supported in the QUIC core library we can only verify the
  // server's certificate, but not the client's.
  void Start(StartConfig config) override;
  // Creates an outgoing stream that is owned by the quic::QuicSession.
  P2PQuicStreamImpl* CreateStream() override;
  P2PQuicTransportStats GetStats() const override;
  // This should not be called until the transport has become connected, and
  // cannot be called with a |datagram| larger than the maximum size given in
  // GetGuaranteedLargestMessagePayload(). Once the datagram has been sent,
  // Delegate::OnDatagramSent will be called. If Delegate::OnDatagramSent is not
  // immediately called, it can be assumed that the datagram is buffered due to
  // congestion control.
  void SendDatagram(Vector<uint8_t> datagram) override;

  // Returns true if a datagram can be sent on the transport.
  bool CanSendDatagram();

  // quic::QuicSession override.
  void OnMessageReceived(quic::QuicStringPiece message) override;
  void OnMessageLost(quic::QuicMessageId message_id) override;
  void OnCanWrite() override;

  // P2PQuicPacketTransport::Delegate override.
  void OnPacketDataReceived(const char* data, size_t data_len) override;

  // quic::QuicCryptoClientStream::ProofHandler overrides used in a client
  // connection to get certificate verification details.

  // Called when the proof verification completes. This information is used
  // for 0 RTT handshakes, which isn't relevant for our P2P handshake.
  void OnProofValid(
      const quic::QuicCryptoClientConfig::CachedState& cached) override {}

  // Called when proof verification become available.
  void OnProofVerifyDetailsAvailable(
      const quic::ProofVerifyDetails& verify_details) override {}

  // quic::QuicConnectionVisitorInterface overrides.
  void OnConnectionClosed(const quic::QuicConnectionCloseFrame& frame,
                          quic::ConnectionCloseSource source) override;
  bool ShouldKeepConnectionAlive() const override;

 protected:
  // quic::QuicSession overrides.
  // Creates a new stream initiated from the remote side. The caller does not
  // own the stream, so the stream is activated and ownership is moved to the
  // quic::QuicSession.
  P2PQuicStreamImpl* CreateIncomingStream(
      quic::QuicStreamId id) override;
  P2PQuicStreamImpl* CreateIncomingStream(
      quic::PendingStream* pending) override;

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
  P2PQuicStreamImpl* CreateStreamInternal(quic::PendingStream* pending);

  // Returns true if datagram was sent, false if it was not because of
  // congestion control blocking.
  bool TrySendDatagram(Vector<uint8_t>& datagram);

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
  std::unique_ptr<quic::QuicConnectionHelperInterface> helper_;

  std::unique_ptr<quic::QuicConnection> connection_;

  // Used to create either a crypto client or server config.
  std::unique_ptr<P2PQuicCryptoConfigFactory> crypto_config_factory_;
  // Used to create a client or server crypto stream.
  std::unique_ptr<P2PQuicCryptoStreamFactory> crypto_stream_factory_;

  std::unique_ptr<quic::QuicCryptoStream> crypto_stream_;
  // Crypto certificate information. Note that currently the handshake is
  // insecure and these are not used...
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  Vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints_;

  bool pre_shared_key_set_ = false;

  quic::Perspective perspective_;
  // Outlives the P2PQuicTransport.
  P2PQuicPacketTransport* packet_transport_;
  P2PQuicTransport::Delegate* delegate_ = nullptr;
  // Owned by whatever creates the P2PQuicTransportImpl. The |clock_| needs to
  // outlive the P2PQuicTransportImpl.
  quic::QuicClock* clock_ = nullptr;
  // The size of the stream delegate's read buffer, used when creating
  // P2PQuicStreams.
  uint32_t stream_delegate_read_buffer_size_;
  // Determines the size of the write buffer when P2PQuicStreams.
  uint32_t stream_write_buffer_size_;

  // For stats:
  uint32_t num_outgoing_streams_created_ = 0;
  uint32_t num_incoming_streams_created_ = 0;
  // The number reported lost on the network by the quic::QuicSession.
  uint32_t num_datagrams_lost_ = 0;

  // Datagrams not yet sent due to congestion control blocking.
  std::queue<Vector<uint8_t>> datagram_buffer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_IMPL_H_
