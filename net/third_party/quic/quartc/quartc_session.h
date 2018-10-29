// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_SESSION_H_
#define NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_SESSION_H_

#include "net/third_party/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_crypto_stream.h"
#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"
#include "net/third_party/quic/quartc/quartc_stream.h"

namespace quic {

// A helper class is used by the QuicCryptoServerStream.
class QuartcCryptoServerStreamHelper : public QuicCryptoServerStream::Helper {
 public:
  QuicConnectionId GenerateConnectionIdForReject(
      QuicConnectionId connection_id) const override;

  bool CanAcceptClientHello(const CryptoHandshakeMessage& message,
                            const QuicSocketAddress& client_address,
                            const QuicSocketAddress& peer_address,
                            const QuicSocketAddress& self_address,
                            QuicString* error_details) const override;
};

// QuartcSession owns and manages a QUIC connection.
class QUIC_EXPORT_PRIVATE QuartcSession
    : public QuicSession,
      public QuartcPacketTransport::Delegate,
      public QuicCryptoClientStream::ProofHandler {
 public:
  QuartcSession(std::unique_ptr<QuicConnection> connection,
                const QuicConfig& config,
                const ParsedQuicVersionVector& supported_versions,
                const QuicString& unique_remote_server_id,
                Perspective perspective,
                QuicConnectionHelperInterface* helper,
                const QuicClock* clock,
                std::unique_ptr<QuartcPacketWriter> packet_writer);
  QuartcSession(const QuartcSession&) = delete;
  QuartcSession& operator=(const QuartcSession&) = delete;
  ~QuartcSession() override;

  // QuicSession overrides.
  QuicCryptoStream* GetMutableCryptoStream() override;

  const QuicCryptoStream* GetCryptoStream() const override;

  QuartcStream* CreateOutgoingBidirectionalStream();

  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;

  // QuicConnectionVisitorInterface overrides.
  void OnCongestionWindowChange(QuicTime now) override;

  void OnConnectionClosed(QuicErrorCode error,
                          const QuicString& error_details,
                          ConnectionCloseSource source) override;

  // QuartcSession methods.

  // Sets a pre-shared key for use during the crypto handshake.  Must be set
  // before StartCryptoHandshake() is called.
  void SetPreSharedKey(QuicStringPiece key);

  void StartCryptoHandshake();

  // Closes the connection with the given human-readable error details.
  // The connection closes with the QUIC_CONNECTION_CANCELLED error code to
  // indicate the application closed it.
  //
  // Informs the peer that the connection has been closed.  This prevents the
  // peer from waiting until the connection times out.
  //
  // Cleans up the underlying QuicConnection's state.  Closing the connection
  // makes it safe to delete the QuartcSession.
  void CloseConnection(const QuicString& details);

  // If the given stream is still open, sends a reset frame to cancel it.
  // Note:  This method cancels a stream by QuicStreamId rather than by pointer
  // (or by a method on QuartcStream) because QuartcSession (and not
  // the caller) owns the streams.  Streams may finish and be deleted before the
  // caller tries to cancel them, rendering the caller's pointers invalid.
  void CancelStream(QuicStreamId stream_id);

  // Callbacks called by the QuartcSession to notify the user of the
  // QuartcSession of certain events.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the crypto handshake is complete.
    virtual void OnCryptoHandshakeComplete() = 0;

    // Called when a new stream is received from the remote endpoint.
    virtual void OnIncomingStream(QuartcStream* stream) = 0;

    // Called when network parameters change in response to an ack frame.
    virtual void OnCongestionControlChange(QuicBandwidth bandwidth_estimate,
                                           QuicBandwidth pacing_rate,
                                           QuicTime::Delta latest_rtt) = 0;

    // Called when the connection is closed. This means all of the streams will
    // be closed and no new streams can be created.
    virtual void OnConnectionClosed(QuicErrorCode error_code,
                                    const QuicString& error_details,
                                    ConnectionCloseSource source) = 0;

    // TODO(zhihuang): Add proof verification.
  };

  // The |delegate| is not owned by QuartcSession.
  void SetDelegate(Delegate* session_delegate);

  // Called when CanWrite() changes from false to true.
  void OnTransportCanWrite() override;

  // Called when a packet has been received and should be handled by the
  // QuicConnection.
  void OnTransportReceived(const char* data, size_t data_len) override;

  // ProofHandler overrides.
  void OnProofValid(const QuicCryptoClientConfig::CachedState& cached) override;

  // Called by the client crypto handshake when proof verification details
  // become available, either because proof verification is complete, or when
  // cached details are used.
  void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) override;

 protected:
  // QuicSession override.
  QuicStream* CreateIncomingStream(QuicStreamId id) override;

  std::unique_ptr<QuartcStream> CreateDataStream(QuicStreamId id,
                                                 spdy::SpdyPriority priority);
  // Activates a QuartcStream.  The session takes ownership of the stream, but
  // returns an unowned pointer to the stream for convenience.
  QuartcStream* ActivateDataStream(std::unique_ptr<QuartcStream> stream);

  void ResetStream(QuicStreamId stream_id, QuicRstStreamErrorCode error);

 private:
  // For crypto handshake.
  std::unique_ptr<QuicCryptoStream> crypto_stream_;
  const QuicString unique_remote_server_id_;
  Perspective perspective_;

  // Packet writer used by |connection_|.
  std::unique_ptr<QuartcPacketWriter> packet_writer_;

  // Take ownership of the QuicConnection.  Note: if |connection_| changes,
  // the new value of |connection_| must be given to |packet_writer_| before any
  // packets are written.  Otherwise, |packet_writer_| will crash.
  std::unique_ptr<QuicConnection> connection_;
  // Not owned by QuartcSession. From the QuartcFactory.
  QuicConnectionHelperInterface* helper_;
  // For recording packet receipt time
  const QuicClock* clock_;
  // Not owned by QuartcSession.
  Delegate* session_delegate_ = nullptr;
  // Used by QUIC crypto server stream to track most recently compressed certs.
  std::unique_ptr<QuicCompressedCertsCache> quic_compressed_certs_cache_;
  // This helper is needed when create QuicCryptoServerStream.
  QuartcCryptoServerStreamHelper stream_helper_;
  // Config for QUIC crypto client stream, used by the client.
  std::unique_ptr<QuicCryptoClientConfig> quic_crypto_client_config_;
  // Config for QUIC crypto server stream, used by the server.
  std::unique_ptr<QuicCryptoServerConfig> quic_crypto_server_config_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_SESSION_H_
