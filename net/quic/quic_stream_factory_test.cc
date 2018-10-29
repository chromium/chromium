// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <memory>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/properties_based_quic_server_info.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_stream_factory_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace net {

namespace {

class MockSSLConfigService : public SSLConfigService {
 public:
  MockSSLConfigService() {}
  ~MockSSLConfigService() override {}

  void GetSSLConfig(SSLConfig* config) override { *config = config_; }

  bool CanShareConnectionWithClientCerts(
      const std::string& hostname) const override {
    return false;
  }

 private:
  SSLConfig config_;
};

}  // namespace

namespace test {

namespace {

enum DestinationType {
  // In pooling tests with two requests for different origins to the same
  // destination, the destination should be
  SAME_AS_FIRST,   // the same as the first origin,
  SAME_AS_SECOND,  // the same as the second origin, or
  DIFFERENT,       // different from both.
};

const char kDefaultServerHostName[] = "www.example.org";
const char kServer2HostName[] = "mail.example.org";
const char kDifferentHostname[] = "different.example.com";
const int kDefaultServerPort = 443;
const char kDefaultUrl[] = "https://www.example.org/";
const char kServer2Url[] = "https://mail.example.org/";
const char kServer3Url[] = "https://docs.example.org/";
const char kServer4Url[] = "https://images.example.org/";
const int kDefaultRTTMilliSecs = 300;
const size_t kMinRetryTimeForDefaultNetworkSecs = 1;
const size_t kWaitTimeForNewNetworkSecs = 10;
const IPAddress kCachedIPAddress = IPAddress(192, 168, 0, 2);
const char kNonCachedIPAddress[] = "192.168.0.1";

// Run QuicStreamFactoryTest instances with all value combinations of version
// and enable_connection_racting.
struct TestParams {
  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ version: " << QuicVersionToString(p.version)
       << ", client_headers_include_h2_stream_dependency: "
       << p.client_headers_include_h2_stream_dependency << " }";
    return os;
  }

  quic::QuicTransportVersion version;
  bool client_headers_include_h2_stream_dependency;
};

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::QuicTransportVersionVector all_supported_versions =
      quic::AllSupportedTransportVersions();
  for (const auto& version : all_supported_versions) {
    params.push_back(TestParams{version, false});
    params.push_back(TestParams{version, true});
  }
  return params;
}

// Run QuicStreamFactoryWithDestinationTest instances with all value
// combinations of version, enable_connection_racting, and destination_type.
struct PoolingTestParams {
  friend std::ostream& operator<<(std::ostream& os,
                                  const PoolingTestParams& p) {
    os << "{ version: " << QuicVersionToString(p.version)
       << ", destination_type: ";
    switch (p.destination_type) {
      case SAME_AS_FIRST:
        os << "SAME_AS_FIRST";
        break;
      case SAME_AS_SECOND:
        os << "SAME_AS_SECOND";
        break;
      case DIFFERENT:
        os << "DIFFERENT";
        break;
    }
    os << ", client_headers_include_h2_stream_dependency: "
       << p.client_headers_include_h2_stream_dependency;
    os << " }";
    return os;
  }

  quic::QuicTransportVersion version;
  DestinationType destination_type;
  bool client_headers_include_h2_stream_dependency;
};

std::vector<PoolingTestParams> GetPoolingTestParams() {
  std::vector<PoolingTestParams> params;
  quic::QuicTransportVersionVector all_supported_versions =
      quic::AllSupportedTransportVersions();
  for (const quic::QuicTransportVersion version : all_supported_versions) {
    params.push_back(PoolingTestParams{version, SAME_AS_FIRST, false});
    params.push_back(PoolingTestParams{version, SAME_AS_FIRST, true});
    params.push_back(PoolingTestParams{version, SAME_AS_SECOND, false});
    params.push_back(PoolingTestParams{version, SAME_AS_SECOND, true});
    params.push_back(PoolingTestParams{version, DIFFERENT, false});
    params.push_back(PoolingTestParams{version, DIFFERENT, true});
  }
  return params;
}

}  // namespace

class QuicHttpStreamPeer {
 public:
  static QuicChromiumClientSession::Handle* GetSessionHandle(
      HttpStream* stream) {
    return static_cast<QuicHttpStream*>(stream)->quic_session();
  }
};

// TestConnectionMigrationSocketFactory will vend sockets with incremental port
// number.
class TestConnectionMigrationSocketFactory : public MockClientSocketFactory {
 public:
  TestConnectionMigrationSocketFactory() : next_source_port_num_(1u) {}
  ~TestConnectionMigrationSocketFactory() override {}

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    SocketDataProvider* data_provider = mock_data().GetNext();
    std::unique_ptr<MockUDPClientSocket> socket(
        new MockUDPClientSocket(data_provider, net_log));
    socket->set_source_port(next_source_port_num_++);
    return std::move(socket);
  }

 private:
  uint16_t next_source_port_num_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionMigrationSocketFactory);
};

class QuicStreamFactoryTestBase : public WithScopedTaskEnvironment {
 protected:
  QuicStreamFactoryTestBase(quic::QuicTransportVersion version,
                            bool client_headers_include_h2_stream_dependency)
      : host_resolver_(new MockHostResolver),
        ssl_config_service_(new MockSSLConfigService),
        socket_factory_(new MockClientSocketFactory),
        random_generator_(0),
        runner_(new TestTaskRunner(&clock_)),
        version_(version),
        client_headers_include_h2_stream_dependency_(
            client_headers_include_h2_stream_dependency),
        client_maker_(version_,
                      0,
                      &clock_,
                      kDefaultServerHostName,
                      quic::Perspective::IS_CLIENT,
                      client_headers_include_h2_stream_dependency_),
        server_maker_(version_,
                      0,
                      &clock_,
                      kDefaultServerHostName,
                      quic::Perspective::IS_SERVER,
                      false),
        cert_verifier_(std::make_unique<MockCertVerifier>()),
        cert_transparency_verifier_(std::make_unique<DoNothingCTVerifier>()),
        scoped_mock_network_change_notifier_(nullptr),
        factory_(nullptr),
        host_port_pair_(kDefaultServerHostName, kDefaultServerPort),
        url_(kDefaultUrl),
        url2_(kServer2Url),
        url3_(kServer3Url),
        url4_(kServer4Url),
        privacy_mode_(PRIVACY_MODE_DISABLED),
        failed_on_default_network_callback_(base::BindRepeating(
            &QuicStreamFactoryTestBase::OnFailedOnDefaultNetwork,
            base::Unretained(this))),
        failed_on_default_network_(false),
        store_server_configs_in_properties_(false),
        close_sessions_on_ip_change_(false),
        goaway_sessions_on_ip_change_(false),
        idle_connection_timeout_seconds_(kIdleConnectionTimeoutSeconds),
        reduced_ping_timeout_seconds_(quic::kPingTimeoutSecs),
        max_time_before_crypto_handshake_seconds_(
            quic::kMaxTimeForCryptoHandshakeSecs),
        max_idle_time_before_crypto_handshake_seconds_(
            quic::kInitialIdleTimeoutSecs),
        migrate_sessions_on_network_change_v2_(false),
        migrate_sessions_early_v2_(false),
        retry_on_alternate_network_before_handshake_(false),
        race_stale_dns_on_connection_(false),
        go_away_on_path_degrading_(false),
        allow_server_migration_(false),
        race_cert_verification_(false),
        estimate_initial_rtt_(false) {
    clock_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void Initialize() {
    DCHECK(!factory_);
    factory_.reset(new QuicStreamFactory(
        net_log_.net_log(), host_resolver_.get(), ssl_config_service_.get(),
        socket_factory_.get(), &http_server_properties_, cert_verifier_.get(),
        &ct_policy_enforcer_, &transport_security_state_,
        cert_transparency_verifier_.get(),
        /*SocketPerformanceWatcherFactory*/ nullptr,
        &crypto_client_stream_factory_, &random_generator_, &clock_,
        quic::kDefaultMaxPacketSize, string(),
        store_server_configs_in_properties_, close_sessions_on_ip_change_,
        goaway_sessions_on_ip_change_,
        /*mark_quic_broken_when_network_blackholes*/ false,
        idle_connection_timeout_seconds_, reduced_ping_timeout_seconds_,
        max_time_before_crypto_handshake_seconds_,
        max_idle_time_before_crypto_handshake_seconds_,
        migrate_sessions_on_network_change_v2_, migrate_sessions_early_v2_,
        retry_on_alternate_network_before_handshake_,
        race_stale_dns_on_connection_, go_away_on_path_degrading_,
        base::TimeDelta::FromSeconds(kMaxTimeOnNonDefaultNetworkSecs),
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        allow_server_migration_, race_cert_verification_, estimate_initial_rtt_,
        client_headers_include_h2_stream_dependency_, connection_options_,
        client_connection_options_,
        /*enable_socket_recv_optimization*/ false));
  }

  void InitializeConnectionMigrationV2Test(
      NetworkChangeNotifier::NetworkList connected_networks) {
    scoped_mock_network_change_notifier_.reset(
        new ScopedMockNetworkChangeNotifier());
    MockNetworkChangeNotifier* mock_ncn =
        scoped_mock_network_change_notifier_->mock_network_change_notifier();
    mock_ncn->ForceNetworkHandlesSupported();
    mock_ncn->SetConnectedNetworksList(connected_networks);
    migrate_sessions_on_network_change_v2_ = true;
    migrate_sessions_early_v2_ = true;
    retry_on_alternate_network_before_handshake_ = true;
    socket_factory_.reset(new TestConnectionMigrationSocketFactory);
    Initialize();
  }

  std::unique_ptr<HttpStream> CreateStream(QuicStreamRequest* request) {
    std::unique_ptr<QuicChromiumClientSession::Handle> session =
        request->ReleaseSessionHandle();
    if (!session || !session->IsConnected())
      return nullptr;

    return std::make_unique<QuicHttpStream>(std::move(session));
  }

  bool HasActiveSession(const HostPortPair& host_port_pair) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::HasActiveSession(factory_.get(), server_id);
  }

  bool HasLiveSession(const HostPortPair& host_port_pair) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::HasLiveSession(factory_.get(), host_port_pair,
                                                 server_id);
  }

  bool HasActiveJob(const HostPortPair& host_port_pair,
                    const PrivacyMode privacy_mode) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 privacy_mode == PRIVACY_MODE_ENABLED);
    return QuicStreamFactoryPeer::HasActiveJob(factory_.get(), server_id);
  }

  bool HasActiveCertVerifierJob(const quic::QuicServerId& server_id) {
    return QuicStreamFactoryPeer::HasActiveCertVerifierJob(factory_.get(),
                                                           server_id);
  }

  // Get the pending, not activated session, if there is only one session alive.
  QuicChromiumClientSession* GetPendingSession(
      const HostPortPair& host_port_pair) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::GetPendingSession(factory_.get(), server_id,
                                                    host_port_pair);
  }

  QuicChromiumClientSession* GetActiveSession(
      const HostPortPair& host_port_pair) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::GetActiveSession(factory_.get(), server_id);
  }

  int GetSourcePortForNewSession(const HostPortPair& destination) {
    return GetSourcePortForNewSessionInner(destination, false);
  }

  int GetSourcePortForNewSessionAndGoAway(const HostPortPair& destination) {
    return GetSourcePortForNewSessionInner(destination, true);
  }

  int GetSourcePortForNewSessionInner(const HostPortPair& destination,
                                      bool goaway_received) {
    // Should only be called if there is no active session for this destination.
    EXPECT_FALSE(HasActiveSession(destination));
    size_t socket_count = socket_factory_->udp_client_socket_ports().size();

    MockQuicData socket_data;
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    GURL url("https://" + destination.host() + "/");
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
            /*cert_verify_flags=*/0, url, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsOk());
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());
    stream.reset();

    QuicChromiumClientSession* session = GetActiveSession(destination);

    if (socket_count + 1 != socket_factory_->udp_client_socket_ports().size()) {
      ADD_FAILURE();
      return 0;
    }

    if (goaway_received) {
      quic::QuicGoAwayFrame goaway(quic::kInvalidControlFrameId,
                                   quic::QUIC_NO_ERROR, 1, "");
      session->connection()->OnGoAwayFrame(goaway);
    }

    factory_->OnSessionClosed(session);
    EXPECT_FALSE(HasActiveSession(destination));
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
    return socket_factory_->udp_client_socket_ports()[socket_count];
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientConnectionClosePacket(quic::QuicPacketNumber num) {
    return client_maker_.MakeConnectionClosePacket(
        num, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!");
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientRstPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicRstStreamErrorCode error_code) {
    quic::QuicStreamId stream_id = GetNthClientInitiatedStreamId(0);
    return client_maker_.MakeRstPacket(packet_number, true, stream_id,
                                       error_code);
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

  void NotifyIPAddressChanged() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Spin the message loop so the notification is delivered.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructGetRequestPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin) {
    spdy::SpdyHeaderBlock headers =
        client_maker_.GetRequestHeaders("GET", "https", "/");
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, stream_id, should_include_version, fin, priority,
        std::move(headers), 0, &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructGetRequestPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamId stream_id,
      quic::QuicStreamId parent_stream_id,
      bool should_include_version,
      bool fin,
      quic::QuicStreamOffset* offset) {
    spdy::SpdyHeaderBlock headers =
        client_maker_.GetRequestHeaders("GET", "https", "/");
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, stream_id, should_include_version, fin, priority,
        std::move(headers), parent_stream_id, &spdy_headers_frame_len, offset);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructGetRequestPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      quic::QuicStreamOffset* offset) {
    return ConstructGetRequestPacket(packet_number, stream_id,
                                     /*parent_stream_id=*/0,
                                     should_include_version, fin, offset);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructOkResponsePacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin) {
    spdy::SpdyHeaderBlock headers = server_maker_.GetResponseHeaders("200 OK");
    size_t spdy_headers_frame_len;
    return server_maker_.MakeResponseHeadersPacket(
        packet_number, stream_id, should_include_version, fin,
        std::move(headers), &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket() {
    return client_maker_.MakeInitialSettingsPacket(1, nullptr);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamOffset* offset) {
    return client_maker_.MakeInitialSettingsPacket(packet_number, offset);
  }

  // Helper method for server migration tests.
  void VerifyServerMigration(const quic::QuicConfig& config,
                             IPEndPoint expected_address) {
    allow_server_migration_ = true;
    Initialize();

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.SetConfig(config);

    // Set up first socket data provider.
    MockQuicData socket_data1;
    socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data1.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after
    // migration.
    MockQuicData socket_data2;
    socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data2.AddWrite(
        SYNCHRONOUS, client_maker_.MakePingPacket(2, /*include_version=*/true));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(3, true, GetNthClientInitiatedStreamId(0),
                                    quic::QUIC_STREAM_CANCELLED));
    socket_data2.AddSocketDataToFactory(socket_factory_.get());

    // Create request and QuicHttpStream.
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                  SocketTag(),
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));
    EXPECT_EQ(OK, callback_.WaitForResult());

    // Run QuicChromiumClientSession::WriteToNewSocket()
    // posted by QuicChromiumClientSession::MigrateToSocket().
    base::RunLoop().RunUntilIdle();

    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());

    // Cause QUIC stream to be created.
    HttpRequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL("https://www.example.org/");
    request_info.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_EQ(OK,
              stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                       net_log_, CompletionOnceCallback()));
    // Ensure that session is alive and active.
    QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(host_port_pair_));

    IPEndPoint actual_address;
    session->GetDefaultSocket()->GetPeerAddress(&actual_address);
    EXPECT_EQ(actual_address, expected_address);
    DVLOG(1) << "Socket connected to: " << actual_address.address().ToString()
             << " " << actual_address.port();
    DVLOG(1) << "Expected address: " << expected_address.address().ToString()
             << " " << expected_address.port();

    stream.reset();
    EXPECT_TRUE(socket_data1.AllReadDataConsumed());
    EXPECT_TRUE(socket_data2.AllReadDataConsumed());
    EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  }

  // Verifies that the QUIC stream factory is initialized correctly.
  void VerifyInitialization() {
    store_server_configs_in_properties_ = true;
    idle_connection_timeout_seconds_ = 500;
    Initialize();
    factory_->set_require_confirmation(false);
    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::ZERO_RTT);
    const quic::QuicConfig* config =
        QuicStreamFactoryPeer::GetConfig(factory_.get());
    EXPECT_EQ(500, config->IdleNetworkTimeout().ToSeconds());

    QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

    const AlternativeService alternative_service1(
        kProtoQUIC, host_port_pair_.host(), host_port_pair_.port());
    AlternativeServiceInfoVector alternative_service_info_vector;
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    alternative_service_info_vector.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service1, expiration, {version_}));
    http_server_properties_.SetAlternativeServices(
        url::SchemeHostPort(url_), alternative_service_info_vector);

    HostPortPair host_port_pair2(kServer2HostName, kDefaultServerPort);
    url::SchemeHostPort server2("https", kServer2HostName, kDefaultServerPort);
    const AlternativeService alternative_service2(
        kProtoQUIC, host_port_pair2.host(), host_port_pair2.port());
    AlternativeServiceInfoVector alternative_service_info_vector2;
    alternative_service_info_vector2.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service2, expiration, {version_}));

    http_server_properties_.SetAlternativeServices(
        server2, alternative_service_info_vector2);
    // Verify that the properties of both QUIC servers are stored in the
    // HTTP properties map.
    EXPECT_EQ(2U, http_server_properties_.alternative_service_map().size());

    http_server_properties_.SetMaxServerConfigsStoredInProperties(
        kDefaultMaxQuicServerEntries);

    quic::QuicServerId quic_server_id(kDefaultServerHostName, 443,
                                      PRIVACY_MODE_DISABLED);
    std::unique_ptr<QuicServerInfo> quic_server_info =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            quic_server_id, &http_server_properties_);

    // Update quic_server_info's server_config and persist it.
    QuicServerInfo::State* state = quic_server_info->mutable_state();
    // Minimum SCFG that passes config validation checks.
    const char scfg[] = {// SCFG
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

    // Create temporary strings becasue Persist() clears string data in |state|.
    string server_config(reinterpret_cast<const char*>(&scfg), sizeof(scfg));
    string source_address_token("test_source_address_token");
    string cert_sct("test_cert_sct");
    string chlo_hash("test_chlo_hash");
    string signature("test_signature");
    string test_cert("test_cert");
    std::vector<string> certs;
    certs.push_back(test_cert);
    state->server_config = server_config;
    state->source_address_token = source_address_token;
    state->cert_sct = cert_sct;
    state->chlo_hash = chlo_hash;
    state->server_config_sig = signature;
    state->certs = certs;

    quic_server_info->Persist();

    quic::QuicServerId quic_server_id2(kServer2HostName, 443,
                                       PRIVACY_MODE_DISABLED);
    std::unique_ptr<QuicServerInfo> quic_server_info2 =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            quic_server_id2, &http_server_properties_);
    // Update quic_server_info2's server_config and persist it.
    QuicServerInfo::State* state2 = quic_server_info2->mutable_state();

    // Minimum SCFG that passes config validation checks.
    const char scfg2[] = {// SCFG
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
                          '8', '7', '3', '4', '5', '6', '2', '1'};

    // Create temporary strings becasue Persist() clears string data in
    // |state2|.
    string server_config2(reinterpret_cast<const char*>(&scfg2), sizeof(scfg2));
    string source_address_token2("test_source_address_token2");
    string cert_sct2("test_cert_sct2");
    string chlo_hash2("test_chlo_hash2");
    string signature2("test_signature2");
    string test_cert2("test_cert2");
    std::vector<string> certs2;
    certs2.push_back(test_cert2);
    state2->server_config = server_config2;
    state2->source_address_token = source_address_token2;
    state2->cert_sct = cert_sct2;
    state2->chlo_hash = chlo_hash2;
    state2->server_config_sig = signature2;
    state2->certs = certs2;

    quic_server_info2->Persist();

    // Verify the MRU order is maintained.
    const QuicServerInfoMap& quic_server_info_map =
        http_server_properties_.quic_server_info_map();
    EXPECT_EQ(2u, quic_server_info_map.size());
    auto quic_server_info_map_it = quic_server_info_map.begin();
    EXPECT_EQ(quic_server_info_map_it->first, quic_server_id2);
    ++quic_server_info_map_it;
    EXPECT_EQ(quic_server_info_map_it->first, quic_server_id);

    host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                              "192.168.0.1", "");

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data;
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  HostPortPair(quic_server_id.host(), quic_server_id.port()),
                  version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());

    EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
        factory_.get(), quic_server_id));
    quic::QuicCryptoClientConfig* crypto_config =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get());
    quic::QuicCryptoClientConfig::CachedState* cached =
        crypto_config->LookupOrCreate(quic_server_id);
    EXPECT_FALSE(cached->server_config().empty());
    EXPECT_TRUE(cached->GetServerConfig());
    EXPECT_EQ(server_config, cached->server_config());
    EXPECT_EQ(source_address_token, cached->source_address_token());
    EXPECT_EQ(cert_sct, cached->cert_sct());
    EXPECT_EQ(chlo_hash, cached->chlo_hash());
    EXPECT_EQ(signature, cached->signature());
    ASSERT_EQ(1U, cached->certs().size());
    EXPECT_EQ(test_cert, cached->certs()[0]);

    EXPECT_TRUE(socket_data.AllWriteDataConsumed());

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data2;
    socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data2.AddSocketDataToFactory(socket_factory_.get());

    host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                              "192.168.0.2", "");

    QuicStreamRequest request2(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request2.Request(
                  HostPortPair(quic_server_id2.host(), quic_server_id2.port()),
                  version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                  /*cert_verify_flags=*/0, GURL("https://mail.example.org/"),
                  net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());

    EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
        factory_.get(), quic_server_id2));
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config->LookupOrCreate(quic_server_id2);
    EXPECT_FALSE(cached2->server_config().empty());
    EXPECT_TRUE(cached2->GetServerConfig());
    EXPECT_EQ(server_config2, cached2->server_config());
    EXPECT_EQ(source_address_token2, cached2->source_address_token());
    EXPECT_EQ(cert_sct2, cached2->cert_sct());
    EXPECT_EQ(chlo_hash2, cached2->chlo_hash());
    EXPECT_EQ(signature2, cached2->signature());
    ASSERT_EQ(1U, cached->certs().size());
    EXPECT_EQ(test_cert2, cached2->certs()[0]);
  }

  void RunTestLoopUntilIdle() {
    while (!runner_->GetPostedTasks().empty())
      runner_->RunNextTask();
  }

  quic::QuicStreamId GetNthClientInitiatedStreamId(int n) {
    return quic::test::GetNthClientInitiatedStreamId(version_, n);
  }

  quic::QuicStreamId GetNthServerInitiatedStreamId(int n) {
    return quic::test::GetNthServerInitiatedStreamId(version_, n);
  }

  void OnFailedOnDefaultNetwork(int rv) { failed_on_default_network_ = true; }

  // Helper methods for tests of connection migration on write error.
  void TestMigrationOnWriteErrorNonMigratableStream(IoMode write_error_mode);
  // Migratable stream triggers write error.
  void TestMigrationOnWriteErrorMixedStreams(IoMode write_error_mode);
  // Non-migratable stream triggers write error.
  void TestMigrationOnWriteErrorMixedStreams2(IoMode write_error_mode);
  void TestMigrationOnWriteErrorMigrationDisabled(IoMode write_error_mode);
  void TestMigrationOnWriteError(IoMode write_error_mode);
  void TestMigrationOnWriteErrorWithMultipleRequests(IoMode write_error_mode);
  void TestMigrationOnWriteErrorNoNewNetwork(IoMode write_error_mode);
  void TestMigrationOnMultipleWriteErrors(
      IoMode write_error_mode_on_old_network,
      IoMode write_error_mode_on_new_network);
  void TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
      bool disconnected);
  void TestMigrationOnWriteErrorWithNotificationQueuedLater(bool disconnected);
  void TestMigrationOnNetworkDisconnected(bool async_write_before);
  void TestMigrationOnNetworkMadeDefault(IoMode write_mode);
  void TestMigrationOnPathDegrading(bool async_write_before);
  void TestMigrateSessionWithDrainingStream(
      IoMode write_mode_for_queued_packet);
  void TestMigrationOnWriteErrorPauseBeforeConnected(IoMode write_error_mode);
  void TestMigrationOnWriteErrorWithMultipleNotifications(
      IoMode write_error_mode,
      bool disconnect_before_connect);
  void TestNoAlternateNetworkBeforeHandshake(quic::QuicErrorCode error);
  void TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QuicErrorCode error);

  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<MockHostResolverBase> host_resolver_;
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  std::unique_ptr<MockClientSocketFactory> socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::test::MockRandom random_generator_;
  quic::MockClock clock_;
  scoped_refptr<TestTaskRunner> runner_;
  const quic::QuicTransportVersion version_;
  const bool client_headers_include_h2_stream_dependency_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  HttpServerPropertiesImpl http_server_properties_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<CTVerifier> cert_transparency_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  std::unique_ptr<ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier_;
  std::unique_ptr<QuicStreamFactory> factory_;
  HostPortPair host_port_pair_;
  GURL url_;
  GURL url2_;
  GURL url3_;
  GURL url4_;

  PrivacyMode privacy_mode_;
  NetLogWithSource net_log_;
  TestCompletionCallback callback_;
  const CompletionRepeatingCallback failed_on_default_network_callback_;
  bool failed_on_default_network_;
  NetErrorDetails net_error_details_;

  // Variables to configure QuicStreamFactory.
  bool store_server_configs_in_properties_;
  bool close_sessions_on_ip_change_;
  bool goaway_sessions_on_ip_change_;
  int idle_connection_timeout_seconds_;
  int reduced_ping_timeout_seconds_;
  int max_time_before_crypto_handshake_seconds_;
  int max_idle_time_before_crypto_handshake_seconds_;
  bool migrate_sessions_on_network_change_v2_;
  bool migrate_sessions_early_v2_;
  bool retry_on_alternate_network_before_handshake_;
  bool race_stale_dns_on_connection_;
  bool go_away_on_path_degrading_;
  bool allow_server_migration_;
  bool race_cert_verification_;
  bool estimate_initial_rtt_;
  quic::QuicTagVector connection_options_;
  quic::QuicTagVector client_connection_options_;
};

class QuicStreamFactoryTest : public QuicStreamFactoryTestBase,
                              public ::testing::TestWithParam<TestParams> {
 protected:
  QuicStreamFactoryTest()
      : QuicStreamFactoryTestBase(
            GetParam().version,
            GetParam().client_headers_include_h2_stream_dependency) {}
};

INSTANTIATE_TEST_CASE_P(VersionIncludeStreamDependencySequence,
                        QuicStreamFactoryTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicStreamFactoryTest, Create) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback_.callback()));
  // Will reset stream 3.
  stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(OK, request3.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback_.callback()));
  stream = CreateStream(&request3);  // Will reset stream 5.
  stream.reset();                    // Will reset stream 7.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CreateZeroRtt) {
  Initialize();
  factory_->set_require_confirmation(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, DefaultInitialRtt) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(session->require_confirmation());
  EXPECT_EQ(100000u, session->connection()->GetStats().srtt_us);
  ASSERT_FALSE(session->config()->HasInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, FactoryDestroyedWhenJobPending) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  auto request = std::make_unique<QuicStreamRequest>(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request->Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  request.reset();
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  // Tearing down a QuicStreamFactory with a pending Job should not cause any
  // crash. crbug.com/768343.
  factory_.reset();
}

TEST_P(QuicStreamFactoryTest, RequireConfirmation) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_require_confirmation(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  IPAddress last_address;
  EXPECT_FALSE(http_server_properties_.GetSupportsQuic(&last_address));

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);

  EXPECT_TRUE(http_server_properties_.GetSupportsQuic(&last_address));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(session->require_confirmation());
}

TEST_P(QuicStreamFactoryTest, DontRequireConfirmationFromSameIP) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_require_confirmation(true);
  http_server_properties_.SetSupportsQuic(IPAddress(192, 0, 2, 33));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_THAT(request.Request(
                  host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                  SocketTag(),
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()),
              IsOk());

  IPAddress last_address;
  EXPECT_FALSE(http_server_properties_.GetSupportsQuic(&last_address));

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_FALSE(session->require_confirmation());

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);

  EXPECT_TRUE(http_server_properties_.GetSupportsQuic(&last_address));
}

TEST_P(QuicStreamFactoryTest, CachedInitialRtt) {
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMilliseconds(10);
  http_server_properties_.SetServerNetworkStats(url::SchemeHostPort(url_),
                                                stats);
  estimate_initial_rtt_ = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(10000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(10000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, 2gInitialRtt) {
  ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_2G);
  estimate_initial_rtt_ = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(1200000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(1200000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, 3gInitialRtt) {
  ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_3G);
  estimate_initial_rtt_ = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(400000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(400000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, GoAway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  session->OnGoAway(quic::QuicGoAwayFrame());

  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, GoAwayForConnectionMigrationWithPortOnly) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  session->OnGoAway(quic::QuicGoAwayFrame(
      quic::kInvalidControlFrameId, quic::QUIC_ERROR_MIGRATING_PORT, 0,
      "peer connection migration due to port change only"));
  NetErrorDetails details;
  EXPECT_FALSE(details.quic_port_migration_detected);
  session->PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_port_migration_detected);
  details.quic_port_migration_detected = false;
  stream->PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_port_migration_detected);

  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, Pooling) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server2(kServer2HostName, kDefaultServerPort);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(host_port_pair_), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, PoolingWithServerMigration) {
  // Set up session to migrate.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 443);
  quic::QuicConfig config;
  config.SetAlternateServerAddressToSend(
      quic::QuicSocketAddress(quic::QuicSocketAddressImpl(alt_address)));

  VerifyServerMigration(config, alt_address);

  // Close server-migrated session.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  session->CloseSessionOnError(0u, quic::QUIC_NO_ERROR,
                               quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Set up server IP, socket, proof, and config for new session.
  HostPortPair server2(kServer2HostName, kDefaultServerPort);
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  std::unique_ptr<quic::QuicEncryptedPacket> settings_packet(
      client_maker_.MakeInitialSettingsPacket(1, nullptr));
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, settings_packet->data(),
                                  settings_packet->length(), 1)};

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  quic::QuicConfig config2;
  crypto_client_stream_factory_.SetConfig(config2);

  // Create new request to cause new session creation.
  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  EXPECT_EQ(OK, callback.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  // EXPECT_EQ(GetActiveSession(host_port_pair_), GetActiveSession(server2));
}

TEST_P(QuicStreamFactoryTest, NoPoolingAfterGoAway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server2(kServer2HostName, kDefaultServerPort);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  factory_->OnSessionGoingAway(GetActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveSession(server2));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(OK,
            request3.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback3.callback()));
  std::unique_ptr<HttpStream> stream3 = CreateStream(&request3);
  EXPECT_TRUE(stream3.get());

  EXPECT_TRUE(HasActiveSession(server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HttpsPooling) {
  Initialize();

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HttpsPoolingWithMatchingPins) {
  Initialize();
  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);
  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  verify_details.cert_verify_result.public_key_hashes.push_back(primary_pin);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingWithDifferentPins) {
  Initialize();

  MockQuicData socket_data1;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);
  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  ProofVerifyDetailsChromium verify_details1 = DefaultProofVerifyDetails();
  uint8_t bad_pin = 3;
  verify_details1.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(bad_pin));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  ProofVerifyDetailsChromium verify_details2 = DefaultProofVerifyDetails();
  verify_details2.cert_verify_result.public_key_hashes.push_back(primary_pin);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, Goaway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Mark the session as going away.  Ensure that while it is still alive
  // that it is no longer active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  factory_->OnSessionGoingAway(session);
  EXPECT_EQ(true,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Create a new request for the same destination and verify that a
  // new session is created.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_NE(session, GetActiveSession(host_port_pair_));
  EXPECT_EQ(true,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MaxOpenStream) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  quic::QuicStreamId stream_id = GetNthClientInitiatedStreamId(0);
  MockQuicData socket_data;
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRstPacket(2, true, stream_id,
                                               quic::QUIC_STREAM_CANCELLED));
  socket_data.AddRead(ASYNC,
                      server_maker_.MakeRstPacket(1, false, stream_id,
                                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  std::vector<std::unique_ptr<HttpStream>> streams;
  // The MockCryptoClientStream sets max_open_streams to be
  // quic::kDefaultMaxStreamsPerConnection / 2.
  for (size_t i = 0; i < quic::kDefaultMaxStreamsPerConnection / 2; i++) {
    QuicStreamRequest request(factory_.get());
    int rv = request.Request(
        host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
        /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
        failed_on_default_network_callback_, callback_.callback());
    if (i == 0) {
      EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
      EXPECT_THAT(callback_.WaitForResult(), IsOk());
    } else {
      EXPECT_THAT(rv, IsOk());
    }
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream);
    EXPECT_EQ(OK,
              stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                       net_log_, CompletionOnceCallback()));
    streams.push_back(std::move(stream));
  }

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                CompletionOnceCallback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  EXPECT_EQ(ERR_IO_PENDING,
            stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                     net_log_, callback_.callback()));

  // Close the first stream.
  streams.front()->Close(false);
  // Trigger exchange of RSTs that in turn allow progress for the last
  // stream.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  // Force close of the connection to suppress the generation of RST
  // packets when streams are torn down, which wouldn't be relevant to
  // this test anyway.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  session->connection()->CloseConnection(
      quic::QUIC_PUBLIC_RESET, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
}

TEST_P(QuicStreamFactoryTest, ResolutionErrorInCreate) {
  Initialize();
  MockQuicData socket_data;
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  host_resolver_->rules()->AddSimulatedFailure(kDefaultServerHostName);

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ConnectErrorInCreate) {
  Initialize();

  MockQuicData socket_data;
  socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CancelCreate) {
  Initialize();
  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  {
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                  SocketTag(),
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));
  }

  base::RunLoop().RunUntilIdle();

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CloseAllSessions) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructClientRstPacket(2, quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeConnectionClosePacket(
                           3, true, quic::QUIC_INTERNAL_ERROR, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Close the session and verify that stream saw the error.
  factory_->CloseAllSessions(ERR_INTERNET_DISCONNECTED,
                             quic::QUIC_INTERNAL_ERROR);
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  stream = CreateStream(&request2);
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Regression test for crbug.com/700617. Test a write error during the
// crypto handshake will not hang QuicStreamFactory::Job and should
// report QUIC_HANDSHAKE_FAILED to upper layers. Subsequent
// QuicStreamRequest should succeed without hanging.
TEST_P(QuicStreamFactoryTest,
       WriteErrorInCryptoConnectWithAsyncHostResolution) {
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, WriteErrorInCryptoConnectWithSyncHostResolution) {
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail immediately.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Check no active session, or active jobs left for this server.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Complete handshake.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CloseSessionsOnIPAddressChanged) {
  close_sessions_on_ip_change_ = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructClientRstPacket(2, quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       3, true, quic::QUIC_IP_ADDRESS_CHANGED, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Check an active session exisits for the destination.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  IPAddress last_address;
  EXPECT_TRUE(http_server_properties_.GetSupportsQuic(&last_address));
  // Change the IP address and verify that stream saw the error and the active
  // session is closed.
  NotifyIPAddressChanged();
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_TRUE(factory_->require_confirmation());
  EXPECT_FALSE(http_server_properties_.GetSupportsQuic(&last_address));
  // Check no active session exists for the destination.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Now attempting to request a stream to the same origin should create
  // a new session.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  stream = CreateStream(&request2);

  // Check a new active session exisits for the destination and the old session
  // is no longer live.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  stream.reset();  // Will reset stream 3.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Test that if goaway_session_on_ip_change is set, old sessions will be marked
// as going away on IP address change instead of being closed. New requests will
// go to a new connection.
TEST_P(QuicStreamFactoryTest, GoAwaySessionsOnIPAddressChanged) {
  goaway_sessions_on_ip_change_ = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                       2, GetNthClientInitiatedStreamId(0),
                                       true, true, &header_stream_offset));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2;
  quic::QuicStreamOffset header_stream_offset2 = 0;
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset2));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Receive an IP address change notification.
  NotifyIPAddressChanged();

  // The connection should still be alive, but marked as going away.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume the data, response should be read from the original connection.
  quic_data1.Resume();
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  // Second request should be sent on a new connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // Check an active session exisits for the destination.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  stream.reset();
  stream2.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnIPAddressChangedWithConnectionMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructClientRstPacket(2, quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  IPAddress last_address;
  EXPECT_TRUE(http_server_properties_.GetSupportsQuic(&last_address));

  // Change the IP address and verify that the connection is unaffected.
  NotifyIPAddressChanged();
  EXPECT_FALSE(factory_->require_confirmation());
  EXPECT_TRUE(http_server_properties_.GetSupportsQuic(&last_address));

  // Attempting a new request to the same origin uses the same connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback_.callback()));
  stream = CreateStream(&request2);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateOnNetworkMadeDefaultWithSynchronousWrite) {
  TestMigrationOnNetworkMadeDefault(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateOnNetworkMadeDefaultWithAsyncWrite) {
  TestMigrationOnNetworkMadeDefault(ASYNC);
}

// Sets up a test which attempts connection migration successfully after probing
// when a new network is made as default and the old default is still available.
// |write_mode| specifies the write mode for the last write before
// OnNetworkMadeDefault is delivered to session.
void QuicStreamFactoryTestBase::TestMigrationOnNetworkMadeDefault(
    IoMode write_mode) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data1.AddWrite(
      write_mode, ConstructGetRequestPacket(2, GetNthClientInitiatedStreamId(0),
                                            true, true, &header_stream_offset));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2;
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(3, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeAckAndPingPacket(4, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(2, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckAndRstPacket(
                          5, false, GetNthClientInitiatedStreamId(0),
                          quic::QUIC_STREAM_CANCELLED, 2, 2, 1, true));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // A task will be posted to migrate to the new default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());

  // Execute the posted task to migrate back to the default network.
  task_runner->RunUntilIdle();
  // Another task to try send a new connectivity probe is posted. And a task to
  // retry migrate back to default network is scheduled.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket, declare probing as successful. And a new task to WriteToNewSocket
  // will be posted to complete migration.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s, which
  // is also cancelled due to the success migration on the previous trial.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);
  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Regression test for http://859674.
// This test veries that a writer will not attempt to write packets until being
// unblocked on both socket level and network level. In this test, a probing
// writer is used to send two connectivity probes to the peer: where the first
// one completes successfully, while a connectivity response is received before
// completes sending the second one. The connection migration attempt will
// proceed while the probing writer is blocked at the socket level, which will
// block the writer on the network level. Once connection migration completes
// successfully, the probing writer will be unblocked on the network level, it
// will not attempt to write new packets until the socket level is unblocked.
TEST_P(QuicStreamFactoryTest, MigratedToBlockedSocketAfterProbing) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                       2, GetNthClientInitiatedStreamId(0),
                                       true, true, &header_stream_offset));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2;
  // First connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(3, true));
  quic_data2.AddRead(ASYNC,
                     ERR_IO_PENDING);  // Pause so that we can control time.
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Second connectivity probe which will complete asynchronously.
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeConnectivityProbingPacket(4, true));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(2, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeAckAndPingPacket(5, false, 1, 1, 1));
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckAndRstPacket(
                          6, false, GetNthClientInitiatedStreamId(0),
                          quic::QUIC_STREAM_CANCELLED, 2, 2, 1, true));

  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // A task will be posted to migrate to the new default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());

  // Execute the posted task to migrate back to the default network.
  task_runner->RunUntilIdle();
  // Another task to resend a new connectivity probe is posted. And a task to
  // retry migrate back to default network is scheduled.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Fast forward to send the second connectivity probe. The write will be
  // asynchronous and complete after the read completes.
  task_runner->FastForwardBy(next_task_delay);

  // Resume quic data and a connectivity probe response will be read on the new
  // socket, declare probing as successful.
  quic_data2.Resume();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network. Second task will retry migrate back to
  // default but cancelled, and the third task will retry send connectivity
  // probe but also cancelled.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Run the message loop to complete the asynchronous write of ack and ping.
  base::RunLoop().RunUntilIdle();

  // Now there are two pending tasks, the nearest one was to retry migrate back
  // to default network and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      expected_delay;
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(expected_delay, next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to retry sending connectivity probe in 0.4s and has
  // also been cancelled due to the successful probing.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  expected_delay =
      base::TimeDelta::FromMilliseconds(3 * 2 * kDefaultRTTMilliSecs) -
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs);
  EXPECT_EQ(expected_delay, next_task_delay);
  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that session times out connection migration attempt
// with signals delivered in the following order (no alternate network is
// available):
// - default network disconnected is delivered: session attempts connection
//   migration but found not alternate network. Session waits for a new network
//   comes up in the next kWaitTimeForNewNetworkSecs seonds.
// - no new network is connected, migration times out. Session is closed.
TEST_P(QuicStreamFactoryTest, MigrationTimeoutWithNoNewNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there are no networks
  // to migrate to, this should cause the session to wait for a new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The migration will not fail until the migration alarm timeout.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(true, session->connection()->writer()->IsWriteBlocked());

  // Migration will be timed out after kWaitTimeForNewNetwokSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromSeconds(kWaitTimeForNewNetworkSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // The connection should now be closed. A request for response
  // headers should fail.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test verifies that connectivity probes will be sent even if there is
// a non-migratable stream. However, when connection migrates to the
// successfully probed path, any non-migratable stream will be reset. And if
// the connection becomes idle then, close the connection.
TEST_P(QuicStreamFactoryTest, OnNetworkMadeDefaultNonMigratableStream) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstAckAndConnectionClosePacket(
          3, false, 5, quic::QUIC_STREAM_CANCELLED,
          quic::QuicTime::Delta::FromMilliseconds(0), 1, 1, 1,
          quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS, "net error"));

  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for probing.
  MockQuicData quic_data1;
  // Connectivity probe to be sent on the new path.
  quic_data1.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(2, true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data1.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Session will start to probe the alternative
  // network. Although there is a non-migratable stream, session will still be
  // active until probing is declared as successful.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume data to read a connectivity probing response, which will cause
  // non-migtable streams to be closed. As session becomes idle, connection will
  // be closed.
  quic_data1.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnNetworkMadeDefaultConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(2, true, GetNthClientInitiatedStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to continue but be marked as going away.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnNetworkDisconnectedNonMigratableStream) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(2, true, GetNthClientInitiatedStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause a RST_STREAM frame to be emitted with
  // quic::QUIC_STREAM_CANCELLED error code, and the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(2, true, GetNthClientInitiatedStreamId(0),
                                  quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause a RST_STREAM frame to be emitted with
  // quic::QUIC_RST_ACKNOWLEDGEMENT error code, and the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnNetworkMadeDefaultNoOpenStreams) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeConnectionClosePacket(
          2, true, quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
          "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnNetworkDisconnectedNoOpenStreams) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test verifies session migrates to the alternate network immediately when
// default network disconnects with a synchronous write before migration.
TEST_P(QuicStreamFactoryTest, MigrateOnDefaultNetworkDisconnectedSync) {
  TestMigrationOnNetworkDisconnected(/*async_write_before*/ false);
}

// This test verifies session migrates to the alternate network immediately when
// default network disconnects with an asynchronously write before migration.
TEST_P(QuicStreamFactoryTest, MigrateOnDefaultNetworkDisconnectedAsync) {
  TestMigrationOnNetworkDisconnected(/*async_write_before*/ true);
}

void QuicStreamFactoryTestBase::TestMigrationOnNetworkDisconnected(
    bool async_write_before) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  int packet_number = 1;
  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructInitialSettingsPacket(packet_number++, &header_stream_offset));
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(0), true,
                       true, &header_stream_offset));
  if (async_write_before) {
    socket_data.AddWrite(ASYNC, OK);
    packet_number++;
  }
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  if (async_write_before)
    session->SendPing();

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_number++, /*include_version=*/true));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndRstPacket(
                       packet_number++, false, GetNthClientInitiatedStreamId(0),
                       quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  runner_->RunNextTask();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // There should be posted tasks not executed, which is to migrate back to
  // default network.
  EXPECT_FALSE(runner_->GetPostedTasks().empty());

  // Receive signal to mark new network as default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test receives NCN signals in the following order:
// - default network disconnected
// - after a pause, new network is connected.
// - new network is made default.
TEST_P(QuicStreamFactoryTest, NewNetworkConnectedAfterNoNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                        2, GetNthClientInitiatedStreamId(0),
                                        true, true, &header_stream_offset));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger connection migration. Since there are no networks
  // to migrate to, this should cause the session to wait for a new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakePingPacket(3, /*include_version=*/true));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            4, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Add a new network and notify the stream factory of a new connected network.
  // This causes a PING packet to be sent over the new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  runner_->RunNextTask();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // There should posted tasks not executed, which is to migrate back to default
  // network.
  EXPECT_FALSE(runner_->GetPostedTasks().empty());

  // Receive signal to mark new network as default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/872011.
// This test verifies that migrate to the probing socket will not trigger
// new packets being read synchronously and generate ACK frame while
// processing the initial connectivity probe response, which may cause a
// connection being closed with INTERNAL_ERROR as pending ACK frame is not
// allowed when processing a new packet.
TEST_P(QuicStreamFactoryTest, MigrateToProbingSocket) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                       packet_number++, &header_stream_offset));
  quic_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(0), true,
                       true, &header_stream_offset));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for probing on the
  // alternate network.
  MockQuicData quic_data2;
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // First connectivity probe to receive from the server, which will complete
  // connection migraiton on path degrading.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Read multiple connectivity probes synchronously.
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(2, false));
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(3, false));
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(4, false));
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeAckPacket(packet_number++, 1, 4, 1, 1, true));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(5, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndRstPacket(
                       packet_number++, false, GetNthClientInitiatedStreamId(0),
                       quic::QUIC_STREAM_CANCELLED, 5, 1, 1, true));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that the connection migrates to the alternate network
// early when path degrading is detected with an ASYNCHRONOUS write before
// migration.
TEST_P(QuicStreamFactoryTest, MigrateEarlyOnPathDegradingAysnc) {
  TestMigrationOnPathDegrading(/*async_write_before_migration*/ true);
}

// This test verifies that the connection migrates to the alternate network
// early when path degrading is detected with a SYNCHRONOUS write before
// migration.
TEST_P(QuicStreamFactoryTest, MigrateEarlyOnPathDegradingSync) {
  TestMigrationOnPathDegrading(/*async_write_before_migration*/ false);
}

void QuicStreamFactoryTestBase::TestMigrationOnPathDegrading(
    bool async_write_before) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                       packet_number++, &header_stream_offset));
  quic_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(0), true,
                       true, &header_stream_offset));
  if (async_write_before) {
    quic_data1.AddWrite(ASYNC, OK);
    packet_number++;
  }
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2;
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(ASYNC, client_maker_.MakeAckAndPingPacket(
                                 packet_number++, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(2, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndRstPacket(
                       packet_number++, false, GetNthClientInitiatedStreamId(0),
                       quic::QUIC_STREAM_CANCELLED, 2, 2, 1, true));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  if (async_write_before)
    session->SendPing();

  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that the session marks itself GOAWAY on path degrading
// and it does not receive any new request
TEST_P(QuicStreamFactoryTest, GoawayOnPathDegrading) {
  go_away_on_path_degrading_ = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                       2, GetNthClientInitiatedStreamId(0),
                                       true, true, &header_stream_offset));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2;
  quic::QuicStreamOffset header_stream_offset2 = 0;
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset2));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Creat request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cerf_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger the connection to report path degrading to the session.
  // Session will mark itself GOAWAY.
  session->connection()->OnPathDegradingTimeout();

  // The connection should still be alive, but marked as going away.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Second request should be sent on a new connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // Resume the data, verify old request can read response on the old session
  // successfully.
  quic_data1.Resume();
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(0U, session->GetNumActiveStreams());

  // Check an active session exists for the destination.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));
  EXPECT_NE(session, session2);

  stream.reset();
  stream2.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that the connection will not migrate to a bad socket
// when path degrading is detected.
TEST_P(QuicStreamFactoryTest, DoNotMigrateToBadSocketOnPathDegrading) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                      2, GetNthClientInitiatedStreamId(0), true,
                                      true, &header_stream_offset));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeAckAndRstPacket(
                         3, false, GetNthClientInitiatedStreamId(0),
                         quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket that will immediately return disconnected.
  // The stream factory will abort probe the alternate network.
  MockConnect bad_connect = MockConnect(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  SequencedSocketData socket_data(bad_connect, base::span<MockRead>(),
                                  base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data);

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume the data, and response header is received over the original network.
  quic_data.Resume();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Verify there is no pending task as probing alternate network is halted.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/847569.
// This test verifies that the connection migrates to the alternate network
// early when there is no active stream but a draining stream.
// The first packet being written after migration is a synchrnous write, which
// will cause a PING packet being sent.
TEST_P(QuicStreamFactoryTest, MigrateSessionWithDrainingStreamSync) {
  TestMigrateSessionWithDrainingStream(SYNCHRONOUS);
}

// Regression test for http://crbug.com/847569.
// This test verifies that the connection migrates to the alternate network
// early when there is no active stream but a draining stream.
// The first packet being written after migration is an asynchronous write, no
// PING packet will be sent.
TEST_P(QuicStreamFactoryTest, MigrateSessionWithDrainingStreamAsync) {
  TestMigrateSessionWithDrainingStream(ASYNC);
}

void QuicStreamFactoryTestBase::TestMigrateSessionWithDrainingStream(
    IoMode write_mode_for_queued_packet) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                       packet_number++, &header_stream_offset));
  quic_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(0), true,
                       true, &header_stream_offset));
  // Read an out of order packet with FIN to drain the stream.
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(2, GetNthClientInitiatedStreamId(0),
                                       false, true));  // keep sending version.
  quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeAckPacket(
                                       packet_number++, 2, 2, 2, 1, true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  MockQuicData quic_data2;
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, false));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(3, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(
      write_mode_for_queued_packet,
      client_maker_.MakeAckPacket(packet_number++, 2, 3, 3, 1, true));
  if (write_mode_for_queued_packet == SYNCHRONOUS) {
    quic_data2.AddWrite(ASYNC,
                        client_maker_.MakePingPacket(packet_number++, false));
  }
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeAckPacket(
                                       packet_number++, 1, 3, 1, 1, true));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run the message loop to receive the out of order packet which contains a
  // FIN and drains the stream.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  // Cause the connection to report path degrading to the session.
  // Session should still start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());
  EXPECT_EQ(1u, session->GetNumDrainingStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  base::RunLoop().RunUntilIdle();

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/835444.
// This test verifies that the connection migrates to the alternate network
// when the alternate network is connected after path has been degrading.
TEST_P(QuicStreamFactoryTest, MigrateOnNewNetworkConnectAfterPathDegrading) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                       2, GetNthClientInitiatedStreamId(0),
                                       true, true, &header_stream_offset));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2;
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(3, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeAckAndPingPacket(4, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(2, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckAndRstPacket(
                          5, false, GetNthClientInitiatedStreamId(0),
                          quic::QUIC_STREAM_CANCELLED, 2, 2, 1, true));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  session->connection()->OnPathDegradingTimeout();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that multiple sessions are migrated on connection
// migration signal.
TEST_P(QuicStreamFactoryTest,
       MigrateMultipleSessionsToBadSocketsAfterDisconnected) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});

  MockQuicData socket_data1;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddWrite(ASYNC, OK);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddWrite(ASYNC, OK);
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.2", "");

  // Create request and QuicHttpStream to create session1.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(OK,
            request1.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  // Create request and QuicHttpStream to create session2.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession* session1 = GetActiveSession(server1);
  QuicChromiumClientSession* session2 = GetActiveSession(server2);
  EXPECT_NE(session1, session2);

  // Cause QUIC stream to be created and send GET so session1 has an open
  // stream.
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = url_;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));
  HttpResponseInfo response1;
  HttpRequestHeaders request_headers1;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers1, &response1,
                                     callback_.callback()));

  // Cause QUIC stream to be created and send GET so session2 has an open
  // stream.
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = url_;
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback_.callback()));

  // Cause both sessions to be paused due to DISCONNECTED.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // Ensure that both sessions are paused but alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session1));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  // Add new sockets to use post migration. Those are bad sockets and will cause
  // migration to fail.
  MockConnect connect_result =
      MockConnect(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  SequencedSocketData socket_data3(connect_result, base::span<MockRead>(),
                                   base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data3);
  SequencedSocketData socket_data4(connect_result, base::span<MockRead>(),
                                   base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data4);

  // Connect the new network and cause migration to bad sockets, causing
  // sessions to close.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session1));
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// This test verifies that session attempts connection migration with signals
// delivered in the following order (no alternate network is available):
// - path degrading is detected: session attempts connection migration but no
//   alternate network is available, session caches path degrading signal in
//   connection and stays on the original network.
// - original network backs up, request is served in the orignal network,
//   session is not marked as going away.
TEST_P(QuicStreamFactoryTest, MigrateOnPathDegradingWithNoNewNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(1, &header_stream_offset));
  quic_data.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                      2, GetNthClientInitiatedStreamId(0), true,
                                      true, &header_stream_offset));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause for path degrading signal.

  // The rest of the data will still flow in the original socket as there is no
  // new network after path degrading.
  quic_data.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeAckAndRstPacket(
                         3, false, GetNthClientInitiatedStreamId(0),
                         quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger connection migration on path degrading. Since there are no networks
  // to migrate to, the session will remain on the original network, not marked
  // as going away.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_TRUE(session->connection()->IsPathDegrading());

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume so that rest of the data will flow in the original socket.
  quic_data.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// This test verifies that session with non-migratable stream will probe the
// alternate network on path degrading, and close the non-migratable streams
// when probe is successful.
TEST_P(QuicStreamFactoryTest, MigrateSessionEarlyNonMigratableStream) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstAckAndConnectionClosePacket(
          3, false, 5, quic::QUIC_STREAM_CANCELLED,
          quic::QuicTime::Delta::FromMilliseconds(0), 1, 1, 1,
          quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for probing.
  MockQuicData quic_data1;
  // Connectivity probe to be sent on the new path.
  quic_data1.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(2, true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data1.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to be continue without migrating.
  session->OnPathDegrading();

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume the data to read the connectivity probing response to declare probe
  // as successful. Non-migratable streams will be closed.
  quic_data1.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionEarlyConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(2, true, GetNthClientInitiatedStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to be continue without migrating.
  session->OnPathDegrading();

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/791886.
// This test verifies that the old packet writer which encountered an
// asynchronous write error will be blocked during migration on write error. New
// packets would not be written until the one with write error is rewritten on
// the new network.
TEST_P(QuicStreamFactoryTest, MigrateSessionOnAysncWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  // base::RunLoop() controls mocked socket writes and reads.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         3, GetNthClientInitiatedStreamId(1),
                                         GetNthClientInitiatedStreamId(0), true,
                                         true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            4, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(5, false, GetNthClientInitiatedStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED, 0));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Request #2 returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream1. This should cause an async write error.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Run the message loop so that asynchronous write completes and a connection
  // migration on write error attempt is posted in QuicStreamFactory's task
  // runner.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  // Send GET request on stream. This will cause another write attempt before
  // migration on write error is exectued.
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback2.callback()));

  // Run the task runner so that migration on write error is finally executed.
  task_runner->RunUntilIdle();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  // There should be one task posted to migrate back to the default network in
  // kMinRetryTimeForDefaultNetworkSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs),
            task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// Verify session is not marked as going away after connection migration on
// write error and migrate back to default network logic is applied to bring the
// migrated session back to the default network. Migration singals delivered
// in the following order (alternate network is always availabe):
// - session on the default network encountered a write error;
// - session successfully migrated to the non-default network;
// - session attempts to migrate back to default network post migration;
// - migration back to the default network is successful.
TEST_P(QuicStreamFactoryTest, MigrateBackToDefaultPostMigrationOnWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData quic_data2;
  quic_data2.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                       2, GetNthClientInitiatedStreamId(0),
                                       true, true, &header_stream_offset));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request. This should cause an async write error.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Run the message loop so that asynchronous write completes and a connection
  // migration on write error attempt is posted in QuicStreamFactory's task
  // runner.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  // Run the task runner so that migration on write error is finally executed.
  task_runner->RunUntilIdle();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  // There should be one task posted to migrate back to the default network in
  // kMinRetryTimeForDefaultNetworkSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs);
  EXPECT_EQ(expected_delay, task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Set up the third socket data provider for migrate back to default network.
  MockQuicData quic_data3;
  // Connectivity probe to be sent on the new path.
  quic_data3.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(3, false));
  // Connectivity probe to receive from the server.
  quic_data3.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(2, false));
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data3.AddWrite(ASYNC, client_maker_.MakeAckPacket(4, 1, 2, 1, 1, true));
  quic_data3.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(5, false, GetNthClientInitiatedStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED, 0));
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  // Fast forward to fire the migrate back timer and verify the session
  // successfully migrates back to the default network.
  task_runner->FastForwardBy(expected_delay);

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be one task posted to one will resend a connectivity probe and
  // the other will retry migrate back, both are cancelled.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  task_runner->FastForwardBy(
      base::TimeDelta::FromSeconds(2 * kMinRetryTimeForDefaultNetworkSecs));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  stream1.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data3.AllReadDataConsumed());
  EXPECT_TRUE(quic_data3.AllWriteDataConsumed());
}

// This test verifies that the connection will not attempt connection migration
// (send connectivity probes on alternate path) when path degrading is detected
// and handshake is not confirmed.
TEST_P(QuicStreamFactoryTest,
       NoMigrationOnPathDegradingBeforeHandshakeConfirmed) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Cause the connection to report path degrading to the session.
  // Session will ignore the signal as handshake is not completed.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test verifies that if a connection is closed with
// QUIC_NETWORK_IDLE_TIMEOUT before handshake is completed and there is no
// alternate network, no new connection will be created.
TEST_P(QuicStreamFactoryTest, NoAlternateNetworkBeforeHandshakeOnIdleTimeout) {
  TestNoAlternateNetworkBeforeHandshake(quic::QUIC_NETWORK_IDLE_TIMEOUT);
}

// This test verifies that if a connection is closed with QUIC_HANDSHAKE_TIMEOUT
// and there is no alternate network, no new connection will be created.
TEST_P(QuicStreamFactoryTest, NoAlternateNetworkOnHandshakeTimeout) {
  TestNoAlternateNetworkBeforeHandshake(quic::QUIC_HANDSHAKE_TIMEOUT);
}

void QuicStreamFactoryTestBase::TestNoAlternateNetworkBeforeHandshake(
    quic::QuicErrorCode quic_error) {
  DCHECK(quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
         quic_error == quic::QUIC_HANDSHAKE_TIMEOUT);
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Cause the connection to report path degrading to the session.
  // Session will ignore the signal as handshake is not completed.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Cause the connection to close due to |quic_error| before handshake.
  quic::QuicString error_details;
  if (quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT) {
    error_details = "No recent network activity.";
  } else {
    error_details = "Handshake timeout expired.";
  }
  session->connection()->CloseConnection(
      quic_error, error_details, quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // A task will be posted to clean up the session in the factory.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  task_runner->FastForwardUntilNoTasksRemain();

  // No new session should be created as there is no alternate network.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NewConnectionBeforeHandshakeAfterIdleTimeout) {
  TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicStreamFactoryTest, NewConnectionAfterHandshakeTimeout) {
  TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QUIC_HANDSHAKE_TIMEOUT);
}

// Sets up a test to verify that a new connection will be created on the
// alternate network after the initial connection fails before handshake with
// signals delivered in the following order (alternate network is available):
// - the default network is not able to complete crypto handshake;
// - the original connection is closed with |quic_error|;
// - a new connection is created on the alternate network and is able to finish
//   crypto handshake;
// - the new session on the alternate network attempts to migrate back to the
//   default network by sending probes;
// - default network being disconnected is delivered: session will stop probing
//   the original network.
// - alternate network is made by default.
void QuicStreamFactoryTestBase::
    TestNewConnectionOnAlternateNetworkBeforeHandshake(
        quic::QuicErrorCode quic_error) {
  DCHECK(quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
         quic_error == quic::QUIC_HANDSHAKE_TIMEOUT);
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // Socket data for connection on the default network.
  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for connection on the alternate network.
  MockQuicData socket_data2;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeDummyCHLOPacket(1));
  socket_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  socket_data2.AddWrite(
      ASYNC, ConstructInitialSettingsPacket(2, &header_stream_offset));
  socket_data2.AddWrite(
      ASYNC, ConstructGetRequestPacket(3, GetNthClientInitiatedStreamId(0),
                                       true, true, &header_stream_offset));
  socket_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            5, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for probing on the default network.
  MockQuicData probing_data;
  probing_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  probing_data.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeConnectivityProbingPacket(4, false));
  probing_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_FALSE(failed_on_default_network_);

  quic::QuicString error_details;
  if (quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT) {
    error_details = "No recent network activity.";
  } else {
    error_details = "Handshake timeout expired.";
  }
  session->connection()->CloseConnection(
      quic_error, error_details, quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // A task will be posted to clean up the session in the factory.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  task_runner->FastForwardUntilNoTasksRemain();

  // Verify a new session is created on the alternate network.
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  QuicChromiumClientSession* session2 = GetPendingSession(host_port_pair_);
  EXPECT_NE(session, session2);
  EXPECT_TRUE(failed_on_default_network_);

  // Confirm the handshake on the alternate network.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  // Resume the data now so that data can be sent and read.
  socket_data2.Resume();

  // Create the stream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));
  // Send the request.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop to finish asynchronous mock write.
  base::RunLoop().RunUntilIdle();
  // Read the response.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // There should be a new task posted to migrate back to the default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There should be two tasks posted. One will retry probing and the other
  // will retry migrate back.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // Deliver the signal that the default network is disconnected.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // Verify no connectivity probes will be sent as probing will be cancelled.
  task_runner->FastForwardUntilNoTasksRemain();
  // Deliver the signal that the alternate network is made default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Test that connection will be closed with PACKET_WRITE_ERROR if a write error
// is triggered before handshake is confirmed and connection migration is turned
// on.
TEST_P(QuicStreamFactoryTest, MigrationOnWriteErrorBeforeHandshakeConfirmed) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Verify new requests can be sent normally.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteError(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that session is alive and not marked as going awya.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnWriteErrorSynchronous) {
  TestMigrationOnWriteError(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnWriteErrorAsync) {
  TestMigrationOnWriteError(ASYNC);
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorNoNewNetwork(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner, to force the migration alarm timeout later.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream. This causes a write error, which triggers
  // a connection migration attempt. Since there are no networks
  // to migrate to, this causes the session to wait for a new network.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Complete any pending writes. Pending async MockQuicData writes
  // are run on the message loop, not on the test runner.
  base::RunLoop().RunUntilIdle();

  // Write error causes migration task to be posted. Spin the loop.
  if (write_error_mode == ASYNC)
    runner_->RunNextTask();

  // Migration has not yet failed. The session should be alive and active.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_TRUE(session->connection()->writer()->IsWriteBlocked());

  // The migration will not fail until the migration alarm timeout.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Force migration alarm timeout to run.
  RunTestLoopUntilIdle();

  // The connection should be closed. A request for response headers
  // should fail.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNoNewNetworkSynchronous) {
  TestMigrationOnWriteErrorNoNewNetwork(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnWriteErrorNoNewNetworkAsync) {
  TestMigrationOnWriteErrorNoNewNetwork(ASYNC);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithMultipleRequestsSync) {
  TestMigrationOnWriteErrorWithMultipleRequests(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithMultipleRequestsAsync) {
  TestMigrationOnWriteErrorWithMultipleRequests(ASYNC);
}

// Sets up a test which verifies that connection migration on write error can
// eventually succeed and rewrite the packet on the new network with *multiple*
// migratable streams.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorWithMultipleRequests(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(4, false, GetNthClientInitiatedStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED, 0));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequestsSync) {
  TestMigrationOnWriteErrorMixedStreams(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequestsAsync) {
  TestMigrationOnWriteErrorMixedStreams(ASYNC);
}

// Sets up a test that verifies connection migration manages to migrate to
// alternate network after encountering a SYNC/ASYNC write error based on
// |write_error_mode| on the original network.
// Note there are mixed types of unfinished requests before migration: one
// migratable and one non-migratable. The *migratable* one triggers write
// error.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMixedStreams(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_number = 1;
  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructInitialSettingsPacket(packet_number++, &header_stream_offset));
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(0), true,
                       true, &header_stream_offset));
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRstPacket(packet_number++, true,
                                               GetNthClientInitiatedStreamId(1),
                                               quic::QUIC_STREAM_CANCELLED, 0));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndRstPacket(
                       packet_number++, false, GetNthClientInitiatedStreamId(0),
                       quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream 1. This should cause a write error, which
  // triggers a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that the session is still alive and not marked as going away.
  // Non-migratable stream should be closed due to migration.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequests2Sync) {
  TestMigrationOnWriteErrorMixedStreams2(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequests2Async) {
  TestMigrationOnWriteErrorMixedStreams2(ASYNC);
}

// The one triggers write error is a non-migratable stream.
// Sets up a test that verifies connection migration manages to migrate to
// alternate network after encountering a SYNC/ASYNC write error based on
// |write_error_mode| on the original network.
// Note there are mixed types of unfinished requests before migration: one
// migratable and one non-migratable. The *non-migratable* one triggers write
// error.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMixedStreams2(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_number = 1;
  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructInitialSettingsPacket(packet_number++, &header_stream_offset));
  socket_data.AddWrite(write_error_mode,
                       ERR_ADDRESS_UNREACHABLE);  // Write error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  // The packet triggered writer error will be sent anyway even if the stream
  // will be cancelled later.
  socket_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(1), true,
                       true, &header_stream_offset));
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRstPacket(packet_number++, true,
                                               GetNthClientInitiatedStreamId(1),
                                               quic::QUIC_STREAM_CANCELLED, 0));
  socket_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++, GetNthClientInitiatedStreamId(0), true,
                       true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndRstPacket(
                       packet_number++, false, GetNthClientInitiatedStreamId(0),
                       quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream 2 which is non-migratable. This should cause a
  // write error, which triggers a connection migration attempt.
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback2.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader. Session is
  // still alive and not marked as going away, non-migratable stream will be
  // closed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream 1.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorNonMigratableStream(
    IoMode write_error_mode) {
  DVLOG(1) << "Mode: "
           << ((write_error_mode == SYNCHRONOUS) ? "SYNCHRONOUS" : "ASYNC");
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run message loop to execute migration attempt.
  base::RunLoop().RunUntilIdle();

  // Migration fails, and session is closed and deleted.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNonMigratableStreamSynchronous) {
  TestMigrationOnWriteErrorNonMigratableStream(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNonMigratableStreamAsync) {
  TestMigrationOnWriteErrorNonMigratableStream(ASYNC);
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMigrationDisabled(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run message loop to execute migration attempt.
  base::RunLoop().RunUntilIdle();
  // Migration fails, and session is closed and deleted.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorMigrationDisabledSynchronous) {
  TestMigrationOnWriteErrorMigrationDisabled(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorMigrationDisabledAsync) {
  TestMigrationOnWriteErrorMigrationDisabled(ASYNC);
}

// Sets up a test which verifies that connection migration on write error can
// eventually succeed and rewrite the packet on the new network with singals
// delivered in the following order (alternate network is always availabe):
// - original network encounters a SYNC/ASYNC write error based on
//   |write_error_mode_on_old_network|, the packet failed to be written is
//   cached, session migrates immediately to the alternate network.
// - an immediate SYNC/ASYNC write error based on
//   |write_error_mode_on_new_network| is encountered after migration to the
//   alternate network, session migrates immediately to the original network.
// - an immediate SYNC/ASYNC write error based on
//   |write_error_mode_on_old_network| is encountered after migration to the
//   original network, session migrates immediately to the alternate network.
// - finally, session successfully sends the packet and reads the response on
//   the alternate network.
// TODO(zhongyi): once https://crbug.com/855666 is fixed, this test should be
// modified to test that session is closed early if hopping between networks
// with consecutive write errors is detected.
void QuicStreamFactoryTestBase::TestMigrationOnMultipleWriteErrors(
    IoMode write_error_mode_on_old_network,
    IoMode write_error_mode_on_new_network) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set up the socket data used by the original network, which encounters a
  // write erorr.
  MockQuicData socket_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data1.AddWrite(write_error_mode_on_old_network,
                        ERR_ADDRESS_UNREACHABLE);  // Write Error
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the socket data used by the alternate network, which also
  // encounters a write error.
  MockQuicData failed_quic_data2;
  failed_quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  failed_quic_data2.AddWrite(write_error_mode_on_new_network, ERR_FAILED);
  failed_quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Set up the third socket data used by original network, which encounters a
  // write error again.
  MockQuicData failed_quic_data1;
  failed_quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  failed_quic_data1.AddWrite(write_error_mode_on_old_network, ERR_FAILED);
  failed_quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the last socket data used by the alternate network, which will
  // finish migration successfully. The request is rewritten to this new socket,
  // and the response to the request is read on this socket.
  MockQuicData socket_data2;
  socket_data2.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  // This should encounter a write error on network 1,
  // then migrate to network 2, which encounters another write error,
  // and migrate again to network 1, which encoutners one more write error.
  // Finally the session migrates to network 2 successfully.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(failed_quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(failed_quic_data2.AllWriteDataConsumed());
  EXPECT_TRUE(failed_quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(failed_quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsSyncSync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ SYNCHRONOUS,
      /*write_error_mode_on_new_network*/ SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsSyncAsync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ SYNCHRONOUS,
      /*write_error_mode_on_new_network*/ ASYNC);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsAsyncSync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ ASYNC,
      /*write_error_mode_on_new_network*/ SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsAsyncAsync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ ASYNC,
      /*write_error_mode_on_new_network*/ ASYNC);
}

// Verifies that a connection is closed when connection migration is triggered
// on network being disconnected and the handshake is not confirmed.
TEST_P(QuicStreamFactoryTest, NoMigrationBeforeHandshakeOnNetworkDisconnected) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use cold start mode to do crypto connect, and send CHLO packet on wire.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Deliver the network notification, which should cause the connection to be
  // closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());

  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Sets up the connection migration test where network change notification is
// queued BEFORE connection migration attempt on write error is posted.
void QuicStreamFactoryTestBase::
    TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
        bool disconnected) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // First queue a network change notification in the message loop.
  if (disconnected) {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkDisconnected(kDefaultNetworkForTests);
  } else {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkMadeDefault(kNewNetworkForTests);
  }
  // Send GET request on stream. This should cause a write error,
  // which triggers a connection migration attempt. This will queue a
  // migration attempt behind the notification in the message loop.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  base::RunLoop().RunUntilIdle();
  // Verify the session is still alive and not marked as going away post
  // migration.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - a notification that default network is disconnected is queued.
// - write error is triggered: session posts a task to attempt connection
//   migration, |migration_pending_| set to true.
// - default network disconnected is delivered: session immediately migrates to
//   the alternate network, |migration_pending_| set to false.
// - connection migration on write error attempt aborts: writer encountered
//   error is no longer in active use.
TEST_P(QuicStreamFactoryTest,
       MigrateOnNetworkDisconnectedWithWriteErrorQueuedLater) {
  TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
      /*disconnected=*/true);
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - a notification that alternate network is made default is queued.
// - write error is triggered: session posts a task to attempt connection
//   migration, block future migrations.
// - new default notification is delivered: migrate back timer spins and task is
//   posted to migrate to the new default network.
// - connection migration on write error attempt proceeds successfully: session
// is
//   marked as going away, future migrations unblocked.
// - migrate back to default network task executed: session is already on the
//   default network, no-op.
TEST_P(QuicStreamFactoryTest,
       MigrateOnWriteErrorWithNetworkMadeDefaultQueuedEarlier) {
  TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
      /*disconnected=*/false);
}

// Sets up the connection migration test where network change notification is
// queued AFTER connection migration attempt on write error is posted.
void QuicStreamFactoryTestBase::
    TestMigrationOnWriteErrorWithNotificationQueuedLater(bool disconnected) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream. This should cause a write error,
  // which triggers a connection migration attempt. This will queue a
  // migration attempt in the message loop.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Now queue a network change notification in the message loop behind
  // the migration attempt.
  if (disconnected) {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkDisconnected(kDefaultNetworkForTests);
  } else {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkMadeDefault(kNewNetworkForTests);
  }

  base::RunLoop().RunUntilIdle();
  // Verify session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - write error is triggered: session posts a task to complete connection
//   migration.
// - a notification that alternate network is made default is queued.
// - connection migration attempt proceeds successfully, session is marked as
//   going away.
// - new default notification is delivered after connection migration has been
//   completed.
TEST_P(QuicStreamFactoryTest,
       MigrateOnWriteErrorWithNetworkMadeDefaultQueuedLater) {
  TestMigrationOnWriteErrorWithNotificationQueuedLater(/*disconnected=*/false);
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - write error is triggered: session posts a task to complete connection
//   migration.
// - a notification that default network is diconnected is queued.
// - connection migration attempt proceeds successfully, session is marked as
//   going away.
// - disconnect notification is delivered after connection migration has been
//   completed.
TEST_P(QuicStreamFactoryTest,
       MigrateOnWriteErrorWithNetworkDisconnectedQueuedLater) {
  TestMigrationOnWriteErrorWithNotificationQueuedLater(/*disconnected=*/true);
}

// This tests connection migration on write error with signals delivered in the
// following order:
// - a synchronous/asynchronous write error is triggered base on
//   |write_error_mode|: connection migration attempt is posted.
// - old default network disconnects, migration waits for a new network.
// - after a pause, new network is connected: session will migrate to new
//   network immediately.
// - migration on writer error is exectued and aborts as writer passed in is no
//   longer active in use.
// - new network is made default.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorPauseBeforeConnected(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(write_error_mode, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // On a DISCONNECTED notification, nothing happens.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // Add a new network and notify the stream factory of a new connected network.
  // This causes a PING packet to be sent over the new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop migration for write error can finish.
  runner_->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // There should be no posted tasks not executed, no way to migrate back to
  // default network.
  EXPECT_TRUE(runner_->GetPostedTasks().empty());

  // Receive signal to mark new network as default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnSyncWriteErrorPauseBeforeConnected) {
  TestMigrationOnWriteErrorPauseBeforeConnected(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnAsyncWriteErrorPauseBeforeConnected) {
  TestMigrationOnWriteErrorPauseBeforeConnected(ASYNC);
}

// This test verifies that when session successfully migrate to the alternate
// network, packet write error on the old writer will be ignored and will not
// trigger connection migration on write error.
TEST_P(QuicStreamFactoryTest, IgnoreWriteErrorFromOldWriterAfterMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can verify whether the migrate on
  // write error task is posted.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakePingPacket(3, /*include_version=*/true));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            4, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // There will be two pending task, one will complete migration with no delay
  // and the other will attempt to migrate back to the default network with
  // delay.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a write error will be delivered to the old
  // packet writer. Verify no additional task is posted.
  socket_data.Resume();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  stream.reset();
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when session successfully migrate to the alternate
// network, packet read error on the old reader will be ignored and will not
// close the connection.
TEST_P(QuicStreamFactoryTest, IgnoreReadErrorFromOldReaderAfterMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakePingPacket(2, /*include_version=*/true));
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         3, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            4, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // There will be two pending task, one will complete migration with no delay
  // and the other will attempt to migrate back to the default network with
  // delay.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that after migration on network is executed, packet
// read error on the old reader will be ignored and will not close the
// connection.
TEST_P(QuicStreamFactoryTest, IgnoreReadErrorOnOldReaderDuringMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakePingPacket(2, /*include_version=*/true));
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         3, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            4, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // There will be two pending task, one will complete migration with no delay
  // and the other will attempt to migrate back to the default network with
  // delay.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that after migration on write error is posted, packet
// read error on the old reader will be ignored and will not close the
// connection.
TEST_P(QuicStreamFactoryTest,
       IgnoreReadErrorOnOldReaderDuringPendingMigrationOnWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(ASYNC, ERR_FAILED);              // Write error.
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);  // Read error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));

  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  socket_data1.AddRead(ASYNC, ERR_FAILED);  // Read error to close connection.
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop to complete asynchronous write and read with errors.
  base::RunLoop().RunUntilIdle();
  // There will be one pending task to complete migration on write error.
  // Verify session is not closed with read error.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Complete migration.
  task_runner->RunUntilIdle();
  // There will be one more task posted attempting to migrate back to the
  // default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume to consume the read error on new socket, which will close
  // the connection.
  socket_data1.Resume();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// Migrate on asynchronous write error, old network disconnects after alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectAfterConnectAysnc) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      ASYNC, /*disconnect_before_connect*/ false);
}

// Migrate on synchronous write error, old network disconnects after alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectAfterConnectSync) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      SYNCHRONOUS, /*disconnect_before_connect*/ false);
}

// Migrate on asynchronous write error, old network disconnects before alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectBeforeConnectAysnc) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      ASYNC, /*disconnect_before_connect*/ true);
}

// Migrate on synchronous write error, old network disconnects before alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectBeforeConnectSync) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      SYNCHRONOUS, /*disconnect_before_connect*/ true);
}

// Setps up test which verifies that session successfully migrate to alternate
// network with signals delivered in the following order:
// *NOTE* Signal (A) and (B) can reverse order based on
// |disconnect_before_connect|.
// - (No alternate network is connected) session connects to
//   kDefaultNetworkForTests.
// - An async/sync write error is encountered based on |write_error_mode|:
//   session posted task to migrate session on write error.
// - Posted task is executed, miration moves to pending state due to lack of
//   alternate network.
// - (A) An alternate network is connected, pending migration completes.
// - (B) Old default network disconnects, no migration will be attempted as
//   session has already migrate to the alternate network.
// - The alternate network is made default.
void QuicStreamFactoryTestBase::
    TestMigrationOnWriteErrorWithMultipleNotifications(
        IoMode write_error_mode,
        bool disconnect_before_connect) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data.AddWrite(write_error_mode, ERR_FAILED);  // Write error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop so that posted task to migrate to socket will be
  // executed. A new task will be posted to wait for a new network.
  base::RunLoop().RunUntilIdle();

  // In this particular code path, the network will not yet be marked
  // as going away and the session will still be alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1;
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            3, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  if (disconnect_before_connect) {
    // Now deliver a DISCONNECT notification.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

    // Now deliver a CONNECTED notification and completes migration.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkConnected(kNewNetworkForTests);
  } else {
    // Now deliver a CONNECTED notification and completes migration.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkConnected(kNewNetworkForTests);

    // Now deliver a DISCONNECT notification.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  }
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // This is the callback for the response headers that returned
  // pending previously, because no result was available.  Check that
  // the result is now available due to the successful migration.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Deliver a MADEDEFAULT notification.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(session, GetActiveSession(host_port_pair_));

  stream.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigration) {
  allow_server_migration_ = true;
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1;
  quic::QuicStreamOffset header_stream_offset = 0;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(1, &header_stream_offset));
  socket_data1.AddWrite(SYNCHRONOUS, ConstructGetRequestPacket(
                                         2, GetNthClientInitiatedStreamId(0),
                                         true, true, &header_stream_offset));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  IPEndPoint ip;
  session->GetDefaultSocket()->GetPeerAddress(&ip);
  DVLOG(1) << "Socket connected to: " << ip.address().ToString() << " "
           << ip.port();

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data2;
  socket_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakePingPacket(3, /*include_version=*/true));
  socket_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(1, GetNthClientInitiatedStreamId(0),
                                       false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            4, false, GetNthClientInitiatedStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 1, 1, 1, true));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  const uint8_t kTestIpAddress[] = {1, 2, 3, 4};
  const uint16_t kTestPort = 123;
  session->Migrate(NetworkChangeNotifier::kInvalidNetworkHandle,
                   IPEndPoint(IPAddress(kTestIpAddress), kTestPort), true,
                   net_log_);

  session->GetDefaultSocket()->GetPeerAddress(&ip);
  DVLOG(1) << "Socket migrated to: " << ip.address().ToString() << " "
           << ip.port();

  // The session should be alive and active.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv4ToIPv4) {
  // Add alternate IPv4 server address to config.
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  quic::QuicConfig config;
  config.SetAlternateServerAddressToSend(
      quic::QuicSocketAddress(quic::QuicSocketAddressImpl(alt_address)));
  VerifyServerMigration(config, alt_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv6ToIPv6) {
  // Add a resolver rule to make initial connection to an IPv6 address.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "fe80::aebc:32ff:febb:1e33", "");
  // Add alternate IPv6 server address to config.
  IPEndPoint alt_address = IPEndPoint(
      IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 123);
  quic::QuicConfig config;
  config.SetAlternateServerAddressToSend(
      quic::QuicSocketAddress(quic::QuicSocketAddressImpl(alt_address)));
  VerifyServerMigration(config, alt_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv6ToIPv4) {
  // Add a resolver rule to make initial connection to an IPv6 address.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "fe80::aebc:32ff:febb:1e33", "");
  // Add alternate IPv4 server address to config.
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  quic::QuicConfig config;
  config.SetAlternateServerAddressToSend(
      quic::QuicSocketAddress(quic::QuicSocketAddressImpl(alt_address)));
  IPEndPoint expected_address(
      ConvertIPv4ToIPv4MappedIPv6(alt_address.address()), alt_address.port());
  VerifyServerMigration(config, expected_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv4ToIPv6Fails) {
  allow_server_migration_ = true;
  Initialize();

  // Add a resolver rule to make initial connection to an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(), "1.2.3.4",
                                            "");
  // Add alternate IPv6 server address to config.
  IPEndPoint alt_address = IPEndPoint(
      IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 123);
  quic::QuicConfig config;
  config.SetAlternateServerAddressToSend(
      quic::QuicSocketAddress(quic::QuicSocketAddressImpl(alt_address)));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  crypto_client_stream_factory_.SetConfig(config);

  // Set up only socket data provider.
  MockQuicData socket_data1;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(2, true, GetNthClientInitiatedStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  IPEndPoint actual_address;
  session->GetDefaultSocket()->GetPeerAddress(&actual_address);
  // No migration should have happened.
  IPEndPoint expected_address =
      IPEndPoint(IPAddress(1, 2, 3, 4), kDefaultServerPort);
  EXPECT_EQ(actual_address, expected_address);
  DVLOG(1) << "Socket connected to: " << actual_address.address().ToString()
           << " " << actual_address.port();
  DVLOG(1) << "Expected address: " << expected_address.address().ToString()
           << " " << expected_address.port();

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnSSLConfigChanged) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructClientRstPacket(2, quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       3, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(1, nullptr));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  ssl_config_service_->NotifySSLConfigChange();
  EXPECT_EQ(ERR_CERT_DATABASE_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_->require_confirmation());

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  stream = CreateStream(&request2);
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnCertDBChanged) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(1, nullptr));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  // Change the CA cert and verify that stream saw the event.
  factory_->OnCertDBChanged();

  EXPECT_FALSE(factory_->require_confirmation());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2);
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_NE(session, session2);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, SharedCryptoConfig) {
  Initialize();

  std::vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r1_host_name("r1");
    string r2_host_name("r2");
    r1_host_name.append(cannoncial_suffixes[i]);
    r2_host_name.append(cannoncial_suffixes[i]);

    HostPortPair host_port_pair1(r1_host_name, 80);
    quic::QuicCryptoClientConfig* crypto_config =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get());
    quic::QuicServerId server_id1(host_port_pair1.host(),
                                  host_port_pair1.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r1_host_name);
    cached1->SetProofValid();

    HostPortPair host_port_pair2(r2_host_name, 80);
    quic::QuicServerId server_id2(host_port_pair2.host(),
                                  host_port_pair2.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config->LookupOrCreate(server_id2);
    EXPECT_EQ(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, CryptoConfigWhenProofIsInvalid) {
  Initialize();
  std::vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r3_host_name("r3");
    string r4_host_name("r4");
    r3_host_name.append(cannoncial_suffixes[i]);
    r4_host_name.append(cannoncial_suffixes[i]);

    HostPortPair host_port_pair1(r3_host_name, 80);
    quic::QuicCryptoClientConfig* crypto_config =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get());
    quic::QuicServerId server_id1(host_port_pair1.host(),
                                  host_port_pair1.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r3_host_name);
    cached1->SetProofInvalid();

    HostPortPair host_port_pair2(r4_host_name, 80);
    quic::QuicServerId server_id2(host_port_pair2.host(),
                                  host_port_pair2.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config->LookupOrCreate(server_id2);
    EXPECT_NE(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->source_address_token().empty());
    EXPECT_FALSE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, EnableNotLoadFromDiskCache) {
  Initialize();
  factory_->set_require_confirmation(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));

  // If we are waiting for disk cache, we would have posted a task. Verify that
  // the CancelWaitForDataReady task hasn't been posted.
  ASSERT_EQ(0u, runner_->GetPostedTasks().size());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ReducePingTimeoutOnConnectionTimeOutOpenStreams) {
  reduced_ping_timeout_seconds_ = 10;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(1, nullptr));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server2(kServer2HostName, kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Quic should use default PING timeout when no previous connection times out
  // with open stream.
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs),
            QuicStreamFactoryPeer::GetPingTimeout(factory_.get()));
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs),
            session->connection()->ping_timeout());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  DVLOG(1)
      << "Created 1st session and initialized a stream. Now trigger timeout";
  session->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // The first connection times out with open stream, QUIC should reduce initial
  // PING time for subsequent connections.
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(10),
            QuicStreamFactoryPeer::GetPingTimeout(factory_.get()));

  // Test two-in-a-row timeouts with open streams.
  DVLOG(1) << "Create 2nd session and timeout with open stream";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  QuicChromiumClientSession* session2 = GetActiveSession(server2);
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(10),
            session2->connection()->ping_timeout());

  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));
  session2->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Verifies that the QUIC stream factory is initialized correctly.
TEST_P(QuicStreamFactoryTest, MaybeInitialize) {
  VerifyInitialization();
}

TEST_P(QuicStreamFactoryTest, StartCertVerifyJob) {
  Initialize();

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Save current state of |race_cert_verification|.
  bool race_cert_verification =
      QuicStreamFactoryPeer::GetRaceCertVerification(factory_.get());

  // Load server config.
  HostPortPair host_port_pair(kDefaultServerHostName, kDefaultServerPort);
  quic::QuicServerId quic_server_id(host_port_pair_.host(),
                                    host_port_pair_.port(),
                                    privacy_mode_ == PRIVACY_MODE_ENABLED);
  QuicStreamFactoryPeer::CacheDummyServerConfig(factory_.get(), quic_server_id);

  QuicStreamFactoryPeer::SetRaceCertVerification(factory_.get(), true);
  EXPECT_FALSE(HasActiveCertVerifierJob(quic_server_id));

  // Start CertVerifyJob.
  quic::QuicAsyncStatus status = QuicStreamFactoryPeer::StartCertVerifyJob(
      factory_.get(), quic_server_id, /*cert_verify_flags=*/0, net_log_);
  if (status == quic::QUIC_PENDING) {
    // Verify CertVerifierJob has started.
    EXPECT_TRUE(HasActiveCertVerifierJob(quic_server_id));

    while (HasActiveCertVerifierJob(quic_server_id)) {
      base::RunLoop().RunUntilIdle();
    }
  }
  // Verify CertVerifierJob has finished.
  EXPECT_FALSE(HasActiveCertVerifierJob(quic_server_id));

  // Start a QUIC request.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Restore |race_cert_verification|.
  QuicStreamFactoryPeer::SetRaceCertVerification(factory_.get(),
                                                 race_cert_verification);

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  // Verify there are no outstanding CertVerifierJobs after request has
  // finished.
  EXPECT_FALSE(HasActiveCertVerifierJob(quic_server_id));
}

TEST_P(QuicStreamFactoryTest, YieldAfterPackets) {
  Initialize();
  factory_->set_require_confirmation(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  QuicStreamFactoryPeer::SetYieldAfterPackets(factory_.get(), 0);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ConstructClientConnectionClosePacket(0));
  socket_data.AddRead(ASYNC, OK);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicChromiumPacketReader::StartReading
  // posts a task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_chromium_packet_reader.cc",
                                       "StartReading");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));

  // Call run_loop so that QuicChromiumPacketReader::OnReadComplete() gets
  // called.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicChromiumPacketReader::StartReading() has posted only one task and
  // yielded the read.
  EXPECT_EQ(1u, observer.executed_count());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_FALSE(stream.get());  // Session is already closed.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, YieldAfterDuration) {
  Initialize();
  factory_->set_require_confirmation(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  QuicStreamFactoryPeer::SetYieldAfterDuration(
      factory_.get(), quic::QuicTime::Delta::FromMilliseconds(-1));

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ConstructClientConnectionClosePacket(0));
  socket_data.AddRead(ASYNC, OK);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicChromiumPacketReader::StartReading
  // posts a task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_chromium_packet_reader.cc",
                                       "StartReading");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK, request.Request(host_port_pair_, version_, privacy_mode_,
                                DEFAULT_PRIORITY, SocketTag(),
                                /*cert_verify_flags=*/0, url_, net_log_,
                                &net_error_details_,
                                failed_on_default_network_callback_,
                                callback_.callback()));

  // Call run_loop so that QuicChromiumPacketReader::OnReadComplete() gets
  // called.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicChromiumPacketReader::StartReading() has posted only one task and
  // yielded the read.
  EXPECT_EQ(1u, observer.executed_count());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_FALSE(stream.get());  // Session is already closed.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerPushSessionAffinity) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));

  string url = "https://www.example.org/";

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  quic::QuicClientPromisedInfo promised(
      session, GetNthServerInitiatedStreamId(0), kDefaultUrl);
  (*QuicStreamFactoryPeer::GetPushPromiseIndex(factory_.get())
        ->promised_by_url())[kDefaultUrl] = &promised;

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(host_port_pair_, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback_.callback()));

  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));
}

TEST_P(QuicStreamFactoryTest, ServerPushPrivacyModeMismatch) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1;
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(2, true, GetNthServerInitiatedStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));

  string url = "https://www.example.org/";
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  quic::QuicClientPromisedInfo promised(
      session, GetNthServerInitiatedStreamId(0), kDefaultUrl);

  quic::QuicClientPushPromiseIndex* index =
      QuicStreamFactoryPeer::GetPushPromiseIndex(factory_.get());

  (*index->promised_by_url())[kDefaultUrl] = &promised;
  EXPECT_EQ(index->GetPromised(kDefaultUrl), &promised);

  // Doing the request should not use the push stream, but rather
  // cancel it because the privacy modes do not match.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                host_port_pair_, version_, PRIVACY_MODE_ENABLED,
                DEFAULT_PRIORITY, SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));
  EXPECT_EQ(index->GetPromised(kDefaultUrl), nullptr);

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Pool to existing session with matching quic::QuicServerId
// even if destination is different.
TEST_P(QuicStreamFactoryTest, PoolByOrigin) {
  Initialize();

  HostPortPair destination1("first.example.com", 443);
  HostPortPair destination2("second.example.com", 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(destination2, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url_, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_TRUE(session1->SharesSameSession(*session2));
  EXPECT_EQ(quic::QuicServerId(host_port_pair_.host(), host_port_pair_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

class QuicStreamFactoryWithDestinationTest
    : public QuicStreamFactoryTestBase,
      public ::testing::TestWithParam<PoolingTestParams> {
 protected:
  QuicStreamFactoryWithDestinationTest()
      : QuicStreamFactoryTestBase(
            GetParam().version,
            GetParam().client_headers_include_h2_stream_dependency),
        destination_type_(GetParam().destination_type),
        hanging_read_(SYNCHRONOUS, ERR_IO_PENDING, 0) {}

  HostPortPair GetDestination() {
    switch (destination_type_) {
      case SAME_AS_FIRST:
        return origin1_;
      case SAME_AS_SECOND:
        return origin2_;
      case DIFFERENT:
        return HostPortPair(kDifferentHostname, 443);
      default:
        NOTREACHED();
        return HostPortPair();
    }
  }

  void AddHangingSocketData() {
    std::unique_ptr<SequencedSocketData> sequenced_socket_data(
        new SequencedSocketData(base::make_span(&hanging_read_, 1),
                                base::span<MockWrite>()));
    socket_factory_->AddSocketDataProvider(sequenced_socket_data.get());
    sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data));
  }

  bool AllDataConsumed() {
    for (const auto& socket_data_ptr : sequenced_socket_data_vector_) {
      if (!socket_data_ptr->AllReadDataConsumed() ||
          !socket_data_ptr->AllWriteDataConsumed()) {
        return false;
      }
    }
    return true;
  }

  DestinationType destination_type_;
  HostPortPair origin1_;
  HostPortPair origin2_;
  MockRead hanging_read_;
  std::vector<std::unique_ptr<SequencedSocketData>>
      sequenced_socket_data_vector_;
};

INSTANTIATE_TEST_CASE_P(VersionIncludeStreamDependencySequence,
                        QuicStreamFactoryWithDestinationTest,
                        ::testing::ValuesIn(GetPoolingTestParams()));

// A single QUIC request fails because the certificate does not match the origin
// hostname, regardless of whether it matches the alternative service hostname.
TEST_P(QuicStreamFactoryWithDestinationTest, InvalidCertificate) {
  if (destination_type_ == DIFFERENT)
    return;

  Initialize();

  GURL url("https://mail.example.com/");
  origin1_ = HostPortPair::FromURL(url);

  // Not used for requests, but this provides a test case where the certificate
  // is valid for the hostname of the alternative service.
  origin2_ = HostPortPair("mail.example.org", 433);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_FALSE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  AddHangingSocketData();

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          /*cert_verify_flags=*/0, url, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_QUIC_HANDSHAKE_FAILED));

  EXPECT_TRUE(AllDataConsumed());
}

// QuicStreamRequest is pooled based on |destination| if certificate matches.
TEST_P(QuicStreamFactoryWithDestinationTest, SharedCertificate) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  std::unique_ptr<quic::QuicEncryptedPacket> settings_packet(
      client_maker_.MakeInitialSettingsPacket(1, nullptr));
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, settings_packet->data(),
                                  settings_packet->length(), 1)};
  std::unique_ptr<SequencedSocketData> sequenced_socket_data(
      new SequencedSocketData(reads, writes));
  socket_factory_->AddSocketDataProvider(sequenced_socket_data.get());
  sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data));

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK, request2.Request(destination, version_, privacy_mode_,
                                 DEFAULT_PRIORITY, SocketTag(),
                                 /*cert_verify_flags=*/0, url2, net_log_,
                                 &net_error_details_,
                                 failed_on_default_network_callback_,
                                 callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_TRUE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(origin1_.host(), origin1_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());

  EXPECT_TRUE(AllDataConsumed());
}

// QuicStreamRequest is not pooled if PrivacyMode differs.
TEST_P(QuicStreamFactoryWithDestinationTest, DifferentPrivacyMode) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  std::unique_ptr<quic::QuicEncryptedPacket> settings_packet(
      client_maker_.MakeInitialSettingsPacket(1, nullptr));
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, settings_packet->data(),
                                  settings_packet->length(), 1)};
  std::unique_ptr<SequencedSocketData> sequenced_socket_data(
      new SequencedSocketData(reads, writes));
  socket_factory_->AddSocketDataProvider(sequenced_socket_data.get());
  sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data));
  std::unique_ptr<SequencedSocketData> sequenced_socket_data1(
      new SequencedSocketData(reads, writes));
  socket_factory_->AddSocketDataProvider(sequenced_socket_data1.get());
  sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data1));

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                destination, version_, PRIVACY_MODE_ENABLED, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  EXPECT_EQ(OK, callback2.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // |request2| does not pool to the first session, because PrivacyMode does not
  // match.  Instead, another session is opened to the same destination, but
  // with a different quic::QuicServerId.
  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_FALSE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(origin1_.host(), origin1_.port(), false),
            session1->server_id());
  EXPECT_EQ(quic::QuicServerId(origin2_.host(), origin2_.port(), true),
            session2->server_id());

  EXPECT_TRUE(AllDataConsumed());
}

// QuicStreamRequest is not pooled if certificate does not match its origin.
TEST_P(QuicStreamFactoryWithDestinationTest, DisjointCertificate) {
  Initialize();

  GURL url1("https://news.example.org/");
  GURL url2("https://mail.example.com/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert1->VerifyNameMatch(origin1_.host()));
  ASSERT_FALSE(cert1->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert1->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert1;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert2->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert2->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert2;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  std::unique_ptr<quic::QuicEncryptedPacket> settings_packet(
      client_maker_.MakeInitialSettingsPacket(1, nullptr));
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, settings_packet->data(),
                                  settings_packet->length(), 1)};
  std::unique_ptr<SequencedSocketData> sequenced_socket_data(
      new SequencedSocketData(reads, writes));
  socket_factory_->AddSocketDataProvider(sequenced_socket_data.get());
  sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data));
  std::unique_ptr<SequencedSocketData> sequenced_socket_data1(
      new SequencedSocketData(reads, writes));
  socket_factory_->AddSocketDataProvider(sequenced_socket_data1.get());
  sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data1));

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // |request2| does not pool to the first session, because the certificate does
  // not match.  Instead, another session is opened to the same destination, but
  // with a different quic::QuicServerId.
  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_FALSE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(origin1_.host(), origin1_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());
  EXPECT_EQ(quic::QuicServerId(origin2_.host(), origin2_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session2->server_id());

  EXPECT_TRUE(AllDataConsumed());
}

// This test verifies that QuicStreamFactory::ClearCachedStatesInCryptoConfig
// correctly transform an origin filter to a ServerIdFilter. Whether the
// deletion itself works correctly is tested in QuicCryptoClientConfigTest.
TEST_P(QuicStreamFactoryTest, ClearCachedStatesInCryptoConfig) {
  Initialize();
  quic::QuicCryptoClientConfig* crypto_config =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get());

  struct TestCase {
    TestCase(const std::string& host,
             int port,
             PrivacyMode privacy_mode,
             quic::QuicCryptoClientConfig* crypto_config)
        : server_id(host, port, privacy_mode),
          state(crypto_config->LookupOrCreate(server_id)) {
      std::vector<string> certs(1);
      certs[0] = "cert";
      state->SetProof(certs, "cert_sct", "chlo_hash", "signature");
      state->set_source_address_token("TOKEN");
      state->SetProofValid();

      EXPECT_FALSE(state->certs().empty());
    }

    quic::QuicServerId server_id;
    quic::QuicCryptoClientConfig::CachedState* state;
  } test_cases[] = {
      TestCase("www.google.com", 443, privacy_mode_, crypto_config),
      TestCase("www.example.com", 443, privacy_mode_, crypto_config),
      TestCase("www.example.com", 4433, privacy_mode_, crypto_config)};

  // Clear cached states for the origin https://www.example.com:4433.
  GURL origin("https://www.example.com:4433");
  factory_->ClearCachedStatesInCryptoConfig(base::Bind(
      static_cast<bool (*)(const GURL&, const GURL&)>(::operator==), origin));
  EXPECT_FALSE(test_cases[0].state->certs().empty());
  EXPECT_FALSE(test_cases[1].state->certs().empty());
  EXPECT_TRUE(test_cases[2].state->certs().empty());

  // Clear all cached states.
  factory_->ClearCachedStatesInCryptoConfig(
      base::Callback<bool(const GURL&)>());
  EXPECT_TRUE(test_cases[0].state->certs().empty());
  EXPECT_TRUE(test_cases[1].state->certs().empty());
  EXPECT_TRUE(test_cases[2].state->certs().empty());
}

// Passes connection options and client connection options to QuicStreamFactory,
// then checks that its internal quic::QuicConfig is correct.
TEST_P(QuicStreamFactoryTest, ConfigConnectionOptions) {
  connection_options_.push_back(quic::kTIME);
  connection_options_.push_back(quic::kTBBR);
  connection_options_.push_back(quic::kREJ);

  client_connection_options_.push_back(quic::kTBBR);
  client_connection_options_.push_back(quic::k1RTT);

  Initialize();

  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(connection_options_, config->SendConnectionOptions());
  EXPECT_TRUE(config->HasClientRequestedIndependentOption(
      quic::kTBBR, quic::Perspective::IS_CLIENT));
  EXPECT_TRUE(config->HasClientRequestedIndependentOption(
      quic::k1RTT, quic::Perspective::IS_CLIENT));
}

// Verifies that the host resolver uses the request priority passed to
// QuicStreamRequest::Request().
TEST_P(QuicStreamFactoryTest, HostResolverUsesRequestPriority) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, MAXIMUM_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->last_request_priority());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Passes |max_time_before_crypto_handshake_seconds| and
// |max_idle_time_before_crypto_handshake_seconds| to QuicStreamFactory, then
// checks that its internal quic::QuicConfig is correct.
TEST_P(QuicStreamFactoryTest, ConfigMaxTimeBeforeCryptoHandshake) {
  max_time_before_crypto_handshake_seconds_ = 11;
  max_idle_time_before_crypto_handshake_seconds_ = 13;

  Initialize();

  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(11),
            config->max_time_before_crypto_handshake());
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(13),
            config->max_idle_time_before_crypto_handshake());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds asynchronously, then crypto handshake fails synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackAsyncSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  // |host_resolver_| has not finished host resolution at this point, so
  // |host_resolution_callback| should not have a result.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  // Allow |host_resolver_| to finish host resolution.
  // Since the request fails immediately after host resolution (getting
  // ERR_FAILED from socket reads/writes), |host_resolution_callback| should be
  // called with ERR_QUIC_PROTOCOL_ERROR since that's the next result in
  // forming the connection.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolution_callback.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, host_resolution_callback.WaitForResult());

  // Calling WaitForHostResolution() a second time should return
  // false since host resolution has finished already.
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds asynchronously, then crypto handshake fails asynchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackAsyncAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  factory_->set_require_confirmation(true);

  MockQuicData socket_data;
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_FAILED);
  socket_data.AddWrite(ASYNC, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  // |host_resolver_| has not finished host resolution at this point, so
  // |host_resolution_callback| should not have a result.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  // Allow |host_resolver_| to finish host resolution. Since crypto handshake
  // will hang after host resolution, |host_resolution_callback| should run with
  // ERR_IO_PENDING since that's the next result in forming the connection.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolution_callback.have_result());
  EXPECT_EQ(ERR_IO_PENDING, host_resolution_callback.WaitForResult());

  // Calling WaitForHostResolution() a second time should return
  // false since host resolution has finished already.
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  EXPECT_FALSE(callback_.have_result());
  socket_data.GetSequencedSocketData()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds synchronously, then crypto handshake fails synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackSyncSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);

  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has finished already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());
  EXPECT_FALSE(callback_.have_result());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds synchronously, then crypto handshake fails asynchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackSyncAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Host resolution will succeed synchronously, but Request() as a whole
  // will fail asynchronously.
  host_resolver_->set_synchronous_mode(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  factory_->set_require_confirmation(true);

  MockQuicData socket_data;
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_FAILED);
  socket_data.AddWrite(ASYNC, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has finished already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  EXPECT_FALSE(callback_.have_result());
  socket_data.GetSequencedSocketData()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution fails
// synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackFailSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Host resolution will fail synchronously.
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());
  host_resolver_->set_synchronous_mode(true);

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has failed already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution fails
// asynchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackFailAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  // Allow |host_resolver_| to fail host resolution. |host_resolution_callback|
  // Should run with ERR_NAME_NOT_RESOLVED since that's the error host
  // resolution failed with.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolution_callback.have_result());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, host_resolution_callback.WaitForResult());

  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());
}

// With dns race experiment turned on, and DNS resolve succeeds synchronously,
// the final connection is established through the resolved DNS. No racing
// connection.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceAndHostResolutionSync) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for synchronous return.
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_THAT(request.Request(
                  host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                  SocketTag(),
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()),
              IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, DNS resolve returns async, no matching cache in
// host resolver, connection should be successful and through resolved DNS. No
// racing connection.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceAndHostResolutionAsync) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  // Cause the host resolution to return.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(host_resolution_callback.WaitForResult(), IsOk());
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, DNS resolve returns async, stale dns used,
// connects synchrounously, and then the resolved DNS matches.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveAsyncStaleMatch) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Check that the racing job is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Resolve dns and return.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kCachedIPAddress.ToString());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale dns used, connect
// async, and then the result matches.
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceHostResolveAsyncConnectAsyncStaleMatch) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_require_confirmation(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Send Crypto handshake so connect will call back.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  base::RunLoop().RunUntilIdle();

  // Check that the racing job is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Resolve dns and call back, make sure job finishes.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kCachedIPAddress.ToString());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale dns used, dns resolve
// return, then connection finishes and matches with the result.
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceHostResolveAsyncStaleMatchConnectAsync) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_require_confirmation(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Finish dns async, check we still need to wait for stale connection async.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_.have_result());

  // Finish stale connection async, and the stale connection should pass dns
  // validation.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kCachedIPAddress.ToString());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connects
// sync, but dns no match
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceHostResolveAsyncStaleSyncNoMatch) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  // Socket for the stale connection which will invoke connection closure.
  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       2, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket for the new connection.
  MockQuicData quic_data2;
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Check the stale connection is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish dns resolution and check the job has finished.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connects
// async, finishes before dns, but no match
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleAsyncResolveAsyncNoMatch) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_require_confirmation(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolvercache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       2, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2;
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Finish the stale connection.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish host resolution and check the job is done.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connects
// async, dns finishes first, but no match
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceResolveAsyncStaleAsyncNoMatch) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_require_confirmation(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  quic_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       1, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2;
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Finish dns resolution, but need to wait for stale connection.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      quic::QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve returns error sync, same behavior
// as experiment is not on
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveError) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set synchronous failure in resolver.
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  MockQuicData quic_data;
  quic_data.AddSocketDataToFactory(socket_factory_.get());
  QuicStreamRequest request(factory_.get());

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
}

// With dns race experiment on, no cache available, dns resolve returns error
// async
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveAsyncError) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set asynchronous failure in resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  MockQuicData quic_data;
  quic_data.AddSocketDataToFactory(socket_factory_.get());
  QuicStreamRequest request(factory_.get());

  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Resolve and expect result that shows the resolution error.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
}

// With dns race experiment on, dns resolve async, staled used and connects
// sync, dns returns error and no connection is established.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleSyncHostResolveError) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set asynchronous failure in resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  // Set up an address in the stale cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  // Socket for the stale connection which is supposed to disconnect.
  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       2, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Check that the stale connection is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish host resolution.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connection
// return error, then dns matches
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleErrorDNSMatches) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in host resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  // Simulate synchronous connect failure.
  MockQuicData quic_data;
  quic_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2;
  quic_data2.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));
}

// With dns race experiment on, dns resolve async, stale used and connection
// returns error, dns no match, new connection is established
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleErrorDNSNoMatch) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in host resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  // Add failure for the stale connection.
  MockQuicData quic_data;
  quic_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2;
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Check that the stale connection fails.
  EXPECT_FALSE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish host resolution and check the job finishes ok.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(
      session->peer_address().impl().socket_address().ToStringWithoutPort(),
      kNonCachedIPAddress);

  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connection
// returns error, dns no match, new connection error
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleErrorDNSNoMatchError) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in host resolver asynchronously.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  // Add failure for stale connection.
  MockQuicData quic_data;
  quic_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Add failure for resolved dns connection.
  MockQuicData quic_data2;
  quic_data2.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Check the stale connection fails.
  EXPECT_FALSE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Check the resolved dns connection fails.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));
}

// With dns race experiment on, dns resolve async and stale connect async, dns
// resolve returns error and then preconnect finishes
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceResolveAsyncErrorStaleAsync) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Add asynchronous failure in host resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());
  factory_->set_require_confirmation(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  // Set up an address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  // Socket data for stale connection which is supposed to disconnect.
  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  quic_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       1, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // host resolution returned but stale connection hasn't finished yet.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async and stale connect async, dns
// resolve returns error and then preconnect fails.
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceResolveAsyncErrorStaleAsyncError) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Add asynchronous failure to host resolver.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_require_confirmation(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  // Set up an address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), ADDRESS_FAMILY_UNSPECIFIED, 0);
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->OnNetworkChange();

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  quic_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       1, true, quic::QUIC_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Host Resolution returns failure but stale connection hasn't finished.
  host_resolver_->ResolveAllPending();

  // Check that the final error is on resolution failure.
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
}

// With dns race experiment on, test that host resolution callback behaves
// normal as experiment is not on
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveAsync) {
  race_stale_dns_on_connection_ = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  MockQuicData quic_data;
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(),
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // Check that expect_on_host_resolution_ is properlly set.
  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(host_resolution_callback.WaitForResult(), IsOk());

  // Check that expect_on_host_resolution_ is flipped back.
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Test that QuicStreamRequests with similar and different tags results in
// reused and unique QUIC streams using appropriately tagged sockets.
TEST_P(QuicStreamFactoryTest, Tag) {
  MockTaggingClientSocketFactory* socket_factory =
      new MockTaggingClientSocketFactory();
  socket_factory_.reset(socket_factory);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Prepare to establish two QUIC sessions.
  MockQuicData socket_data;
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  MockQuicData socket_data2;
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

#if defined(OS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
#else
  // On non-Android platforms we can only use the default constructor.
  SocketTag tag1, tag2;
#endif

  // Request a stream with |tag1|.
  QuicStreamRequest request1(factory_.get());
  int rv = request1.Request(
      host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, tag1,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_THAT(callback_.GetResult(rv), IsOk());
  EXPECT_EQ(socket_factory->GetLastProducedUDPSocket()->tag(), tag1);
  EXPECT_TRUE(socket_factory->GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());
  std::unique_ptr<QuicChromiumClientSession::Handle> stream1 =
      request1.ReleaseSessionHandle();
  EXPECT_TRUE(stream1);
  EXPECT_TRUE(stream1->IsConnected());

  // Request a stream with |tag1| and verify underlying session is reused.
  QuicStreamRequest request2(factory_.get());
  rv = request2.Request(
      host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, tag1,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_THAT(callback_.GetResult(rv), IsOk());
  std::unique_ptr<QuicChromiumClientSession::Handle> stream2 =
      request2.ReleaseSessionHandle();
  EXPECT_TRUE(stream2);
  EXPECT_TRUE(stream2->IsConnected());
  EXPECT_TRUE(stream2->SharesSameSession(*stream1));

  // Request a stream with |tag2| and verify a new session is created.
  QuicStreamRequest request3(factory_.get());
  rv = request3.Request(
      host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, tag2,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_THAT(callback_.GetResult(rv), IsOk());
  EXPECT_EQ(socket_factory->GetLastProducedUDPSocket()->tag(), tag2);
  EXPECT_TRUE(socket_factory->GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());
  std::unique_ptr<QuicChromiumClientSession::Handle> stream3 =
      request3.ReleaseSessionHandle();
  EXPECT_TRUE(stream3);
  EXPECT_TRUE(stream3->IsConnected());
#if defined(OS_ANDROID)
  EXPECT_FALSE(stream3->SharesSameSession(*stream1));
#else
  // Same tag should reuse session.
  EXPECT_TRUE(stream3->SharesSameSession(*stream1));
#endif
}

}  // namespace test
}  // namespace net
