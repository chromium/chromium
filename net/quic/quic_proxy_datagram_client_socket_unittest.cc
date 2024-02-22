// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_datagram_client_socket.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/session_usage.h"
#include "net/base/test_proxy_delegate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_headers.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net::test {

namespace {

constexpr char kOriginHost[] = "www.google.com";
constexpr int kOriginPort = 443;
constexpr char kProxyHost[] = "myproxy.example";
constexpr int kProxyPort = 6121;
constexpr char kUserAgent[] = "Mozilla/1.0";
}  // anonymous namespace

// TODO(crbug.com/1524411): Refactor into test base superclass for quic proxy
// client socket unit tests.
class QuicProxyDatagramClientSocketTest
    : public ::testing::TestWithParam<quic::ParsedQuicVersion>,
      public WithTaskEnvironment {
 public:
  QuicProxyDatagramClientSocketTest(const QuicProxyDatagramClientSocketTest&) =
      delete;
  QuicProxyDatagramClientSocketTest& operator=(
      const QuicProxyDatagramClientSocketTest&) = delete;

 protected:
  static const bool kFin = true;

  QuicProxyDatagramClientSocketTest()
      : version_(GetParam()),
        client_data_stream_id1_(quic::QuicUtils::GetFirstBidirectionalStreamId(
            version_.transport_version,
            quic::Perspective::IS_CLIENT)),
        mock_quic_data_(version_),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        connection_id_(quic::test::TestConnectionId(2)),
        client_maker_(version_,
                      connection_id_,
                      &clock_,
                      kProxyHost,
                      quic::Perspective::IS_CLIENT),
        server_maker_(version_,
                      connection_id_,
                      &clock_,
                      kProxyHost,
                      quic::Perspective::IS_SERVER,
                      false),
        user_agent_(kUserAgent),
        proxy_endpoint_(url::kHttpsScheme, kProxyHost, kProxyPort),
        destination_endpoint_(url::kHttpsScheme, kOriginHost, kOriginPort),
        http_auth_cache_(
            false /* key_server_entries_by_network_anonymization_key */),
        host_resolver_(std::make_unique<MockCachingHostResolver>()),
        http_auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    IPAddress ip(192, 0, 2, 33);
    proxy_peer_addr_ = IPEndPoint(ip, 443);
    clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    quic::QuicEnableVersion(version_);
  }

  void SetUp() override {}

  void TearDown() override {
    sock_.reset();
    EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
  }

  void Initialize() {
    auto socket = std::make_unique<MockUDPClientSocket>(
        mock_quic_data_.InitializeAndGetSequencedSocketData(), NetLog::Get());
    socket->Connect(proxy_peer_addr_);
    runner_ = base::MakeRefCounted<TestTaskRunner>(&clock_);
    send_algorithm_ = new quic::test::MockSendAlgorithm();
    EXPECT_CALL(*send_algorithm_, InRecovery()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
        .WillRepeatedly(Return(quic::kMaxOutgoingPacketSize));
    EXPECT_CALL(*send_algorithm_, PacingRate(_))
        .WillRepeatedly(Return(quic::QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .WillRepeatedly(Return(quic::QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, GetCongestionControlType())
        .Times(AnyNumber());
    helper_ = std::make_unique<QuicChromiumConnectionHelper>(
        &clock_, &random_generator_);
    alarm_factory_ =
        std::make_unique<QuicChromiumAlarmFactory>(runner_.get(), &clock_);

    QuicChromiumPacketWriter* writer = new QuicChromiumPacketWriter(
        socket.get(), base::SingleThreadTaskRunner::GetCurrentDefault().get());
    quic::QuicConnection* connection = new quic::QuicConnection(
        connection_id_, quic::QuicSocketAddress(),
        net::ToQuicSocketAddress(proxy_peer_addr_), helper_.get(),
        alarm_factory_.get(), writer, true /* owns_writer */,
        quic::Perspective::IS_CLIENT, quic::test::SupportedVersions(version_),
        connection_id_generator_);
    connection->set_visitor(&visitor_);
    quic::test::QuicConnectionPeer::SetSendAlgorithm(connection,
                                                     send_algorithm_);

    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());

    verify_details_.cert_verify_result.verified_cert = test_cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);

    base::TimeTicks dns_end = base::TimeTicks::Now();
    base::TimeTicks dns_start = dns_end - base::Milliseconds(1);

    session_ = std::make_unique<QuicChromiumClientSession>(
        connection, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        &transport_security_state_, &ssl_config_service_,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)),
        QuicSessionKey("mail.example.org", 80, PRIVACY_MODE_DISABLED,
                       ProxyChain::Direct(), SessionUsage::kDestination,
                       SocketTag(), NetworkAnonymizationKey(),
                       SecureDnsPolicy::kAllow,
                       /*require_dns_https_alpn=*/false),
        /*require_confirmation=*/false,
        /*migrate_session_early_v2=*/false,
        /*migrate_session_on_network_change_v2=*/false,
        /*default_network=*/handles::kInvalidNetworkHandle,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/true, /*allow_port_migration=*/false,
        kDefaultIdleSessionMigrationPeriod, /*multi_port_probing_interval=*/0,
        kMaxTimeOnNonDefaultNetwork,
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        /*cert_verify_flags=*/0, quic::test::DefaultQuicConfig(),
        std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config_),
        dns_start, dns_end, base::DefaultTickClock::GetInstance(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*socket_performance_watcher=*/nullptr, HostResolverEndpointResult(),
        NetLog::Get());

    writer->set_delegate(session_.get());

    session_->Initialize();

    // Blackhole QPACK decoder stream instead of constructing mock writes.
    session_->qpack_decoder()->set_qpack_stream_sender_delegate(
        &noop_qpack_stream_sender_delegate_);

    TestCompletionCallback callback;
    EXPECT_THAT(session_->CryptoConnect(callback.callback()), IsOk());
    EXPECT_TRUE(session_->OneRttKeysAvailable());

    session_handle_ = session_->CreateHandle(
        url::SchemeHostPort(url::kHttpsScheme, "mail.example.org", 80));
    EXPECT_THAT(session_handle_->RequestStream(true, callback.callback(),
                                               TRAFFIC_ANNOTATION_FOR_TESTS),
                IsOk());
    stream_handle_ = session_handle_->ReleaseStream();
    EXPECT_TRUE(stream_handle_->IsOpen());

    sock_ = std::make_unique<QuicProxyDatagramClientSocket>(
        destination_endpoint_.GetURL(), user_agent_,
        NetLogWithSource::Make(NetLogSourceType::NONE));

    session_->StartReading();
  }

  void PopulateConnectRequestIR(spdy::Http2HeaderBlock* block) {
    DCHECK(destination_endpoint_.scheme() == url::kHttpsScheme);

    std::string host = destination_endpoint_.host();
    uint16_t port = destination_endpoint_.port();

    (*block)[":method"] = "CONNECT";
    (*block)[":protocol"] = "connect-udp";
    (*block)[":scheme"] = destination_endpoint_.scheme();
    // Port is removed if 443 since that is the default port number for HTTPS.
    (*block)[":authority"] =
        port != 443 ? base::StrCat({host, ":", base::NumberToString(port)})
                    : host;
    (*block)[":path"] = "/";
    (*block)["capsule-protocol"] = "?1";
  }

  // Helper functions for constructing packets sent by the client

  std::unique_ptr<quic::QuicReceivedPacket> ConstructSettingsPacket(
      uint64_t packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received) {
    return client_maker_.MakeAckAndRstPacket(
        packet_number, client_data_stream_id1_, error_code, largest_received,
        smallest_received,
        /*include_stop_sending_if_v99=*/true);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructConnectRequestPacket(
      uint64_t packet_number,
      RequestPriority request_priority = LOWEST) {
    spdy::Http2HeaderBlock block;
    PopulateConnectRequestIR(&block);
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, client_data_stream_id1_, !kFin,
        ConvertRequestPriorityToQuicPriority(request_priority),
        std::move(block), nullptr);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerConnectReplyPacket(
      uint64_t packet_number,
      bool fin,
      size_t* header_length = nullptr) {
    spdy::Http2HeaderBlock block;
    block[":status"] = "200";

    return server_maker_.MakeResponseHeadersPacket(
        packet_number, client_data_stream_id1_, fin, std::move(block),
        header_length);
  }

  void AssertConnectSucceeds() {
    TestCompletionCallback callback;
    ASSERT_THAT(
        sock_->ConnectViaStream(local_addr_, proxy_peer_addr_,
                                std::move(stream_handle_), callback.callback()),
        IsError(ERR_IO_PENDING));
    ASSERT_THAT(callback.WaitForResult(), IsOk());
  }

  void AssertConnectFails(int result) {
    TestCompletionCallback callback;
    ASSERT_THAT(
        sock_->ConnectViaStream(local_addr_, proxy_peer_addr_,
                                std::move(stream_handle_), callback.callback()),
        IsError(ERR_IO_PENDING));
    ASSERT_EQ(result, callback.WaitForResult());
  }

  RecordingNetLogObserver net_log_observer_;
  quic::test::QuicFlagSaver saver_;
  const quic::ParsedQuicVersion version_;
  const quic::QuicStreamId client_data_stream_id1_;

  // order of destruction of these members matter
  quic::MockClock clock_;
  MockQuicData mock_quic_data_;
  std::unique_ptr<QuicChromiumConnectionHelper> helper_;
  std::unique_ptr<QuicChromiumClientSession> session_;
  std::unique_ptr<QuicChromiumClientSession::Handle> session_handle_;
  std::unique_ptr<QuicChromiumClientStream::Handle> stream_handle_;
  std::unique_ptr<QuicProxyDatagramClientSocket> sock_;
  std::unique_ptr<TestProxyDelegate> proxy_delegate_;

  raw_ptr<quic::test::MockSendAlgorithm> send_algorithm_;
  scoped_refptr<TestTaskRunner> runner_;

  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  testing::StrictMock<quic::test::MockQuicConnectionVisitor> visitor_;
  TransportSecurityState transport_security_state_;
  SSLConfigServiceDefaults ssl_config_service_;
  quic::QuicCryptoClientConfig crypto_config_;

  const quic::QuicConnectionId connection_id_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  IPEndPoint proxy_peer_addr_;
  IPEndPoint local_addr_;
  quic::test::MockRandom random_generator_{0};
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::test::MockConnectionIdGenerator connection_id_generator_;

  std::string user_agent_;
  url::SchemeHostPort proxy_endpoint_;
  url::SchemeHostPort destination_endpoint_;
  HttpAuthCache http_auth_cache_;
  std::unique_ptr<MockHostResolverBase> host_resolver_;
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory_;

  TestCompletionCallback read_callback_;
  scoped_refptr<IOBuffer> read_buf_;

  TestCompletionCallback write_callback_;

  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
};

TEST_P(QuicProxyDatagramClientSocketTest, ConnectSendsCorrectRequest) {
  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  Initialize();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
}

TEST_P(QuicProxyDatagramClientSocketTest, ConnectFails) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  Initialize();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_QUIC_PROTOCOL_ERROR);

  ASSERT_FALSE(sock_->IsConnected());
}

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicProxyDatagramClientSocketTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

}  // namespace net::test
