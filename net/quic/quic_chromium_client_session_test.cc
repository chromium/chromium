// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session_peer.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_connectivity_monitor.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/simple_quic_framer.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using testing::_;

namespace net::test {
namespace {

const IPEndPoint kIpEndPoint = IPEndPoint(IPAddress::IPv4AllZeros(), 0);
const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;
const size_t kMaxReadersPerQuicSession = 5;

const handles::NetworkHandle kDefaultNetworkForTests = 1;
const handles::NetworkHandle kNewNetworkForTests = 2;

// A subclass of QuicChromiumClientSession that allows OnPathDegrading to be
// mocked.
class TestingQuicChromiumClientSession : public QuicChromiumClientSession {
 public:
  using QuicChromiumClientSession::QuicChromiumClientSession;

  MOCK_METHOD(void, OnPathDegrading, (), (override));

  void ReallyOnPathDegrading() { QuicChromiumClientSession::OnPathDegrading(); }
};

class QuicChromiumClientSessionTest
    : public ::testing::TestWithParam<quic::ParsedQuicVersion>,
      public WithTaskEnvironment {
 public:
  QuicChromiumClientSessionTest()
      : version_(GetParam()),
        config_(quic::test::DefaultQuicConfig()),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        default_read_(
            std::make_unique<MockRead>(SYNCHRONOUS, ERR_IO_PENDING, 0)),
        socket_data_(std::make_unique<SequencedSocketData>(
            base::make_span(default_read_.get(), 1u),
            base::span<MockWrite>())),
        helper_(&clock_, &random_),
        transport_security_state_(std::make_unique<TransportSecurityState>()),
        session_key_(kServerHostname,
                     kServerPort,
                     PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(),
                     SessionUsage::kDestination,
                     SocketTag(),
                     NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
        destination_(url::kHttpsScheme, kServerHostname, kServerPort),
        default_network_(handles::kInvalidNetworkHandle),
        client_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(&random_),
                      &clock_,
                      kServerHostname,
                      quic::Perspective::IS_CLIENT),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(&random_),
                      &clock_,
                      kServerHostname,
                      quic::Perspective::IS_SERVER,
                      false) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic::QuicEnableVersion(version_);
    // Advance the time, because timers do not like uninitialized times.
    clock_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void ResetHandleOnError(
      std::unique_ptr<QuicChromiumClientSession::Handle>* handle,
      int net_error) {
    EXPECT_NE(OK, net_error);
    handle->reset();
  }

 protected:
  void Initialize() {
    if (socket_data_) {
      socket_factory_.AddSocketDataProvider(socket_data_.get());
    }
    std::unique_ptr<DatagramClientSocket> socket =
        socket_factory_.CreateDatagramClientSocket(
            DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
    socket->Connect(kIpEndPoint);
    QuicChromiumPacketWriter* writer = new net::QuicChromiumPacketWriter(
        socket.get(), base::SingleThreadTaskRunner::GetCurrentDefault().get());
    quic::QuicConnection* connection = new quic::QuicConnection(
        quic::QuicUtils::CreateRandomConnectionId(&random_),
        quic::QuicSocketAddress(), ToQuicSocketAddress(kIpEndPoint), &helper_,
        &alarm_factory_, writer, true, quic::Perspective::IS_CLIENT,
        quic::test::SupportedVersions(version_), connection_id_generator_);
    session_ = std::make_unique<TestingQuicChromiumClientSession>(
        connection, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        transport_security_state_.get(), &ssl_config_service_,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)),
        QuicSessionAliasKey(url::SchemeHostPort(), session_key_),
        /*require_confirmation=*/false, migrate_session_early_v2_,
        /*migrate_session_on_network_change_v2=*/false, default_network_,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/false, allow_port_migration_,
        kDefaultIdleSessionMigrationPeriod, /*multi_port_probing_interval=*/0,
        kMaxTimeOnNonDefaultNetwork,
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        /*cert_verify_flags=*/0, config_,
        std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config_),
        "CONNECTION_UNKNOWN", base::TimeTicks::Now(), base::TimeTicks::Now(),
        base::DefaultTickClock::GetInstance(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*socket_performance_watcher=*/nullptr, ConnectionEndpointMetadata(),
        /*report_ecn=*/true, /*enable_origin_frame=*/true,
        NetLogWithSource::Make(NetLogSourceType::NONE));
    if (connectivity_monitor_) {
      connectivity_monitor_->SetInitialDefaultNetwork(default_network_);
      session_->AddConnectivityObserver(connectivity_monitor_.get());
    }

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    verify_details_.cert_verify_result.verified_cert = cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    session_->Initialize();
    // Blackhole QPACK decoder stream instead of constructing mock writes.
    session_->qpack_decoder()->set_qpack_stream_sender_delegate(
        &noop_qpack_stream_sender_delegate_);
    session_->StartReading();
    writer->set_delegate(session_.get());
  }

  void TearDown() override {
    if (session_) {
      if (connectivity_monitor_) {
        session_->RemoveConnectivityObserver(connectivity_monitor_.get());
      }
      session_->CloseSessionOnError(
          ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
          quic::ConnectionCloseBehavior::SILENT_CLOSE);
    }
  }

  void CompleteCryptoHandshake() {
    ASSERT_THAT(session_->CryptoConnect(callback_.callback()), IsOk());
  }

  std::unique_ptr<QuicChromiumPacketWriter> CreateQuicChromiumPacketWriter(
      DatagramClientSocket* socket,
      QuicChromiumClientSession* session) const {
    auto writer = std::make_unique<QuicChromiumPacketWriter>(
        socket, base::SingleThreadTaskRunner::GetCurrentDefault().get());
    writer->set_delegate(session);
    return writer;
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        version_.transport_version, n);
  }

  size_t GetMaxAllowedOutgoingBidirectionalStreams() {
    return quic::test::QuicSessionPeer::ietf_streamid_manager(session_.get())
        ->max_outgoing_bidirectional_streams();
  }

  const quic::ParsedQuicVersion version_;
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  quic::QuicConfig config_;
  quic::QuicCryptoClientConfig crypto_config_;
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLog::Get(), NetLogSourceType::NONE)};
  MockClientSocketFactory socket_factory_;
  std::unique_ptr<MockRead> default_read_;
  std::unique_ptr<SequencedSocketData> socket_data_;
  quic::MockClock clock_;
  quic::test::MockRandom random_{0};
  QuicChromiumConnectionHelper helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  SSLConfigServiceDefaults ssl_config_service_;
  QuicSessionKey session_key_;
  url::SchemeHostPort destination_;
  std::unique_ptr<TestingQuicChromiumClientSession> session_;
  handles::NetworkHandle default_network_;
  std::unique_ptr<QuicConnectivityMonitor> connectivity_monitor_;
  raw_ptr<quic::QuicConnectionVisitorInterface> visitor_;
  TestCompletionCallback callback_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  ProofVerifyDetailsChromium verify_details_;
  bool migrate_session_early_v2_ = false;
  bool allow_port_migration_ = false;
  quic::test::MockConnectionIdGenerator connection_id_generator_;
  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicChromiumClientSessionTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

// Basic test of ProofVerifyDetailsChromium is converted to SSLInfo retrieved
// through QuicChromiumClientSession::GetSSLInfo(). Doesn't test some of the
// more complicated fields.
TEST_P(QuicChromiumClientSessionTest, GetSSLInfo1) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  ProofVerifyDetailsChromium details;
  details.is_fatal_cert_error = false;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  details.cert_verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  SSLInfo ssl_info;
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_valid());

  EXPECT_EQ(details.is_fatal_cert_error, ssl_info.is_fatal_cert_error);
  EXPECT_TRUE(ssl_info.cert->EqualsIncludingChain(
      details.cert_verify_result.verified_cert.get()));
  EXPECT_EQ(details.cert_verify_result.cert_status, ssl_info.cert_status);
  EXPECT_EQ(details.cert_verify_result.is_issued_by_known_root,
            ssl_info.is_issued_by_known_root);
  EXPECT_EQ(details.cert_verify_result.policy_compliance,
            ssl_info.ct_policy_compliance);
}

// Just like GetSSLInfo1, but uses different values.
TEST_P(QuicChromiumClientSessionTest, GetSSLInfo2) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  ProofVerifyDetailsChromium details;
  details.is_fatal_cert_error = false;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = false;
  details.cert_verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  SSLInfo ssl_info;
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_valid());

  EXPECT_EQ(details.is_fatal_cert_error, ssl_info.is_fatal_cert_error);
  EXPECT_TRUE(ssl_info.cert->EqualsIncludingChain(
      details.cert_verify_result.verified_cert.get()));
  EXPECT_EQ(details.cert_verify_result.cert_status, ssl_info.cert_status);
  EXPECT_EQ(details.cert_verify_result.is_issued_by_known_root,
            ssl_info.is_issued_by_known_root);
  EXPECT_EQ(details.cert_verify_result.policy_compliance,
            ssl_info.ct_policy_compliance);
}

TEST_P(QuicChromiumClientSessionTest, IsFatalErrorNotSetForNonFatalError) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  SSLInfo ssl_info;
  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.cert_status = CERT_STATUS_DATE_INVALID;
  details.is_fatal_cert_error = false;
  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_FALSE(ssl_info.is_fatal_cert_error);
}

TEST_P(QuicChromiumClientSessionTest, IsFatalErrorSetForFatalError) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  SSLInfo ssl_info;
  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.cert_status = CERT_STATUS_DATE_INVALID;
  details.is_fatal_cert_error = true;
  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_fatal_cert_error);
}

TEST_P(QuicChromiumClientSessionTest, CryptoConnect) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();
}

TEST_P(QuicChromiumClientSessionTest, Handle) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  NetLogWithSource session_net_log = session_->net_log();
  EXPECT_EQ(NetLogSourceType::QUIC_SESSION, session_net_log.source().type);
  EXPECT_EQ(NetLog::Get(), session_net_log.net_log());

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  EXPECT_TRUE(handle->IsConnected());
  EXPECT_FALSE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  IPEndPoint address;
  EXPECT_EQ(OK, handle->GetPeerAddress(&address));
  EXPECT_EQ(kIpEndPoint, address);
  EXPECT_TRUE(handle->CreatePacketBundler().get() != nullptr);

  CompleteCryptoHandshake();

  EXPECT_TRUE(handle->OneRttKeysAvailable());

  // Request a stream and verify that a stream was created.
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());

  // Veirfy that the handle works correctly after the session is closed.
  EXPECT_FALSE(handle->IsConnected());
  EXPECT_TRUE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, handle->GetPeerAddress(&address));
  EXPECT_TRUE(handle->CreatePacketBundler().get() == nullptr);
  {
    // Verify that CreateHandle() works even after the session is closed.
    std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
        session_->CreateHandle(destination_);
    EXPECT_FALSE(handle2->IsConnected());
    EXPECT_TRUE(handle2->OneRttKeysAvailable());
    ASSERT_EQ(ERR_CONNECTION_CLOSED,
              handle2->RequestStream(/*requires_confirmation=*/false,
                                     callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  session_.reset();

  // Verify that the handle works correctly after the session is deleted.
  EXPECT_FALSE(handle->IsConnected());
  EXPECT_TRUE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, handle->GetPeerAddress(&address));
  EXPECT_TRUE(handle->CreatePacketBundler().get() == nullptr);
  ASSERT_EQ(
      ERR_CONNECTION_CLOSED,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
}

TEST_P(QuicChromiumClientSessionTest, StreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConfirmationRequiredStreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/true,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, StreamRequestBeforeConfirmation) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/true, callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  CompleteCryptoHandshake();

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, CancelStreamRequestBeforeRelease) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and cancel it without releasing the stream.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  handle.reset();

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, AsyncStreamRequest) {
  MockQuicData quic_data(version_);
  uint64_t packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
  // at the limit of 50.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  // Similarly, requesting the 52nd stream will also send a STREAMS_BLOCKED.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  // After the STREAMS_BLOCKED is sent, receive a MAX_STREAMS to increase
  // the limit to 100.
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                                   /*stream_count=*/100,
                                                   /*unidirectional=*/false)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that subsequent requests cannot
  // proceed immediately.
  EXPECT_EQ(GetMaxAllowedOutgoingBidirectionalStreams(), 50u);
  for (size_t i = 0; i < 50; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(session_->GetNumActiveStreams(), 50u);

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  // Request a second stream and verify that it's also pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close two stream to open up sending credits.
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
  quic::QuicRstStreamFrame rst2(quic::kInvalidControlFrameId,
                                GetNthClientInitiatedBidirectionalStreamId(1),
                                quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst2);
  // To close the streams completely, we need to also receive STOP_SENDING
  // frames.
  quic::QuicStopSendingFrame stop_sending(
      quic::kInvalidControlFrameId,
      GetNthClientInitiatedBidirectionalStreamId(0),
      quic::QUIC_STREAM_CANCELLED);
  session_->OnStopSendingFrame(stop_sending);
  quic::QuicStopSendingFrame stop_sending2(
      quic::kInvalidControlFrameId,
      GetNthClientInitiatedBidirectionalStreamId(1),
      quic::QUIC_STREAM_CANCELLED);
  session_->OnStopSendingFrame(stop_sending2);

  EXPECT_FALSE(callback.have_result());
  EXPECT_FALSE(callback2.have_result());

  // Pump the message loop to read the packet containing the MAX_STREAMS frame.
  base::RunLoop().RunUntilIdle();

  // Make sure that both requests were unblocked.
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);
  ASSERT_TRUE(callback2.have_result());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_TRUE(handle2->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1021938.
// When the connection is closed, there may be tasks queued in the message loop
// to read the last packet, reading that packet should not crash.
TEST_P(QuicChromiumClientSessionTest, ReadAfterConnectionClose) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
  // at the limit of 50.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  // This packet will be read after connection is closed.
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request two streams which will both be pending.
  // In V99 each will generate a max stream id for each attempt.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);

  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(
          /*requires_confirmation=*/false,
          base::BindOnce(&QuicChromiumClientSessionTest::ResetHandleOnError,
                         base::Unretained(this), &handle2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  session_->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "Timed out",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle2.get());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ClosedWithAsyncStreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
  // at the limit of 50.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request two streams which will both be pending.
  // In V99 each will generate a max stream id for each attempt.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);

  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(
          /*requires_confirmation=*/false,
          base::BindOnce(&QuicChromiumClientSessionTest::ResetHandleOnError,
                         base::Unretained(this), &handle2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  session_->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "Timed out",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle2.get());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, CancelPendingStreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  // This node receives the RST_STREAM+STOP_SENDING, it responds
  // with only a RST_STREAM.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Cancel the pending stream request.
  handle.reset();

  // Close a stream and ensure that no new stream is created.
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
  // We require a STOP_SENDING as well as a RESET_STREAM to fully close the
  // stream.
  quic::QuicStopSendingFrame stop_sending(
      quic::kInvalidControlFrameId,
      GetNthClientInitiatedBidirectionalStreamId(0),
      quic::QUIC_STREAM_CANCELLED);
  session_->OnStopSendingFrame(stop_sending);
  EXPECT_EQ(kMaxOpenStreams - 1, session_->GetNumActiveStreams());

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseBeforeStreamRequest) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());

  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->connection()->SendPing();

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();

  // Request a stream and verify that it failed.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_CONNECTION_CLOSED,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseBeforeHandshakeConfirmed) {
  if (version_.UsesTls()) {
    // TODO(nharper, b/112643533): Figure out why this test fails when TLS is
    // enabled and fix it.
    return;
  }

  // Force the connection close packet to use long headers with connection ID.
  server_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);

  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/true, callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close the connection and verify that the StreamRequest completes with
  // an error.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseWithPendingStreamRequest) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->connection()->SendPing();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close the connection and verify that the StreamRequest completes with
  // an error.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MaxNumStreams) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // Initial configuration is 50 dynamic streams. Taking into account
  // the static stream (headers), expect to block on when hitting the limit
  // of 50 streams
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  // For the second CreateOutgoingStream that fails because of hitting the
  // stream count limit.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(4)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                                   /*stream_count=*/50 + 2,
                                                   /*unidirectional=*/false)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();

  std::vector<QuicChromiumClientStream*> streams;
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }
  // This stream, the 51st dynamic stream, can not be opened.
  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));

  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Close a stream and ensure I can now open a new one.
  quic::QuicStreamId stream_id = streams[0]->id();
  session_->ResetStream(stream_id, quic::QUIC_RST_ACKNOWLEDGEMENT);

  // Pump data, bringing in the max-stream-id
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));
  quic::QuicRstStreamFrame rst1(quic::kInvalidControlFrameId, stream_id,
                                quic::QUIC_STREAM_NO_ERROR, 0);
  session_->OnRstStream(rst1);
  EXPECT_EQ(kMaxOpenStreams - 1, session_->GetNumActiveStreams());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));
}

// Regression test for crbug.com/968621.
TEST_P(QuicChromiumClientSessionTest, PendingStreamOnRst) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      ASYNC,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic::QuicStreamFrame data(GetNthServerInitiatedUnidirectionalStreamId(0),
                             false, 1, std::string_view("SP"));
  session_->OnStreamFrame(data);
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
}

// Regression test for crbug.com/971361.
TEST_P(QuicChromiumClientSessionTest, ClosePendingStream) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      ASYNC,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic::QuicStreamId id = GetNthServerInitiatedUnidirectionalStreamId(0);
  quic::QuicStreamFrame data(id, false, 1, std::string_view("SP"));
  session_->OnStreamFrame(data);
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  session_->ResetStream(id, quic::QUIC_STREAM_NO_ERROR);
}

TEST_P(QuicChromiumClientSessionTest, MaxNumStreamsViaRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                                   /*stream_count=*/52,
                                                   /*unidirectional=*/false)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  std::vector<QuicChromiumClientStream*> streams;
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close a stream and ensure I can now open a new one.
  quic::QuicStreamId stream_id = streams[0]->id();
  session_->ResetStream(stream_id, quic::QUIC_RST_ACKNOWLEDGEMENT);
  quic::QuicRstStreamFrame rst1(quic::kInvalidControlFrameId, stream_id,
                                quic::QUIC_STREAM_NO_ERROR, 0);
  session_->OnRstStream(rst1);
  // Pump data, bringing in the max-stream-id
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);
}

TEST_P(QuicChromiumClientSessionTest, GoAwayReceived) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_->OnHttp3GoAway(0);
  EXPECT_EQ(nullptr, QuicChromiumClientSessionPeer::CreateOutgoingStream(
                         session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, CanPool) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  EXPECT_TRUE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_ENABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                     /*require_dns_https_alpn=*/false)));
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag1,
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag2,
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
#endif
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED,
                     ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                                       "bar", 443),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kProxy, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));

  EXPECT_TRUE(session_->CanPool(
      "mail.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_TRUE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "mail.google.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));

  const SchemefulSite kSiteFoo(GURL("http://foo.test/"));

  // Check that NetworkAnonymizationKey is respected when feature is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    EXPECT_TRUE(session_->CanPool(
        "mail.example.com",
        QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                       SessionUsage::kDestination, SocketTag(),
                       NetworkAnonymizationKey::CreateSameSite(kSiteFoo),
                       SecureDnsPolicy::kAllow,
                       /*require_dns_https_alpn=*/false)));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    EXPECT_FALSE(session_->CanPool(
        "mail.example.com",
        QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                       SessionUsage::kDestination, SocketTag(),
                       NetworkAnonymizationKey::CreateSameSite(kSiteFoo),
                       SecureDnsPolicy::kAllow,
                       /*require_dns_https_alpn=*/false)));
  }
}

// Much as above, but uses a non-empty NetworkAnonymizationKey.
TEST_P(QuicChromiumClientSessionTest, CanPoolWithNetworkAnonymizationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  const SchemefulSite kSiteFoo(GURL("http://foo.test/"));
  const SchemefulSite kSiteBar(GURL("http://bar.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSiteFoo);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSiteBar);

  session_key_ = QuicSessionKey(
      kServerHostname, kServerPort, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), kNetworkAnonymizationKey1,
      SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false);

  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  EXPECT_TRUE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_ENABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag1,
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag2,
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
#endif
  EXPECT_TRUE(session_->CanPool(
      "mail.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_TRUE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "mail.google.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));

  EXPECT_FALSE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey2, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
}

TEST_P(QuicChromiumClientSessionTest, ConnectionNotPooledWithDifferentPin) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  // Configure the TransportSecurityStateSource so that kPreloadedPKPHost will
  // have static PKP pins set.
  ScopedTransportSecurityStateSource scoped_security_state_source;

  // |net::test_default::kHSTSSource| defines pins for kPreloadedPKPHost.
  // (This hostname must be in the spdy_pooling.pem SAN.)
  const char kPreloadedPKPHost[] = "www.example.org";
  // A hostname without any static state.  (This hostname isn't in
  // spdy_pooling.pem SAN, but that's okay because the
  // ProofVerifyDetailsChromium are faked.)
  const char kNoPinsHost[] = "no-pkp.example.org";

  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  transport_security_state_->EnableStaticPinsForTesting();
  transport_security_state_->SetPinningListAlwaysTimelyForTesting(true);

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  uint8_t bad_pin = 3;
  details.cert_verify_result.public_key_hashes.push_back(
      GetTestHashValue(bad_pin));

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  QuicChromiumClientSessionPeer::SetHostname(session_.get(), kNoPinsHost);

  EXPECT_FALSE(session_->CanPool(
      kPreloadedPKPHost,
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
}

TEST_P(QuicChromiumClientSessionTest, ConnectionPooledWithMatchingPin) {
  ScopedTransportSecurityStateSource scoped_security_state_source;

  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  transport_security_state_->EnableStaticPinsForTesting();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  details.cert_verify_result.public_key_hashes.push_back(primary_pin);

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  QuicChromiumClientSessionPeer::SetHostname(session_.get(), "www.example.org");

  EXPECT_TRUE(session_->CanPool(
      "mail.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocket) {
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MockQuicData quic_data(version_);
  int packet_num = 1;
  int peer_packet_num = 1;
  socket_data_.reset();
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddRead(ASYNC, server_maker_.Packet(peer_packet_num++)
                               .AddNewConnectionIdFrame(cid_on_new_path,
                                                        /*sequence_number=*/1u,
                                                        /*retire_prior_to=*/0u)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make new connection ID available after handshake completion.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  char data[] = "ABCD";
  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.Packet(packet_num++)
                          .AddAckFrame(/*first_received=*/1,
                                       /*largest_received=*/peer_packet_num - 1,
                                       /*smallest_received=*/1)
                          .AddPingFrame()
                          .Build());
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0), false,
                          std::string_view(data))
          .Build());
  quic_data2.AddSocketDataToFactory(&socket_factory_);
  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::RANDOM_BIND,
                                                 NetLog::Get(), NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  auto new_reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(new_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      /*report_ecn=*/true, net_log_with_source_);
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

  IPEndPoint local_address;
  new_reader->socket()->GetLocalAddress(&local_address);
  IPEndPoint peer_address;
  new_reader->socket()->GetPeerAddress(&peer_address);
  // Migrate session.
  EXPECT_TRUE(session_->MigrateToSocket(
      ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
      std::move(new_reader), std::move(new_writer)));
  // Spin message loop to complete migration.
  base::RunLoop().RunUntilIdle();

  // Write data to session.
  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  quic::test::QuicStreamPeer::SendBuffer(stream).SaveStreamData(data);
  quic::test::QuicStreamPeer::SetStreamBytesWritten(4, stream);
  session_->WritevData(stream->id(), 4, 0, quic::NO_FIN,
                       quic::NOT_RETRANSMISSION,
                       quic::ENCRYPTION_FORWARD_SECURE);

  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocketMaxReaders) {
  MockQuicData quic_data(version_);
  socket_data_.reset();
  int packet_num = 1;
  int peer_packet_num = 1;
  quic::QuicConnectionId next_cid = quic::QuicUtils::CreateRandomConnectionId(
      quiche::QuicheRandom::GetInstance());
  uint64_t next_cid_sequence_number = 1u;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC,
                    server_maker_.Packet(peer_packet_num++)
                        .AddNewConnectionIdFrame(
                            next_cid, next_cid_sequence_number,
                            /*retire_prior_to=*/next_cid_sequence_number - 1)
                        .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make connection ID available for the first migration.
  quic_data.Resume();

  /* Migration succeeds when maximum number of readers is not reached.*/
  for (size_t i = 0; i < kMaxReadersPerQuicSession - 1; ++i) {
    MockQuicData quic_data2(version_);
    client_maker_.set_connection_id(next_cid);
    quic_data2.AddWrite(
        SYNCHRONOUS, client_maker_.Packet(packet_num++)
                         .AddAckFrame(/*first_received=*/1,
                                      /*largest_received=*/peer_packet_num - 1,
                                      /*smallest_received=*/1)
                         .AddPingFrame()
                         .Build());
    quic_data2.AddRead(ASYNC, ERR_IO_PENDING);
    quic_data2.AddWrite(
        ASYNC, client_maker_.Packet(packet_num++)
                   .AddRetireConnectionIdFrame(
                       /*sequence_number=*/next_cid_sequence_number - 1)
                   .Build());
    next_cid = quic::QuicUtils::CreateRandomConnectionId(
        quiche::QuicheRandom::GetInstance());
    ++next_cid_sequence_number;
    quic_data2.AddRead(ASYNC,
                       server_maker_.Packet(peer_packet_num++)
                           .AddNewConnectionIdFrame(
                               next_cid, next_cid_sequence_number,
                               /*retire_prior_to=*/next_cid_sequence_number - 1)
                           .Build());
    quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    quic_data2.AddSocketDataToFactory(&socket_factory_);

    // Create connected socket.
    std::unique_ptr<DatagramClientSocket> new_socket =
        socket_factory_.CreateDatagramClientSocket(
            DatagramSocket::RANDOM_BIND, NetLog::Get(), NetLogSource());
    EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

    // Create reader and writer.
    auto new_reader = std::make_unique<QuicChromiumPacketReader>(
        std::move(new_socket), &clock_, session_.get(),
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        /*report_ecn=*/true, net_log_with_source_);
    new_reader->StartReading();
    std::unique_ptr<QuicChromiumPacketWriter> new_writer(
        CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

    IPEndPoint local_address;
    new_reader->socket()->GetLocalAddress(&local_address);
    IPEndPoint peer_address;
    new_reader->socket()->GetPeerAddress(&peer_address);
    // Migrate session.
    EXPECT_TRUE(session_->MigrateToSocket(
        ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
        std::move(new_reader), std::move(new_writer)));
    // Spin message loop to complete migration.
    base::RunLoop().RunUntilIdle();
    alarm_factory_.FireAlarm(
        quic::test::QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(
            session_->connection()));
    // Make new connection ID available for subsequent migration.
    quic_data2.Resume();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(quic_data2.AllReadDataConsumed());
    EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
  }

  /* Migration fails when maximum number of readers is reached.*/
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddSocketDataToFactory(&socket_factory_);
  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::RANDOM_BIND,
                                                 NetLog::Get(), NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  auto new_reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(new_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      /*report_ecn=*/true, net_log_with_source_);
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

  IPEndPoint local_address;
  new_reader->socket()->GetLocalAddress(&local_address);
  IPEndPoint peer_address;
  new_reader->socket()->GetPeerAddress(&peer_address);
  EXPECT_FALSE(session_->MigrateToSocket(
      ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
      std::move(new_reader), std::move(new_writer)));
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocketReadError) {
  MockQuicData quic_data(version_);
  socket_data_.reset();
  int packet_num = 1;
  int peer_packet_num = 1;

  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddRead(ASYNC, server_maker_.Packet(peer_packet_num++)
                               .AddNewConnectionIdFrame(cid_on_new_path,
                                                        /*sequence_number=*/1u,
                                                        /*retire_prior_to=*/0u)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_NETWORK_CHANGED);

  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make new connection ID available after handshake completion.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.Packet(packet_num++)
                          .AddAckFrame(/*first_received=*/1,
                                       /*largest_received=*/peer_packet_num - 1,
                                       /*smallest_received=*/1)
                          .AddPingFrame()
                          .Build());
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data2.AddRead(ASYNC, server_maker_.Packet(1).AddPingFrame().Build());
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data2.AddRead(ASYNC, ERR_NETWORK_CHANGED);
  quic_data2.AddSocketDataToFactory(&socket_factory_);

  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::RANDOM_BIND,
                                                 NetLog::Get(), NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  auto new_reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(new_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      /*report_ecn=*/true, net_log_with_source_);
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

  IPEndPoint local_address;
  new_reader->socket()->GetLocalAddress(&local_address);
  IPEndPoint peer_address;
  new_reader->socket()->GetPeerAddress(&peer_address);
  // Store old socket and migrate session.
  EXPECT_TRUE(session_->MigrateToSocket(
      ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
      std::move(new_reader), std::move(new_writer)));
  // Spin message loop to complete migration.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      quic::test::QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(
          session_->connection())
          ->IsSet());

  // Read error on old socket does not impact session.
  quic_data.Resume();
  EXPECT_TRUE(session_->connection()->connected());
  quic_data2.Resume();

  // Read error on new socket causes session close.
  EXPECT_TRUE(session_->connection()->connected());
  quic_data2.Resume();
  EXPECT_FALSE(session_->connection()->connected());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, RetransmittableOnWireTimeout) {
  migrate_session_early_v2_ = true;

  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());

  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1).AddAckFrame(1, packet_num - 1, 1).Build());

  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open a stream since the connection only sends PINGs to keep a
  // retransmittable packet on the wire if there's an open stream.
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));

  quic::QuicAlarm& alarm =
      quic::test::QuicConnectionPeer::GetPingAlarm(session_->connection());
  EXPECT_FALSE(alarm.IsSet());

  // Send PING, which will be ACKed by the server. After the ACK, there will be
  // no retransmittable packets on the wire, so the alarm should be set.
  session_->connection()->SendPing();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(alarm.IsSet());
  EXPECT_EQ(
      clock_.ApproximateNow() + quic::QuicTime::Delta::FromMilliseconds(200),
      alarm.deadline());

  // Advance clock and simulate the alarm firing. This should cause a PING to be
  // sent.
  clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(200));
  alarm_factory_.FireAlarm(&alarm);
  base::RunLoop().RunUntilIdle();

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1043531.
TEST_P(QuicChromiumClientSessionTest, ResetOnEmptyResponseHeaders) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      ASYNC,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_GENERAL_PROTOCOL_ERROR)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_GENERAL_PROTOCOL_ERROR)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  auto session_handle = session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  EXPECT_EQ(OK, session_handle->RequestStream(/*requires_confirmation=*/false,
                                              callback.callback(),
                                              TRAFFIC_ANNOTATION_FOR_TESTS));

  auto stream_handle = session_handle->ReleaseStream();
  EXPECT_TRUE(stream_handle->IsOpen());

  auto* stream = quic::test::QuicSessionPeer::GetOrCreateStream(
      session_.get(), stream_handle->id());

  const quic::QuicHeaderList empty_response_headers;
  static_cast<quic::QuicSpdyStream*>(stream)->OnStreamHeaderList(
      /* fin = */ false, /* frame_len = */ 0, empty_response_headers);

  // QuicSpdyStream::OnStreamHeaderList() calls
  // QuicChromiumClientStream::OnInitialHeadersComplete() with the empty
  // header list, and QuicChromiumClientStream signals an error.
  quiche::HttpHeaderBlock header_block;
  int rv = stream_handle->ReadInitialHeaders(&header_block,
                                             CompletionOnceCallback());
  EXPECT_THAT(rv, IsError(net::ERR_QUIC_PROTOCOL_ERROR));

  base::RunLoop().RunUntilIdle();
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// This test verifies that when handles::NetworkHandle is not supported and
// there is no network change, session reports to the connectivity monitor
// correctly on path degrading detection and recovery.
TEST_P(QuicChromiumClientSessionTest,
       DegradingWithoutNetworkChange_NoNetworkHandle) {
  // Add a connectivity monitor for testing.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  // Fire path degrading detection.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Fire again.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Close the session but keep the session around, the connectivity monitor
  // will not remove the tracking immediately.
  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Delete the session will remove the degrading count in connectivity
  // monitor.
  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

// This test verifies that when multi-port and port migration is enabled, path
// degrading won't trigger port migration.
TEST_P(QuicChromiumClientSessionTest, DegradingWithMultiPortEnabled) {
  // Default network is always set to handles::kInvalidNetworkHandle.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);
  allow_port_migration_ = true;
  auto options = config_.SendConnectionOptions();
  config_.SetClientConnectionOptions(quic::QuicTagVector{quic::kMPQC});
  config_.SetConnectionOptionsToSend(options);

  Initialize();
  EXPECT_TRUE(session_->connection()->multi_port_stats());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  EXPECT_EQ(
      UNKNOWN_CAUSE,
      QuicChromiumClientSessionPeer::GetCurrentMigrationCause(session_.get()));
}

// This test verifies that when the handles::NetworkHandle is not supported, and
// there are speculated network change reported via OnIPAddressChange, session
// still reports to the connectivity monitor correctly on path degrading
// detection and recovery.
TEST_P(QuicChromiumClientSessionTest, DegradingWithIPAddressChange) {
  // Default network is always set to handles::kInvalidNetworkHandle.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // When handles::NetworkHandle is not supported, network change is notified
  // via IP address change.
  connectivity_monitor_->OnIPAddressChanged();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // When handles::NetworkHandle is not supported and IP address changes,
  // session either goes away or gets closed. When it goes away,
  // reporting to connectivity monitor is disabled.
  connectivity_monitor_->OnSessionGoingAwayOnIPAddressChange(session_.get());

  // Even if session detects recovery or degradation, this session is no longer
  // on the default network and connectivity monitor will not update.
  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

// This test verifies that when handles::NetworkHandle is supported but
// migration is not supported and there's no network change, session reports to
// connectivity monitor correctly on path degrading detection or recovery.
// Default network change is currently reported with valid
// handles::NetworkHandles while session's current network interface is tracked
// by |default_network_|.
TEST_P(QuicChromiumClientSessionTest,
       DegradingOnDeafultNetwork_WithoutMigration) {
  default_network_ = kDefaultNetworkForTests;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());
  // Close the session but keep the session around, the connectivity monitor
  // should not remove the count immediately.
  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Delete the session will remove the degrading count in connectivity
  // monitor.
  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

// This test verifies that when handles::NetworkHandle is supported but
// migrations is not supported and there is network changes, session reports to
// the connectivity monitor correctly on path degrading detection or recovery.
TEST_P(QuicChromiumClientSessionTest,
       DegradingWithDeafultNetworkChange_WithoutMigration) {
  default_network_ = kDefaultNetworkForTests;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Simulate the default network change.
  connectivity_monitor_->OnDefaultNetworkUpdated(kNewNetworkForTests);
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
  session_->OnNetworkMadeDefault(kNewNetworkForTests);

  // Session stays on the old default network, and recovers.
  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Session degrades again on the old default.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Simulate that default network switches back to the old default.
  connectivity_monitor_->OnDefaultNetworkUpdated(kDefaultNetworkForTests);
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
  session_->OnNetworkMadeDefault(kDefaultNetworkForTests);

  // Session recovers again on the (old) default.
  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Session degrades again on the (old) default.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

TEST_P(QuicChromiumClientSessionTest, WriteErrorDuringCryptoConnect) {
  // Add a connectivity monitor for testing.
  default_network_ = kDefaultNetworkForTests;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData quic_data(version_);
  // Trigger a packet write error when sending packets in crypto connect.
  quic_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  ASSERT_THAT(session_->CryptoConnect(callback_.callback()),
              IsError(ERR_QUIC_HANDSHAKE_FAILED));
  // Verify error count is properly recorded.
  EXPECT_EQ(1u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_ADDRESS_UNREACHABLE));
  EXPECT_EQ(0u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_CONNECTION_RESET));

  // Simulate a default network change, write error stats should be reset.
  connectivity_monitor_->OnDefaultNetworkUpdated(kNewNetworkForTests);
  EXPECT_EQ(0u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_ADDRESS_UNREACHABLE));
}

TEST_P(QuicChromiumClientSessionTest, WriteErrorAfterHandshakeConfirmed) {
  // Add a connectivity monitor for testing.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  // When sending the PING packet, trigger a packet write error.
  quic_data.AddWrite(SYNCHRONOUS, ERR_CONNECTION_RESET);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->connection()->SendPing();

  // Verify error count is properly recorded.
  EXPECT_EQ(1u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_CONNECTION_RESET));
  EXPECT_EQ(0u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_ADDRESS_UNREACHABLE));

  connectivity_monitor_->OnIPAddressChanged();

  // If network handle is supported, IP Address change is a no-op. Otherwise it
  // clears all stats.
  size_t expected_error_count =
      NetworkChangeNotifier::AreNetworkHandlesSupported() ? 1u : 0u;
  EXPECT_EQ(
      expected_error_count,
      connectivity_monitor_->GetCountForWriteErrorCode(ERR_CONNECTION_RESET));
}

// Much like above, but checking that ECN marks are reported.
TEST_P(QuicChromiumClientSessionTest, ReportsReceivedEcn) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(net::features::kReportEcn);

  MockQuicData mock_quic_data(version_);
  int write_packet_num = 1, read_packet_num = 0;
  quic::QuicEcnCounts ecn(1, 0, 0);  // 1 ECT(0) packet received
  mock_quic_data.AddWrite(
      ASYNC, client_maker_.MakeInitialSettingsPacket(write_packet_num++));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.MakeInitialSettingsPacket(read_packet_num++));
  server_maker_.set_ecn_codepoint(quic::ECN_ECT0);
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                           .AddAckFrame(0, 1, 0, ecn)
                                           .Build());
  server_maker_.set_ecn_codepoint(quic::ECN_ECT1);
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  server_maker_.set_ecn_codepoint(quic::ECN_CE);
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  ecn.ect1 = 1;
  ecn.ce = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                           .AddAckFrame(0, 3, 0, ecn)
                                           .Build());
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, OnOriginFrame) {
  const std::string kExampleOrigin1 = "https://www.example.com";
  const std::string kExampleOrigin2 = "https://www.example.com:443";
  const std::string kExampleOrigin3 = "https://www.example.com:8443";
  const std::string kExampleOrigin4 = "http://www.example.com:8080";
  const std::string kInvalidOrigin1 = "https://www.example.com/";
  const std::string kInvalidOrigin2 = "www.example.com";

  GURL url1(base::StrCat({kExampleOrigin1, "/"}));
  url::SchemeHostPort origin1(url1);
  ASSERT_TRUE(origin1.IsValid());
  GURL url2(base::StrCat({kExampleOrigin2, "/"}));
  url::SchemeHostPort origin2(url2);
  ASSERT_TRUE(origin2.IsValid());
  GURL url3(base::StrCat({kExampleOrigin3, "/"}));
  url::SchemeHostPort origin3(url3);
  ASSERT_TRUE(origin3.IsValid());
  GURL url4(base::StrCat({kExampleOrigin4, "/"}));
  url::SchemeHostPort origin4(url4);
  ASSERT_TRUE(origin4.IsValid());

  quic::OriginFrame frame;

  Initialize();

  ASSERT_TRUE(session_->received_origins().empty());

  frame.origins.push_back(kExampleOrigin1);
  session_->OnOriginFrame(frame);
  EXPECT_EQ(1u, session_->received_origins().size());
  EXPECT_TRUE(session_->received_origins().count(origin1));
  EXPECT_TRUE(session_->received_origins().count(origin2));
  EXPECT_FALSE(session_->received_origins().count(origin3));
  EXPECT_FALSE(session_->received_origins().count(origin4));

  frame.origins.push_back(kExampleOrigin2);
  frame.origins.push_back(kInvalidOrigin1);
  frame.origins.push_back(kInvalidOrigin2);
  frame.origins.push_back(kExampleOrigin3);
  frame.origins.push_back(kExampleOrigin4);
  session_->OnOriginFrame(frame);
  EXPECT_EQ(3u, session_->received_origins().size());

  EXPECT_TRUE(session_->received_origins().count(origin1));
  EXPECT_TRUE(session_->received_origins().count(origin2));
  EXPECT_TRUE(session_->received_origins().count(origin3));
  EXPECT_TRUE(session_->received_origins().count(origin4));
}

}  // namespace
}  // namespace net::test
