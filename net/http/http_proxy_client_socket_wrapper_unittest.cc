// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_wrapper.h"

#include <cstdio>
#include <memory>

#include "build/build_config.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/quic_versions.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kProxyHost[] = "proxy.example.org";
const int kProxyPort = 6121;
const char kOriginHost[] = "www.google.org";
const int kOriginPort = 443;
const char kUserAgent[] = "Mozilla/1.0";

class MockSSLConfigService : public SSLConfigService {
 public:
  MockSSLConfigService() = default;
  ~MockSSLConfigService() override = default;

  void GetSSLConfig(SSLConfig* config) override { *config = config_; }

  bool CanShareConnectionWithClientCerts(
      const std::string& hostname) const override {
    return false;
  }

 private:
  SSLConfig config_;
};

};  // namespace

namespace test {

class HttpProxyClientSocketWrapperTest
    : public ::testing::TestWithParam<
          std::tuple<quic::QuicTransportVersion, bool>>,
      public WithScopedTaskEnvironment {
 protected:
  static const bool kFin = true;
  static const bool kIncludeVersion = true;
  static const bool kSendFeedback = true;

  HttpProxyClientSocketWrapperTest()
      : proxy_host_port_(kProxyHost, kProxyPort),
        endpoint_host_port_(kOriginHost, kOriginPort),
        ssl_config_service_(new MockSSLConfigService()),
        cert_verifier_(new MockCertVerifier()),
        channel_id_service_(
            new ChannelIDService(new DefaultChannelIDStore(nullptr))),
        cert_transparency_verifier_(new DoNothingCTVerifier()),
        random_generator_(0),
        quic_version_(std::get<0>(GetParam())),
        client_data_stream_id1_(
            quic::QuicUtils::GetHeadersStreamId(quic_version_) + 2),
        client_headers_include_h2_stream_dependency_(std::get<1>(GetParam())),
        client_maker_(quic_version_,
                      0,
                      &clock_,
                      kProxyHost,
                      quic::Perspective::IS_CLIENT,
                      client_headers_include_h2_stream_dependency_),
        server_maker_(quic_version_,
                      0,
                      &clock_,
                      kProxyHost,
                      quic::Perspective::IS_SERVER,
                      false),
        header_stream_offset_(0),
        response_offset_(0),
        store_server_configs_in_properties_(false),
        idle_connection_timeout_seconds_(kIdleConnectionTimeoutSeconds),
        reduced_ping_timeout_seconds_(quic::kPingTimeoutSecs),
        allow_server_migration_(false),
        race_cert_verification_(false),
        estimate_initial_rtt_(false),
        quic_stream_factory_(nullptr),
        privacy_mode_(PRIVACY_MODE_DISABLED),
        http_auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        client_socket_wrapper_(nullptr) {
    clock_.AdvanceTime(
        quic::QuicTime::Delta::FromSeconds(1));  // why is this here???
  }

  void Initialize() {
    DCHECK(!quic_stream_factory_);
    quic_stream_factory_.reset(new QuicStreamFactory(
        net_log_.net_log(), &host_resolver_, ssl_config_service_.get(),
        &socket_factory_, &http_server_properties_, cert_verifier_.get(),
        &ct_policy_enforcer_, &transport_security_state_,
        cert_transparency_verifier_.get(),
        /*SocketPerformanceWatcherFactory=*/nullptr,
        &crypto_client_stream_factory_, &random_generator_, &clock_,
        quic::kDefaultMaxPacketSize, /*user_agent_id=*/kUserAgent,
        store_server_configs_in_properties_,
        /*close_sessions_on_ip_change=*/true,
        /*goaway_sessions_on_ip_change=*/false,
        /*mark_quic_broken_when_network_blackholes=*/false,
        idle_connection_timeout_seconds_, reduced_ping_timeout_seconds_,
        /*max_time_before_crypto_handshake_seconds=*/
        quic::kMaxTimeForCryptoHandshakeSecs,
        /*max_idle_time_before_crypto_handshake_seconds=*/
        quic::kInitialIdleTimeoutSecs,
        /*migrate_sessions_on_network_change_v2=*/false,
        /*migrate_sessions_early_v2=*/false,
        /*retry_on_alternate_network_before_handshake=*/false,
        /*race_stale_dns_on_connection=*/false,
        /*go_away_on_path_degrading=*/false,
        base::TimeDelta::FromSeconds(kMaxTimeOnNonDefaultNetworkSecs),
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        allow_server_migration_, race_cert_verification_, estimate_initial_rtt_,
        client_headers_include_h2_stream_dependency_, connection_options_,
        client_connection_options_,
        /*enable_socket_recv_optimization=*/false));
  }

  void PopulateConnectRequestIR(spdy::SpdyHeaderBlock* block) {
    (*block)[":method"] = "CONNECT";
    (*block)[":authority"] = endpoint_host_port_.ToString();
    (*block)["user-agent"] = kUserAgent;
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructSettingsPacket(
      quic::QuicPacketNumber packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number,
                                                   &header_stream_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructConnectRequestPacket(
      quic::QuicPacketNumber packet_number) {
    spdy::SpdyHeaderBlock block;
    PopulateConnectRequestIR(&block);
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, client_data_stream_id1_, kIncludeVersion, !kFin,
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
        std::move(block), 0, nullptr, &header_stream_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerConnectReplyPacket(
      quic::QuicPacketNumber packet_number,
      bool fin) {
    spdy::SpdyHeaderBlock block;
    block[":status"] = "200";

    return server_maker_.MakeResponseHeadersPacket(
        packet_number, client_data_stream_id1_, !kIncludeVersion, fin,
        std::move(block), nullptr, &response_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicPacketNumber largest_received,
      quic::QuicPacketNumber smallest_received,
      quic::QuicPacketNumber least_unacked) {
    return client_maker_.MakeAckAndRstPacket(
        packet_number, !kIncludeVersion, client_data_stream_id1_, error_code,
        largest_received, smallest_received, least_unacked, kSendFeedback);
  }

  static ProofVerifyDetailsChromium DefaultProofVerifyDetails() {
    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());
    ProofVerifyDetailsChromium verify_details;
    verify_details.cert_verify_result.verified_cert = test_cert;
    verify_details.cert_verify_result.is_issued_by_known_root = true;
    return verify_details;
  }

  HostPortPair proxy_host_port_;
  HostPortPair endpoint_host_port_;

  quic::MockClock clock_;
  MockQuicData mock_quic_data_;

  // QuicStreamFactory environment
  NetLogWithSource net_log_;
  MockHostResolver host_resolver_;
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  MockTaggingClientSocketFactory socket_factory_;
  HttpServerPropertiesImpl http_server_properties_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  std::unique_ptr<ChannelIDService> channel_id_service_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<DoNothingCTVerifier> cert_transparency_verifier_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::test::MockRandom random_generator_;

  const quic::QuicTransportVersion quic_version_;
  const quic::QuicStreamId client_data_stream_id1_;
  const bool client_headers_include_h2_stream_dependency_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  quic::QuicStreamOffset header_stream_offset_;
  quic::QuicStreamOffset response_offset_;

  // Variables to configure QuicStreamFactory.
  bool store_server_configs_in_properties_;
  int idle_connection_timeout_seconds_;
  int reduced_ping_timeout_seconds_;
  bool allow_server_migration_;
  bool race_cert_verification_;
  bool estimate_initial_rtt_;
  quic::QuicTagVector connection_options_;
  quic::QuicTagVector client_connection_options_;

  std::unique_ptr<QuicStreamFactory> quic_stream_factory_;

  // HttpProxyClientSocketWrapper environment
  PrivacyMode privacy_mode_;
  HttpAuthCache http_auth_cache_;
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory_;

  std::unique_ptr<HttpProxyClientSocketWrapper> client_socket_wrapper_;
};

TEST_P(HttpProxyClientSocketWrapperTest, QuicProxy) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndConnectionClosePacket(
                       4, false, quic::QuicTime::Delta::FromMilliseconds(0), 1,
                       1, 1, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  mock_quic_data_.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<TransportSocketParams> transport_params =
      new TransportSocketParams(
          proxy_host_port_, false, OnHostResolutionCallback(),
          TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT);

  scoped_refptr<SSLSocketParams> ssl_params = new SSLSocketParams(
      transport_params, nullptr, nullptr, proxy_host_port_, SSLConfig(),
      privacy_mode_, false /* ignore_certificate_errors */);
  transport_params = nullptr;

  client_socket_wrapper_.reset(new HttpProxyClientSocketWrapper(
      /*group_name=*/std::string(), /*requiest_priority=*/DEFAULT_PRIORITY,
      /*socket_tag=*/SocketTag(),
      /*respect_limits=*/ClientSocketPool::RespectLimits::DISABLED,
      /*connect_timeout_duration=*/base::TimeDelta::FromHours(1),
      /*proxy_negotiation_timeout_duration=*/base::TimeDelta::FromHours(1),
      /*transport_pool=*/nullptr, /*ssl_pool=*/nullptr,
      /*transport_params=*/nullptr, ssl_params, quic_version_, kUserAgent,
      endpoint_host_port_, &http_auth_cache_, http_auth_handler_factory_.get(),
      /*spdy_session_pool=*/nullptr, quic_stream_factory_.get(),
      /*is_trusted_proxy=*/false, /*tunnel=*/true, TRAFFIC_ANNOTATION_FOR_TESTS,
      net_log_));

  TestCompletionCallback callback;
  client_socket_wrapper_->Connect(callback.callback());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  client_socket_wrapper_.reset();
  EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
}

// Test that the SocketTag is appropriately applied to the underlying socket
// for QUIC proxies.
#if defined(OS_ANDROID)
TEST_P(HttpProxyClientSocketWrapperTest, QuicProxySocketTag) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndConnectionClosePacket(
                       4, false, quic::QuicTime::Delta::FromMilliseconds(0), 1,
                       1, 1, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  mock_quic_data_.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<TransportSocketParams> transport_params =
      new TransportSocketParams(
          proxy_host_port_, false, OnHostResolutionCallback(),
          TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT);

  scoped_refptr<SSLSocketParams> ssl_params = new SSLSocketParams(
      transport_params, nullptr, nullptr, proxy_host_port_, SSLConfig(),
      privacy_mode_, false /* ignore_certificate_errors */);
  transport_params = nullptr;
  SocketTag tag(getuid(), 0x87654321);

  client_socket_wrapper_.reset(new HttpProxyClientSocketWrapper(
      /*group_name=*/std::string(), /*requiest_priority=*/DEFAULT_PRIORITY,
      /*socket_tag=*/tag,
      /*respect_limits=*/ClientSocketPool::RespectLimits::DISABLED,
      /*connect_timeout_duration=*/base::TimeDelta::FromHours(1),
      /*proxy_negotiation_timeout_duration=*/base::TimeDelta::FromHours(1),
      /*transport_pool=*/nullptr, /*ssl_pool=*/nullptr,
      /*transport_params=*/nullptr, ssl_params, quic_version_, kUserAgent,
      endpoint_host_port_, &http_auth_cache_, http_auth_handler_factory_.get(),
      /*spdy_session_pool=*/nullptr, quic_stream_factory_.get(),
      /*is_trusted_proxy=*/false, /*tunnel=*/true, TRAFFIC_ANNOTATION_FOR_TESTS,
      net_log_));

  TestCompletionCallback callback;
  client_socket_wrapper_->Connect(callback.callback());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(socket_factory_.GetLastProducedUDPSocket()->tag(), tag);
  EXPECT_TRUE(socket_factory_.GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());

  client_socket_wrapper_.reset();
  EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
}
#endif

INSTANTIATE_TEST_CASE_P(
    VersionIncludeStreamDependencySequence,
    HttpProxyClientSocketWrapperTest,
    ::testing::Combine(
        ::testing::ValuesIn(quic::AllSupportedTransportVersions()),
        ::testing::Bool()));

};  // namespace test
};  // namespace net
