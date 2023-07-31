// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_session.h"

namespace net {

class MockCryptoClientStream : public quic::QuicCryptoClientStream,
                               public quic::QuicCryptoHandshaker {
 public:
  // HandshakeMode enumerates the handshake mode MockCryptoClientStream should
  // mock in CryptoConnect.
  enum HandshakeMode {
    // CONFIRM_HANDSHAKE indicates that CryptoConnect will immediately confirm
    // the handshake and establish encryption.  This behavior will never happen
    // in the field, but is convenient for higher level tests.
    CONFIRM_HANDSHAKE,

    // ZERO_RTT indicates that CryptoConnect will establish encryption but will
    // not confirm the handshake.
    ZERO_RTT,

    // ASYNC_ZERO_RTT indicates that 0-RTT setup will be completed
    // asynchronously. This is possible in TLS. Tests need to call
    // NotifySessionZeroRttComplete() to setup 0-RTT encryption.
    ASYNC_ZERO_RTT,

    // COLD_START indicates that CryptoConnect will neither establish encryption
    // nor confirm the handshake.
    COLD_START,

    // COLD_START_WITH_CHLO_SENT indicates that CryptoConnection will attempt to
    // establish encryption by sending the initial CHLO packet on wire, which
    // contains an empty CryptoHandshakeMessage. It will not confirm the
    // hanshake though.
    COLD_START_WITH_CHLO_SENT,
  };

  MockCryptoClientStream(
      const quic::QuicServerId& server_id,
      quic::QuicSpdyClientSessionBase* session,
      std::unique_ptr<quic::ProofVerifyContext> verify_context,
      const quic::QuicConfig& config,
      quic::QuicCryptoClientConfig* crypto_config,
      HandshakeMode handshake_mode,
      const net::ProofVerifyDetailsChromium* proof_verify_details_,
      bool use_mock_crypter);

  MockCryptoClientStream(const MockCryptoClientStream&) = delete;
  MockCryptoClientStream& operator=(const MockCryptoClientStream&) = delete;

  ~MockCryptoClientStream() override;

  // CryptoFramerVisitorInterface implementation.
  void OnHandshakeMessage(const quic::CryptoHandshakeMessage& message) override;

  // QuicCryptoClientStream implementation.
  bool CryptoConnect() override;
  bool encryption_established() const override;
  bool one_rtt_keys_available() const override;
  quic::HandshakeState GetHandshakeState() const override;
  const quic::QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override;
  quic::CryptoMessageParser* crypto_message_parser() override;
  void OnOneRttPacketAcknowledged() override;
  std::unique_ptr<quic::QuicDecrypter>
  AdvanceKeysAndCreateCurrentOneRttDecrypter() override;
  bool EarlyDataAccepted() const override;
  // Override QuicCryptoClientStream::SetServerApplicationStateForResumption()
  // to avoid tripping over the DCHECK on handshaker state.
  void SetServerApplicationStateForResumption(
      std::unique_ptr<quic::ApplicationState> application_state) override {}

  // Notify session that 1-RTT key is available.
  void NotifySessionOneRttKeyAvailable();

  // Notify session that 0-RTT setup is complete.
  void NotifySessionZeroRttComplete();

  base::WeakPtr<MockCryptoClientStream> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void setHandshakeConfirmedForce(bool state);

  static quic::CryptoHandshakeMessage GetDummyCHLOMessage();

 protected:
  using quic::QuicCryptoClientStream::session;

 private:
  void SetConfigNegotiated();

  // Called from CryptoConnect to set appropriate values in
  // |crypto_negotiated_params_|.
  void FillCryptoParams();

  HandshakeMode handshake_mode_;
  bool encryption_established_ = false;
  bool handshake_confirmed_ = false;
  quiche::QuicheReferenceCountedPointer<quic::QuicCryptoNegotiatedParameters>
      crypto_negotiated_params_;
  quic::CryptoFramer crypto_framer_;
  bool use_mock_crypter_;

  const quic::QuicServerId server_id_;
  raw_ptr<const net::ProofVerifyDetailsChromium> proof_verify_details_;
  const quic::QuicConfig config_;
  base::WeakPtrFactory<MockCryptoClientStream> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_H_
