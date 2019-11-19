// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "base/macros.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"

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
  ~MockCryptoClientStream() override;

  // CryptoFramerVisitorInterface implementation.
  void OnHandshakeMessage(const quic::CryptoHandshakeMessage& message) override;

  // QuicCryptoClientStream implementation.
  bool CryptoConnect() override;
  bool encryption_established() const override;
  bool handshake_confirmed() const override;
  const quic::QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override;
  quic::CryptoMessageParser* crypto_message_parser() override;

  // Invokes the sessions's CryptoHandshakeEvent method with the specified
  // event.
  void SendOnCryptoHandshakeEvent(
      quic::QuicSession::CryptoHandshakeEvent event);

  static quic::CryptoHandshakeMessage GetDummyCHLOMessage();

 protected:
  using quic::QuicCryptoClientStream::session;

 private:
  void SetConfigNegotiated();

  // Called from CryptoConnect to set appropriate values in
  // |crypto_negotiated_params_|.
  void FillCryptoParams();

  HandshakeMode handshake_mode_;
  bool encryption_established_;
  bool handshake_confirmed_;
  quic::QuicReferenceCountedPointer<quic::QuicCryptoNegotiatedParameters>
      crypto_negotiated_params_;
  quic::CryptoFramer crypto_framer_;
  bool use_mock_crypter_;

  const quic::QuicServerId server_id_;
  const net::ProofVerifyDetailsChromium* proof_verify_details_;
  const quic::QuicConfig config_;

  DISALLOW_COPY_AND_ASSIGN(MockCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_H_
