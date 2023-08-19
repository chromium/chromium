// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_crypto_client_stream.h"

#include "net/base/ip_endpoint.h"
#include "net/quic/address_utils.h"
#include "net/quic/mock_decrypter.h"
#include "net/quic/mock_encrypter.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
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
using quic::kQBIC;
using quic::Perspective;
using quic::ProofVerifyContext;
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
using quic::QuicTagVector;
using quic::QuicTime;
using quic::TransportParameters;
using quic::test::StrictTaggingDecrypter;
using quic::test::TaggingEncrypter;
using std::string;

namespace net {
namespace {

static constexpr int k8ByteConnectionId = 8;

}  // namespace

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
                             session,
                             /*has_application_state = */ true),
      QuicCryptoHandshaker(this, session),
      handshake_mode_(handshake_mode),
      crypto_negotiated_params_(new QuicCryptoNegotiatedParameters),
      use_mock_crypter_(use_mock_crypter),
      server_id_(server_id),
      proof_verify_details_(proof_verify_details),
      config_(config) {
  crypto_framer_.set_visitor(this);
  // Simulate a negotiated cipher_suite with a fake value.
  crypto_negotiated_params_->cipher_suite = 1;
}

MockCryptoClientStream::~MockCryptoClientStream() = default;

void MockCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  OnUnrecoverableError(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
                       "Forced mock failure");
}

bool MockCryptoClientStream::CryptoConnect() {
  DCHECK(session()->version().UsesTls());
  IPEndPoint local_ip;
  static_cast<QuicChromiumClientSession*>(session())
      ->GetDefaultSocket()
      ->GetLocalAddress(&local_ip);
  session()->connection()->SetSelfAddress(ToQuicSocketAddress(local_ip));

  IPEndPoint peer_ip;
  static_cast<QuicChromiumClientSession*>(session())
      ->GetDefaultSocket()
      ->GetPeerAddress(&peer_ip);
  quic::test::QuicConnectionPeer::SetEffectivePeerAddress(
      session()->connection(), ToQuicSocketAddress(peer_ip));

  if (session()->connection()->version().KnowsWhichDecrypterToUse()) {
    session()->connection()->InstallDecrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
  } else {
    session()->connection()->SetAlternativeDecrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE),
        /*latch_once_used=*/false);
  }
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
              std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
        } else {
          session()->connection()->SetDecrypter(
              ENCRYPTION_ZERO_RTT,
              std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
        }
        SetConfigNegotiated();
        session()->OnNewEncryptionKeyAvailable(
            ENCRYPTION_ZERO_RTT,
            std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
      }
      if (!session()->connection()->connected()) {
        break;
      }
      session()->DiscardOldEncryptionKey(ENCRYPTION_INITIAL);
      break;
    }

    case ASYNC_ZERO_RTT: {
      handshake_confirmed_ = false;
      FillCryptoParams();
      if (proof_verify_details_) {
        reinterpret_cast<QuicSpdyClientSessionBase*>(session())
            ->OnProofVerifyDetailsAvailable(*proof_verify_details_);
      }
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
              std::make_unique<StrictTaggingDecrypter>(
                  ENCRYPTION_FORWARD_SECURE));
        } else {
          session()->connection()->SetDecrypter(
              ENCRYPTION_FORWARD_SECURE,
              std::make_unique<StrictTaggingDecrypter>(
                  ENCRYPTION_FORWARD_SECURE));
        }
        session()->connection()->SetEncrypter(ENCRYPTION_INITIAL, nullptr);
      }
      session()->OnNewEncryptionKeyAvailable(
          ENCRYPTION_FORWARD_SECURE,
          std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
      if (!session()->connection()->connected()) {
        break;
      }
      session()->OnTlsHandshakeComplete();
      session()->DiscardOldEncryptionKey(ENCRYPTION_INITIAL);
      session()->NeuterHandshakeData();
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
      SendHandshakeMessage(GetDummyCHLOMessage(), ENCRYPTION_INITIAL);
      break;
    }
  }

  return session()->connection()->connected();
}

bool MockCryptoClientStream::encryption_established() const {
  return encryption_established_;
}

bool MockCryptoClientStream::one_rtt_keys_available() const {
  return handshake_confirmed_;
}

quic::HandshakeState MockCryptoClientStream::GetHandshakeState() const {
  return handshake_confirmed_ ? quic::HANDSHAKE_CONFIRMED
                              : quic::HANDSHAKE_START;
}

void MockCryptoClientStream::setHandshakeConfirmedForce(bool state) {
  handshake_confirmed_ = state;
}

bool MockCryptoClientStream::EarlyDataAccepted() const {
  // This value is only used for logging. The return value doesn't matter.
  return false;
}

const QuicCryptoNegotiatedParameters&
MockCryptoClientStream::crypto_negotiated_params() const {
  return *crypto_negotiated_params_;
}

CryptoMessageParser* MockCryptoClientStream::crypto_message_parser() {
  return &crypto_framer_;
}

// Tests using MockCryptoClientStream() do not care about the handshaker's
// state.  Intercept and ignore the calls calls to prevent DCHECKs within the
// handshaker from failing.
void MockCryptoClientStream::OnOneRttPacketAcknowledged() {}

std::unique_ptr<quic::QuicDecrypter>
MockCryptoClientStream::AdvanceKeysAndCreateCurrentOneRttDecrypter() {
  return std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE);
}

void MockCryptoClientStream::NotifySessionZeroRttComplete() {
  DCHECK(session()->version().UsesTls());
  encryption_established_ = true;
  handshake_confirmed_ = false;
  session()->connection()->InstallDecrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_ZERO_RTT));
  SetConfigNegotiated();
  session()->OnNewEncryptionKeyAvailable(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));

  session()->DiscardOldEncryptionKey(ENCRYPTION_INITIAL);
}

void MockCryptoClientStream::NotifySessionOneRttKeyAvailable() {
  encryption_established_ = true;
  handshake_confirmed_ = true;
  DCHECK(session()->version().UsesTls());
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
          std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
    } else {
      session()->connection()->SetDecrypter(
          ENCRYPTION_FORWARD_SECURE,
          std::make_unique<StrictTaggingDecrypter>(ENCRYPTION_FORWARD_SECURE));
    }
    session()->connection()->SetEncrypter(ENCRYPTION_INITIAL, nullptr);
    session()->OnNewEncryptionKeyAvailable(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
  }
  SetConfigNegotiated();
  session()->OnTlsHandshakeComplete();
  session()->DiscardOldEncryptionKey(ENCRYPTION_INITIAL);
  session()->DiscardOldEncryptionKey(ENCRYPTION_ZERO_RTT);
  session()->NeuterHandshakeData();
}

// static
CryptoHandshakeMessage MockCryptoClientStream::GetDummyCHLOMessage() {
  CryptoHandshakeMessage message;
  message.set_tag(quic::kCHLO);
  return message;
}

void MockCryptoClientStream::SetConfigNegotiated() {
  DCHECK(session()->version().UsesTls());
  QuicTagVector cgst;
// TODO(rtenneti): Enable the following code after BBR code is checked in.
#if 0
  cgst.push_back(kTBBR);
#endif
  cgst.push_back(kQBIC);
  QuicConfig config(config_);
  config.SetBytesForConnectionIdToSend(k8ByteConnectionId);
  config.SetMaxBidirectionalStreamsToSend(kDefaultMaxStreamsPerConnection / 2);
  config.SetMaxUnidirectionalStreamsToSend(kDefaultMaxStreamsPerConnection / 2);
  config.SetInitialMaxStreamDataBytesIncomingBidirectionalToSend(
      quic::kMinimumFlowControlSendWindow);
  config.SetInitialMaxStreamDataBytesOutgoingBidirectionalToSend(
      quic::kMinimumFlowControlSendWindow);
  config.SetInitialMaxStreamDataBytesUnidirectionalToSend(
      quic::kMinimumFlowControlSendWindow);

  auto connection_id = quic::test::TestConnectionId();
  config.SetStatelessResetTokenToSend(
      quic::QuicUtils::GenerateStatelessResetToken(connection_id));
  if (session()->perspective() == Perspective::IS_CLIENT) {
    config.SetOriginalConnectionIdToSend(
        session()->connection()->connection_id());
    config.SetInitialSourceConnectionIdToSend(
        session()->connection()->connection_id());
  } else {
    config.SetInitialSourceConnectionIdToSend(
        session()->connection()->client_connection_id());
  }

  TransportParameters params;
  ASSERT_TRUE(config.FillTransportParameters(&params));
  std::string error_details;
  QuicErrorCode error = session()->config()->ProcessTransportParameters(
      params, /*is_resumption=*/false, &error_details);
  ASSERT_EQ(QUIC_NO_ERROR, error);
  ASSERT_TRUE(session()->config()->negotiated());
  session()->OnConfigNegotiated();
}

void MockCryptoClientStream::FillCryptoParams() {
  DCHECK(session()->version().UsesTls());
  crypto_negotiated_params_->cipher_suite = TLS1_CK_AES_128_GCM_SHA256 & 0xffff;
  crypto_negotiated_params_->key_exchange_group = SSL_CURVE_X25519;
  crypto_negotiated_params_->peer_signature_algorithm =
      SSL_SIGN_ECDSA_SECP256R1_SHA256;
}

}  // namespace net
