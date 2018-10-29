// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_crypto_client_stream.h"

#include <memory>

#include "net/third_party/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_sequencer_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simple_quic_framer.h"

using testing::_;

namespace quic {
namespace test {
namespace {

const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;

class QuicCryptoClientStreamTest : public QuicTest {
 public:
  QuicCryptoClientStreamTest()
      : supported_versions_(AllSupportedVersions()),
        server_id_(kServerHostname, kServerPort, false),
        crypto_config_(crypto_test_utils::ProofVerifierForTesting(),
                       TlsClientHandshaker::CreateSslCtx()) {
    CreateConnection();
  }

  void CreateConnection() {
    connection_ =
        new PacketSavingConnection(&client_helper_, &alarm_factory_,
                                   Perspective::IS_CLIENT, supported_versions_);
    // Advance the time, because timers do not like uninitialized times.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));

    session_ = QuicMakeUnique<TestQuicSpdyClientSession>(
        connection_, DefaultQuicConfig(), server_id_, &crypto_config_);
  }

  void CompleteCryptoHandshake() {
    if (stream()->handshake_protocol() != PROTOCOL_TLS1_3) {
      EXPECT_CALL(*session_, OnProofValid(testing::_));
    }
    EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_))
        .Times(testing::AnyNumber());
    stream()->CryptoConnect();
    QuicConfig config;
    crypto_test_utils::HandshakeWithFakeServer(&config, &server_helper_,
                                               &alarm_factory_, connection_,
                                               stream(), server_options_);
  }

  QuicCryptoClientStream* stream() {
    return session_->GetMutableCryptoStream();
  }

  MockQuicConnectionHelper server_helper_;
  MockQuicConnectionHelper client_helper_;
  MockAlarmFactory alarm_factory_;
  PacketSavingConnection* connection_;
  ParsedQuicVersionVector supported_versions_;
  std::unique_ptr<TestQuicSpdyClientSession> session_;
  QuicServerId server_id_;
  CryptoHandshakeMessage message_;
  QuicCryptoClientConfig crypto_config_;
  crypto_test_utils::FakeServerOptions server_options_;
};

TEST_F(QuicCryptoClientStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream()->encryption_established());
  EXPECT_FALSE(stream()->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterSHLO) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterTlsHandshake) {
  FLAGS_quic_supports_tls_handshake = true;
  supported_versions_.clear();
  for (QuicTransportVersion transport_version :
       AllSupportedTransportVersions()) {
    supported_versions_.push_back(
        ParsedQuicVersion(PROTOCOL_TLS1_3, transport_version));
  }
  CreateConnection();
  CompleteCryptoHandshake();
  EXPECT_EQ(PROTOCOL_TLS1_3, stream()->handshake_protocol());
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();

  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE, _, _));
  message_.set_tag(kCHLO);
  crypto_test_utils::SendHandshakeMessageToStream(stream(), message_,
                                                  Perspective::IS_CLIENT);
}

TEST_F(QuicCryptoClientStreamTest, BadMessageType) {
  stream()->CryptoConnect();

  message_.set_tag(kCHLO);

  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                            "Expected REJ", _));
  crypto_test_utils::SendHandshakeMessageToStream(stream(), message_,
                                                  Perspective::IS_CLIENT);
}

TEST_F(QuicCryptoClientStreamTest, NegotiatedParameters) {
  CompleteCryptoHandshake();

  const QuicConfig* config = session_->config();
  EXPECT_EQ(kMaximumIdleTimeoutSecs, config->IdleNetworkTimeout().ToSeconds());

  const QuicCryptoNegotiatedParameters& crypto_params(
      stream()->crypto_negotiated_params());
  EXPECT_EQ(crypto_config_.aead[0], crypto_params.aead);
  EXPECT_EQ(crypto_config_.kexs[0], crypto_params.key_exchange);
}

TEST_F(QuicCryptoClientStreamTest, ExpiredServerConfig) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  // Recreate connection with the new config.
  CreateConnection();

  // Advance time 5 years to ensure that we pass the expiry time of the cached
  // server config.
  connection_->AdvanceTime(
      QuicTime::Delta::FromSeconds(60 * 60 * 24 * 365 * 5));

  EXPECT_CALL(*session_, OnProofValid(testing::_));
  stream()->CryptoConnect();
  // Check that a client hello was sent.
  ASSERT_EQ(1u, connection_->encrypted_packets_.size());
  EXPECT_EQ(ENCRYPTION_NONE, connection_->encryption_level());
}

TEST_F(QuicCryptoClientStreamTest, ClockSkew) {
  // Test that if the client's clock is skewed with respect to the server,
  // the handshake succeeds. In the past, the client would get the server
  // config, notice that it had already expired and then close the connection.

  // Advance time 5 years to ensure that we pass the expiry time in the server
  // config, but the TTL is used instead.
  connection_->AdvanceTime(
      QuicTime::Delta::FromSeconds(60 * 60 * 24 * 365 * 5));

  // The handshakes completes!
  CompleteCryptoHandshake();
}

TEST_F(QuicCryptoClientStreamTest, InvalidCachedServerConfig) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  // Recreate connection with the new config.
  CreateConnection();

  QuicCryptoClientConfig::CachedState* state =
      crypto_config_.LookupOrCreate(server_id_);

  std::vector<QuicString> certs = state->certs();
  QuicString cert_sct = state->cert_sct();
  QuicString signature = state->signature();
  QuicString chlo_hash = state->chlo_hash();
  state->SetProof(certs, cert_sct, chlo_hash, signature + signature);

  EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_))
      .Times(testing::AnyNumber());
  stream()->CryptoConnect();
  // Check that a client hello was sent.
  ASSERT_EQ(1u, connection_->encrypted_packets_.size());
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdate) {
  // Test that the crypto client stream can receive server config updates after
  // the connection has been established.
  CompleteCryptoHandshake();

  QuicCryptoClientConfig::CachedState* state =
      crypto_config_.LookupOrCreate(server_id_);

  // Ensure cached STK is different to what we send in the handshake.
  EXPECT_NE("xstk", state->source_address_token());

  // Initialize using {...} syntax to avoid trailing \0 if converting from
  // string.
  unsigned char stk[] = {'x', 's', 't', 'k'};

  // Minimum SCFG that passes config validation checks.
  unsigned char scfg[] = {// SCFG
                          0x53, 0x43, 0x46, 0x47,
                          // num entries
                          0x01, 0x00,
                          // padding
                          0x00, 0x00,
                          // EXPY
                          0x45, 0x58, 0x50, 0x59,
                          // EXPY end offset
                          0x08, 0x00, 0x00, 0x00,
                          // Value
                          '1', '2', '3', '4', '5', '6', '7', '8'};

  CryptoHandshakeMessage server_config_update;
  server_config_update.set_tag(kSCUP);
  server_config_update.SetValue(kSourceAddressTokenTag, stk);
  server_config_update.SetValue(kSCFG, scfg);
  const uint64_t expiry_seconds = 60 * 60 * 24 * 2;
  server_config_update.SetValue(kSTTL, expiry_seconds);

  crypto_test_utils::SendHandshakeMessageToStream(
      stream(), server_config_update, Perspective::IS_SERVER);

  // Make sure that the STK and SCFG are cached correctly.
  EXPECT_EQ("xstk", state->source_address_token());

  const QuicString& cached_scfg = state->server_config();
  test::CompareCharArraysWithHexError(
      "scfg", cached_scfg.data(), cached_scfg.length(),
      reinterpret_cast<char*>(scfg), QUIC_ARRAYSIZE(scfg));

  QuicStreamSequencer* sequencer = QuicStreamPeer::sequencer(stream());
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdateWithCert) {
  // Test that the crypto client stream can receive and use server config
  // updates with certificates after the connection has been established.
  CompleteCryptoHandshake();

  // Build a server config update message with certificates
  QuicCryptoServerConfig crypto_config(
      QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
      crypto_test_utils::ProofSourceForTesting(), KeyExchangeSource::Default(),
      TlsServerHandshaker::CreateSslCtx());
  crypto_test_utils::FakeServerOptions options;
  crypto_test_utils::SetupCryptoServerConfigForTest(
      connection_->clock(), QuicRandom::GetInstance(), &crypto_config, options);
  SourceAddressTokens tokens;
  QuicCompressedCertsCache cache(1);
  CachedNetworkParameters network_params;
  CryptoHandshakeMessage server_config_update;

  class Callback : public BuildServerConfigUpdateMessageResultCallback {
   public:
    Callback(bool* ok, CryptoHandshakeMessage* message)
        : ok_(ok), message_(message) {}
    void Run(bool ok, const CryptoHandshakeMessage& message) override {
      *ok_ = ok;
      *message_ = message;
    }

   private:
    bool* ok_;
    CryptoHandshakeMessage* message_;
  };

  // Note: relies on the callback being invoked synchronously
  bool ok = false;
  crypto_config.BuildServerConfigUpdateMessage(
      session_->connection()->transport_version(), stream()->chlo_hash(),
      tokens, QuicSocketAddress(QuicIpAddress::Loopback6(), 1234),
      QuicIpAddress::Loopback6(), connection_->clock(),
      QuicRandom::GetInstance(), &cache, stream()->crypto_negotiated_params(),
      &network_params,
      std::unique_ptr<BuildServerConfigUpdateMessageResultCallback>(
          new Callback(&ok, &server_config_update)));
  EXPECT_TRUE(ok);

  EXPECT_CALL(*session_, OnProofValid(testing::_));
  crypto_test_utils::SendHandshakeMessageToStream(
      stream(), server_config_update, Perspective::IS_SERVER);

  // Recreate connection with the new config and verify a 0-RTT attempt.
  CreateConnection();

  EXPECT_CALL(*connection_, OnCanWrite());
  EXPECT_CALL(*session_, OnProofValid(testing::_));
  EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_))
      .Times(testing::AnyNumber());
  stream()->CryptoConnect();
  EXPECT_TRUE(session_->IsEncryptionEstablished());
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdateBeforeHandshake) {
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE, _, _));
  CryptoHandshakeMessage server_config_update;
  server_config_update.set_tag(kSCUP);
  crypto_test_utils::SendHandshakeMessageToStream(
      stream(), server_config_update, Perspective::IS_SERVER);
}

TEST_F(QuicCryptoClientStreamTest, NoChannelID) {
  crypto_config_.SetChannelIDSource(nullptr);

  CompleteCryptoHandshake();
  EXPECT_FALSE(stream()->WasChannelIDSent());
  EXPECT_FALSE(stream()->WasChannelIDSourceCallbackRun());
}

TEST_F(QuicCryptoClientStreamTest, TokenBindingNegotiation) {
  server_options_.token_binding_params = QuicTagVector{kTB10, kP256};
  crypto_config_.tb_key_params = QuicTagVector{kTB10};

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
  EXPECT_EQ(kTB10,
            stream()->crypto_negotiated_params().token_binding_key_param);
}

TEST_F(QuicCryptoClientStreamTest, NoTokenBindingWithoutServerSupport) {
  crypto_config_.tb_key_params = QuicTagVector{kTB10, kP256};

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
  EXPECT_EQ(0u, stream()->crypto_negotiated_params().token_binding_key_param);
}

TEST_F(QuicCryptoClientStreamTest, NoTokenBindingWithoutClientSupport) {
  server_options_.token_binding_params = QuicTagVector{kTB10, kP256};

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
  EXPECT_EQ(0u, stream()->crypto_negotiated_params().token_binding_key_param);
}

TEST_F(QuicCryptoClientStreamTest, TokenBindingNotNegotiated) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
  EXPECT_EQ(0u, stream()->crypto_negotiated_params().token_binding_key_param);
}

TEST_F(QuicCryptoClientStreamTest, NoTokenBindingInPrivacyMode) {
  server_options_.token_binding_params = QuicTagVector{kTB10};
  crypto_config_.tb_key_params = QuicTagVector{kTB10};
  server_id_ = QuicServerId(kServerHostname, kServerPort, true);
  CreateConnection();

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->handshake_confirmed());
  EXPECT_EQ(0u, stream()->crypto_negotiated_params().token_binding_key_param);
}

class QuicCryptoClientStreamStatelessTest : public QuicTest {
 public:
  QuicCryptoClientStreamStatelessTest()
      : client_crypto_config_(crypto_test_utils::ProofVerifierForTesting(),
                              TlsClientHandshaker::CreateSslCtx()),
        server_crypto_config_(QuicCryptoServerConfig::TESTING,
                              QuicRandom::GetInstance(),
                              crypto_test_utils::ProofSourceForTesting(),
                              KeyExchangeSource::Default(),
                              TlsServerHandshaker::CreateSslCtx()),
        server_compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
        server_id_(kServerHostname, kServerPort, false) {
    TestQuicSpdyClientSession* client_session = nullptr;
    CreateClientSessionForTest(server_id_,
                               /* supports_stateless_rejects= */ true,
                               QuicTime::Delta::FromSeconds(100000),
                               AllSupportedVersions(), &helper_,
                               &alarm_factory_, &client_crypto_config_,
                               &client_connection_, &client_session);
    CHECK(client_session);
    client_session_.reset(client_session);
  }

  QuicCryptoServerStream* server_stream() {
    return server_session_->GetMutableCryptoStream();
  }

  void AdvanceHandshakeWithFakeServer() {
    client_session_->GetMutableCryptoStream()->CryptoConnect();
    EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*server_session_->helper(), GenerateConnectionIdForReject(_))
        .Times(testing::AnyNumber());
    crypto_test_utils::AdvanceHandshake(
        client_connection_, client_session_->GetMutableCryptoStream(), 0,
        server_connection_, server_stream(), 0);
  }

  // Initializes the server_stream_ for stateless rejects.
  void InitializeFakeStatelessRejectServer() {
    TestQuicSpdyServerSession* server_session = nullptr;
    CreateServerSessionForTest(
        server_id_, QuicTime::Delta::FromSeconds(100000),
        ParsedVersionOfIndex(CurrentSupportedVersions(), 0), &helper_,
        &alarm_factory_, &server_crypto_config_,
        &server_compressed_certs_cache_, &server_connection_, &server_session);
    CHECK(server_session);
    server_session_.reset(server_session);
    server_session_->OnSuccessfulVersionNegotiation(
        CurrentSupportedVersions()[0]);
    crypto_test_utils::FakeServerOptions options;
    crypto_test_utils::SetupCryptoServerConfigForTest(
        server_connection_->clock(), server_connection_->random_generator(),
        &server_crypto_config_, options);
    SetQuicReloadableFlag(enable_quic_stateless_reject_support, true);
  }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;

  // Client crypto stream state
  PacketSavingConnection* client_connection_;
  std::unique_ptr<TestQuicSpdyClientSession> client_session_;
  QuicCryptoClientConfig client_crypto_config_;

  // Server crypto stream state
  PacketSavingConnection* server_connection_;
  std::unique_ptr<TestQuicSpdyServerSession> server_session_;
  QuicCryptoServerConfig server_crypto_config_;
  QuicCompressedCertsCache server_compressed_certs_cache_;
  QuicServerId server_id_;
};

TEST_F(QuicCryptoClientStreamStatelessTest, StatelessReject) {
  SetQuicReloadableFlag(enable_quic_stateless_reject_support, true);

  QuicCryptoClientConfig::CachedState* client_state =
      client_crypto_config_.LookupOrCreate(server_id_);

  EXPECT_FALSE(client_state->has_server_designated_connection_id());
  EXPECT_CALL(*client_session_, OnProofValid(testing::_));

  InitializeFakeStatelessRejectServer();
  EXPECT_CALL(*client_connection_,
              CloseConnection(QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, _, _));
  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, _, _));
  AdvanceHandshakeWithFakeServer();

  EXPECT_EQ(1, server_stream()->NumHandshakeMessages());
  EXPECT_EQ(0, server_stream()->NumHandshakeMessagesWithServerNonces());

  EXPECT_FALSE(client_session_->IsEncryptionEstablished());
  EXPECT_FALSE(client_session_->IsCryptoHandshakeConfirmed());
  // Even though the handshake was not complete, the cached client_state is
  // complete, and can be used for a subsequent successful handshake.
  EXPECT_TRUE(client_state->IsComplete(QuicWallTime::FromUNIXSeconds(0)));

  ASSERT_TRUE(client_state->has_server_nonce());
  ASSERT_FALSE(client_state->GetNextServerNonce().empty());
  ASSERT_TRUE(client_state->has_server_designated_connection_id());
  QuicConnectionId server_designated_id =
      client_state->GetNextServerDesignatedConnectionId();
  QuicConnectionId expected_id =
      server_session_->connection()->random_generator()->RandUint64();
  EXPECT_EQ(expected_id, server_designated_id);
  EXPECT_FALSE(client_state->has_server_designated_connection_id());
}

}  // namespace
}  // namespace test
}  // namespace quic
