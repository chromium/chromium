// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_crypto_client_stream.h"

#include "net/quic/mock_decrypter.h"
#include "net/quic/mock_encrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_config_peer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using quic::CLIENT;
using quic::ConnectionCloseBehavior;
using quic::CryptoHandshakeMessage;
using quic::CryptoMessageParser;
using quic::ENCRYPTION_FORWARD_SECURE;
using quic::ENCRYPTION_INITIAL;
using quic::ENCRYPTION_ZERO_RTT;
using quic::kAESG;
using quic::kC255;
using quic::kDefaultMaxStreamsPerConnection;
using quic::kMaximumIdleTimeoutSecs;
using quic::kQBIC;
using quic::NullDecrypter;
using quic::NullEncrypter;
using quic::PACKET_8BYTE_CONNECTION_ID;
using quic::Perspective;
using quic::ProofVerifyContext;
using quic::PROTOCOL_TLS1_3;
using quic::QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE;
using quic::QUIC_NO_ERROR;
using quic::QUIC_PROOF_INVALID;
using quic::QuicConfig;
using quic::QuicCryptoClientConfig;
using quic::QuicCryptoNegotiatedParameters;
using quic::QuicErrorCode;
using quic::QuicServerId;
using quic::QuicSession;
using quic::QuicSpdyClientSessionBase;
using quic::QuicStringPiece;
using quic::QuicTagVector;
using quic::QuicTime;
using quic::TransportParameters;
using std::string;

namespace net {

MockCryptoClientStream::MockCryptoClientStream(
    const QuicServerId& server_id,
    QuicSpdyClientSessionBase* session,
    std::unique_ptr<ProofVerifyContext> verify_context,
    const QuicConfig& config,
    QuicCryptoClientConfig* crypto_config,
    HandshakeMode handshake_mode,
    const net::ProofVerifyDetailsChromium* proof_verify_details,
    bool use_mock_crypter)
    : QuicCryptoClientStream(server_id,
                             session,
                             std::move(verify_context),
                             crypto_config,
                             session),
      QuicCryptoHandshaker(this, session),
      handshake_mode_(handshake_mode),
      encryption_established_(false),
      handshake_confirmed_(false),
      crypto_negotiated_params_(new QuicCryptoNegotiatedParameters),
      use_mock_crypter_(use_mock_crypter),
      server_id_(server_id),
      proof_verify_details_(proof_verify_details),
      config_(config) {
  crypto_framer_.set_visitor(this);
}

MockCryptoClientStream::~MockCryptoClientStream() {}

void MockCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  CloseConnectionWithDetails(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
                             "Forced mock failure");
}

bool MockCryptoClientStream::CryptoConnect() {
  if (proof_verify_details_) {
    if (!proof_verify_details_->cert_verify_result.verified_cert
             ->VerifyNameMatch(server_id_.host())) {
      handshake_confirmed_ = false;
      encryption_established_ = false;
      session()->connection()->CloseConnection(
          QUIC_PROOF_INVALID, "proof invalid",
          ConnectionCloseBehavior::SILENT_CLOSE);
      return false;
    }
  }

  switch (handshake_mode_) {
    case ZERO_RTT: {
      encryption_established_ = true;
      handshake_confirmed_ = false;
      FillCryptoParams();
      if (proof_verify_details_) {
        reinterpret_cast<QuicSpdyClientSessionBase*>(session())
            ->OnProofVerifyDetailsAvailable(*proof_verify_details_);
      }
      if (use_mock_crypter_) {
        if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
          session()->connection()->InstallDecrypter(
              ENCRYPTION_ZERO_RTT,
              std::make_unique<MockDecrypter>(Perspective::IS_CLIENT));
        } else {
          session()->connection()->SetDecrypter(
              ENCRYPTION_ZERO_RTT,
              std::make_unique<MockDecrypter>(Perspective::IS_CLIENT));
        }
        session()->connection()->SetEncrypter(
            ENCRYPTION_ZERO_RTT,
            std::make_unique<MockEncrypter>(Perspective::IS_CLIENT));
      } else {
        if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
          session()->connection()->InstallDecrypter(
              ENCRYPTION_ZERO_RTT,
              std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
        } else {
          session()->connection()->SetDecrypter(
              ENCRYPTION_ZERO_RTT,
              std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
        }
        session()->connection()->SetEncrypter(
            ENCRYPTION_ZERO_RTT,
            std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
      }
      session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
      session()->OnCryptoHandshakeEvent(QuicSession::ENCRYPTION_ESTABLISHED);
      break;
    }

    case CONFIRM_HANDSHAKE: {
      encryption_established_ = true;
      handshake_confirmed_ = true;
      FillCryptoParams();
      if (proof_verify_details_) {
        reinterpret_cast<QuicSpdyClientSessionBase*>(session())
            ->OnProofVerifyDetailsAvailable(*proof_verify_details_);
      }
      SetConfigNegotiated();
      if (use_mock_crypter_) {
        if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
          session()->connection()->InstallDecrypter(
              ENCRYPTION_FORWARD_SECURE,
              std::make_unique<MockDecrypter>(Perspective::IS_CLIENT));
        } else {
          session()->connection()->SetDecrypter(
              ENCRYPTION_FORWARD_SECURE,
              std::make_unique<MockDecrypter>(Perspective::IS_CLIENT));
        }
        session()->connection()->SetEncrypter(
            ENCRYPTION_FORWARD_SECURE,
            std::make_unique<MockEncrypter>(Perspective::IS_CLIENT));
      } else {
        if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
          session()->connection()->InstallDecrypter(
              ENCRYPTION_FORWARD_SECURE,
              std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
        } else {
          session()->connection()->SetDecrypter(
              ENCRYPTION_FORWARD_SECURE,
              std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
        }
        session()->connection()->SetEncrypter(ENCRYPTION_INITIAL, nullptr);
        session()->connection()->SetEncrypter(
            ENCRYPTION_FORWARD_SECURE,
            std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
      }
      session()->connection()->SetDefaultEncryptionLevel(
          ENCRYPTION_FORWARD_SECURE);
      session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
      session()->connection()->OnHandshakeComplete();
      break;
    }

    case COLD_START: {
      handshake_confirmed_ = false;
      encryption_established_ = false;
      break;
    }

    case COLD_START_WITH_CHLO_SENT: {
      handshake_confirmed_ = false;
      encryption_established_ = false;
      SendHandshakeMessage(GetDummyCHLOMessage());
      break;
    }
  }

  return session()->connection()->connected();
}

bool MockCryptoClientStream::encryption_established() const {
  return encryption_established_;
}

bool MockCryptoClientStream::handshake_confirmed() const {
  return handshake_confirmed_;
}

const QuicCryptoNegotiatedParameters&
MockCryptoClientStream::crypto_negotiated_params() const {
  return *crypto_negotiated_params_;
}

CryptoMessageParser* MockCryptoClientStream::crypto_message_parser() {
  return &crypto_framer_;
}

void MockCryptoClientStream::SendOnCryptoHandshakeEvent(
    QuicSession::CryptoHandshakeEvent event) {
  encryption_established_ = true;
  if (event == QuicSession::HANDSHAKE_CONFIRMED) {
    handshake_confirmed_ = true;
    SetConfigNegotiated();
    if (use_mock_crypter_) {
      if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
        session()->connection()->InstallDecrypter(
            ENCRYPTION_FORWARD_SECURE,
            std::make_unique<MockDecrypter>(Perspective::IS_CLIENT));
      } else {
        session()->connection()->SetDecrypter(
            ENCRYPTION_FORWARD_SECURE,
            std::make_unique<MockDecrypter>(Perspective::IS_CLIENT));
      }
      session()->connection()->SetEncrypter(
          ENCRYPTION_FORWARD_SECURE,
          std::make_unique<MockEncrypter>(Perspective::IS_CLIENT));
    } else {
      if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
        session()->connection()->InstallDecrypter(
            ENCRYPTION_FORWARD_SECURE,
            std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
      } else {
        session()->connection()->SetDecrypter(
            ENCRYPTION_FORWARD_SECURE,
            std::make_unique<NullDecrypter>(Perspective::IS_CLIENT));
      }
      session()->connection()->SetEncrypter(ENCRYPTION_INITIAL, nullptr);
      session()->connection()->SetEncrypter(
          ENCRYPTION_FORWARD_SECURE,
          std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
    }
    session()->connection()->SetDefaultEncryptionLevel(
        ENCRYPTION_FORWARD_SECURE);
  }
  session()->OnCryptoHandshakeEvent(event);
}

// static
CryptoHandshakeMessage MockCryptoClientStream::GetDummyCHLOMessage() {
  CryptoHandshakeMessage message;
  message.set_tag(quic::kCHLO);
  return message;
}

void MockCryptoClientStream::SetConfigNegotiated() {
  ASSERT_FALSE(session()->config()->negotiated());
  QuicTagVector cgst;
// TODO(rtenneti): Enable the following code after BBR code is checked in.
#if 0
  cgst.push_back(kTBBR);
#endif
  cgst.push_back(kQBIC);
  QuicConfig config(config_);
  config.SetIdleNetworkTimeout(
      QuicTime::Delta::FromSeconds(2 * kMaximumIdleTimeoutSecs),
      QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs));
  config.SetBytesForConnectionIdToSend(PACKET_8BYTE_CONNECTION_ID);
  config.SetMaxIncomingBidirectionalStreamsToSend(
      kDefaultMaxStreamsPerConnection / 2);
  config.SetMaxIncomingUnidirectionalStreamsToSend(
      kDefaultMaxStreamsPerConnection / 2);
  config.SetInitialMaxStreamDataBytesIncomingBidirectionalToSend(
      quic::kMinimumFlowControlSendWindow);
  config.SetInitialMaxStreamDataBytesOutgoingBidirectionalToSend(
      quic::kMinimumFlowControlSendWindow);
  config.SetInitialMaxStreamDataBytesUnidirectionalToSend(
      quic::kMinimumFlowControlSendWindow);

  QuicErrorCode error;
  std::string error_details;
  if (session()->connection()->version().handshake_protocol ==
      PROTOCOL_TLS1_3) {
    TransportParameters params;
    ASSERT_TRUE(config.FillTransportParameters(&params));
    error = session()->config()->ProcessTransportParameters(params, CLIENT,
                                                            &error_details);
  } else {
    CryptoHandshakeMessage msg;
    config.ToHandshakeMessage(
        &msg, session()->connection()->version().transport_version);
    error = session()->config()->ProcessPeerHello(msg, CLIENT, &error_details);
  }
  ASSERT_EQ(QUIC_NO_ERROR, error);
  ASSERT_TRUE(session()->config()->negotiated());
  session()->OnConfigNegotiated();
}

void MockCryptoClientStream::FillCryptoParams() {
  if (session()->connection()->version().handshake_protocol ==
      quic::PROTOCOL_QUIC_CRYPTO) {
    crypto_negotiated_params_->key_exchange = kC255;
    crypto_negotiated_params_->aead = kAESG;
    return;
  }
  crypto_negotiated_params_->cipher_suite = TLS1_CK_AES_128_GCM_SHA256 & 0xffff;
  crypto_negotiated_params_->key_exchange_group = SSL_CURVE_X25519;
  crypto_negotiated_params_->peer_signature_algorithm =
      SSL_SIGN_ECDSA_SECP256R1_SHA256;
}

}  // namespace net
