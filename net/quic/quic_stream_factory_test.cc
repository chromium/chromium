// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"
#include <sys/types.h>

#include <memory>
#include <ostream>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/properties_based_quic_server_info.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_client_session_peer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_stream_factory_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_constants.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_path_validator_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quiche/spdy/test_tools/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using std::string;

namespace net::test {

class QuicHttpStreamPeer {
 public:
  static QuicChromiumClientSession::Handle* GetSessionHandle(
      HttpStream* stream) {
    return static_cast<QuicHttpStream*>(stream)->quic_session();
  }
};

namespace {

const char kDefaultServerHostName[] = "www.example.org";
const char kServer2HostName[] = "mail.example.org";
const char kServer3HostName[] = "docs.example.org";
const char kServer4HostName[] = "images.example.org";
const char kServer5HostName[] = "accounts.example.org";
const char kDifferentHostname[] = "different.example.com";
const int kDefaultServerPort = 443;
const char kDefaultUrl[] = "https://www.example.org/";
const char kServer2Url[] = "https://mail.example.org/";
const char kServer3Url[] = "https://docs.example.org/";
const char kServer4Url[] = "https://images.example.org/";
const char kServer5Url[] = "https://images.example.org/";
const size_t kMinRetryTimeForDefaultNetworkSecs = 1;
const size_t kWaitTimeForNewNetworkSecs = 10;
const quic::QuicConnectionId kNewCID = quic::test::TestConnectionId(12345678);

// Run QuicStreamFactoryTest instances with all value combinations of version
// and enable_connection_racting.
struct TestParams {
  quic::ParsedQuicVersion version;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return ParsedQuicVersionToString(p.version);
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      AllSupportedQuicVersions();
  for (const auto& version : all_supported_versions) {
    params.push_back(TestParams{version});
  }
  return params;
}

}  // namespace

// TestConnectionMigrationSocketFactory will vend sockets with incremental fake
// IPV4 address.
class TestConnectionMigrationSocketFactory : public MockClientSocketFactory {
 public:
  TestConnectionMigrationSocketFactory() = default;

  TestConnectionMigrationSocketFactory(
      const TestConnectionMigrationSocketFactory&) = delete;
  TestConnectionMigrationSocketFactory& operator=(
      const TestConnectionMigrationSocketFactory&) = delete;

  ~TestConnectionMigrationSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    SocketDataProvider* data_provider = mock_data().GetNext();
    auto socket = std::make_unique<MockUDPClientSocket>(data_provider, net_log);
    socket->set_source_host(IPAddress(192, 0, 2, next_source_host_num_++));
    return std::move(socket);
  }

 private:
  uint8_t next_source_host_num_ = 1u;
};

// TestPortMigrationSocketFactory will vend sockets with incremental port
// number.
class TestPortMigrationSocketFactory : public MockClientSocketFactory {
 public:
  TestPortMigrationSocketFactory() = default;

  TestPortMigrationSocketFactory(const TestPortMigrationSocketFactory&) =
      delete;
  TestPortMigrationSocketFactory& operator=(
      const TestPortMigrationSocketFactory&) = delete;

  ~TestPortMigrationSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    SocketDataProvider* data_provider = mock_data().GetNext();
    auto socket = std::make_unique<MockUDPClientSocket>(data_provider, net_log);
    socket->set_source_port(next_source_port_num_++);
    return std::move(socket);
  }

 private:
  uint16_t next_source_port_num_ = 1u;
};

class MockQuicStreamFactory : public QuicStreamFactory {
 public:
  MockQuicStreamFactory(
      NetLog* net_log,
      HostResolver* host_resolver,
      SSLConfigService* ssl_config_service,
      ClientSocketFactory* client_socket_factory,
      HttpServerProperties* http_server_properties,
      CertVerifier* cert_verifier,
      CTPolicyEnforcer* ct_policy_enforcer,
      TransportSecurityState* transport_security_state,
      SCTAuditingDelegate* sct_auditing_delegate,
      SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
      QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
      QuicContext* context)
      : QuicStreamFactory(net_log,
                          host_resolver,
                          ssl_config_service,
                          client_socket_factory,
                          http_server_properties,
                          cert_verifier,
                          ct_policy_enforcer,
                          transport_security_state,
                          sct_auditing_delegate,
                          socket_performance_watcher_factory,
                          quic_crypto_client_stream_factory,
                          context) {}

  MockQuicStreamFactory(const MockQuicStreamFactory&) = delete;
  MockQuicStreamFactory& operator=(const MockQuicStreamFactory&) = delete;

  ~MockQuicStreamFactory() override = default;

  MOCK_METHOD0(MockFinishConnectAndConfigureSocket, void());

  void FinishConnectAndConfigureSocket(CompletionOnceCallback callback,
                                       DatagramClientSocket* socket,
                                       const SocketTag& socket_tag,
                                       int rv) override {
    QuicStreamFactory::FinishConnectAndConfigureSocket(std::move(callback),
                                                       socket, socket_tag, rv);
    MockFinishConnectAndConfigureSocket();
  }
};

class QuicStreamFactoryTestBase : public WithTaskEnvironment {
 protected:
  QuicStreamFactoryTestBase(
      quic::ParsedQuicVersion version,
      std::vector<base::test::FeatureRef> enabled_features = {},
      std::vector<base::test::FeatureRef> disabled_features = {})
      : host_resolver_(std::make_unique<MockHostResolver>(
            /*default_result=*/MockHostResolverBase::RuleResolver::
                GetLocalhostResult())),
        socket_factory_(std::make_unique<MockClientSocketFactory>()),
        runner_(base::MakeRefCounted<TestTaskRunner>(context_.mock_clock())),
        version_(version),
        client_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(
                          context_.random_generator()),
                      context_.clock(),
                      kDefaultServerHostName,
                      quic::Perspective::IS_CLIENT,
                      true),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(
                          context_.random_generator()),
                      context_.clock(),
                      kDefaultServerHostName,
                      quic::Perspective::IS_SERVER,
                      false),
        http_server_properties_(std::make_unique<HttpServerProperties>()),
        cert_verifier_(std::make_unique<MockCertVerifier>()),
        failed_on_default_network_callback_(base::BindRepeating(
            &QuicStreamFactoryTestBase::OnFailedOnDefaultNetwork,
            base::Unretained(this))),
        quic_params_(context_.params()) {
    enabled_features.push_back(features::kAsyncQuicSession);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    FLAGS_quic_enable_http3_grease_randomness = false;
    context_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void Initialize() {
    DCHECK(!factory_);
    factory_ = std::make_unique<QuicStreamFactory>(
        net_log_.net_log(), host_resolver_.get(), &ssl_config_service_,
        socket_factory_.get(), http_server_properties_.get(),
        cert_verifier_.get(), &ct_policy_enforcer_, &transport_security_state_,
        /*sct_auditing_delegate=*/nullptr,
        /*SocketPerformanceWatcherFactory*/ nullptr,
        &crypto_client_stream_factory_, &context_);
  }

  void InitializeConnectionMigrationV2Test(
      NetworkChangeNotifier::NetworkList connected_networks) {
    scoped_mock_network_change_notifier_ =
        std::make_unique<ScopedMockNetworkChangeNotifier>();
    MockNetworkChangeNotifier* mock_ncn =
        scoped_mock_network_change_notifier_->mock_network_change_notifier();
    mock_ncn->ForceNetworkHandlesSupported();
    mock_ncn->SetConnectedNetworksList(connected_networks);
    quic_params_->migrate_sessions_on_network_change_v2 = true;
    quic_params_->migrate_sessions_early_v2 = true;
    socket_factory_ = std::make_unique<TestConnectionMigrationSocketFactory>();
    Initialize();
  }

  // Make a NEW_CONNECTION_ID frame available for client such that connection
  // migration can begin with a new connection ID. A side effect of calling
  // this function is that ACK_FRAME that should have been sent for the first
  // packet read might be skipped in the unit test. If the order of ACKing is
  // important for a test, use QuicTestPacketMaker::MakeNewConnectionIdPacket
  // instead.
  void MaybeMakeNewConnectionIdAvailableToSession(
      const quic::QuicConnectionId& new_cid,
      quic::QuicSession* session,
      uint64_t sequence_number = 1u) {
    quic::QuicNewConnectionIdFrame new_cid_frame;
    new_cid_frame.connection_id = new_cid;
    new_cid_frame.sequence_number = sequence_number;
    new_cid_frame.retire_prior_to = 0u;
    new_cid_frame.stateless_reset_token =
        quic::QuicUtils::GenerateStatelessResetToken(
            new_cid_frame.connection_id);
    session->connection()->OnNewConnectionIdFrame(new_cid_frame);
  }

  std::unique_ptr<HttpStream> CreateStream(QuicStreamRequest* request) {
    std::unique_ptr<QuicChromiumClientSession::Handle> session =
        request->ReleaseSessionHandle();
    if (!session || !session->IsConnected())
      return nullptr;

    std::set<std::string> dns_aliases =
        session->GetDnsAliasesForSessionKey(request->session_key());
    return std::make_unique<QuicHttpStream>(std::move(session),
                                            std::move(dns_aliases));
  }

  bool HasActiveSession(
      const url::SchemeHostPort& scheme_host_port,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey()) {
    quic::QuicServerId server_id(scheme_host_port.host(),
                                 scheme_host_port.port(), false);
    return QuicStreamFactoryPeer::HasActiveSession(factory_.get(), server_id,
                                                   network_anonymization_key);
  }

  bool HasActiveJob(const url::SchemeHostPort& scheme_host_port,
                    const PrivacyMode privacy_mode,
                    bool require_dns_https_alpn = false) {
    quic::QuicServerId server_id(scheme_host_port.host(),
                                 scheme_host_port.port(),
                                 privacy_mode == PRIVACY_MODE_ENABLED);
    return QuicStreamFactoryPeer::HasActiveJob(factory_.get(), server_id,
                                               require_dns_https_alpn);
  }

  // Get the pending, not activated session, if there is only one session alive.
  QuicChromiumClientSession* GetPendingSession(
      const url::SchemeHostPort& scheme_host_port) {
    quic::QuicServerId server_id(scheme_host_port.host(),
                                 scheme_host_port.port(), false);
    return QuicStreamFactoryPeer::GetPendingSession(factory_.get(), server_id,
                                                    scheme_host_port);
  }

  QuicChromiumClientSession* GetActiveSession(
      const url::SchemeHostPort& scheme_host_port,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey(),
      bool require_dns_https_alpn = false) {
    quic::QuicServerId server_id(scheme_host_port.host(),
                                 scheme_host_port.port(), false);
    return QuicStreamFactoryPeer::GetActiveSession(factory_.get(), server_id,
                                                   network_anonymization_key,
                                                   require_dns_https_alpn);
  }

  int GetSourcePortForNewSessionAndGoAway(
      const url::SchemeHostPort& destination) {
    return GetSourcePortForNewSessionInner(destination, true);
  }

  int GetSourcePortForNewSessionInner(const url::SchemeHostPort& destination,
                                      bool goaway_received) {
    // Should only be called if there is no active session for this destination.
    EXPECT_FALSE(HasActiveSession(destination));
    size_t socket_count = socket_factory_->udp_client_socket_ports().size();

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    GURL url("https://" + destination.host() + "/");
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  ConstructServerConnectionClosePacket(uint64_t num) {
    return server_maker_.MakeConnectionClosePacket(
        num, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!");
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code) {
    quic::QuicStreamId stream_id =
        GetNthClientInitiatedBidirectionalStreamId(0);
    return client_maker_.MakeRstPacket(packet_number, stream_id, error_code);
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
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin) {
    spdy::Http2HeaderBlock headers =
        client_maker_.GetRequestHeaders("GET", "https", "/");
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    return client_maker_.MakeRequestHeadersPacket(packet_number, stream_id, fin,
                                                  priority, std::move(headers),
                                                  &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructOkResponsePacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin) {
    spdy::Http2HeaderBlock headers = server_maker_.GetResponseHeaders("200");
    size_t spdy_headers_frame_len;
    return server_maker_.MakeResponseHeadersPacket(packet_number, stream_id,
                                                   fin, std::move(headers),
                                                   &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket() {
    return client_maker_.MakeInitialSettingsPacket(1);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number);
  }

  // Helper method for server migration tests.
  void VerifyServerMigration(const quic::QuicConfig& config,
                             IPEndPoint expected_address) {
    quic_params_->allow_server_migration = true;
    Initialize();

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.SetConfig(config);

    // Set up first socket data provider.
    MockQuicData socket_data1(version_);
    socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data1.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after
    // migration.
    MockQuicData socket_data2(version_);
    client_maker_.set_connection_id(kNewCID);
    socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    int packet_num = 1;
    socket_data2.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
    socket_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakePingPacket(packet_num++));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRetireConnectionIdPacket(packet_num++,
                                                   /*sequence_number=*/0u));
    socket_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeDataPacket(
                              packet_num++, GetQpackDecoderStreamId(), false,
                              StreamCancellationQpackDecoderInstruction(0)));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
    socket_data2.AddSocketDataToFactory(socket_factory_.get());

    // Create request and QuicHttpStream.
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
    stream->RegisterRequest(&request_info);
    EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                           CompletionOnceCallback()));
    // Ensure that session is alive and active.
    QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  // If |vary_network_anonymization_key| is true, stores data for two different
  // NetworkAnonymizationKeys, but the same server. If false, stores data for
  // two different servers, using the same NetworkAnonymizationKey.
  void VerifyInitialization(bool vary_network_anonymization_key) {
    const SchemefulSite kSite1(GURL("https://foo.test/"));
    const SchemefulSite kSite2(GURL("https://bar.test/"));

    const auto network_anonymization_key1 =
        NetworkAnonymizationKey::CreateSameSite(kSite1);
    quic::QuicServerId quic_server_id1(
        kDefaultServerHostName, kDefaultServerPort, PRIVACY_MODE_DISABLED);

    NetworkAnonymizationKey network_anonymization_key2;
    quic::QuicServerId quic_server_id2;

    if (vary_network_anonymization_key) {
      network_anonymization_key2 =
          NetworkAnonymizationKey::CreateSameSite(kSite2);
      quic_server_id2 = quic_server_id1;
    } else {
      network_anonymization_key2 = network_anonymization_key1;
      quic_server_id2 = quic::QuicServerId(kServer2HostName, kDefaultServerPort,
                                           PRIVACY_MODE_DISABLED);
    }

    quic_params_->max_server_configs_stored_in_properties = 1;
    quic_params_->idle_connection_timeout = base::Seconds(500);
    Initialize();
    factory_->set_is_quic_known_to_work_on_current_network(true);
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
        kProtoQUIC, scheme_host_port_.host(), scheme_host_port_.port());
    AlternativeServiceInfoVector alternative_service_info_vector;
    base::Time expiration = base::Time::Now() + base::Days(1);
    alternative_service_info_vector.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service1, expiration, {version_}));
    http_server_properties_->SetAlternativeServices(
        url::SchemeHostPort("https", quic_server_id1.host(),
                            quic_server_id1.port()),
        network_anonymization_key1, alternative_service_info_vector);

    const AlternativeService alternative_service2(
        kProtoQUIC, quic_server_id2.host(), quic_server_id2.port());
    AlternativeServiceInfoVector alternative_service_info_vector2;
    alternative_service_info_vector2.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service2, expiration, {version_}));

    http_server_properties_->SetAlternativeServices(
        url::SchemeHostPort("https", quic_server_id2.host(),
                            quic_server_id2.port()),
        network_anonymization_key2, alternative_service_info_vector2);
    // Verify that the properties of both QUIC servers are stored in the
    // HTTP properties map.
    EXPECT_EQ(2U,
              http_server_properties_->server_info_map_for_testing().size());

    http_server_properties_->SetMaxServerConfigsStoredInProperties(
        kDefaultMaxQuicServerEntries);

    std::unique_ptr<QuicServerInfo> quic_server_info =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            quic_server_id1, network_anonymization_key1,
            http_server_properties_.get());

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

    // Create temporary strings because Persist() clears string data in |state|.
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

    std::unique_ptr<QuicServerInfo> quic_server_info2 =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            quic_server_id2, network_anonymization_key2,
            http_server_properties_.get());
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

    // Create temporary strings because Persist() clears string data in
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
    const HttpServerProperties::QuicServerInfoMap& quic_server_info_map =
        http_server_properties_->quic_server_info_map();
    EXPECT_EQ(2u, quic_server_info_map.size());
    auto quic_server_info_map_it = quic_server_info_map.begin();
    EXPECT_EQ(quic_server_info_map_it->first.server_id, quic_server_id2);
    ++quic_server_info_map_it;
    EXPECT_EQ(quic_server_info_map_it->first.server_id, quic_server_id1);

    host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                              "192.168.0.1", "");

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  url::SchemeHostPort(url::kHttpsScheme, quic_server_id1.host(),
                                      quic_server_id1.port()),
                  version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                  network_anonymization_key1, SecureDnsPolicy::kAllow,
                  /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());

    EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
        factory_.get(), quic_server_id1, network_anonymization_key1));

    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1 =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_anonymization_key1);
    quic::QuicCryptoClientConfig::CachedState* cached =
        crypto_config_handle1->GetConfig()->LookupOrCreate(quic_server_id1);
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
    MockQuicData socket_data2(version_);
    socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    client_maker_.Reset();
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data2.AddSocketDataToFactory(socket_factory_.get());

    host_resolver_->rules()->ClearRules();
    host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                              "192.168.0.2", "");

    QuicStreamRequest request2(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request2.Request(
            url::SchemeHostPort(url::kHttpsScheme, quic_server_id2.host(),
                                quic_server_id2.port()),
            version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
            network_anonymization_key2, SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
            /*cert_verify_flags=*/0,
            vary_network_anonymization_key ? url_
                                           : GURL("https://mail.example.org/"),
            net_log_, &net_error_details_, failed_on_default_network_callback_,
            callback_.callback()));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());

    EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
        factory_.get(), quic_server_id2, network_anonymization_key2));
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2 =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_anonymization_key2);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config_handle2->GetConfig()->LookupOrCreate(quic_server_id2);
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

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) const {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetQpackDecoderStreamId() const {
    return quic::test::GetNthClientInitiatedUnidirectionalStreamId(
        version_.transport_version, 1);
  }

  std::string StreamCancellationQpackDecoderInstruction(int n) const {
    return StreamCancellationQpackDecoderInstruction(n, true);
  }

  std::string StreamCancellationQpackDecoderInstruction(
      int n,
      bool create_stream) const {
    const quic::QuicStreamId cancelled_stream_id =
        GetNthClientInitiatedBidirectionalStreamId(n);
    EXPECT_LT(cancelled_stream_id, 63u);

    const char opcode = 0x40;
    if (create_stream) {
      return {0x03, static_cast<char>(opcode | cancelled_stream_id)};
    } else {
      return {static_cast<char>(opcode | cancelled_stream_id)};
    }
  }

  std::string ConstructDataHeader(size_t body_len) {
    quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
        body_len, quiche::SimpleBufferAllocator::Get());
    return std::string(buffer.data(), buffer.size());
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      absl::string_view data) {
    return server_maker_.MakeDataPacket(packet_number, stream_id, fin, data);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        version_.transport_version, n);
  }

  void OnFailedOnDefaultNetwork(int rv) { failed_on_default_network_ = true; }

  // Helper methods for tests of connection migration on write error.
  void TestMigrationOnWriteErrorNonMigratableStream(IoMode write_error_mode,
                                                    bool migrate_idle_sessions);
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
  void
  TestThatBlackHoleIsDisabledOnNoNewNetworkThenResumedAfterConnectingToANetwork(
      bool is_blackhole_disabled_after_disconnecting);
  void TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QuicErrorCode error);
  void TestOnNetworkMadeDefaultNonMigratableStream(bool migrate_idle_sessions);
  void TestMigrateSessionEarlyNonMigratableStream(bool migrate_idle_sessions);
  void TestOnNetworkDisconnectedNoOpenStreams(bool migrate_idle_sessions);
  void TestOnNetworkMadeDefaultNoOpenStreams(bool migrate_idle_sessions);
  void TestOnNetworkDisconnectedNonMigratableStream(bool migrate_idle_sessions);

  // Port migrations.
  void TestSimplePortMigrationOnPathDegrading();

  // Tests for DNS HTTPS record with alpn.
  void TestRequireDnsHttpsAlpn(
      std::vector<HostResolverEndpointResult> endpoints,
      bool expect_success);

  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<MockHostResolverBase> host_resolver_;
  TestSSLConfigService ssl_config_service_{SSLContextConfig()};
  std::unique_ptr<MockClientSocketFactory> socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockQuicContext context_;
  scoped_refptr<TestTaskRunner> runner_;
  const quic::ParsedQuicVersion version_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  TransportSecurityState transport_security_state_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  std::unique_ptr<ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier_;
  std::unique_ptr<QuicStreamFactory> factory_;
  url::SchemeHostPort scheme_host_port_{
      url::kHttpsScheme, kDefaultServerHostName, kDefaultServerPort};
  GURL url_{kDefaultUrl};
  GURL url2_{kServer2Url};
  GURL url3_{kServer3Url};
  GURL url4_{kServer4Url};
  GURL url5_{kServer5Url};

  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  NetLogWithSource net_log_;
  TestCompletionCallback callback_;
  const CompletionRepeatingCallback failed_on_default_network_callback_;
  bool failed_on_default_network_ = false;
  NetErrorDetails net_error_details_;

  raw_ptr<QuicParams> quic_params_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class QuicStreamFactoryTest : public QuicStreamFactoryTestBase,
                              public ::testing::TestWithParam<TestParams> {
 protected:
  QuicStreamFactoryTest() : QuicStreamFactoryTestBase(GetParam().version) {}
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicStreamFactoryTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicStreamFactoryTest, CreateSyncQuicSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Will reset stream 3.
  stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(OK,
            request3.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  stream = CreateStream(&request3);  // Will reset stream 5.
  stream.reset();                    // Will reset stream 7.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CreateAsyncQuicSession) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Will reset stream 3.
  stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(OK,
            request3.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  stream = CreateStream(&request3);  // Will reset stream 5.
  stream.reset();                    // Will reset stream 7.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test uses synchronous QUIC session creation
TEST_P(QuicStreamFactoryTest, SyncCreateZeroRtt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, AsyncCreateZeroRtt) {
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  int rv = request.Request(
      scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Regression test for crbug.com/1117331.
TEST_P(QuicStreamFactoryTest, AsyncZeroRtt) {
  Initialize();

  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(nullptr, CreateStream(&request));

  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, DefaultInitialRtt) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(session->require_confirmation());
  EXPECT_EQ(100000u, session->connection()->GetStats().srtt_us);
  ASSERT_FALSE(session->config()->HasInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, FactoryDestroyedWhenJobPending) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  auto request = std::make_unique<QuicStreamRequest>(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request->Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  request.reset();
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  // Tearing down a QuicStreamFactory with a pending Job should not cause any
  // crash. crbug.com/768343.
  factory_.reset();
}

TEST_P(QuicStreamFactoryTest, RequireConfirmation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(session->require_confirmation());
}

TEST_P(QuicStreamFactoryTest, RequireConfirmationAsyncQuicSession) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(session->require_confirmation());
}

TEST_P(QuicStreamFactoryTest, DontRequireConfirmationFromSameIP) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(false);
  http_server_properties_->SetLastLocalAddressWhenQuicWorked(
      IPAddress(192, 0, 2, 33));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_FALSE(session->require_confirmation());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());
}

TEST_P(QuicStreamFactoryTest, CachedInitialRtt) {
  ServerNetworkStats stats;
  stats.srtt = base::Milliseconds(10);
  http_server_properties_->SetServerNetworkStats(
      url::SchemeHostPort(url_), NetworkAnonymizationKey(), stats);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_EQ(10000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(10000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

// Test that QUIC sessions use the cached RTT from HttpServerProperties for the
// correct NetworkAnonymizationKey.
TEST_P(QuicStreamFactoryTest, CachedInitialRttWithNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkAnonymizationKey for
       // QuicSessionAliasKey to include NetworkAnonymizationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  ServerNetworkStats stats;
  stats.srtt = base::Milliseconds(10);
  http_server_properties_->SetServerNetworkStats(
      url::SchemeHostPort(url_), kNetworkAnonymizationKey1, stats);
  quic_params_->estimate_initial_rtt = true;
  Initialize();

  for (const auto& network_anonymization_key :
       {kNetworkAnonymizationKey1, kNetworkAnonymizationKey2,
        NetworkAnonymizationKey()}) {
    SCOPED_TRACE(network_anonymization_key.ToDebugString());

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    QuicTestPacketMaker packet_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        true);

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddWrite(SYNCHRONOUS,
                         packet_maker.MakeInitialSettingsPacket(1));
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), network_anonymization_key, SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsOk());
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());

    QuicChromiumClientSession* session =
        GetActiveSession(scheme_host_port_, network_anonymization_key);
    if (network_anonymization_key == kNetworkAnonymizationKey1) {
      EXPECT_EQ(10000, session->connection()->GetStats().srtt_us);
      ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
      EXPECT_EQ(10000u, session->config()->GetInitialRoundTripTimeUsToSend());
    } else {
      EXPECT_EQ(quic::kInitialRttMs * 1000,
                session->connection()->GetStats().srtt_us);
      EXPECT_FALSE(session->config()->HasInitialRoundTripTimeUsToSend());
    }
  }
}

TEST_P(QuicStreamFactoryTest, 2gInitialRtt) {
  ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_2G);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_EQ(1000000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(1200000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, 3gInitialRtt) {
  ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_3G);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_EQ(400000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(400000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, GoAway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);

  session->OnHttp3GoAway(0);

  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Makes sure that setting and clearing ServerNetworkStats respects the
// NetworkAnonymizationKey.
TEST_P(QuicStreamFactoryTest, ServerNetworkStatsWithNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  const NetworkAnonymizationKey kNetworkAnonymizationKeys[] = {
      kNetworkAnonymizationKey1, kNetworkAnonymizationKey2,
      NetworkAnonymizationKey()};

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkAnonymizationKey for
       // QuicSessionAliasKey to include NetworkAnonymizationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();
  Initialize();

  // For each server, set up and tear down a QUIC session cleanly, and check
  // that stats have been added to HttpServerProperties using the correct
  // NetworkAnonymizationKey.
  for (size_t i = 0; i < std::size(kNetworkAnonymizationKeys); ++i) {
    SCOPED_TRACE(i);

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    QuicTestPacketMaker packet_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        true);

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddWrite(SYNCHRONOUS,
                         packet_maker.MakeInitialSettingsPacket(1));
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), kNetworkAnonymizationKeys[i], SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsOk());
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());

    QuicChromiumClientSession* session =
        GetActiveSession(scheme_host_port_, kNetworkAnonymizationKeys[i]);

    session->OnHttp3GoAway(0);

    EXPECT_FALSE(
        HasActiveSession(scheme_host_port_, kNetworkAnonymizationKeys[i]));

    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());

    for (size_t j = 0; j < std::size(kNetworkAnonymizationKeys); ++j) {
      // Stats up to kNetworkAnonymizationKeys[j] should have been populated,
      // all others should remain empty.
      if (j <= i) {
        EXPECT_TRUE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkAnonymizationKeys[j]));
      } else {
        EXPECT_FALSE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkAnonymizationKeys[j]));
      }
    }
  }

  // Use unmocked crypto stream to do crypto connect, since crypto errors result
  // in deleting network stats..
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // For each server, simulate an error during session creation, and check that
  // stats have been deleted from HttpServerProperties using the correct
  // NetworkAnonymizationKey.
  for (size_t i = 0; i < std::size(kNetworkAnonymizationKeys); ++i) {
    SCOPED_TRACE(i);

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
    socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), kNetworkAnonymizationKeys[i], SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_QUIC_HANDSHAKE_FAILED));

    EXPECT_FALSE(
        HasActiveSession(scheme_host_port_, kNetworkAnonymizationKeys[i]));

    for (size_t j = 0; j < std::size(kNetworkAnonymizationKeys); ++j) {
      // Stats up to kNetworkAnonymizationKeys[j] should have been deleted, all
      // others should still be populated.
      if (j <= i) {
        EXPECT_FALSE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkAnonymizationKeys[j]));
      } else {
        EXPECT_TRUE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkAnonymizationKeys[j]));
      }
    }
  }
}

TEST_P(QuicStreamFactoryTest, Pooling) {
  quic_params_->supported_versions = {version_};
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  const IPEndPoint kRightIP(*IPAddress::FromIPLiteral("192.168.0.1"),
                            kDefaultServerPort);
  const IPEndPoint kWrongIP(*IPAddress::FromIPLiteral("192.168.0.2"),
                            kDefaultServerPort);
  const std::string kRightALPN = quic::AlpnForVersion(version_);
  const std::string kWrongALPN = "h2";

  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                              kDefaultServerPort);
  url::SchemeHostPort server3(url::kHttpsScheme, kServer3HostName,
                              kDefaultServerPort);
  url::SchemeHostPort server4(url::kHttpsScheme, kServer4HostName,
                              kDefaultServerPort);
  url::SchemeHostPort server5(url::kHttpsScheme, kServer5HostName,
                              kDefaultServerPort);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  // `server2` resolves to the same IP address via A/AAAA records, i.e. without
  // ALPN information.
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // `server3` resolves to the same IP address, but only via an alternative
  // endpoint with matching ALPN.
  std::vector<HostResolverEndpointResult> endpoints(1);
  endpoints[0].ip_endpoints = {kRightIP};
  endpoints[0].metadata.supported_protocol_alpns = {kRightALPN};
  host_resolver_->rules()->AddRule(
      server3.host(),
      MockHostResolverBase::RuleResolver::RuleResult({std::move(endpoints)}));

  // `server4` resolves to the same IP address, but only via an alternative
  // endpoint with a mismatching ALPN.
  endpoints = std::vector<HostResolverEndpointResult>(2);
  endpoints[0].ip_endpoints = {kRightIP};
  endpoints[0].metadata.supported_protocol_alpns = {kWrongALPN};
  endpoints[1].ip_endpoints = {kWrongIP};
  endpoints[1].metadata.supported_protocol_alpns = {kRightALPN};
  host_resolver_->rules()->AddRule(
      server4.host(),
      MockHostResolverBase::RuleResolver::RuleResult({std::move(endpoints)}));

  // `server5` resolves to the same IP address via A/AAAA records, i.e. without
  // ALPN information.
  host_resolver_->rules()->AddIPLiteralRule(server5.host(), "192.168.0.1", "");

  // Establish a QUIC session to pool against.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // `server2` can pool with the existing session. Although the endpoint does
  // not specify ALPN, we connect here with preexisting knowledge of the version
  // (from Alt-Svc), so an A/AAAA match is sufficient.
  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  EXPECT_EQ(GetActiveSession(scheme_host_port_), GetActiveSession(server2));

  // `server3` can pool with the existing session. The endpoint's ALPN protocol
  // matches.
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(OK,
            request3.Request(
                server3, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url3_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream3 = CreateStream(&request3);
  EXPECT_TRUE(stream3.get());
  EXPECT_EQ(GetActiveSession(scheme_host_port_), GetActiveSession(server3));

  // `server4` cannot pool with the existing session. No endpoint matches both
  // IP and ALPN protocol.
  QuicStreamRequest request4(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request4.Request(
                server4, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url4_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream4 = CreateStream(&request4);
  EXPECT_TRUE(stream4.get());
  EXPECT_NE(GetActiveSession(scheme_host_port_), GetActiveSession(server4));

  // `server5` cannot pool with the existing session. Although the IP address
  // matches, if we connect without prior knowledge of QUIC support, endpoints
  // are only eligible for cross-name pooling when associated with a QUIC ALPN.
  //
  // Without pooling, the DNS response is insufficient to start a QUIC
  // connection, so the connection will fail.
  QuicStreamRequest request5(factory_.get());
  EXPECT_EQ(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN,
            request5.Request(
                server5, quic::ParsedQuicVersion::Unsupported(), privacy_mode_,
                DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
                SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/true,
                /*cert_verify_flags=*/0, url5_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/639916.
TEST_P(QuicStreamFactoryTest, PoolingWithServerMigration) {
  // Set up session to migrate.
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 443);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));
  config.SetPreferredAddressConnectionIdAndTokenToSend(
      kNewCID, quic::QuicUtils::GenerateStatelessResetToken(kNewCID));
  quic::QuicConnectionId cid_on_old_path =
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator());
  VerifyServerMigration(config, alt_address);

  // Close server-migrated session.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  session->CloseSessionOnError(0u, quic::QUIC_NO_ERROR,
                               quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  client_maker_.Reset();
  // Set up server IP, socket, proof, and config for new session.
  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                              kDefaultServerPort);
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.set_connection_id(cid_on_old_path);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

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
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  EXPECT_EQ(OK, callback.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());

  EXPECT_TRUE(HasActiveSession(server2));

  // No zombie entry in session map.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
}

TEST_P(QuicStreamFactoryTest, NoPoolingAfterGoAway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                              kDefaultServerPort);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  factory_->OnSessionGoingAway(GetActiveSession(scheme_host_port_));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveSession(server2));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request3.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback3.callback()));
  EXPECT_THAT(callback3.WaitForResult(), IsOk());
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  url::SchemeHostPort server1(url::kHttpsScheme, kDefaultServerHostName, 443);
  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  url::SchemeHostPort server1(url::kHttpsScheme, kDefaultServerHostName, 443);
  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName, 443);
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
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingWithDifferentPins) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  Initialize();

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  url::SchemeHostPort server1(url::kHttpsScheme, kDefaultServerHostName, 443);
  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName, 443);
  transport_security_state_.EnableStaticPinsForTesting();
  transport_security_state_.SetPinningListAlwaysTimelyForTesting(true);
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
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Mark the session as going away.  Ensure that while it is still alive
  // that it is no longer active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  factory_->OnSessionGoingAway(session);
  EXPECT_EQ(true,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  // Create a new request for the same destination and verify that a
  // new session is created.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_NE(session, GetActiveSession(scheme_host_port_));
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

  quic::QuicStreamId stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        packet_num++, 50,
                                        /*unidirectional=*/false));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), false,
                           StreamCancellationQpackDecoderInstruction(0)));
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRstPacket(packet_num++, stream_id,
                                               quic::QUIC_STREAM_CANCELLED));
  socket_data.AddRead(ASYNC, server_maker_.MakeRstPacket(
                                 1, stream_id, quic::QUIC_STREAM_CANCELLED));
  socket_data.AddRead(
      ASYNC, server_maker_.MakeMaxStreamsPacket(2, 52,
                                                /*unidirectional=*/false));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeAckPacket(packet_num++, 2, 1));
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
        scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
        SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
    stream->RegisterRequest(&request_info);
    EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                           CompletionOnceCallback()));
    streams.push_back(std::move(stream));
  }

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, CompletionOnceCallback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(ERR_IO_PENDING,
            stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                     callback_.callback()));

  // Close the first stream.
  streams.front()->Close(false);
  // Trigger exchange of RSTs that in turn allow progress for the last
  // stream.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  // Force close of the connection to suppress the generation of RST
  // packets when streams are torn down, which wouldn't be relevant to
  // this test anyway.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  session->connection()->CloseConnection(
      quic::QUIC_PUBLIC_RESET, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
}

TEST_P(QuicStreamFactoryTest, ResolutionErrorInCreate) {
  Initialize();
  MockQuicData socket_data(version_);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  host_resolver_->rules()->AddSimulatedFailure(kDefaultServerHostName);

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test uses synchronous QUIC session creation.
TEST_P(QuicStreamFactoryTest, SyncConnectErrorInCreate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  Initialize();

  MockQuicData socket_data(version_);
  socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, AsyncConnectErrorInCreate) {
  Initialize();

  MockQuicData socket_data(version_);
  socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test uses synchronous QUIC session creation.
TEST_P(QuicStreamFactoryTest, SyncCancelCreate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  Initialize();
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  {
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));
  }

  base::RunLoop().RunUntilIdle();

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, AsyncCancelCreate) {
  Initialize();
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  {
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
            /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));
  }

  base::RunLoop().RunUntilIdle();

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       packet_num++, quic::QUIC_PEER_GOING_AWAY, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Close the session and verify that stream saw the error.
  factory_->CloseAllSessions(ERR_INTERNET_DISCONNECTED,
                             quic::QUIC_PEER_GOING_AWAY);
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
       WriteErrorInCryptoConnectWithAsyncHostResolutionSyncSessionCreation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       WriteErrorInCryptoConnectWithAsyncHostResolutionAsyncSessionCreation) {
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       WriteErrorInCryptoConnectWithSyncHostResolutionSyncQuicSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail immediately.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Check no active session, or active jobs left for this server.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));

  base::RunLoop().RunUntilIdle();
  // Complete handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       WriteErrorInCryptoConnectWithSyncHostResolutionAsyncQuicSession) {
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail immediately.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  // Check no active session, or active jobs left for this server.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));

  base::RunLoop().RunUntilIdle();
  // Complete handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Regression test for crbug.com/1409382. Test that OnCreateSessionComplete()
// will not crash if sessions are closed after FinishCreateSession runs.
TEST_P(QuicStreamFactoryTest, CloseSessionDuringCreation) {
  quic_params_->close_sessions_on_ip_change = true;
  // close_sessions_on_ip_change == true requires
  // migrate_sessions_on_network_change_v2 == false.
  quic_params_->migrate_sessions_on_network_change_v2 = false;
  auto factory = MockQuicStreamFactory(
      net_log_.net_log(), host_resolver_.get(), &ssl_config_service_,
      socket_factory_.get(), http_server_properties_.get(),
      cert_verifier_.get(), &ct_policy_enforcer_, &transport_security_state_,
      /*sct_auditing_delegate=*/nullptr,
      /*SocketPerformanceWatcherFactory*/ nullptr,
      &crypto_client_stream_factory_, &context_);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       packet_num, quic::QUIC_IP_ADDRESS_CHANGED, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(&factory);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // QuicStreamFactory should be notified of IP address change after
  // FinishConnectAndConfigureSocket runs FinishCreateSession.
  EXPECT_CALL(factory, MockFinishConnectAndConfigureSocket()).WillOnce([] {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  });

  // Session should have been created before the factory is notified of IP
  // address change.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  quic::QuicServerId server_id(scheme_host_port_.host(),
                               scheme_host_port_.port(), false);
  EXPECT_TRUE(QuicStreamFactoryPeer::HasActiveSession(
      &factory, server_id, NetworkAnonymizationKey()));
  QuicChromiumClientSession* session = QuicStreamFactoryPeer::GetActiveSession(
      &factory, server_id, NetworkAnonymizationKey());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(&factory, session));

  base::RunLoop().RunUntilIdle();

  // Session should now be closed.
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(
      &factory, server_id, NetworkAnonymizationKey()));
}

TEST_P(QuicStreamFactoryTest, CloseSessionsOnIPAddressChanged) {
  quic_params_->close_sessions_on_ip_change = true;
  // close_sessions_on_ip_change == true requires
  // migrate_sessions_on_network_change_v2 == false.
  quic_params_->migrate_sessions_on_network_change_v2 = false;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                       packet_num, quic::QUIC_IP_ADDRESS_CHANGED, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Check an active session exists for the destination.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());
  // Change the IP address and verify that stream saw the error and the active
  // session is closed.
  NotifyIPAddressChanged();
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());
  // Check no active session exists for the destination.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  // Now attempting to request a stream to the same origin should create
  // a new session.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  stream = CreateStream(&request2);

  // Check a new active session exists for the destination and the old session
  // is no longer live.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  QuicChromiumClientSession* session2 = GetActiveSession(scheme_host_port_);
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
  quic_params_->goaway_sessions_on_ip_change = true;
  // close_sessions_on_ip_change == true requires
  // migrate_sessions_on_network_change_v2 == false.
  quic_params_->migrate_sessions_on_network_change_v2 = false;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data1(version_);
  int packet_num = 1;
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_num++));
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Receive an IP address change notification.
  NotifyIPAddressChanged();

  // The connection should still be alive, but marked as going away.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
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
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // Check an active session exists for the destination.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  QuicChromiumClientSession* session2 = GetActiveSession(scheme_host_port_);
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), false,
                           StreamCancellationQpackDecoderInstruction(0)));
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num, quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  // Change the IP address and verify that the connection is unaffected.
  NotifyIPAddressChanged();
  EXPECT_TRUE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  // Attempting a new request to the same origin uses the same connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  int packet_num = 1;
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_num++));
  quic_data1.AddWrite(
      write_mode,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  // in-flight SETTINGS and requests will be retransmitted. Since data is
  // already sent on the new address, ping will no longer be sent.
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeCombinedRetransmissionPacket(
                          /*original_packet_numbers=*/{1, 2}, packet_num++));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckAndDataPacket(
                          packet_num++, GetQpackDecoderStreamId(), 2, 2, false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  // Due to lack of alternate network, session will not migrate connection.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // A task was posted to migrate to the new default network. Execute that task.
  task_runner->RunUntilIdle();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket, declare probing as successful. And a new task to WriteToNewSocket
  // will be posted to complete migration.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be a task that will complete the migration to the new network.
  task_runner->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  int packet_num = 1;
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_num++));
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  MockQuicData quic_data2(version_);
  // First connectivity probe to be sent on the new path.
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  quic_data2.AddRead(ASYNC,
                     ERR_IO_PENDING);  // Pause so that we can control time.
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  // Second connectivity probe which will complete asynchronously.
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeCombinedRetransmissionPacket(
                          /*original_packet_numbers=*/{1, 2}, packet_num++));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeAckAndRetireConnectionIdPacket(packet_num++, 2, 1, 0u));
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeDataPacket(
                          packet_num++, GetQpackDecoderStreamId(), false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // A task was posted to migrate to the new default network. Execute that task.
  task_runner->RunUntilIdle();

  // Manually trigger retransmission of PATH_CHALLENGE.
  auto* path_validator =
      quic::test::QuicConnectionPeer::path_validator(session->connection());
  quic::test::QuicPathValidatorPeer::retry_timer(path_validator)->Cancel();
  path_validator->OnRetryTimeout();

  // Resume quic data and a connectivity probe response will be read on the new
  // socket, declare probing as successful.
  quic_data2.Resume();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // There should be a task that will complete the migration to the new network.
  task_runner->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Run the message loop to complete the asynchronous write of ack and ping.
  base::RunLoop().RunUntilIdle();

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
//   comes up in the next kWaitTimeForNewNetworkSecs seconds.
// - no new network is connected, migration times out. Session is closed.
TEST_P(QuicStreamFactoryTest, MigrationTimeoutWithNoNewNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Trigger connection migration. Since there are no networks
  // to migrate to, this should cause the session to wait for a new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The migration will not fail until the migration alarm timeout.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(true, session->connection()->writer()->IsWriteBlocked());

  // Migration will be timed out after kWaitTimeForNewNetwokSecs.
  task_runner->FastForwardBy(base::Seconds(kWaitTimeForNewNetworkSecs));

  // The connection should now be closed. A request for response
  // headers should fail.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test verifies that connectivity probes will be sent even if there is
// a non-migratable stream. However, when connection migrates to the
// successfully probed path, any non-migratable streams will be reset.
TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNonMigratableStream_MigrateIdleSessions) {
  TestOnNetworkMadeDefaultNonMigratableStream(true);
}

// This test verifies that connectivity probes will be sent even if there is
// a non-migratable stream. However, when connection migrates to the
// successfully probed path, any non-migratable stream will be reset. And if
// the connection becomes idle then, close the connection.
TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNonMigratableStream_DoNotMigrateIdleSessions) {
  TestOnNetworkMadeDefaultNonMigratableStream(false);
}

void QuicStreamFactoryTestBase::TestOnNetworkMadeDefaultNonMigratableStream(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));

  // Set up the second socket data provider that is used for probing.
  quic::QuicConnectionId cid_on_old_path =
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator());
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  MockQuicData quic_data1(version_);
  // Connectivity probe to be sent on the new path.
  quic_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data1.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));

  if (migrate_idle_sessions) {
    quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    quic_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataAndRstPacket(
                            packet_num++, GetQpackDecoderStreamId(),
                            StreamCancellationQpackDecoderInstruction(0),
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED));
    quic_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRetransmissionPacket(1, packet_num++));
    // Ping packet to send after migration is completed.
    quic_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
    quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, 0u));
  } else {
    client_maker_.set_connection_id(cid_on_old_path);
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataRstAckAndConnectionClosePacket(
                         packet_num++, GetQpackDecoderStreamId(),
                         StreamCancellationQpackDecoderInstruction(0),
                         GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_STREAM_CANCELLED, 1, 1,
                         quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                         "net error", /*path_response_frame*/ 0x1b));
  }

  socket_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Trigger connection migration. Session will start to probe the alternative
  // network. Although there is a non-migratable stream, session will still be
  // active until probing is declared as successful.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume data to read a connectivity probing response, which will cause
  // non-migtable streams to be closed.
  quic_data1.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(scheme_host_port_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  base::RunLoop().RunUntilIdle();

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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), false,
                           StreamCancellationQpackDecoderInstruction(0)));
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to continue but be marked as going away.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNonMigratableStream_DoNotMigrateIdleSessions) {
  TestOnNetworkDisconnectedNonMigratableStream(false);
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNonMigratableStream_MigrateIdleSessions) {
  TestOnNetworkDisconnectedNonMigratableStream(true);
}

void QuicStreamFactoryTestBase::TestOnNetworkDisconnectedNonMigratableStream(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData failed_socket_data(version_);
  quic::QuicConnectionId cid_on_old_path =
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator());
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MockQuicData socket_data(version_);
  if (migrate_idle_sessions) {
    failed_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    int packet_num = 1;
    failed_socket_data.AddWrite(SYNCHRONOUS,
                                ConstructInitialSettingsPacket(packet_num++));
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    failed_socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), false,
                         StreamCancellationQpackDecoderInstruction(0)));
    failed_socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
    failed_socket_data.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after migration.
    client_maker_.set_connection_id(cid_on_new_path);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    socket_data.AddWrite(SYNCHRONOUS,
                         client_maker_.MakeCombinedRetransmissionPacket(
                             {3, 1, 2}, packet_num++));
    // Ping packet to send after migration.
    socket_data.AddWrite(SYNCHRONOUS,
                         client_maker_.MakePingPacket(packet_num++));
    socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRetireConnectionIdPacket(packet_num++, 0u));
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  } else {
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    int packet_num = 1;
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
    socket_data.AddWrite(SYNCHRONOUS,
                         client_maker_.MakeDataPacket(
                             packet_num++, GetQpackDecoderStreamId(), false,
                             StreamCancellationQpackDecoderInstruction(0)));
    socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause a RST_STREAM frame to be emitted with
  // quic::QUIC_STREAM_CANCELLED error code.
  // If migrate idle session, the connection will then be migrated to the
  // alternate network. Otherwise, the connection will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_EQ(migrate_idle_sessions,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(scheme_host_port_));

  if (migrate_idle_sessions) {
    EXPECT_EQ(0u, session->GetNumActiveStreams());
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(failed_socket_data.AllReadDataConsumed());
    EXPECT_TRUE(failed_socket_data.AllWriteDataConsumed());
  }
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNoOpenStreams_DoNotMigrateIdleSessions) {
  TestOnNetworkMadeDefaultNoOpenStreams(false);
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNoOpenStreams_MigrateIdleSessions) {
  TestOnNetworkMadeDefaultNoOpenStreams(true);
}

void QuicStreamFactoryTestBase::TestOnNetworkMadeDefaultNoOpenStreams(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  if (!migrate_idle_sessions) {
    socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeConnectionClosePacket(
            packet_num, quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
            "net error"));
  }
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MockQuicData quic_data1(version_);
  if (migrate_idle_sessions) {
    client_maker_.set_connection_id(cid_on_new_path);
    // Set up the second socket data provider that is used for probing.
    // Connectivity probe to be sent on the new path.
    quic_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
    quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
    // Connectivity probe to receive from the server.
    quic_data1.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
    quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // in-flight SETTINGS and requests will be retransmitted. Since data is
    // already sent on the new address, ping will no longer be sent.
    quic_data1.AddWrite(ASYNC, client_maker_.MakeRetransmissionPacket(
                                   /*original_packet_number=*/1, packet_num++));
    quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, 0u));
    quic_data1.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(session->HasActiveRequestStreams());
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(scheme_host_port_));

  if (migrate_idle_sessions) {
    quic_data1.Resume();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(quic_data1.AllReadDataConsumed());
    EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  }
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNoOpenStreams_DoNotMigateIdleSessions) {
  TestOnNetworkDisconnectedNoOpenStreams(false);
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNoOpenStreams_MigateIdleSessions) {
  TestOnNetworkDisconnectedNoOpenStreams(true);
}

void QuicStreamFactoryTestBase::TestOnNetworkDisconnectedNoOpenStreams(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData default_socket_data(version_);
  default_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  default_socket_data.AddWrite(SYNCHRONOUS,
                               ConstructInitialSettingsPacket(packet_num++));
  default_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData alternate_socket_data(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  if (migrate_idle_sessions) {
    client_maker_.set_connection_id(cid_on_new_path);
    // Set up second socket data provider that is used after migration.
    alternate_socket_data.AddRead(SYNCHRONOUS,
                                  ERR_IO_PENDING);  // Hanging read.
    alternate_socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRetransmissionPacket(1, packet_num++));
    // Ping packet to send after migration.
    alternate_socket_data.AddWrite(SYNCHRONOUS,
                                   client_maker_.MakePingPacket(packet_num++));
    alternate_socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRetireConnectionIdPacket(packet_num++, 0u));
    alternate_socket_data.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is active.
  auto* session = GetActiveSession(scheme_host_port_);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(scheme_host_port_));

  EXPECT_TRUE(default_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(default_socket_data.AllWriteDataConsumed());
  if (migrate_idle_sessions) {
    EXPECT_TRUE(alternate_socket_data.AllReadDataConsumed());
    EXPECT_TRUE(alternate_socket_data.AllWriteDataConsumed());
  }
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
  client_maker_.set_save_packet_frames(true);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  int packet_number = 1;
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  socket_data.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++,
                       GetNthClientInitiatedBidirectionalStreamId(0), true));
  if (async_write_before) {
    socket_data.AddWrite(ASYNC, OK);
    packet_number++;
  }
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  if (async_write_before)
    session->connection()->SendPing();

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeCombinedRetransmissionPacket({1, 2}, packet_number++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_number++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_number++, 0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_number++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  base::RunLoop().RunUntilIdle();
  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  runner_->RunNextTask();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  client_maker_.set_save_packet_frames(true);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeCombinedRetransmissionPacket({1, 2}, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, 0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Add a new network and notify the stream factory of a new connected network.
  // This causes a PING packet to be sent over the new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  runner_->RunNextTask();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for probing on the
  // alternate network.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // First connectivity probe to receive from the server, which will complete
  // connection migraiton on path degrading.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  // Read multiple connectivity probes synchronously.
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(2));
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(3));
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(4));
  quic_data2.AddWrite(ASYNC, client_maker_.MakeAckAndRetransmissionPacket(
                                 packet_number++, 1, 4, 1, {1, 2}));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_number++, 0u));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 5, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndDataPacket(
                       packet_number++, GetQpackDecoderStreamId(), 5, 1, false,
                       StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be a task that will complete the migration to the new network.
  task_runner->RunUntilIdle();

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel migrate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  task_runner->FastForwardBy(base::Seconds(kMinRetryTimeForDefaultNetworkSecs));

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that the connection migrates to the alternate network
// early when path degrading is detected with an ASYNCHRONOUS write before
// migration.
TEST_P(QuicStreamFactoryTest, MigrateEarlyOnPathDegradingAsync) {
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  if (async_write_before) {
    quic_data1.AddWrite(ASYNC, OK);
    packet_number++;
  }
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  // in-flight SETTINGS and requests will be retransmitted. Since data is
  // already sent on the new address, ping will no longer be sent.
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeCombinedRetransmissionPacket(
                          /*original_packet_numbers=*/{1, 2}, packet_number++));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_number++, 0u));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndDataPacket(
                       packet_number++, GetQpackDecoderStreamId(), 2, 2, false,
                       StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  if (async_write_before)
    session->connection()->SendPing();

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be a task that will complete the migration to the new network.
  task_runner->RunUntilIdle();

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  task_runner->FastForwardBy(base::Seconds(kMinRetryTimeForDefaultNetworkSecs));

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionEarlyProbingWriterError) {
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
  MockQuicData quic_data1(version_);
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.

  // Set up the second socket data provider that is used for path validation.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_old_path =
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator());
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  ++packet_number;  // Account for the packet encountering write error.
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));

  // Connection ID is retired on the old path.
  client_maker_.set_connection_id(cid_on_old_path);
  quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_number++,
                                       /*sequence_number=*/1u));

  quic_data1.AddSocketDataToFactory(socket_factory_.get());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  // However, the probing writer will fail. This should result in a failed probe
  // but no connection close.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // There should be one task of notifying the session that probing failed, and
  // a second as a DoNothingAs callback.
  EXPECT_TRUE(session->connection()->HasPendingPathValidation());
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);
  // Verify that path validation is cancelled.
  EXPECT_FALSE(session->connection()->HasPendingPathValidation());

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  quic_data1.Resume();
  // Response headers are received on the original network..
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionEarlyProbingWriterErrorThreeNetworks) {
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
  base::RunLoop().RunUntilIdle();

  quic::QuicConnectionId cid_on_path1 =
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator());
  quic::QuicConnectionId cid_on_path2 = quic::test::TestConnectionId(12345678);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.

  // Set up the second socket data provider that is used for path validation.
  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(cid_on_path2);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  packet_number++;  // Account for packet encountering write error.

  // Connection ID is retired on the old path.
  client_maker_.set_connection_id(cid_on_path1);
  quic_data1.AddWrite(ASYNC, client_maker_.MakeRetireConnectionIdPacket(
                                 packet_number++,
                                 /*sequence_number=*/1u));

  // A socket will be created for a new path, but there would be no write
  // due to lack of new connection ID.
  MockQuicData quic_data3(version_);
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Pause

  quic_data1.AddSocketDataToFactory(socket_factory_.get());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_path2, session);
  base::RunLoop().RunUntilIdle();
  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  // However, the probing writer will fail. This should result in a failed probe
  // but no connection close.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // There should be one task of notifying the session that probing failed, and
  // one that was posted as a DoNothingAs callback.
  EXPECT_TRUE(session->connection()->HasPendingPathValidation());
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Trigger another path degrading, but this time another network is available.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kDefaultNetworkForTests, 3});
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();

  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);
  // Verify that the task is executed.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // No pending path validation as there is no connection ID available.
  EXPECT_FALSE(session->connection()->HasPendingPathValidation());

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  quic_data1.Resume();
  // Response headers are received on the original network..
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  base::RunLoop().RunUntilIdle();
  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  base::RunLoop().RunUntilIdle();
  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MultiPortSessionWithMigration) {
  // Turning on MPQC will implicitly turn on port migration.
  quic_params_->client_connection_options.push_back(quic::kMPQC);
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  quic_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for multi-port
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);

  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeConnectivityProbingPacket(2));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeAckAndPingPacket(4,
                                                /*largest_received=*/2,
                                                /*smallest_received=*/1));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       5, /*sequence_number=*/0u));
  quic_data2.AddRead(ASYNC, server_maker_.MakeAckPacket(3, 5, 1));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  quic_data2.AddWrite(ASYNC, client_maker_.MakeDataPacket(
                                 6, GetQpackDecoderStreamId(), false,
                                 StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeRstPacket(
                          7, GetNthClientInitiatedBidirectionalStreamId(0),
                          quic::QUIC_STREAM_CANCELLED));

  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  // Manually initialize the connection's self address. In real life, the
  // initialization will be done during crypto handshake.
  IPEndPoint ip;
  session->GetDefaultSocket()->GetLocalAddress(&ip);
  quic::test::QuicConnectionPeer::SetSelfAddress(session->connection(),
                                                 ToQuicSocketAddress(ip));

  // This will trigger multi-port path creation.
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);
  base::RunLoop().RunUntilIdle();

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Disable connection migration on the request streams.
  // This should have no effect for port migration.
  QuicChromiumClientStream* chrome_stream =
      static_cast<QuicChromiumClientStream*>(
          quic::test::QuicSessionPeer::GetStream(
              session, GetNthClientInitiatedBidirectionalStreamId(0)));
  EXPECT_TRUE(chrome_stream);
  chrome_stream->DisableConnectionMigrationToCellularNetwork();

  // Resume quic data and a connectivity probe response will be read on the new
  // socket. This makes the multi-port path ready to migrate.
  quic_data2.Resume();

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Cause the connection to report path degrading to the session.
  // Session will start migrate to multi-port path immediately.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  // The connection should still be degrading because no new packets are
  // received from the new path.
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The response is received on the new path.
  quic_data2.Resume();
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());
  task_runner->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Receives an ack from the server, this will be considered forward progress.
  quic_data2.Resume();
  task_runner->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  stream.reset();
  task_runner->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, SuccessfullyMigratedToServerPreferredAddress) {
  IPEndPoint server_preferred_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  FLAGS_quic_enable_chaos_protection = false;
  quic_params_->connection_options.push_back(quic::kSPAD);
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(
      ToQuicSocketAddress(server_preferred_address));
  quic::test::QuicConfigPeer::SetPreferredAddressConnectionIdAndToken(
      &config, kNewCID, quic::QuicUtils::GenerateStatelessResetToken(kNewCID));
  crypto_client_stream_factory_.SetConfig(config);
  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data1.AddWrite(ASYNC,
                      client_maker_.MakeDummyCHLOPacket(packet_number++));
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used to validate server
  // preferred address.
  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(kNewCID);
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  base::RunLoop().RunUntilIdle();

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  ASSERT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_FALSE(
      session->connection()->GetStats().server_preferred_address_validated);
  EXPECT_FALSE(session->connection()
                   ->GetStats()
                   .failed_to_validate_server_preferred_address);
  const quic::QuicSocketAddress peer_address = session->peer_address();

  quic_data2.Resume();
  EXPECT_FALSE(session->connection()->HasPendingPathValidation());
  EXPECT_TRUE(
      session->connection()->GetStats().server_preferred_address_validated);
  EXPECT_FALSE(session->connection()
                   ->GetStats()
                   .failed_to_validate_server_preferred_address);
  EXPECT_NE(session->peer_address(), peer_address);
  EXPECT_EQ(session->peer_address(),
            ToQuicSocketAddress(server_preferred_address));

  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, FailedToValidateServerPreferredAddress) {
  IPEndPoint server_preferred_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  FLAGS_quic_enable_chaos_protection = false;
  quic_params_->connection_options.push_back(quic::kSPAD);
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(
      ToQuicSocketAddress(server_preferred_address));
  quic::test::QuicConfigPeer::SetPreferredAddressConnectionIdAndToken(
      &config, kNewCID, quic::QuicUtils::GenerateStatelessResetToken(kNewCID));
  crypto_client_stream_factory_.SetConfig(config);
  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data1.AddWrite(ASYNC,
                      client_maker_.MakeDummyCHLOPacket(packet_number++));
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used to validate server
  // preferred address.
  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(kNewCID);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // One PATH_CHALLENGE + 2 retires.
  for (size_t i = 0; i < quic::QuicPathValidator::kMaxRetryTimes + 1; ++i) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeConnectivityProbingPacket(packet_number++));
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  base::RunLoop().RunUntilIdle();

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  ASSERT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_FALSE(
      session->connection()->GetStats().server_preferred_address_validated);
  EXPECT_FALSE(session->connection()
                   ->GetStats()
                   .failed_to_validate_server_preferred_address);
  const quic::QuicSocketAddress peer_address = session->peer_address();

  auto* path_validator =
      quic::test::QuicConnectionPeer::path_validator(session->connection());
  for (size_t i = 0; i < quic::QuicPathValidator::kMaxRetryTimes + 1; ++i) {
    quic::test::QuicPathValidatorPeer::retry_timer(path_validator)->Cancel();
    path_validator->OnRetryTimeout();
  }

  EXPECT_FALSE(session->connection()->HasPendingPathValidation());
  EXPECT_FALSE(
      session->connection()->GetStats().server_preferred_address_validated);
  EXPECT_TRUE(session->connection()
                  ->GetStats()
                  .failed_to_validate_server_preferred_address);
  EXPECT_EQ(session->peer_address(), peer_address);
  EXPECT_NE(session->peer_address(),
            ToQuicSocketAddress(server_preferred_address));

  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigratePortOnPathDegrading_WithoutNetworkHandle_PathValidator) {
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  TestSimplePortMigrationOnPathDegrading();
}

TEST_P(QuicStreamFactoryTest, PortMigrationDisabledOnPathDegrading) {
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeDataPacket(
                          packet_number++, GetQpackDecoderStreamId(), false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Disable connection migration on the request streams.
  // This should have no effect for port migration.
  QuicChromiumClientStream* chrome_stream =
      static_cast<QuicChromiumClientStream*>(
          quic::test::QuicSessionPeer::GetStream(
              session, GetNthClientInitiatedBidirectionalStreamId(0)));
  EXPECT_TRUE(chrome_stream);
  chrome_stream->DisableConnectionMigrationToCellularNetwork();

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Manually initialize the connection's self address. In real life, the
  // initialization will be done during crypto handshake.
  IPEndPoint ip;
  session->GetDefaultSocket()->GetLocalAddress(&ip);
  quic::test::QuicConnectionPeer::SetSelfAddress(session->connection(),
                                                 ToQuicSocketAddress(ip));

  // Set session config to have active migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Cause the connection to report path degrading to the session.
  // Session will start to probe a different port.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();

  // The session should stay alive as if nothing happened.
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       PortMigrationProbingReceivedStatelessReset_PathValidator) {
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // hanging read
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeDataPacket(
                          packet_number + 1, GetQpackDecoderStreamId(), false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number + 2,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for migration probing.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_number));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Stateless reset to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeStatelessResetPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Manually initialize the connection's self address. In real life, the
  // initialization will be done during crypto handshake.
  IPEndPoint ip;
  session->GetDefaultSocket()->GetLocalAddress(&ip);
  quic::test::QuicConnectionPeer::SetSelfAddress(session->connection(),
                                                 ToQuicSocketAddress(ip));

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe a different port.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a STATELESS_RESET is read from the probing path.
  quic_data2.Resume();

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Verify that the session is still active, and the request stream is active.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigratePortOnPathDegrading_WithNetworkHandle_PathValidator) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  TestSimplePortMigrationOnPathDegrading();
}

TEST_P(QuicStreamFactoryTest,
       MigratePortOnPathDegrading_WithMigration_PathValidator) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  TestSimplePortMigrationOnPathDegrading();
}

TEST_P(
    QuicStreamFactoryTest,
    TestPostNetworkOnMadeDefaultWhileConnectionMigrationFailOnUnexpectedErrorTwoDifferentSessions) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddWrite(ASYNC, OK);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddWrite(ASYNC, OK);
  socket_data2.AddSocketDataToFactory(socket_factory_.get());
  // Add new sockets to use post migration. Those are bad sockets and will cause
  // migration to fail.
  MockConnect connect_result = MockConnect(ASYNC, ERR_UNEXPECTED);
  SequencedSocketData socket_data3(connect_result, base::span<MockRead>(),
                                   base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data3);
  SequencedSocketData socket_data4(connect_result, base::span<MockRead>(),
                                   base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data4);

  url::SchemeHostPort server1(url::kHttpsScheme, kDefaultServerHostName, 443);
  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.2", "");

  // Create request and QuicHttpStream to create session1.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  // Create request and QuicHttpStream to create session2.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));
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
  stream2->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback_.callback()));

  EXPECT_EQ(2u, crypto_client_stream_factory_.streams().size());

  crypto_client_stream_factory_.streams()[0]->setHandshakeConfirmedForce(false);
  crypto_client_stream_factory_.streams()[1]->setHandshakeConfirmedForce(false);

  std::unique_ptr<QuicChromiumClientSession::Handle> handle1 =
      session1->CreateHandle(server1);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session2->CreateHandle(server2);
  mock_ncn->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  mock_ncn->NotifyNetworkMadeDefault(kNewNetworkForTests);

  NetErrorDetails details;
  handle1->PopulateNetErrorDetails(&details);
  EXPECT_EQ(
      quic::QuicErrorCode::QUIC_CONNECTION_MIGRATION_HANDSHAKE_UNCONFIRMED,
      details.quic_connection_error);
  EXPECT_EQ(false, details.quic_connection_migration_successful);

  handle2->PopulateNetErrorDetails(&details);
  EXPECT_EQ(
      quic::QuicErrorCode::QUIC_CONNECTION_MIGRATION_HANDSHAKE_UNCONFIRMED,
      details.quic_connection_error);
  EXPECT_EQ(false, details.quic_connection_migration_successful);
}

TEST_P(QuicStreamFactoryTest,
       TestPostNetworkMadeDefaultWhileConnectionMigrationFailBeforeHandshake) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_num = 1;
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  crypto_client_stream_factory_.last_stream()->setHandshakeConfirmedForce(
      false);

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session->CreateHandle(scheme_host_port_);
  mock_ncn->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  mock_ncn->NotifyNetworkConnected(kNewNetworkForTests);
  mock_ncn->NotifyNetworkMadeDefault(kNewNetworkForTests);

  NetErrorDetails details;
  handle->PopulateNetErrorDetails(&details);
  EXPECT_EQ(
      quic::QuicErrorCode::QUIC_CONNECTION_MIGRATION_HANDSHAKE_UNCONFIRMED,
      details.quic_connection_error);
  EXPECT_EQ(false, details.quic_connection_migration_successful);
}

// See crbug/1465889 for more details on what scenario is being tested.
TEST_P(
    QuicStreamFactoryTest,
    TestPostNetworkOnMadeDefaultWhileConnectionMigrationFailOnNoActiveStreams) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_num = 1;
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeConnectionClosePacket(
          packet_num, quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
          "net error"));

  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(session->HasActiveRequestStreams());

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session->CreateHandle(scheme_host_port_);
  mock_ncn->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  mock_ncn->NotifyNetworkConnected(kNewNetworkForTests);
  mock_ncn->NotifyNetworkMadeDefault(kNewNetworkForTests);

  NetErrorDetails details;
  handle->PopulateNetErrorDetails(&details);
  EXPECT_EQ(
      quic::QuicErrorCode::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
      details.quic_connection_error);
  EXPECT_EQ(false, details.quic_connection_migration_successful);
}

// See crbug/1465889 for more details on what scenario is being tested.
TEST_P(
    QuicStreamFactoryTest,
    TestPostNetworkOnMadeDefaultWhileConnectionMigrationFailOnUnexpectedError) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_num = 1;
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  MockQuicData quic_data2(version_);
  quic_data2.AddConnect(ASYNC, ERR_UNEXPECTED);

  quic_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session->CreateHandle(scheme_host_port_);
  mock_ncn->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  mock_ncn->NotifyNetworkConnected(kNewNetworkForTests);
  mock_ncn->NotifyNetworkMadeDefault(kNewNetworkForTests);

  NetErrorDetails details;
  handle->PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QuicErrorCode::QUIC_CONNECTION_MIGRATION_INTERNAL_ERROR,
            details.quic_connection_error);
  EXPECT_EQ(false, details.quic_connection_migration_successful);
}

// See crbug/1465889 for more details on what scenario is being tested.
TEST_P(QuicStreamFactoryTest,
       TestPostNetworkOnMadeDefaultWhileConnectionMigrationIsFailing) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_num = 1;
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  quic_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session->CreateHandle(scheme_host_port_);
  mock_ncn->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  mock_ncn->NotifyNetworkConnected(kNewNetworkForTests);
  mock_ncn->NotifyNetworkMadeDefault(kNewNetworkForTests);

  NetErrorDetails details;
  handle->PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QuicErrorCode::QUIC_CONNECTION_MIGRATION_TOO_MANY_CHANGES,
            details.quic_connection_error);
  EXPECT_EQ(false, details.quic_connection_migration_successful);
}

void QuicStreamFactoryTestBase::
    TestThatBlackHoleIsDisabledOnNoNewNetworkThenResumedAfterConnectingToANetwork(
        bool is_blackhole_disabled_after_disconnecting) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_num = 1;
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakePingPacket(packet_num++));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_num++, /*sequence_number=*/0u));

  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeDataPacket(
                          packet_num++, GetQpackDecoderStreamId(), false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

  quic_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);
  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  handles::NetworkHandle old_network = session->GetCurrentNetwork();
  // Forcefully disconnect the current network. This should stop the blackhole
  // detector since there is no other available network.
  mock_ncn->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  if (is_blackhole_disabled_after_disconnecting) {
    EXPECT_FALSE(
        session->connection()->blackhole_detector().IsDetectionInProgress());
  } else {
    EXPECT_TRUE(
        session->connection()->blackhole_detector().IsDetectionInProgress());
  }

  // This will fire migrateImmediately which will connect to a new socket on the
  // new network.
  mock_ncn->NotifyNetworkConnected(kNewNetworkForTests);

  // Execute the tasks that are added to the task runner from
  // NotifyNetworkConnected.
  task_runner->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Verify that we are on the new network.
  EXPECT_TRUE(old_network != session->GetCurrentNetwork());
  EXPECT_TRUE(session->GetCurrentNetwork() == kNewNetworkForTests);

  // Verify that blackhole detector is still active.
  EXPECT_TRUE(
      session->connection()->blackhole_detector().IsDetectionInProgress());

  // Verify that we also received the response on the new path.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());
}
// When the feature is disabled, the blackhole detector should stay enabled
// when there is no available network. resumed once a new network has been
// connected to.
TEST_P(
    QuicStreamFactoryTest,
    VerifyThatBlackHoleIsDisabledOnNoAvailableNetworkThenResumedAfterConnectingToNewNetwork_FeatureDisabled) {
  TestThatBlackHoleIsDisabledOnNoNewNetworkThenResumedAfterConnectingToANetwork(
      false);
}

// When the feature is enabled, the blackhole detector should be disabled
// when there is no available network. resumed once a new network has been
// connected to.
TEST_P(
    QuicStreamFactoryTest,
    VerifyThatBlackHoleIsDisabledOnNoAvailableNetworkThenResumedAfterConnectingToNewNetwork_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kDisableBlackholeOnNoNewNetwork},
      // disabled_features
      {});
  TestThatBlackHoleIsDisabledOnNoNewNetworkThenResumedAfterConnectingToANetwork(
      true);
}

void QuicStreamFactoryTestBase::TestSimplePortMigrationOnPathDegrading() {
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);

  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(ASYNC, client_maker_.MakePingPacket(packet_number++));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckAndRetireConnectionIdPacket(
                          packet_number++,
                          /*largest_received=*/2,
                          /*smallest_received=*/1, /*sequence_number=*/0u));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_number++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Disable connection migration on the request streams.
  // This should have no effect for port migration.
  QuicChromiumClientStream* chrome_stream =
      static_cast<QuicChromiumClientStream*>(
          quic::test::QuicSessionPeer::GetStream(
              session, GetNthClientInitiatedBidirectionalStreamId(0)));
  EXPECT_TRUE(chrome_stream);
  chrome_stream->DisableConnectionMigrationToCellularNetwork();

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Manually initialize the connection's self address. In real life, the
  // initialization will be done during crypto handshake.
  IPEndPoint ip;
  session->GetDefaultSocket()->GetLocalAddress(&ip);
  quic::test::QuicConnectionPeer::SetSelfAddress(session->connection(),
                                                 ToQuicSocketAddress(ip));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe a different port.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // There should be one pending task as the probe posted a DoNothingAs
  // callback.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  task_runner->ClearPendingTasks();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  // Successful port migration causes the path no longer degrading on the same
  // network.
  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // There should be pending tasks, the nearest one will complete
  // migration to the new port.
  task_runner->RunUntilIdle();

  // Fire any outstanding quic alarms.
  base::RunLoop().RunUntilIdle();

  // Response headers are received over the new port.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Now there may be one pending task to send connectivity probe that has been
  // cancelled due to successful migration.
  task_runner->FastForwardUntilNoTasksRemain();

  // Verify that the session is still alive, and the request stream is still
  // alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  chrome_stream = static_cast<QuicChromiumClientStream*>(
      quic::test::QuicSessionPeer::GetStream(
          session, GetNthClientInitiatedBidirectionalStreamId(0)));
  EXPECT_TRUE(chrome_stream);

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MultiplePortMigrationsExceedsMaxLimit_iQUICStyle) {
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  int server_packet_num = 1;
  // Perform 4 round of successful migration, and the 5th round will
  // cancel after successful probing due to hitting the limit.
  for (int i = 0; i <= 4; i++) {
    // Set up a different socket data provider that is used for
    // probing and migration.
    MockQuicData quic_data2(version_);
    // Connectivity probe to be sent on the new path.
    uint64_t new_cid = 12345678;
    quic::QuicConnectionId cid_on_new_path =
        quic::test::TestConnectionId(new_cid + i);
    client_maker_.set_connection_id(cid_on_new_path);
    MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session, i + 1);
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeConnectivityProbingPacket(packet_number));
    packet_number++;
    quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
    // Connectivity probe to receive from the server.
    quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(
                                  server_packet_num++));
    if (i == 0) {
      // Retire old connection id and send ping packet after migration is
      // completed.
      quic_data2.AddWrite(
          SYNCHRONOUS,
          client_maker_.MakeRetireConnectionIdPacket(packet_number++,
                                                     /*sequence_number=*/0u));
      quic_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakePingPacket(packet_number++));
    } else if (i != 4) {
      quic_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeAckAndRetireConnectionIdPacket(
                              packet_number++, 1 + 2 * i, 1 + 2 * i, i));
      quic_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakePingPacket(packet_number++));
    }

    if (i == 4) {
      // Add one more synchronous read on the last probing reader. The
      // reader should be deleted on the read before this one.
      // The test will verify this read is not consumed.
      quic_data2.AddRead(
          SYNCHRONOUS,
          server_maker_.MakeConnectivityProbingPacket(server_packet_num++));
    } else {
      quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(
                                    server_packet_num++));
    }

    if (i == 3) {
      // On the last allowed port migration, read one more packet so
      // that ACK is sent. The next round of migration (which hits the limit)
      // will not send any proactive ACK when reading the successful probing
      // response.
      quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(
                                    server_packet_num++));
      quic_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeAckPacket(packet_number++, 9, 9));
    }
    quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // EOF.
    quic_data2.AddSocketDataToFactory(socket_factory_.get());

    EXPECT_EQ(0u,
              QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

    // Cause the connection to report path degrading to the session.
    // Session will start to probe a different port.
    session->connection()->OnPathDegradingDetected();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1u,
              QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

    // The retry mechanism is internal to path validator.
    EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

    // The connection should still be alive, and not marked as going away.
    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(scheme_host_port_));
    EXPECT_EQ(1u, session->GetNumActiveStreams());

    // Resume quic data and a connectivity probe response will be read on the
    // new socket.
    quic_data2.Resume();
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(scheme_host_port_));
    EXPECT_EQ(1u, session->GetNumActiveStreams());

    if (i < 4) {
      // There's a pending task to complete migration to the new port.
      task_runner->RunUntilIdle();
    } else {
      // Last attempt to migrate will abort due to hitting the limit of max
      // number of allowed migrations.
      task_runner->FastForwardUntilNoTasksRemain();
    }

    EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
    // The last round of migration will abort upon reading the probing response.
    // Future reads in the same socket is ignored.
    EXPECT_EQ(i != 4, quic_data2.AllReadDataConsumed());
  }

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigratePortOnPathDegrading_MigrateIdleSession_PathValidator) {
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  quic_params_->migrate_idle_sessions = true;
  socket_factory_ = std::make_unique<TestPortMigrationSocketFactory>();
  Initialize();

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // The client session will receive the response first and closes its only
  // stream.
  quic_data1.AddRead(ASYNC,
                     ConstructOkResponsePacket(
                         1, GetNthClientInitiatedBidirectionalStreamId(0),
                         /*fin = */ true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Pause
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(2));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeAckAndPingPacket(packet_number++, 2, 1));

  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeRetireConnectionIdPacket(
                          packet_number++, /*sequence_number=*/0u));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Disable connection migration on the request streams.
  // This should have no effect for port migration.
  QuicChromiumClientStream* chrome_stream =
      static_cast<QuicChromiumClientStream*>(
          quic::test::QuicSessionPeer::GetStream(
              session, GetNthClientInitiatedBidirectionalStreamId(0)));
  EXPECT_TRUE(chrome_stream);
  chrome_stream->DisableConnectionMigrationToCellularNetwork();

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Manually initialize the connection's self address. In real life, the
  // initialization will be done during crypto handshake.
  IPEndPoint ip;
  session->GetDefaultSocket()->GetLocalAddress(&ip);
  quic::test::QuicConnectionPeer::SetSelfAddress(session->connection(),
                                                 ToQuicSocketAddress(ip));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe a different port.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  // A response will be received on the current path and closes the request
  // stream.
  quic_data1.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // There should be one pending task as the probe posted a DoNothingAs
  // callback.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  task_runner->ClearPendingTasks();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  // Successful port migration causes the path no longer degrading on the same
  // network.
  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // There should be pending tasks, the nearest one will complete
  // migration to the new port.
  task_runner->RunUntilIdle();

  // Fire any outstanding quic alarms.
  base::RunLoop().RunUntilIdle();

  // Now there may be one pending task to send connectivity probe that has been
  // cancelled due to successful migration.
  task_runner->FastForwardUntilNoTasksRemain();

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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

  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeAckAndDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), 1, 1, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
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
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume the data, and response header is received over the original network.
  quic_data.Resume();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());
  // There should be one pending task left as the probe posted a
  // DoNothingAsCallback.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_number++));
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructGetRequestPacket(
                          packet_number++,
                          GetNthClientInitiatedBidirectionalStreamId(0), true));
  // Read an out of order packet with FIN to drain the stream.
  quic_data1.AddRead(ASYNC,
                     ConstructOkResponsePacket(
                         2, GetNthClientInitiatedBidirectionalStreamId(0),
                         true));  // keep sending version.
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(3));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(write_mode_for_queued_packet,
                      client_maker_.MakeAckAndRetransmissionPacket(
                          packet_number++, 2, 3, 3, {1, 2}));
  if (write_mode_for_queued_packet == SYNCHRONOUS) {
    quic_data2.AddWrite(ASYNC, client_maker_.MakePingPacket(packet_number++));
  }
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_number++, 0u));
  server_maker_.Reset();
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckPacket(packet_number++, 1, 3, 1));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run the message loop to receive the out of order packet which contains a
  // FIN and drains the stream.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session should still start to probe the alternate network.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());
  EXPECT_TRUE(session->HasActiveRequestStreams());

  // There should be a task that will complete the migration to the new network.
  task_runner->RunUntilIdle();

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(base::Seconds(kMinRetryTimeForDefaultNetworkSecs));

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  int packet_num = 1;
  quic_data1.AddWrite(SYNCHRONOUS,
                      ConstructInitialSettingsPacket(packet_num++));
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));
  // in-flight SETTINGS and requests will be retransmitted. Since data is
  // already sent on the new address, ping will no longer be sent.
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeCombinedRetransmissionPacket(
                          /*original_packet_numbers=*/{1, 2}, packet_num++));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_num++, 0u));
  quic_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckAndDataPacket(
                          packet_num++, GetQpackDecoderStreamId(), 2, 2, false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be a task that will complete the migration to the new network.
  task_runner->RunUntilIdle();

  // Although the session successfully migrates, it is still considered
  // degrading sessions.
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // There's one more task to mgirate back to the default network in 0.4s.
  task_runner->FastForwardBy(base::Seconds(kMinRetryTimeForDefaultNetworkSecs));

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddWrite(ASYNC, OK);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddWrite(ASYNC, OK);
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  url::SchemeHostPort server1(url::kHttpsScheme, kDefaultServerHostName, 443);
  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.2", "");

  // Create request and QuicHttpStream to create session1.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  // Create request and QuicHttpStream to create session2.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));
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
  stream2->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));
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

  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause for path degrading signal.

  // The rest of the data will still flow in the original socket as there is no
  // new network after path degrading.
  quic_data.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeAckAndDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), 1, 1, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger connection migration on path degrading. Since there are no networks
  // to migrate to, the session will remain on the original network, not marked
  // as going away.
  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(session->connection()->IsPathDegrading());
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume so that rest of the data will flow in the original socket.
  quic_data.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// This test verifies that session with non-migratable stream will probe the
// alternate network on path degrading, and close the non-migratable streams
// when probe is successful.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionEarlyNonMigratableStream_DoNotMigrateIdleSessions) {
  TestMigrateSessionEarlyNonMigratableStream(false);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionEarlyNonMigratableStream_MigrateIdleSessions) {
  TestMigrateSessionEarlyNonMigratableStream(true);
}

void QuicStreamFactoryTestBase::TestMigrateSessionEarlyNonMigratableStream(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));

  // Set up the second socket data provider that is used for probing.
  MockQuicData quic_data1(version_);
  quic::QuicConnectionId cid_on_old_path =
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator());
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Connectivity probe to be sent on the new path.
  quic_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data1.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(1));

  if (migrate_idle_sessions) {
    quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    quic_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataAndRstPacket(
                            packet_num++, GetQpackDecoderStreamId(),
                            StreamCancellationQpackDecoderInstruction(0),
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED));
    quic_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRetransmissionPacket(1, packet_num++));
    // Ping packet to send after migration is completed.
    quic_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
    quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, 0u));
  } else {
    client_maker_.set_connection_id(cid_on_old_path);
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataRstAckAndConnectionClosePacket(
                         packet_num++, GetQpackDecoderStreamId(),
                         StreamCancellationQpackDecoderInstruction(0),
                         GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_STREAM_CANCELLED, 1, 1,
                         quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                         "net error", 0x1b));
  }

  socket_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to migrate.
  session->OnPathDegrading();

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume the data to read the connectivity probing response to declare probe
  // as successful. Non-migratable streams will be closed.
  quic_data1.Resume();
  if (migrate_idle_sessions)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(scheme_host_port_));
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), false,
                           StreamCancellationQpackDecoderInstruction(0)));
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
TEST_P(QuicStreamFactoryTest, MigrateSessionOnAsyncWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  // base::RunLoop() controls mocked socket writes and reads.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  ConstructGetRequestPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true);
  spdy::Http2HeaderBlock headers =
      client_maker_.GetRequestHeaders("GET", "https", "/");
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_len;
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRetransmissionAndRequestHeadersPacket(
          {1, 2}, packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
          true, priority, std::move(headers), &spdy_headers_frame_len));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeDataPacket(
                       packet_num++, GetQpackDecoderStreamId(),
                       /* fin = */ false,
                       StreamCancellationQpackDecoderInstruction(1, false)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED,
                                  /*include_stop_sending_if_v99=*/true));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Request #2 returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream2->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  // Fire the retire connection ID alarm.
  base::RunLoop().RunUntilIdle();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  // There should be one task posted to migrate back to the default network in
  // kMinRetryTimeForDefaultNetworkSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::Seconds(kMinRetryTimeForDefaultNetworkSecs),
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  int peer_packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData quic_data2(version_);
  quic::QuicConnectionId cid1 = quic::test::TestConnectionId(12345678);
  quic::QuicConnectionId cid2 = quic::test::TestConnectionId(87654321);

  client_maker_.set_connection_id(cid1);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeCombinedRetransmissionPacket(
                          /*original_packet_numbers=*/{1, 2}, packet_num++));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakePingPacket(packet_num++));
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                       packet_num++,
                                       /*sequence_number=*/0u));
  quic_data2.AddRead(ASYNC, server_maker_.MakeAckAndNewConnectionIdPacket(
                                peer_packet_num++, packet_num - 1, 1u, cid2,
                                /*sequence_number=*/2u,
                                /*retire_prior_to=*/1u));
  quic_data2.AddRead(ASYNC,
                     ConstructOkResponsePacket(
                         peer_packet_num++,
                         GetNthClientInitiatedBidirectionalStreamId(0), false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  MaybeMakeNewConnectionIdAvailableToSession(cid1, session);

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
  // Make sure the alarm that retires connection ID on the old path is fired.
  base::RunLoop().RunUntilIdle();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  // There should be one task posted to migrate back to the default network in
  // kMinRetryTimeForDefaultNetworkSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta expected_delay =
      base::Seconds(kMinRetryTimeForDefaultNetworkSecs);
  EXPECT_EQ(expected_delay, task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Set up the third socket data provider for migrate back to default network.
  MockQuicData quic_data3(version_);
  client_maker_.set_connection_id(cid2);
  // Connectivity probe to be sent on the new path.
  quic_data3.AddWrite(
      SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(packet_num++));
  // Connectivity probe to receive from the server.
  quic_data3.AddRead(
      ASYNC, server_maker_.MakeConnectivityProbingPacket(peer_packet_num++));
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // There is no other data to retransmit as they have been acknowledged by
  // the packet containing NEW_CONNECTION_ID frame from the server.
  quic_data3.AddWrite(ASYNC, client_maker_.MakeAckPacket(
                                 packet_num++,
                                 /*first_received=*/1,
                                 /*largest_received=*/peer_packet_num - 1,
                                 /*smallest_received=*/1));

  quic_data3.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeDataPacket(
                          packet_num++, GetQpackDecoderStreamId(), false,
                          StreamCancellationQpackDecoderInstruction(0)));
  quic_data3.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED,
                                  /*include_stop_sending_if_v99=*/true));
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  // Fast forward to fire the migrate back timer and verify the session
  // successfully migrates back to the default network.
  task_runner->FastForwardBy(expected_delay);

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be one task posted to one will resend a connectivity probe and
  // the other will retry migrate back, both are cancelled.
  task_runner->FastForwardUntilNoTasksRemain();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

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
  FLAGS_quic_enable_chaos_protection = false;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Cause the connection to report path degrading to the session.
  // Session will ignore the signal as handshake is not completed.
  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));

  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
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
  FLAGS_quic_enable_chaos_protection = false;
  DCHECK(quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
         quic_error == quic::QUIC_HANDSHAKE_TIMEOUT);
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  EXPECT_EQ(0u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  // Cause the connection to report path degrading to the session.
  // Session will ignore the signal as handshake is not completed.
  session->connection()->OnPathDegradingDetected();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, QuicStreamFactoryPeer::GetNumDegradingSessions(factory_.get()));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Cause the connection to close due to |quic_error| before handshake.
  std::string error_details;
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
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));
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
  FLAGS_quic_enable_chaos_protection = false;
  // TODO(https://crbug.com/1295460): Make this test work with asynchronous QUIC
  // session creation. This test only works with synchronous session creation
  // for now.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  quic_params_->retry_on_alternate_network_before_handshake = true;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // Socket data for connection on the default network.
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for connection on the alternate network.
  MockQuicData socket_data2(version_);
  int packet_num = 1;
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDummyCHLOPacket(packet_num++));
  socket_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket(packet_num++));
  socket_data2.AddWrite(
      ASYNC,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int probing_packet_num = packet_num++;
  socket_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/1u));
  socket_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeDataPacket(
                       packet_num++, GetQpackDecoderStreamId(), /*fin=*/false,
                       StreamCancellationQpackDecoderInstruction(0)));
  socket_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for probing on the default network.
  MockQuicData probing_data(version_);
  quic::QuicConnectionId cid_on_path1 = quic::test::TestConnectionId(1234567);
  client_maker_.set_connection_id(cid_on_path1);
  probing_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  probing_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeConnectivityProbingPacket(probing_packet_num));
  probing_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_FALSE(failed_on_default_network_);

  std::string error_details;
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
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  QuicChromiumClientSession* session2 = GetPendingSession(scheme_host_port_);
  EXPECT_NE(session, session2);
  EXPECT_TRUE(failed_on_default_network_);

  // Confirm the handshake on the alternate network.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_path1, session2);
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));
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
  EXPECT_EQ(base::Seconds(kMinRetryTimeForDefaultNetworkSecs), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

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
  DCHECK(!quic_params_->retry_on_alternate_network_before_handshake);
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Verify new requests can be sent normally.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Test that if the original connection is closed with QUIC_PACKET_WRITE_ERROR
// before handshake is confirmed and new connection before handshake is turned
// on, a new connection will be retried on the alternate network.
TEST_P(QuicStreamFactoryTest,
       RetryConnectionOnWriteErrorBeforeHandshakeConfirmed) {
  FLAGS_quic_enable_chaos_protection = false;
  quic_params_->retry_on_alternate_network_before_handshake = true;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // Socket data for connection on the default network.
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for connection on the alternate network.
  MockQuicData socket_data2(version_);
  int packet_num = 1;
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDummyCHLOPacket(packet_num++));
  socket_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket(packet_num++));
  socket_data2.AddWrite(
      ASYNC,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(
      SYNCHRONOUS, client_maker_.MakeAckAndDataPacket(
                       packet_num++, GetQpackDecoderStreamId(), 1, 1, false,
                       StreamCancellationQpackDecoderInstruction(0)));
  socket_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Ensure that the session is alive but not active.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_TRUE(HasActiveJob(scheme_host_port_, privacy_mode_));
  base::RunLoop().RunUntilIdle();
  QuicChromiumClientSession* session = GetPendingSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  // Confirm the handshake on the alternate network.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));
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
  client_maker_.set_save_packet_frames(true);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeCombinedRetransmissionPacket(
                            /*original_packet_numbers=*/{1, 2}, packet_num++));
  socket_data1.AddWrite(ASYNC, client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
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

  // Verify that session is alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_TRUE(session->connection()->writer()->IsWriteBlocked());

  // The migration will not fail until the migration alarm timeout.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Force migration alarm timeout to run.
  RunTestLoopUntilIdle();

  // The connection should be closed. A request for response headers
  // should fail.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));

  NetErrorDetails error_details;
  stream->PopulateNetErrorDetails(&error_details);
  EXPECT_EQ(error_details.quic_connection_error,
            quic::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK);

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
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeCombinedRetransmissionPacket(
                            /*original_packet_numbers=*/{1, 2}, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeDataPacket(
                       packet_num++, GetQpackDecoderStreamId(), false,
                       StreamCancellationQpackDecoderInstruction(1, false)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED,
                                  /*include_stop_sending_if_v99=*/true));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream2->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  int packet_number = 1;
  MockQuicData socket_data(version_);

  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(1234567);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_number++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRetransmissionRstAndDataPacket(
                       /*original_packet_numbers=*/{1, 2}, packet_number++,
                       GetNthClientInitiatedBidirectionalStreamId(1),
                       quic::QUIC_STREAM_CANCELLED, GetQpackDecoderStreamId(),
                       StreamCancellationQpackDecoderInstruction(1)));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_number++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_number++,
                                         /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_number++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0, false)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream2->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  int packet_number = 1;
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  socket_data.AddWrite(write_error_mode,
                       ERR_ADDRESS_UNREACHABLE);  // Write error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after migration. The
  // request is rewritten to this new socket, and the response to the request is
  // read on this new socket.
  MockQuicData socket_data1(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_number++,
                            GetNthClientInitiatedBidirectionalStreamId(1),
                            /*fin=*/true);
  std::vector<uint64_t> original_packet_numbers = {1};
  uint64_t retransmit_frame_count = 2;
  original_packet_numbers.push_back(2);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRetransmissionRstAndDataPacket(
                       original_packet_numbers, packet_number++,
                       GetNthClientInitiatedBidirectionalStreamId(1),
                       quic::QUIC_STREAM_CANCELLED, GetQpackDecoderStreamId(),
                       StreamCancellationQpackDecoderInstruction(1),
                       retransmit_frame_count));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_number++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_number++,
                                         /*sequence_number=*/0u));
  socket_data1.AddWrite(
      SYNCHRONOUS, ConstructGetRequestPacket(
                       packet_number++,
                       GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_number++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0, false)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_number++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream1->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream2->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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

// This test verifies that when a connection encounters a packet write error, it
// will cancel non-migratable streams, and migrate to the alternate network.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorNonMigratableStream(
    IoMode write_error_mode,
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData failed_socket_data(version_);
  MockQuicData socket_data(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  int packet_num = 1;
  if (migrate_idle_sessions) {
    // The socket data provider for the original socket before migration.
    failed_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    failed_socket_data.AddWrite(SYNCHRONOUS,
                                ConstructInitialSettingsPacket(packet_num++));
    failed_socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
    failed_socket_data.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after migration.
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    client_maker_.set_connection_id(cid_on_new_path);
    // Increment packet number to account for packet write error on the old
    // path. Also save the packet in client_maker_ for constructing the
    // retransmission packet.
    ConstructGetRequestPacket(packet_num++,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              /*fin=*/true);
    std::vector<uint64_t> original_packet_numbers = {1};
    uint64_t retransmit_frame_count = 2;
    original_packet_numbers.push_back(2);
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRetransmissionRstAndDataPacket(
                         original_packet_numbers, packet_num++,
                         GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_STREAM_CANCELLED, GetQpackDecoderStreamId(),
                         StreamCancellationQpackDecoderInstruction(0),
                         retransmit_frame_count));
    socket_data.AddWrite(SYNCHRONOUS,
                         client_maker_.MakePingPacket(packet_num++));
    socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRetireConnectionIdPacket(packet_num++,
                                                   /*sequence_number=*/0u));
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  } else {
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run message loop to execute migration attempt.
  base::RunLoop().RunUntilIdle();

  // Migration closes the non-migratable stream and:
  // if migrate idle session is enabled, it migrates to the alternate network
  // successfully; otherwise the connection is closed.
  EXPECT_EQ(migrate_idle_sessions,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(scheme_host_port_));

  if (migrate_idle_sessions) {
    EXPECT_TRUE(failed_socket_data.AllReadDataConsumed());
    EXPECT_TRUE(failed_socket_data.AllWriteDataConsumed());
  }
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(
    QuicStreamFactoryTest,
    MigrateSessionOnWriteErrorNonMigratableStreamSync_DoNotMigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(SYNCHRONOUS, false);
}

TEST_P(
    QuicStreamFactoryTest,
    MigrateSessionOnWriteErrorNonMigratableStreamAsync_DoNotMigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(ASYNC, false);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNonMigratableStreamSync_MigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(SYNCHRONOUS, true);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNonMigratableStreamAsync_MigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(ASYNC, true);
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMigrationDisabled(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
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

// For IETF QUIC, this test the following scenario:
// - original network encounters a SYNC/ASYNC write error based on
//   |write_error_mode_on_old_network|, the packet failed to be written is
//   cached, session migrates immediately to the alternate network.
// - an immediate SYNC/ASYNC write error based on
//   |write_error_mode_on_new_network| is encountered after migration to the
//   alternate network, session migrates immediately to the original network.
// - After a new socket for the original network is created and starts to read,
//   connection migration fails due to lack of unused connection ID and
//   connection is closed.
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
  client_maker_.set_save_packet_frames(true);

  // Set up the socket data used by the original network, which encounters a
  // write error.
  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(write_error_mode_on_old_network,
                        ERR_ADDRESS_UNREACHABLE);  // Write Error
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the socket data used by the alternate network, which
  // - is not used to write as migration fails due to lack of connection ID.
  // - encounters a write error in gQUIC.
  MockQuicData failed_quic_data2(version_);
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  failed_quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  failed_quic_data2.AddWrite(write_error_mode_on_new_network, ERR_FAILED);
  failed_quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Set up the third socket data used by original network, which
  // - encounters a write error again.
  MockQuicData failed_quic_data1(version_);
  failed_quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  failed_quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  // This should encounter a write error on network 1,
  // then migrate to network 2, which encounters another write error,
  // and migrate again to network 1, which encoutners one more write error.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  base::RunLoop().RunUntilIdle();
  // Connection is closed as there is no connection ID available yet for the
  // second migration.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(failed_quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(failed_quic_data2.AllWriteDataConsumed());
  EXPECT_TRUE(failed_quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(failed_quic_data1.AllWriteDataConsumed());
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
  FLAGS_quic_enable_chaos_protection = false;
  // TODO(https://crbug.com/1295460): Make this test work with asynchronous QUIC
  // session creation. This test only works with synchronous session creation
  // for now.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use cold start mode to do crypto connect, and send CHLO packet on wire.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  // Deliver the network notification, which should cause the connection to be
  // closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());

  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
  EXPECT_FALSE(HasActiveJob(scheme_host_port_, privacy_mode_));
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
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeCombinedRetransmissionPacket(
                            /*original_packet_numbers=*/{1, 2}, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, /*sequence_number=*/0u));

  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);

  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeCombinedRetransmissionPacket(
                            /*original_packet_numbers=*/{1, 2}, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream. This should cause a write error,
  // which triggers a connection migration attempt. This will queue a
  // migration attempt in the message loop.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  base::RunLoop().RunUntilIdle();

  // Now queue a network change notification in the message loop behind
  // the migration attempt.
  if (disconnected) {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkDisconnected(kDefaultNetworkForTests);
  } else {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkMadeDefault(kNewNetworkForTests);
  }

  // Verify session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(write_error_mode, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRetransmissionAndRetireConnectionIdPacket(
                       packet_num++,
                       /*original_packet_numbers=*/{1, 2},
                       /*sequence_number=*/0u));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop migration for write error can finish.
  runner_->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner so that we can verify whether the migrate on
  // write error task is posted.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddWrite(
      ASYNC, ERR_ADDRESS_UNREACHABLE,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeCombinedRetransmissionPacket({1, 2}, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRetransmissionPacket(1, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(
      SYNCHRONOUS, client_maker_.MakeRetransmissionPacket(1, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when connection migration on path degrading is
// enabled, and no custom retransmittable on wire timeout is specified, the
// default value is used.
TEST_P(QuicStreamFactoryTest, DefaultRetransmittableOnWireTimeoutForMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MockQuicData socket_data(version_);
  int packet_num = 1;
  int peer_packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddRead(ASYNC, server_maker_.MakeNewConnectionIdPacket(
                                 peer_packet_num++, cid_on_new_path,
                                 /*sequence_number=*/1u,
                                 /*retire_prior_to=*/0u));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRetransmissionPacket(
                            packet_num++, /*first_received=*/1,
                            /*largest_received=*/1, /*smallest_received=*/1,
                            /*original_packet_numbers=*/{1}));
  // The PING packet sent post migration.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, /*sequence_number=*/0u));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immediately.
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 peer_packet_num++,
                 GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(ASYNC, server_maker_.MakeDataPacket(
                                  peer_packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  false, "Hello World"));

  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(
                                        peer_packet_num++, packet_num, 1));
  socket_data1.AddWrite(
      ASYNC, client_maker_.MakeAckPacket(packet_num++, peer_packet_num - 2, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(
      kDefaultRetransmittableOnWireTimeout.InMilliseconds()));
  task_runner->FastForwardBy(kDefaultRetransmittableOnWireTimeout);

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when connection migration on path degrading is
// enabled, and a custom retransmittable on wire timeout is specified, the
// custom value is used.
TEST_P(QuicStreamFactoryTest, CustomRetransmittableOnWireTimeoutForMigration) {
  constexpr base::TimeDelta custom_timeout_value = base::Milliseconds(200);
  quic_params_->retransmittable_on_wire_timeout = custom_timeout_value;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MockQuicData socket_data(version_);
  int packet_num = 1;
  int peer_packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddRead(ASYNC, server_maker_.MakeNewConnectionIdPacket(
                                 peer_packet_num++, cid_on_new_path,
                                 /*sequence_number=*/1u,
                                 /*retire_prior_to=*/0u));
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRetransmissionPacket(
                            packet_num++, /*first_received=*/1,
                            /*largest_received=*/1, /*smallest_received=*/1,
                            /*original_packet_numbers=*/{1}));
  // The PING packet sent post migration.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 peer_packet_num++,
                 GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(ASYNC, server_maker_.MakeDataPacket(
                                  peer_packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  /*fin=*/false, "Hello World"));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(
                                        peer_packet_num++, packet_num, 1));
  socket_data1.AddWrite(
      ASYNC, client_maker_.MakeAckPacket(packet_num++, peer_packet_num - 2, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(
      custom_timeout_value.InMilliseconds()));
  task_runner->FastForwardBy(custom_timeout_value);

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when no migration is enabled, but a custom value for
// retransmittable-on-wire timeout is specified, the ping alarm is set up to
// send retransmittable pings with the custom value.
TEST_P(QuicStreamFactoryTest, CustomRetransmittableOnWireTimeout) {
  constexpr base::TimeDelta custom_timeout_value = base::Milliseconds(200);
  quic_params_->retransmittable_on_wire_timeout = custom_timeout_value;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 "Hello World"));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1));
  socket_data1.AddWrite(ASYNC, client_maker_.MakeAckPacket(packet_num++, 2, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(
      custom_timeout_value.InMilliseconds()));
  task_runner->FastForwardBy(custom_timeout_value);

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when no migration is enabled, and no custom value
// for retransmittable-on-wire timeout is specified, the ping alarm will not
// send any retransmittable pings.
TEST_P(QuicStreamFactoryTest, NoRetransmittableOnWireTimeout) {
  // Use non-default initial srtt so that if QPACK emits additional setting
  // packet, it will not have the same retransmission timeout as the
  // default value of retransmittable-on-wire-ping timeout.
  ServerNetworkStats stats;
  stats.srtt = base::Milliseconds(200);
  http_server_properties_->SetServerNetworkStats(
      url::SchemeHostPort(url_), NetworkAnonymizationKey(), stats);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 "Hello World"));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1));
  socket_data1.AddWrite(ASYNC, client_maker_.MakeAckPacket(packet_num++, 2, 1));
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Verify the ping alarm is set, but not with the default timeout.
  const quic::QuicAlarm* const ping_alarm =
      quic::test::QuicConnectionPeer::GetPingAlarm(session->connection());
  ASSERT_TRUE(ping_alarm);
  ASSERT_TRUE(ping_alarm->IsSet());
  quic::QuicTime::Delta delay =
      ping_alarm->deadline() - context_.clock()->ApproximateNow();
  EXPECT_NE(kDefaultRetransmittableOnWireTimeout.InMilliseconds(),
            delay.ToMilliseconds());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when only migration on network change is enabled, and
// a custom value for retransmittable-on-wire is specified, the ping alarm will
// send retransmittable pings to the peer with custom value.
TEST_P(QuicStreamFactoryTest,
       CustomRetransmittableOnWireTimeoutWithMigrationOnNetworkChangeOnly) {
  constexpr base::TimeDelta custom_timeout_value = base::Milliseconds(200);
  quic_params_->retransmittable_on_wire_timeout = custom_timeout_value;
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 "Hello World"));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1));
  socket_data1.AddWrite(ASYNC, client_maker_.MakeAckPacket(packet_num++, 2, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(
      custom_timeout_value.InMilliseconds()));
  task_runner->FastForwardBy(custom_timeout_value);

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when only migration on network change is enabled, and
// no custom value for retransmittable-on-wire is specified, the ping alarm will
// NOT send retransmittable pings to the peer with custom value.
TEST_P(QuicStreamFactoryTest,
       NoRetransmittableOnWireTimeoutWithMigrationOnNetworkChangeOnly) {
  // Use non-default initial srtt so that if QPACK emits additional setting
  // packet, it will not have the same retransmission timeout as the
  // default value of retransmittable-on-wire-ping timeout.
  ServerNetworkStats stats;
  stats.srtt = base::Milliseconds(200);
  http_server_properties_->SetServerNetworkStats(
      url::SchemeHostPort(url_), NetworkAnonymizationKey(), stats);
  quic_params_->estimate_initial_rtt = true;
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 "Hello World"));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1));
  socket_data1.AddWrite(ASYNC, client_maker_.MakeAckPacket(packet_num++, 2, 1));
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Verify the ping alarm is set, but not with the default timeout.
  const quic::QuicAlarm* const ping_alarm =
      quic::test::QuicConnectionPeer::GetPingAlarm(session->connection());
  ASSERT_TRUE(ping_alarm);
  ASSERT_TRUE(ping_alarm->IsSet());
  quic::QuicTime::Delta delay =
      ping_alarm->deadline() - context_.clock()->ApproximateNow();
  EXPECT_NE(kDefaultRetransmittableOnWireTimeout.InMilliseconds(),
            delay.ToMilliseconds());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
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
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(ASYNC, ERR_FAILED);              // Write error.
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);  // Read error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeCombinedRetransmissionPacket(
                            /*original_packet_numbers=*/{1, 2}, packet_num++));
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Complete migration.
  task_runner->RunUntilIdle();
  // There will be one more task posted attempting to migrate back to the
  // default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
       MigrateSessionOnWriteErrorWithDisconnectAfterConnectAsync) {
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
       MigrateSessionOnWriteErrorWithDisconnectBeforeConnectAsync) {
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

// Sets up test which verifies that session successfully migrate to alternate
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
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(write_error_mode, ERR_FAILED);  // Write error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  // Increment packet number to account for packet write error on the old
  // path. Also save the packet in client_maker_ for constructing the
  // retransmission packet.
  ConstructGetRequestPacket(packet_num++,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            /*fin=*/true);
  socket_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeCombinedRetransmissionPacket(
                            /*original_packet_numbers=*/{1, 2}, packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++, /*sequence_number=*/0u));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
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
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  EXPECT_EQ(OK,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(session, GetActiveSession(scheme_host_port_));

  stream.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies after session migrates off the default network, it keeps
// retrying migrate back to the default network until successfully gets on the
// default network or the idle migration period threshold is exceeded.
// The default threshold is 30s.
TEST_P(QuicStreamFactoryTest, DefaultIdleMigrationPeriod) {
  quic_params_->migrate_idle_sessions = true;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner and a test tick tock.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetTickClock(factory_.get(),
                                      task_runner->GetMockTickClock());

  quic::QuicConnectionId cid1 = quic::test::TestConnectionId(1234567);
  quic::QuicConnectionId cid2 = quic::test::TestConnectionId(2345671);
  quic::QuicConnectionId cid3 = quic::test::TestConnectionId(3456712);
  quic::QuicConnectionId cid4 = quic::test::TestConnectionId(4567123);
  quic::QuicConnectionId cid5 = quic::test::TestConnectionId(5671234);
  quic::QuicConnectionId cid6 = quic::test::TestConnectionId(6712345);
  quic::QuicConnectionId cid7 = quic::test::TestConnectionId(7123456);

  int peer_packet_num = 1;
  MockQuicData default_socket_data(version_);
  default_socket_data.AddRead(
      SYNCHRONOUS,
      server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid1,
                                              /*sequence_number=*/1u,
                                              /*retire_prior_to=*/0u));
  default_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  default_socket_data.AddWrite(SYNCHRONOUS,
                               ConstructInitialSettingsPacket(packet_num++));
  default_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after migration.
  MockQuicData alternate_socket_data(version_);
  client_maker_.set_connection_id(cid1);
  alternate_socket_data.AddWrite(SYNCHRONOUS,
                                 client_maker_.MakeAckAndRetransmissionPacket(
                                     packet_num++,
                                     /*first_received=*/1,
                                     /*largest_received=*/peer_packet_num - 1,
                                     /*smallest_received=*/1,
                                     /*original_packet_numbers=*/{1}));
  alternate_socket_data.AddWrite(SYNCHRONOUS,
                                 client_maker_.MakePingPacket(packet_num++));
  alternate_socket_data.AddWrite(ASYNC,
                                 client_maker_.MakeRetireConnectionIdPacket(
                                     packet_num++, /*sequence_number=*/0u));
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid2,
                                                     /*sequence_number=*/2u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/2u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid3,
                                                     /*sequence_number=*/3u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/3u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid4,
                                                     /*sequence_number=*/4u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/4u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid5,
                                                     /*sequence_number=*/5u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/5u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid6,
                                                     /*sequence_number=*/6u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/6u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid7,
                                                     /*sequence_number=*/7u,
                                                     /*retire_prior_to=*/1u));
  alternate_socket_data.AddRead(SYNCHRONOUS,
                                ERR_IO_PENDING);  // Hanging read.
  alternate_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up probing socket for migrating back to the default network.
  MockQuicData quic_data(version_);                // retry count: 0.
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data1(version_);                // retry count: 1
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2(version_);                // retry count: 2
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data3(version_);                // retry count: 3
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data3.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data4(version_);                // retry count: 4
  quic_data4.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data4.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data4.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data5(version_);                // retry count: 5
  quic_data5.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data5.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data5.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is active.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The nearest task will complete migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // The migrate back timer will fire. Due to default network
  // being disconnected, no attempt will be exercised to migrate back.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::Seconds(kMinRetryTimeForDefaultNetworkSecs),
            task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Deliver the signal that the old default network now backs up.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  // A task is posted to migrate back to the default network immediately.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // Retry migrate back in 1, 2, 4, 8, 16s.
  // Session will be closed due to idle migration timeout.
  for (int i = 0; i < 5; i++) {
    // Fire retire connection ID alarm.
    base::RunLoop().RunUntilIdle();
    // Make new connection ID available.
    alternate_socket_data.Resume();
    EXPECT_TRUE(HasActiveSession(scheme_host_port_));
    // A task is posted to migrate back to the default network in 2^i seconds.
    EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
    EXPECT_EQ(base::Seconds(UINT64_C(1) << i),
              task_runner->NextPendingTaskDelay());
    task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  }

  EXPECT_TRUE(default_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(default_socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CustomIdleMigrationPeriod) {
  // The customized threshold is 15s.
  quic_params_->migrate_idle_sessions = true;
  quic_params_->idle_session_migration_period = base::Seconds(15);
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  // Using a testing task runner and a test tick tock.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetTickClock(factory_.get(),
                                      task_runner->GetMockTickClock());

  quic::QuicConnectionId cid1 = quic::test::TestConnectionId(1234567);
  quic::QuicConnectionId cid2 = quic::test::TestConnectionId(2345671);
  quic::QuicConnectionId cid3 = quic::test::TestConnectionId(3456712);
  quic::QuicConnectionId cid4 = quic::test::TestConnectionId(4567123);
  quic::QuicConnectionId cid5 = quic::test::TestConnectionId(5671234);
  quic::QuicConnectionId cid6 = quic::test::TestConnectionId(6712345);

  int peer_packet_num = 1;
  MockQuicData default_socket_data(version_);
  default_socket_data.AddRead(
      SYNCHRONOUS,
      server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid1,
                                              /*sequence_number=*/1u,
                                              /*retire_prior_to=*/0u));
  default_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  default_socket_data.AddWrite(SYNCHRONOUS,
                               ConstructInitialSettingsPacket(packet_num++));
  default_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after migration.
  MockQuicData alternate_socket_data(version_);
  client_maker_.set_connection_id(cid1);
  alternate_socket_data.AddWrite(SYNCHRONOUS,
                                 client_maker_.MakeAckAndRetransmissionPacket(
                                     packet_num++,
                                     /*first_received=*/1,
                                     /*largest_received=*/peer_packet_num - 1,
                                     /*smallest_received=*/1,
                                     /*original_packet_numbers=*/{1}));
  alternate_socket_data.AddWrite(SYNCHRONOUS,
                                 client_maker_.MakePingPacket(packet_num++));
  alternate_socket_data.AddWrite(ASYNC,
                                 client_maker_.MakeRetireConnectionIdPacket(
                                     packet_num++, /*sequence_number=*/0u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid2,
                                                     /*sequence_number=*/2u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/2u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid3,
                                                     /*sequence_number=*/3u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/3u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid4,
                                                     /*sequence_number=*/4u,
                                                     /*retire_prior_to=*/1u));
  ++packet_num;  // Probing packet on default network encounters write error.
  alternate_socket_data.AddWrite(
      ASYNC, client_maker_.MakeAckAndRetireConnectionIdPacket(
                 packet_num++,
                 /*largest_received=*/peer_packet_num - 1,
                 /*smallest_received=*/1,
                 /*sequence_number=*/4u));
  alternate_socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  alternate_socket_data.AddRead(
      ASYNC, server_maker_.MakeNewConnectionIdPacket(peer_packet_num++, cid5,
                                                     /*sequence_number=*/5u,
                                                     /*retire_prior_to=*/1u));
  alternate_socket_data.AddRead(SYNCHRONOUS,
                                ERR_IO_PENDING);  // Hanging read.
  alternate_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up probing socket for migrating back to the default network.
  MockQuicData quic_data(version_);                // retry count: 0.
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data1(version_);                // retry count: 1
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2(version_);                // retry count: 2
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data3(version_);                // retry count: 3
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data3.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data4(version_);                // retry count: 4
  quic_data4.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data4.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data4.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is active.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The nearest task will complete migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // The migrate back timer will fire. Due to default network
  // being disconnected, no attempt will be exercised to migrate back.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::Seconds(kMinRetryTimeForDefaultNetworkSecs),
            task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Deliver the signal that the old default network now backs up.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  // A task is posted to migrate back to the default network immediately.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // Retry migrate back in 1, 2, 4, 8s.
  // Session will be closed due to idle migration timeout.
  for (int i = 0; i < 4; i++) {
    // Fire retire connection ID alarm.
    base::RunLoop().RunUntilIdle();
    // Make new connection ID available.
    alternate_socket_data.Resume();
    EXPECT_TRUE(HasActiveSession(scheme_host_port_));
    // A task is posted to migrate back to the default network in 2^i seconds.
    EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
    EXPECT_EQ(base::Seconds(UINT64_C(1) << i),
              task_runner->NextPendingTaskDelay());
    task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  }

  EXPECT_TRUE(default_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(default_socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigration) {
  quic_params_->allow_server_migration = true;
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  NetErrorDetails details;
  EXPECT_FALSE(details.quic_connection_migration_attempted);
  EXPECT_FALSE(details.quic_connection_migration_successful);
  session->PopulateNetErrorDetails(&details);
  EXPECT_FALSE(details.quic_connection_migration_attempted);
  EXPECT_FALSE(details.quic_connection_migration_successful);

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
  MockQuicData socket_data2(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  socket_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeCombinedRetransmissionPacket({1, 2}, packet_num++));
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++));
  socket_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeRetireConnectionIdPacket(
                                         packet_num++,
                                         /*sequence_number=*/0u));
  socket_data2.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeDataPacket(
          packet_num++, GetQpackDecoderStreamId(),
          /*fin=*/false, StreamCancellationQpackDecoderInstruction(0)));
  socket_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  const uint8_t kTestIpAddress[] = {1, 2, 3, 4};
  const uint16_t kTestPort = 123;
  base::RunLoop run_loop;
  QuicChromiumClientSession::MigrationCallback migration_callback =
      base::BindLambdaForTesting(
          [&run_loop](MigrationResult result) { run_loop.Quit(); });
  session->Migrate(handles::kInvalidNetworkHandle,
                   IPEndPoint(IPAddress(kTestIpAddress), kTestPort), true,
                   std::move(migration_callback));
  run_loop.Run();
  session->GetDefaultSocket()->GetPeerAddress(&ip);
  DVLOG(1) << "Socket migrated to: " << ip.address().ToString() << " "
           << ip.port();

  // The session should be alive and active.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  session->PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_connection_migration_attempted);
  EXPECT_TRUE(details.quic_connection_migration_successful);

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

TEST_P(QuicStreamFactoryTest, ServerMigrationNonMigratableStream) {
  quic_params_->allow_server_migration = true;
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.set_save_packet_frames(true);

  int packet_num = 1;
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true));
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), false,
                           StreamCancellationQpackDecoderInstruction(0)));
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++, quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MaybeMakeNewConnectionIdAvailableToSession(cid_on_new_path, session);

  // Disable connection migration on the request streams.
  QuicChromiumClientStream* chrome_stream =
      static_cast<QuicChromiumClientStream*>(
          quic::test::QuicSessionPeer::GetStream(
              session, GetNthClientInitiatedBidirectionalStreamId(0)));
  EXPECT_TRUE(chrome_stream);
  chrome_stream->DisableConnectionMigrationToCellularNetwork();

  NetErrorDetails details;
  EXPECT_FALSE(details.quic_connection_migration_attempted);
  EXPECT_FALSE(details.quic_connection_migration_successful);
  session->PopulateNetErrorDetails(&details);
  EXPECT_FALSE(details.quic_connection_migration_attempted);
  EXPECT_FALSE(details.quic_connection_migration_successful);

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // The specific network isn't important, we just want something !=
  // handles::kInvalidNetworkHandle to specify a non-default network.
  constexpr handles::NetworkHandle kNonDefaultNetwork = 1;
  constexpr uint8_t kTestIpAddress[] = {1, 2, 3, 4};
  constexpr uint16_t kTestPort = 123;
  base::RunLoop run_loop;
  QuicChromiumClientSession::MigrationCallback migration_callback =
      base::BindLambdaForTesting(
          [&run_loop](MigrationResult result) { run_loop.Quit(); });
  session->Migrate(kNonDefaultNetwork,
                   IPEndPoint(IPAddress(kTestIpAddress), kTestPort), true,
                   std::move(migration_callback));
  run_loop.Run();
  // The session should exist but no longer be active since its only stream has
  // been reset.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  session->PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_connection_migration_attempted);
  EXPECT_FALSE(details.quic_connection_migration_successful);

  // Run the message loop so that data queued due to the reset is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that the request failed since connection the stream couldn't be
  // migrated.
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(nullptr, response.headers);

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv4ToIPv4) {
  // Add alternate IPv4 server address to config.
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));
  config.SetPreferredAddressConnectionIdAndTokenToSend(
      kNewCID, quic::QuicUtils::GenerateStatelessResetToken(kNewCID));
  VerifyServerMigration(config, alt_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv6ToIPv6) {
  // Add a resolver rule to make initial connection to an IPv6 address.
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "fe80::aebc:32ff:febb:1e33", "");
  // Add alternate IPv6 server address to config.
  IPEndPoint alt_address = IPEndPoint(
      IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 123);
  quic::QuicConfig config;
  config.SetIPv6AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));
  config.SetPreferredAddressConnectionIdAndTokenToSend(
      kNewCID, quic::QuicUtils::GenerateStatelessResetToken(kNewCID));
  VerifyServerMigration(config, alt_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv6ToIPv4Fails) {
  quic_params_->allow_server_migration = true;
  Initialize();

  // Add a resolver rule to make initial connection to an IPv6 address.
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "fe80::aebc:32ff:febb:1e33", "");
  // Add alternate IPv4 server address to config.
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  crypto_client_stream_factory_.SetConfig(config);

  // Set up only socket data provider.
  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  IPEndPoint actual_address;
  session->GetDefaultSocket()->GetPeerAddress(&actual_address);
  // No migration should have happened.
  IPEndPoint expected_address =
      IPEndPoint(IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0xae, 0xbc, 0x32, 0xff,
                           0xfe, 0xbb, 0x1e, 0x33),
                 kDefaultServerPort);
  EXPECT_EQ(actual_address, expected_address);
  DVLOG(1) << "Socket connected to: " << actual_address.address().ToString()
           << " " << actual_address.port();
  DVLOG(1) << "Expected address: " << expected_address.address().ToString()
           << " " << expected_address.port();

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv4ToIPv6Fails) {
  quic_params_->allow_server_migration = true;
  Initialize();

  // Add a resolver rule to make initial connection to an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(), "1.2.3.4",
                                            "");
  // Add alternate IPv6 server address to config.
  IPEndPoint alt_address = IPEndPoint(
      IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 123);
  quic::QuicConfig config;
  config.SetIPv6AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  crypto_client_stream_factory_.SetConfig(config);

  // Set up only socket data provider.
  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDataPacket(
                            packet_num++, GetQpackDecoderStreamId(), false,
                            StreamCancellationQpackDecoderInstruction(0)));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

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

TEST_P(QuicStreamFactoryTest, OnCertDBChanged) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);

  // Synthesize a CertDatabase change notification and verify that stream saw
  // the event.
  CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2);
  QuicChromiumClientSession* session2 = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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

TEST_P(QuicStreamFactoryTest, OnCertVerifierChanged) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);

  // Synthesize a CertVerifier change notification and verify that stream saw
  // the event.
  cert_verifier_->SimulateOnCertVerifierChanged();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2);
  QuicChromiumClientSession* session2 = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
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
  cannoncial_suffixes.emplace_back(".c.youtube.com");
  cannoncial_suffixes.emplace_back(".googlevideo.com");

  for (const auto& cannoncial_suffix : cannoncial_suffixes) {
    string r1_host_name("r1");
    string r2_host_name("r2");
    r1_host_name.append(cannoncial_suffix);
    r2_host_name.append(cannoncial_suffix);

    url::SchemeHostPort scheme_host_port1(url::kHttpsScheme, r1_host_name, 80);
    // Need to hold onto this through the test, to keep the
    // QuicCryptoClientConfig alive.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               NetworkAnonymizationKey());
    quic::QuicServerId server_id1(scheme_host_port1.host(),
                                  scheme_host_port1.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r1_host_name);
    cached1->SetProofValid();

    url::SchemeHostPort scheme_host_port2(url::kHttpsScheme, r2_host_name, 80);
    quic::QuicServerId server_id2(scheme_host_port2.host(),
                                  scheme_host_port2.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id2);
    EXPECT_EQ(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, CryptoConfigWhenProofIsInvalid) {
  Initialize();
  std::vector<string> cannoncial_suffixes;
  cannoncial_suffixes.emplace_back(".c.youtube.com");
  cannoncial_suffixes.emplace_back(".googlevideo.com");

  for (const auto& cannoncial_suffix : cannoncial_suffixes) {
    string r3_host_name("r3");
    string r4_host_name("r4");
    r3_host_name.append(cannoncial_suffix);
    r4_host_name.append(cannoncial_suffix);

    url::SchemeHostPort scheme_host_port1(url::kHttpsScheme, r3_host_name, 80);
    // Need to hold onto this through the test, to keep the
    // QuicCryptoClientConfig alive.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               NetworkAnonymizationKey());
    quic::QuicServerId server_id1(scheme_host_port1.host(),
                                  scheme_host_port1.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r3_host_name);
    cached1->SetProofInvalid();

    url::SchemeHostPort scheme_host_port2(url::kHttpsScheme, r4_host_name, 80);
    quic::QuicServerId server_id2(scheme_host_port2.host(),
                                  scheme_host_port2.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id2);
    EXPECT_NE(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->source_address_token().empty());
    EXPECT_FALSE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, EnableNotLoadFromDiskCache) {
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  // If we are waiting for disk cache, we would have posted a task. Verify that
  // the CancelWaitForDataReady task hasn't been posted.
  ASSERT_EQ(0u, runner_->GetPostedTasks().size());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ReducePingTimeoutOnConnectionTimeOutOpenStreams) {
  quic_params_->reduced_ping_timeout = base::Seconds(10);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                              kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Quic should use default PING timeout when no previous connection times out
  // with open stream.
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs),
            QuicStreamFactoryPeer::GetPingTimeout(factory_.get()));
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                         CompletionOnceCallback()));

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
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  QuicChromiumClientSession* session2 = GetActiveSession(server2);

  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  stream2->RegisterRequest(&request_info);
  EXPECT_EQ(OK, stream2->InitializeStream(false, DEFAULT_PRIORITY, net_log_,
                                          CompletionOnceCallback()));
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
  VerifyInitialization(false /* vary_network_anonymization_key */);
}

TEST_P(QuicStreamFactoryTest, MaybeInitializeWithNetworkAnonymizationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkAnonymizationKey for
       // QuicSessionAliasKey to include NetworkAnonymizationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  VerifyInitialization(true /* vary_network_anonymization_key */);
}

// Without NetworkAnonymizationKeys enabled for HttpServerProperties, there
// should only be one global CryptoCache.
TEST_P(QuicStreamFactoryTest, CryptoConfigCache) {
  const char kUserAgentId[] = "spoon";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);

  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  const SchemefulSite kSite3(GURL("https://baz.test/"));
  const auto kNetworkAnonymizationKey3 =
      NetworkAnonymizationKey::CreateSameSite(kSite3);

  Initialize();

  // Create a QuicCryptoClientConfigHandle for kNetworkAnonymizationKey1, and
  // set the user agent.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey1);
  crypto_config_handle1->GetConfig()->set_user_agent_id(kUserAgentId);
  EXPECT_EQ(kUserAgentId, crypto_config_handle1->GetConfig()->user_agent_id());

  // Create another crypto config handle using a different
  // NetworkAnonymizationKey while the first one is still alive should return
  // the same config, with the user agent that was just set.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey2);
  EXPECT_EQ(kUserAgentId, crypto_config_handle2->GetConfig()->user_agent_id());

  // Destroying both handles and creating a new one with yet another
  // NetworkAnonymizationKey should again return the same config.
  crypto_config_handle1.reset();
  crypto_config_handle2.reset();

  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle3 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey3);
  EXPECT_EQ(kUserAgentId, crypto_config_handle3->GetConfig()->user_agent_id());
}

// With different NetworkAnonymizationKeys enabled for HttpServerProperties,
// there should only be one global CryptoCache per NetworkAnonymizationKey.
TEST_P(QuicStreamFactoryTest, CryptoConfigCacheWithNetworkAnonymizationKey) {
  const char kUserAgentId1[] = "spoon";
  const char kUserAgentId2[] = "fork";
  const char kUserAgentId3[] = "another spoon";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkAnonymizationKey for
       // QuicSessionAliasKey to include NetworkAnonymizationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);

  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  const SchemefulSite kSite3(GURL("https://baz.test/"));
  const auto kNetworkAnonymizationKey3 =
      NetworkAnonymizationKey::CreateSameSite(kSite3);

  Initialize();

  // Create a QuicCryptoClientConfigHandle for kNetworkAnonymizationKey1, and
  // set the user agent.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey1);
  crypto_config_handle1->GetConfig()->set_user_agent_id(kUserAgentId1);
  EXPECT_EQ(kUserAgentId1, crypto_config_handle1->GetConfig()->user_agent_id());

  // Create another crypto config handle using a different
  // NetworkAnonymizationKey while the first one is still alive should return a
  // different config.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey2);
  EXPECT_EQ("", crypto_config_handle2->GetConfig()->user_agent_id());
  crypto_config_handle2->GetConfig()->set_user_agent_id(kUserAgentId2);
  EXPECT_EQ(kUserAgentId1, crypto_config_handle1->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId2, crypto_config_handle2->GetConfig()->user_agent_id());

  // Creating handles with the same NIKs while the old handles are still alive
  // should result in getting the same CryptoConfigs.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1_2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey1);
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2_2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey2);
  EXPECT_EQ(kUserAgentId1,
            crypto_config_handle1_2->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId2,
            crypto_config_handle2_2->GetConfig()->user_agent_id());

  // Destroying all handles and creating a new one with yet another
  // NetworkAnonymizationKey return yet another config.
  crypto_config_handle1.reset();
  crypto_config_handle2.reset();
  crypto_config_handle1_2.reset();
  crypto_config_handle2_2.reset();

  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle3 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkAnonymizationKey3);
  EXPECT_EQ("", crypto_config_handle3->GetConfig()->user_agent_id());
  crypto_config_handle3->GetConfig()->set_user_agent_id(kUserAgentId3);
  EXPECT_EQ(kUserAgentId3, crypto_config_handle3->GetConfig()->user_agent_id());
  crypto_config_handle3.reset();

  // The old CryptoConfigs should be recovered when creating handles with the
  // same NIKs as before.
  crypto_config_handle2 = QuicStreamFactoryPeer::GetCryptoConfig(
      factory_.get(), kNetworkAnonymizationKey2);
  crypto_config_handle1 = QuicStreamFactoryPeer::GetCryptoConfig(
      factory_.get(), kNetworkAnonymizationKey1);
  crypto_config_handle3 = QuicStreamFactoryPeer::GetCryptoConfig(
      factory_.get(), kNetworkAnonymizationKey3);
  EXPECT_EQ(kUserAgentId1, crypto_config_handle1->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId2, crypto_config_handle2->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId3, crypto_config_handle3->GetConfig()->user_agent_id());
}

// Makes Verifies MRU behavior of the crypto config caches. Without
// NetworkAnonymizationKeys enabled, behavior is uninteresting, since there's
// only one cache, so nothing is ever evicted.
TEST_P(QuicStreamFactoryTest, CryptoConfigCacheMRUWithNetworkAnonymizationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkAnonymizationKey for
       // QuicSessionAliasKey to include NetworkAnonymizationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});

  const int kNumSessionsToMake = kMaxRecentCryptoConfigs + 5;

  Initialize();

  // Make more entries than the maximum, setting a unique user agent for each,
  // and keeping the handles alives.
  std::vector<std::unique_ptr<QuicCryptoClientConfigHandle>>
      crypto_config_handles;
  std::vector<NetworkAnonymizationKey> network_anonymization_keys;
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SchemefulSite site(GURL(base::StringPrintf("https://foo%i.test/", i)));
    network_anonymization_keys.emplace_back(
        NetworkAnonymizationKey::CreateSameSite(site));

    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_anonymization_keys[i]);
    crypto_config_handle->GetConfig()->set_user_agent_id(
        base::NumberToString(i));
    crypto_config_handles.emplace_back(std::move(crypto_config_handle));
  }

  // Since all the handles are still alive, nothing should be evicted yet.
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(base::NumberToString(i),
              crypto_config_handles[i]->GetConfig()->user_agent_id());

    // A new handle for the same NIK returns the same crypto config.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_anonymization_keys[i]);
    EXPECT_EQ(base::NumberToString(i),
              crypto_config_handle->GetConfig()->user_agent_id());
  }

  // Destroying the only remaining handle for a NIK results in evicting entries,
  // until there are exactly |kMaxRecentCryptoConfigs| handles.
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(base::NumberToString(i),
              crypto_config_handles[i]->GetConfig()->user_agent_id());

    crypto_config_handles[i].reset();

    // A new handle for the same NIK will return a new config, if the config was
    // evicted. Otherwise, it will return the same one.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_anonymization_keys[i]);
    if (kNumSessionsToMake - i > kNumSessionsToMake) {
      EXPECT_EQ("", crypto_config_handle->GetConfig()->user_agent_id());
    } else {
      EXPECT_EQ(base::NumberToString(i),
                crypto_config_handle->GetConfig()->user_agent_id());
    }
  }
}

// Similar to above test, but uses real requests, and doesn't keep Handles
// around, so evictions happen immediately.
TEST_P(QuicStreamFactoryTest,
       CryptoConfigCacheMRUWithRealRequestsAndWithNetworkAnonymizationKey) {
  const int kNumSessionsToMake = kMaxRecentCryptoConfigs + 5;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkAnonymizationKey for
       // QuicSessionAliasKey to include NetworkAnonymizationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  std::vector<NetworkAnonymizationKey> network_anonymization_keys;
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SchemefulSite site(GURL(base::StringPrintf("https://foo%i.test/", i)));
    network_anonymization_keys.emplace_back(
        NetworkAnonymizationKey::CreateSameSite(site));
  }

  const quic::QuicServerId kQuicServerId(
      kDefaultServerHostName, kDefaultServerPort, PRIVACY_MODE_DISABLED);

  quic_params_->max_server_configs_stored_in_properties = 1;
  quic_params_->idle_connection_timeout = base::Seconds(500);
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(500, config->IdleNetworkTimeout().ToSeconds());
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();

  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SCOPED_TRACE(i);
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

    const AlternativeService alternative_service1(
        kProtoQUIC, scheme_host_port_.host(), scheme_host_port_.port());
    AlternativeServiceInfoVector alternative_service_info_vector;
    base::Time expiration = base::Time::Now() + base::Days(1);
    alternative_service_info_vector.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service1, expiration, {version_}));
    http_server_properties_->SetAlternativeServices(
        url::SchemeHostPort(url_), network_anonymization_keys[i],
        alternative_service_info_vector);

    http_server_properties_->SetMaxServerConfigsStoredInProperties(
        kDefaultMaxQuicServerEntries);

    std::unique_ptr<QuicServerInfo> quic_server_info =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            kQuicServerId, network_anonymization_keys[i],
            http_server_properties_.get());

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

    // Create temporary strings because Persist() clears string data in |state|.
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

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    // For the close socket message.
    socket_data.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddSocketDataToFactory(socket_factory_.get());
    client_maker_.Reset();

    QuicStreamRequest request(factory_.get());
    int rv = request.Request(
        url::SchemeHostPort(url::kHttpsScheme, kDefaultServerHostName,
                            kDefaultServerPort),
        version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
        network_anonymization_keys[i], SecureDnsPolicy::kAllow,
        /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
        /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
        failed_on_default_network_callback_, callback_.callback());
    EXPECT_THAT(callback_.GetResult(rv), IsOk());

    // While the session is still alive, there should be
    // kMaxRecentCryptoConfigs+1 CryptoConfigCaches alive, since active configs
    // don't count towards the limit.
    for (int j = 0; j < kNumSessionsToMake; ++j) {
      SCOPED_TRACE(j);
      EXPECT_EQ(
          i - (kMaxRecentCryptoConfigs + 1) < j && j <= i,
          !QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
              factory_.get(), kQuicServerId, network_anonymization_keys[j]));
    }

    // Close the sessions, which should cause its CryptoConfigCache to be moved
    // to the MRU cache, potentially evicting the oldest entry..
    factory_->CloseAllSessions(ERR_FAILED, quic::QUIC_PEER_GOING_AWAY);

    // There should now be at most kMaxRecentCryptoConfigs live
    // CryptoConfigCaches
    for (int j = 0; j < kNumSessionsToMake; ++j) {
      SCOPED_TRACE(j);
      EXPECT_EQ(
          i - kMaxRecentCryptoConfigs < j && j <= i,
          !QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
              factory_.get(), kQuicServerId, network_anonymization_keys[j]));
    }
  }
}

TEST_P(QuicStreamFactoryTest, YieldAfterPackets) {
  if (!version_.SupportsClientConnectionIds()) {
    // When the version allows omitting the connection ID in short headers,
    // this test lacks the proper initialization for clients to successfully
    // process the packet.
    return;
  }
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  QuicStreamFactoryPeer::SetYieldAfterPackets(factory_.get(), 0);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ConstructServerConnectionClosePacket(1));
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicChromiumPacketReader::StartReading
  // posts a task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_chromium_packet_reader.cc",
                                       "StartReading");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  // Call run_loop so that QuicChromiumPacketReader::OnReadComplete() gets
  // called.
  base::RunLoop().RunUntilIdle();

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
  if (!version_.SupportsClientConnectionIds()) {
    // When the version allows omitting the connection ID in short headers,
    // this test lacks the proper initialization for clients to successfully
    // process the packet.
    return;
  }
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  QuicStreamFactoryPeer::SetYieldAfterDuration(
      factory_.get(), quic::QuicTime::Delta::FromMilliseconds(-1));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ConstructServerConnectionClosePacket(1));
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(scheme_host_port_.host(),
                                            "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicChromiumPacketReader::StartReading
  // posts a task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_chromium_packet_reader.cc",
                                       "StartReading");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  // Call run_loop so that QuicChromiumPacketReader::OnReadComplete() gets
  // called.
  base::RunLoop().RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicChromiumPacketReader::StartReading() has posted only one task and
  // yielded the read.
  EXPECT_EQ(1u, observer.executed_count());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_FALSE(stream.get());  // Session is already closed.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Pool to existing session with matching quic::QuicServerId
// even if destination is different.
TEST_P(QuicStreamFactoryTest, PoolByOrigin) {
  Initialize();

  url::SchemeHostPort destination1(url::kHttpsScheme, "first.example.com", 443);
  url::SchemeHostPort destination2(url::kHttpsScheme, "second.example.com",
                                   443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                destination1, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                destination2, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_TRUE(session1->SharesSameSession(*session2));
  EXPECT_EQ(
      quic::QuicServerId(scheme_host_port_.host(), scheme_host_port_.port(),
                         privacy_mode_ == PRIVACY_MODE_ENABLED),
      session1->server_id());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

namespace {

enum DestinationType {
  // In pooling tests with two requests for different origins to the same
  // destination, the destination should be
  SAME_AS_FIRST,   // the same as the first origin,
  SAME_AS_SECOND,  // the same as the second origin, or
  DIFFERENT,       // different from both.
};

// Run QuicStreamFactoryWithDestinationTest instances with all value
// combinations of version, enable_connection_racting, and destination_type.
struct PoolingTestParams {
  quic::ParsedQuicVersion version;
  DestinationType destination_type;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const PoolingTestParams& p) {
  const char* destination_string = "";
  switch (p.destination_type) {
    case SAME_AS_FIRST:
      destination_string = "SAME_AS_FIRST";
      break;
    case SAME_AS_SECOND:
      destination_string = "SAME_AS_SECOND";
      break;
    case DIFFERENT:
      destination_string = "DIFFERENT";
      break;
  }
  return base::StrCat(
      {ParsedQuicVersionToString(p.version), "_", destination_string});
}

std::vector<PoolingTestParams> GetPoolingTestParams() {
  std::vector<PoolingTestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      AllSupportedQuicVersions();
  for (const quic::ParsedQuicVersion& version : all_supported_versions) {
    params.push_back(PoolingTestParams{version, SAME_AS_FIRST});
    params.push_back(PoolingTestParams{version, SAME_AS_SECOND});
    params.push_back(PoolingTestParams{version, DIFFERENT});
  }
  return params;
}

}  // namespace

class QuicStreamFactoryWithDestinationTest
    : public QuicStreamFactoryTestBase,
      public ::testing::TestWithParam<PoolingTestParams> {
 protected:
  QuicStreamFactoryWithDestinationTest()
      : QuicStreamFactoryTestBase(GetParam().version),
        destination_type_(GetParam().destination_type),
        hanging_read_(SYNCHRONOUS, ERR_IO_PENDING, 0) {}

  url::SchemeHostPort GetDestination() {
    switch (destination_type_) {
      case SAME_AS_FIRST:
        return origin1_;
      case SAME_AS_SECOND:
        return origin2_;
      case DIFFERENT:
        return url::SchemeHostPort(url::kHttpsScheme, kDifferentHostname, 443);
      default:
        NOTREACHED();
        return url::SchemeHostPort();
    }
  }

  void AddHangingSocketData() {
    auto sequenced_socket_data = std::make_unique<SequencedSocketData>(
        base::make_span(&hanging_read_, 1u), base::span<MockWrite>());
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
  url::SchemeHostPort origin1_;
  url::SchemeHostPort origin2_;
  MockRead hanging_read_;
  std::vector<std::unique_ptr<SequencedSocketData>>
      sequenced_socket_data_vector_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicStreamFactoryWithDestinationTest,
                         ::testing::ValuesIn(GetPoolingTestParams()),
                         ::testing::PrintToStringParamName());

// A single QUIC request fails because the certificate does not match the origin
// hostname, regardless of whether it matches the alternative service hostname.
TEST_P(QuicStreamFactoryWithDestinationTest, InvalidCertificate) {
  if (destination_type_ == DIFFERENT)
    return;

  Initialize();

  GURL url("https://mail.example.com/");
  origin1_ = url::SchemeHostPort(url);

  // Not used for requests, but this provides a test case where the certificate
  // is valid for the hostname of the alternative service.
  origin2_ = url::SchemeHostPort(url::kHttpsScheme, "mail.example.org", 433);

  url::SchemeHostPort destination = GetDestination();

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
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                destination, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  origin1_ = url::SchemeHostPort(url1);
  origin2_ = url::SchemeHostPort(url2);

  url::SchemeHostPort destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                destination, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                destination, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
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

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// QuicStreamRequest is not pooled if PrivacyMode differs.
TEST_P(QuicStreamFactoryWithDestinationTest, DifferentPrivacyMode) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = url::SchemeHostPort(url1);
  origin2_ = url::SchemeHostPort(url2);

  url::SchemeHostPort destination = GetDestination();

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

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// QuicStreamRequest is not pooled if the secure_dns_policy field differs.
TEST_P(QuicStreamFactoryWithDestinationTest, DifferentSecureDnsPolicy) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = url::SchemeHostPort(url1);
  origin2_ = url::SchemeHostPort(url2);

  url::SchemeHostPort destination = GetDestination();

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

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
          SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
          /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
          /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  EXPECT_EQ(OK, callback2.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // |request2| does not pool to the first session, because |secure_dns_policy|
  // does not match.
  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_FALSE(session1->SharesSameSession(*session2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// QuicStreamRequest is not pooled if certificate does not match its origin.
TEST_P(QuicStreamFactoryWithDestinationTest, DisjointCertificate) {
  Initialize();

  GURL url1("https://news.example.org/");
  GURL url2("https://mail.example.com/");
  origin1_ = url::SchemeHostPort(url1);
  origin2_ = url::SchemeHostPort(url2);

  url::SchemeHostPort destination = GetDestination();

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

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request1.Request(
                destination, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                destination, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// This test verifies that QuicStreamFactory::ClearCachedStatesInCryptoConfig
// correctly transform an origin filter to a ServerIdFilter. Whether the
// deletion itself works correctly is tested in QuicCryptoClientConfigTest.
TEST_P(QuicStreamFactoryTest, ClearCachedStatesInCryptoConfig) {
  Initialize();
  // Need to hold onto this through the test, to keep the QuicCryptoClientConfig
  // alive.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             NetworkAnonymizationKey());

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
    raw_ptr<quic::QuicCryptoClientConfig::CachedState> state;
  } test_cases[] = {TestCase("www.google.com", 443, privacy_mode_,
                             crypto_config_handle->GetConfig()),
                    TestCase("www.example.com", 443, privacy_mode_,
                             crypto_config_handle->GetConfig()),
                    TestCase("www.example.com", 4433, privacy_mode_,
                             crypto_config_handle->GetConfig())};

  // Clear cached states for the origin https://www.example.com:4433.
  GURL origin("https://www.example.com:4433");
  factory_->ClearCachedStatesInCryptoConfig(base::BindRepeating(
      static_cast<bool (*)(const GURL&, const GURL&)>(::operator==), origin));
  EXPECT_FALSE(test_cases[0].state->certs().empty());
  EXPECT_FALSE(test_cases[1].state->certs().empty());
  EXPECT_TRUE(test_cases[2].state->certs().empty());

  // Clear all cached states.
  factory_->ClearCachedStatesInCryptoConfig(
      base::RepeatingCallback<bool(const GURL&)>());
  EXPECT_TRUE(test_cases[0].state->certs().empty());
  EXPECT_TRUE(test_cases[1].state->certs().empty());
  EXPECT_TRUE(test_cases[2].state->certs().empty());
}

// Passes connection options and client connection options to QuicStreamFactory,
// then checks that its internal quic::QuicConfig is correct.
TEST_P(QuicStreamFactoryTest, ConfigConnectionOptions) {
  quic_params_->connection_options.push_back(quic::kTIME);
  quic_params_->connection_options.push_back(quic::kTBBR);
  quic_params_->connection_options.push_back(quic::kREJ);

  quic_params_->client_connection_options.push_back(quic::kTBBR);
  quic_params_->client_connection_options.push_back(quic::k1RTT);

  Initialize();

  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(quic_params_->connection_options, config->SendConnectionOptions());
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, MAXIMUM_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->last_request_priority());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HostResolverRequestReprioritizedOnSetPriority) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, MAXIMUM_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->last_request_priority());
  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->request_priority(1));

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());
  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->request_priority(2));

  request.SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, host_resolver_->request_priority(1));
  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->request_priority(2));
}

// Verifies that the host resolver uses the disable secure DNS setting and
// NetworkAnonymizationKey passed to QuicStreamRequest::Request().
TEST_P(QuicStreamFactoryTest, HostResolverUsesParams) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionConnectionsByNetworkIsolationKey,
       features::kSplitHostCacheByNetworkIsolationKey},
      // disabled_features
      {});

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), kNetworkAnonymizationKey, SecureDnsPolicy::kDisable,
          /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(net::SecureDnsPolicy::kDisable,
            host_resolver_->last_secure_dns_policy());
  ASSERT_TRUE(
      host_resolver_->last_request_network_anonymization_key().has_value());
  EXPECT_EQ(kNetworkAnonymizationKey,
            host_resolver_->last_request_network_anonymization_key().value());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ConfigMaxTimeBeforeCryptoHandshake) {
  quic_params_->max_time_before_crypto_handshake = base::Seconds(11);
  quic_params_->max_idle_time_before_crypto_handshake = base::Seconds(13);
  Initialize();

  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(11),
            config->max_time_before_crypto_handshake());
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(13),
            config->max_idle_time_before_crypto_handshake());
}

// Verify ResultAfterQuicSessionCreationCallback behavior when the crypto
// handshake fails.
TEST_P(QuicStreamFactoryTest, ResultAfterQuicSessionCreationCallbackFail) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback quic_session_callback;
  EXPECT_TRUE(
      request.WaitForQuicSessionCreation(quic_session_callback.callback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(quic_session_callback.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, quic_session_callback.WaitForResult());

  // Calling WaitForQuicSessionCreation() a second time should return
  // false since the session has been created.
  EXPECT_FALSE(
      request.WaitForQuicSessionCreation(quic_session_callback.callback()));

  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterQuicSessionCreationCallback behavior when the crypto
// handshake succeeds synchronously.
TEST_P(QuicStreamFactoryTest,
       ResultAfterQuicSessionCreationCallbackSuccessSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, OK);
  socket_data.AddWrite(SYNCHRONOUS, OK);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback quic_session_callback;
  EXPECT_TRUE(
      request.WaitForQuicSessionCreation(quic_session_callback.callback()));

  EXPECT_EQ(OK, quic_session_callback.WaitForResult());

  // Calling WaitForQuicSessionCreation() a second time should return
  // false since the session has been created.
  EXPECT_FALSE(
      request.WaitForQuicSessionCreation(quic_session_callback.callback()));

  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(OK, callback_.WaitForResult());
}

// Verify ResultAfterQuicSessionCreationCallback behavior when the crypto
// handshake succeeds asynchronously.
TEST_P(QuicStreamFactoryTest,
       ResultAfterQuicSessionCreationCallbackSuccessAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, OK);
  socket_data.AddWrite(SYNCHRONOUS, OK);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback quic_session_callback;
  EXPECT_TRUE(
      request.WaitForQuicSessionCreation(quic_session_callback.callback()));

  EXPECT_EQ(ERR_IO_PENDING, quic_session_callback.WaitForResult());

  // Send Crypto handshake so connect will call back.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Calling WaitForQuicSessionCreation() a second time should return
  // false since the session has been created.
  EXPECT_FALSE(
      request.WaitForQuicSessionCreation(quic_session_callback.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds asynchronously, then crypto handshake fails synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackAsyncSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  EXPECT_EQ(ERR_IO_PENDING, host_resolution_callback.WaitForResult());

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
  factory_->set_is_quic_known_to_work_on_current_network(false);

  MockQuicData socket_data(version_);
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_FAILED);
  socket_data.AddWrite(ASYNC, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has finished already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());
  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
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
  factory_->set_is_quic_known_to_work_on_current_network(false);

  MockQuicData socket_data(version_);
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_FAILED);
  socket_data.AddWrite(ASYNC, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
  host_resolver_->rules()->AddSimulatedFailure(scheme_host_port_.host());
  host_resolver_->set_synchronous_mode(true);

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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

  host_resolver_->rules()->AddSimulatedFailure(scheme_host_port_.host());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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

// Test that QuicStreamRequests with similar and different tags results in
// reused and unique QUIC streams using appropriately tagged sockets.
TEST_P(QuicStreamFactoryTest, Tag) {
  socket_factory_ = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory =
      static_cast<MockTaggingClientSocketFactory*>(socket_factory_.get());
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Prepare to establish two QUIC sessions.
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

#if BUILDFLAG(IS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
#else
  // On non-Android platforms we can only use the default constructor.
  SocketTag tag1, tag2;
#endif

  // Request a stream with |tag1|.
  QuicStreamRequest request1(factory_.get());
  int rv = request1.Request(
      scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY, tag1,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
      scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY, tag1,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
      scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY, tag2,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
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
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(stream3->SharesSameSession(*stream1));
#else
  // Same tag should reuse session.
  EXPECT_TRUE(stream3->SharesSameSession(*stream1));
#endif
}

TEST_P(QuicStreamFactoryTest, ReadErrorClosesConnection) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_CONNECTION_REFUSED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream to trigger creation of the session.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that the session is alive and active before we read the error.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Resume the socket data to get the read error delivered.
  socket_data.Resume();
  // Ensure that the session is no longer active.
  EXPECT_FALSE(HasActiveSession(scheme_host_port_));
}

TEST_P(QuicStreamFactoryTest, MessageTooBigReadErrorDoesNotCloseConnection) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_MSG_TOO_BIG);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream to trigger creation of the session.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that the session is alive and active before we read the error.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Resume the socket data to get the read error delivered.
  socket_data.Resume();
  // Ensure that the session is still active.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
}

TEST_P(QuicStreamFactoryTest, ZeroLengthReadDoesNotCloseConnection) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, 0);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream to trigger creation of the session.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that the session is alive and active before we read the error.
  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));

  // Resume the socket data to get the zero-length read delivered.
  socket_data.Resume();
  // Ensure that the session is still active.
  EXPECT_TRUE(HasActiveSession(scheme_host_port_));
}

TEST_P(QuicStreamFactoryTest, DnsAliasesCanBeAccessedFromStream) {
  std::vector<std::string> dns_aliases(
      {"alias1", "alias2", scheme_host_port_.host()});
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(
      scheme_host_port_.host(), "192.168.0.1", std::move(dns_aliases));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  EXPECT_THAT(
      stream->GetDnsAliases(),
      testing::ElementsAre("alias1", "alias2", scheme_host_port_.host()));
}

TEST_P(QuicStreamFactoryTest, NoAdditionalDnsAliases) {
  std::vector<std::string> dns_aliases;
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(
      scheme_host_port_.host(), "192.168.0.1", std::move(dns_aliases));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  EXPECT_THAT(stream->GetDnsAliases(),
              testing::ElementsAre(scheme_host_port_.host()));
}

TEST_P(QuicStreamFactoryTest, DoNotUseDnsAliases) {
  std::vector<std::string> dns_aliases({"alias1", "alias2"});
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(
      scheme_host_port_.host(), "192.168.0.1", std::move(dns_aliases));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/false, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  EXPECT_TRUE(stream->GetDnsAliases().empty());
}

TEST_P(QuicStreamFactoryTest, ConnectErrorInCreateWithDnsAliases) {
  std::vector<std::string> dns_aliases({"alias1", "alias2"});
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(
      scheme_host_port_.host(), "192.168.0.1", std::move(dns_aliases));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, RequireDnsHttpsAlpnNoHttpsRecord) {
  std::vector<HostResolverEndpointResult> endpoints(1);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  TestRequireDnsHttpsAlpn(std::move(endpoints), /*expect_success=*/false);
}

TEST_P(QuicStreamFactoryTest, RequireDnsHttpsAlpnMatch) {
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {
      quic::AlpnForVersion(version_)};
  // Add a final non-protocol endpoint at the end.
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  TestRequireDnsHttpsAlpn(std::move(endpoints), /*expect_success=*/true);
}

TEST_P(QuicStreamFactoryTest, RequireDnsHttpsAlpnUnknownAlpn) {
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {"unknown"};
  // Add a final non-protocol endpoint at the end.
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  TestRequireDnsHttpsAlpn(std::move(endpoints), /*expect_success=*/false);
}

TEST_P(QuicStreamFactoryTest, RequireDnsHttpsAlpnUnknownAndSupportedAlpn) {
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {
      "unknown", quic::AlpnForVersion(version_)};
  // Add a final non-protocol endpoint at the end.
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  TestRequireDnsHttpsAlpn(std::move(endpoints), /*expect_success=*/true);
}

// QUIC has many string representations of versions. Only the ALPN name is
// acceptable in HTTPS/SVCB records.
TEST_P(QuicStreamFactoryTest, RequireDnsHttpsNotAlpnName) {
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {
      quic::ParsedQuicVersionToString(version_)};
  // Add a final non-protocol endpoint at the end.
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  TestRequireDnsHttpsAlpn(std::move(endpoints), /*expect_success=*/false);
}

// If the only routes come from HTTPS/SVCB records (impossible until
// https://crbug.com/1417033 is implemented), we should still pick up the
// address from the HTTPS record.
TEST_P(QuicStreamFactoryTest, RequireDnsHttpsRecordOnly) {
  std::vector<HostResolverEndpointResult> endpoints(1);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {
      quic::AlpnForVersion(version_)};
  TestRequireDnsHttpsAlpn(std::move(endpoints), /*expect_success=*/true);
}

void QuicStreamFactoryTestBase::TestRequireDnsHttpsAlpn(
    std::vector<HostResolverEndpointResult> endpoints,
    bool expect_success) {
  quic_params_->supported_versions = {version_};
  host_resolver_ = std::make_unique<MockHostResolver>();
  host_resolver_->rules()->AddRule(
      scheme_host_port_.host(),
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{scheme_host_port_.host()}));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, quic::ParsedQuicVersion::Unsupported(),
                privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/true,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  if (expect_success) {
    EXPECT_THAT(callback_.WaitForResult(), IsOk());
  } else {
    EXPECT_THAT(callback_.WaitForResult(),
                IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
  }
}

namespace {

// Run QuicStreamFactoryDnsAliasPoolingTest instances with all value
// combinations of version, H2 stream dependency or not, DNS alias use or not,
// and example DNS aliases. `expected_dns_aliases*` params are dependent on
// `use_dns_aliases`, `dns_aliases1`, and `dns_aliases2`.
struct DnsAliasPoolingTestParams {
  quic::ParsedQuicVersion version;
  bool use_dns_aliases;
  std::set<std::string> dns_aliases1;
  std::set<std::string> dns_aliases2;
  std::set<std::string> expected_dns_aliases1;
  std::set<std::string> expected_dns_aliases2;
};

std::string PrintToString(const std::set<std::string>& set) {
  std::string joined;
  for (const std::string& str : set) {
    if (!joined.empty())
      joined += "_";
    joined += str;
  }
  return joined;
}

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const DnsAliasPoolingTestParams& p) {
  return base::StrCat({ParsedQuicVersionToString(p.version), "_",
                       (p.use_dns_aliases ? "" : "DoNot"), "UseDnsAliases_1st_",
                       PrintToString(p.dns_aliases1), "_2nd_",
                       PrintToString(p.dns_aliases2)});
}

std::vector<DnsAliasPoolingTestParams> GetDnsAliasPoolingTestParams() {
  std::vector<DnsAliasPoolingTestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      AllSupportedQuicVersions();
  for (const quic::ParsedQuicVersion& version : all_supported_versions) {
    params.push_back(DnsAliasPoolingTestParams{version,
                                               false /* use_dns_aliases */,
                                               {} /* dns_aliases1 */,
                                               {} /* dns_aliases2 */,
                                               {} /* expected_dns_aliases1 */,
                                               {} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{
        version,
        true /* use_dns_aliases */,
        {} /* dns_aliases1 */,
        {} /* dns_aliases2 */,
        {kDefaultServerHostName} /* expected_dns_aliases1 */,
        {kServer2HostName} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{version,
                                               false /* use_dns_aliases */,
                                               {"alias1", "alias2", "alias3"},
                                               {} /* dns_aliases2 */,
                                               {} /* expected_dns_aliases1 */,
                                               {} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{
        version,
        true /* use_dns_aliases */,
        {"alias1", "alias2", "alias3"} /* dns_aliases1 */,
        {} /* dns_aliases2 */,
        {"alias1", "alias2", "alias3"} /* expected_dns_aliases1 */,
        {kServer2HostName} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{
        version,
        false /* use_dns_aliases */,
        {"alias1", "alias2", "alias3"} /* dns_aliases1 */,
        {"alias3", "alias4", "alias5"} /* dns_aliases2 */,
        {} /* expected_dns_aliases1 */,
        {} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{
        version,
        true /* use_dns_aliases */,
        {"alias1", "alias2", "alias3"} /* dns_aliases1 */,
        {"alias3", "alias4", "alias5"} /* dns_aliases2 */,
        {"alias1", "alias2", "alias3"} /* expected_dns_aliases1 */,
        {"alias3", "alias4", "alias5"} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{
        version,
        false /* use_dns_aliases */,
        {} /* dns_aliases1 */,
        {"alias3", "alias4", "alias5"} /* dns_aliases2 */,
        {} /* expected_dns_aliases1 */,
        {} /* expected_dns_aliases2 */});
    params.push_back(DnsAliasPoolingTestParams{
        version,
        true /* use_dns_aliases */,
        {} /* dns_aliases1 */,
        {"alias3", "alias4", "alias5"} /* dns_aliases2 */,
        {kDefaultServerHostName} /* expected_dns_aliases1 */,
        {"alias3", "alias4", "alias5"} /* expected_dns_aliases2 */});
  }
  return params;
}

}  // namespace

class QuicStreamFactoryDnsAliasPoolingTest
    : public QuicStreamFactoryTestBase,
      public ::testing::TestWithParam<DnsAliasPoolingTestParams> {
 protected:
  QuicStreamFactoryDnsAliasPoolingTest()
      : QuicStreamFactoryTestBase(GetParam().version),
        use_dns_aliases_(GetParam().use_dns_aliases),
        dns_aliases1_(GetParam().dns_aliases1),
        dns_aliases2_(GetParam().dns_aliases2),
        expected_dns_aliases1_(GetParam().expected_dns_aliases1),
        expected_dns_aliases2_(GetParam().expected_dns_aliases2) {}

  const bool use_dns_aliases_;
  const std::set<std::string> dns_aliases1_;
  const std::set<std::string> dns_aliases2_;
  const std::set<std::string> expected_dns_aliases1_;
  const std::set<std::string> expected_dns_aliases2_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicStreamFactoryDnsAliasPoolingTest,
                         ::testing::ValuesIn(GetDnsAliasPoolingTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicStreamFactoryDnsAliasPoolingTest, IPPooling) {
  Initialize();

  const GURL kUrl1(kDefaultUrl);
  const GURL kUrl2(kServer2Url);
  const url::SchemeHostPort kOrigin1 = url::SchemeHostPort(kUrl1);
  const url::SchemeHostPort kOrigin2 = url::SchemeHostPort(kUrl2);

  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(
      kOrigin1.host(), "192.168.0.1", std::move(dns_aliases1_));
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(
      kOrigin2.host(), "192.168.0.1", std::move(dns_aliases2_));

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(kOrigin1.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(kOrigin2.host()));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          kOrigin1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow, use_dns_aliases_,
          /*require_dns_https_alpn=*/false, /*cert_verify_flags=*/0, kUrl1,
          net_log_, &net_error_details_, failed_on_default_network_callback_,
          callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(kOrigin1));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          kOrigin2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkAnonymizationKey(), SecureDnsPolicy::kAllow, use_dns_aliases_,
          /*require_dns_https_alpn=*/false, /*cert_verify_flags=*/0, kUrl2,
          net_log_, &net_error_details_, failed_on_default_network_callback_,
          callback2.callback()));
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  EXPECT_TRUE(HasActiveSession(kOrigin2));

  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_TRUE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(kOrigin1.host(), kOrigin1.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  EXPECT_EQ(expected_dns_aliases1_, stream1->GetDnsAliases());
  EXPECT_EQ(expected_dns_aliases2_, stream2->GetDnsAliases());
}

class QuicStreamFactoryEchTest : public QuicStreamFactoryTestBase,
                                 public ::testing::TestWithParam<TestParams> {
 protected:
  QuicStreamFactoryEchTest()
      : QuicStreamFactoryTestBase(GetParam().version,
                                  /*enabled_features=*/
                                  {features::kEncryptedClientHello,
                                   features::kEncryptedClientHelloQuic}) {}
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicStreamFactoryEchTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

// Test that, even if DNS does not provide ECH keys, ECH GREASE is enabled.
TEST_P(QuicStreamFactoryEchTest, EchGrease) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  ASSERT_TRUE(session);
  quic::QuicSSLConfig config = session->GetSSLConfig();
  EXPECT_TRUE(config.ech_grease_enabled);
  EXPECT_TRUE(config.ech_config_list.empty());
}

// Test that, connections where we discover QUIC from Alt-Svc (as opposed to
// HTTPS-RR), ECH is picked up from DNS.
TEST_P(QuicStreamFactoryEchTest, EchWithQuicFromAltSvc) {
  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoint.metadata.supported_protocol_alpns = {quic::AlpnForVersion(version_)};
  endpoint.metadata.ech_config_list = {1, 2, 3, 4};

  host_resolver_ = std::make_unique<MockHostResolver>();
  host_resolver_->rules()->AddRule(
      scheme_host_port_.host(),
      MockHostResolverBase::RuleResolver::RuleResult({endpoint}));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  ASSERT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session = GetActiveSession(scheme_host_port_);
  ASSERT_TRUE(session);
  quic::QuicSSLConfig config = session->GetSSLConfig();
  EXPECT_EQ(std::string(endpoint.metadata.ech_config_list.begin(),
                        endpoint.metadata.ech_config_list.end()),
            config.ech_config_list);
}

// Test that, connections where we discover QUIC from HTTPS-RR (as opposed to
// Alt-Svc), ECH is picked up from DNS.
TEST_P(QuicStreamFactoryEchTest, EchWithQuicFromHttpsRecord) {
  quic_params_->supported_versions = {version_};
  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoint.metadata.supported_protocol_alpns = {quic::AlpnForVersion(version_)};
  endpoint.metadata.ech_config_list = {1, 2, 3, 4};

  host_resolver_ = std::make_unique<MockHostResolver>();
  host_resolver_->rules()->AddRule(
      scheme_host_port_.host(),
      MockHostResolverBase::RuleResolver::RuleResult({endpoint}));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, quic::ParsedQuicVersion::Unsupported(),
                privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/true,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  ASSERT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session =
      GetActiveSession(scheme_host_port_, NetworkAnonymizationKey(),
                       /*require_dns_https_alpn=*/true);
  ASSERT_TRUE(session);
  quic::QuicSSLConfig config = session->GetSSLConfig();
  EXPECT_EQ(std::string(endpoint.metadata.ech_config_list.begin(),
                        endpoint.metadata.ech_config_list.end()),
            config.ech_config_list);
}

// Test that, when ECH is disabled, neither ECH nor ECH GREASE are configured.
TEST_P(QuicStreamFactoryEchTest, EchDisabled) {
  quic_params_->supported_versions = {version_};
  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoint.metadata.supported_protocol_alpns = {quic::AlpnForVersion(version_)};
  endpoint.metadata.ech_config_list = {1, 2, 3, 4};

  host_resolver_ = std::make_unique<MockHostResolver>();
  host_resolver_->rules()->AddRule(
      scheme_host_port_.host(),
      MockHostResolverBase::RuleResolver::RuleResult({endpoint}));

  SSLContextConfig ssl_config;
  ssl_config.ech_enabled = false;
  ssl_config_service_.UpdateSSLConfigAndNotify(ssl_config);

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, quic::ParsedQuicVersion::Unsupported(),
                privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/true,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  ASSERT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session =
      GetActiveSession(scheme_host_port_, NetworkAnonymizationKey(),
                       /*require_dns_https_alpn=*/true);
  ASSERT_TRUE(session);
  quic::QuicSSLConfig config = session->GetSSLConfig();
  EXPECT_TRUE(config.ech_config_list.empty());
  EXPECT_FALSE(config.ech_grease_enabled);
}

// Test that, when the server supports ECH, the connection should use
// SVCB-reliant behavior.
TEST_P(QuicStreamFactoryEchTest, EchSvcbReliant) {
  // The HTTPS-RR route only advertises HTTP/2 and is therefore incompatible
  // with QUIC. The fallback A/AAAA is compatible, but is ineligible in
  // ECH-capable clients.
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {"h2"};
  endpoints[0].metadata.ech_config_list = {1, 2, 3, 4};
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};

  host_resolver_ = std::make_unique<MockHostResolver>();
  host_resolver_->rules()->AddRule(
      scheme_host_port_.host(),
      MockHostResolverBase::RuleResolver::RuleResult(std::move(endpoints)));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(),
              IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
}

// Test that, when ECH is disabled, SVCB-reliant behavior doesn't trigger.
TEST_P(QuicStreamFactoryEchTest, EchDisabledSvcbOptional) {
  // The HTTPS-RR route only advertises HTTP/2 and is therefore incompatible
  // with QUIC. The fallback A/AAAA is compatible, but is ineligible in
  // ECH-capable clients.
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {"h2"};
  endpoints[0].metadata.ech_config_list = {1, 2, 3, 4};
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};

  host_resolver_ = std::make_unique<MockHostResolver>();
  host_resolver_->rules()->AddRule(
      scheme_host_port_.host(),
      MockHostResolverBase::RuleResolver::RuleResult(std::move(endpoints)));

  // But this client is not ECH-capable, so the connection should succeed.
  SSLContextConfig ssl_config;
  ssl_config.ech_enabled = false;
  ssl_config_service_.UpdateSSLConfigAndNotify(ssl_config);

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(
                scheme_host_port_, version_, privacy_mode_, DEFAULT_PRIORITY,
                SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                /*use_dns_aliases=*/true, /*require_dns_https_alpn=*/false,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
}

}  // namespace net::test
