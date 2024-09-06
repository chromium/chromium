// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_TEST_BASE_H_
#define NET_QUIC_QUIC_SESSION_POOL_TEST_BASE_H_

#include <sys/types.h>

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_error_details.h"
#include "net/base/privacy_mode.h"
#include "net/base/session_usage.h"
#include "net/base/test_proxy_delegate.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_stream.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/platform/impl/quic_test_flags_utils.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_session.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"

namespace net::test {

class QuicSessionPoolTestBase : public WithTaskEnvironment {
 public:
  static constexpr char kDefaultServerHostName[] = "www.example.org";
  static constexpr char kServer2HostName[] = "mail.example.org";
  static constexpr char kServer3HostName[] = "docs.example.org";
  static constexpr char kServer4HostName[] = "images.example.org";
  static constexpr char kServer5HostName[] = "accounts.example.org";
  static constexpr char kProxy1HostName[] = "proxy1.example.org";
  static constexpr char kProxy2HostName[] = "proxy2.example.org";
  static constexpr char kDifferentHostname[] = "different.example.com";
  static constexpr int kDefaultServerPort = 443;
  static constexpr char kDefaultUrl[] = "https://www.example.org/";
  static constexpr char kServer2Url[] = "https://mail.example.org/";
  static constexpr char kServer3Url[] = "https://docs.example.org/";
  static constexpr char kServer4Url[] = "https://images.example.org/";
  static constexpr char kServer5Url[] = "https://images.example.org/";
  static constexpr char kProxy1Url[] = "https://proxy1.example.org/";
  static constexpr char kProxy2Url[] = "https://proxy2.example.org/";
  static constexpr size_t kMinRetryTimeForDefaultNetworkSecs = 1;
  static constexpr size_t kWaitTimeForNewNetworkSecs = 10;
  static constexpr uint64_t kConnectUdpContextId = 0;

 protected:
  explicit QuicSessionPoolTestBase(
      quic::ParsedQuicVersion version,
      std::vector<base::test::FeatureRef> enabled_features = {},
      std::vector<base::test::FeatureRef> disabled_features = {});
  ~QuicSessionPoolTestBase();

  void Initialize();

  // Make a NEW_CONNECTION_ID frame available for client such that connection
  // migration can begin with a new connection ID. A side effect of calling
  // this function is that ACK_FRAME that should have been sent for the first
  // packet read might be skipped in the unit test. If the order of ACKing is
  // important for a test, use QuicTestPacketMaker::MakeNewConnectionIdPacket
  // instead.
  void MaybeMakeNewConnectionIdAvailableToSession(
      const quic::QuicConnectionId& new_cid,
      quic::QuicSession* session,
      uint64_t sequence_number = 1u);

  // Helper for building requests and invoking `QuicSessionRequest::Request`.
  // This `Request` method has lots of arguments, most of which are always at
  // their default values, so this helper supports specifying only the
  // non-default arguments relevant to a specific test.
  struct RequestBuilder {
    RequestBuilder(QuicSessionPoolTestBase* test, QuicSessionPool* pool);
    explicit RequestBuilder(QuicSessionPoolTestBase* test);
    ~RequestBuilder();

    RequestBuilder(const RequestBuilder&) = delete;
    RequestBuilder& operator=(const RequestBuilder&) = delete;

    // Call the request's `Request` method with the parameters in the builder.
    // The builder becomes invalid after this call.
    int CallRequest();

    // Arguments to request.Request().
    url::SchemeHostPort destination{url::kHttpsScheme, kDefaultServerHostName,
                                    kDefaultServerPort};
    quic::ParsedQuicVersion quic_version;
    ProxyChain proxy_chain = ProxyChain::Direct();
    std::optional<NetworkTrafficAnnotationTag> proxy_annotation_tag =
        TRAFFIC_ANNOTATION_FOR_TESTS;
    raw_ptr<HttpUserAgentSettings> http_user_agent_settings = nullptr;
    SessionUsage session_usage = SessionUsage::kDestination;
    PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;
    RequestPriority priority = DEFAULT_PRIORITY;
    SocketTag socket_tag;
    NetworkAnonymizationKey network_anonymization_key;
    SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow;
    bool require_dns_https_alpn = false;
    int cert_verify_flags = 0;
    GURL url = GURL(kDefaultUrl);
    NetLogWithSource net_log;
    NetErrorDetails net_error_details;
    CompletionOnceCallback failed_on_default_network_callback;
    CompletionOnceCallback callback;

    // The resulting request.
    QuicSessionRequest request;
  };

  std::unique_ptr<HttpStream> CreateStream(QuicSessionRequest* request);

  bool HasActiveSession(
      const url::SchemeHostPort& scheme_host_port,
      PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey(),
      const ProxyChain& proxy_chain = ProxyChain::Direct(),
      SessionUsage session_usage = SessionUsage::kDestination,
      bool require_dns_https_alpn = false);
  bool HasActiveJob(const url::SchemeHostPort& scheme_host_port,
                    const PrivacyMode privacy_mode,
                    bool require_dns_https_alpn = false);

  // Get the pending, not activated session, if there is only one session alive.
  QuicChromiumClientSession* GetPendingSession(
      const url::SchemeHostPort& scheme_host_port);
  QuicChromiumClientSession* GetActiveSession(
      const url::SchemeHostPort& scheme_host_port,
      PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey(),
      const ProxyChain& proxy_chain = ProxyChain::Direct(),
      SessionUsage session_usage = SessionUsage::kDestination,
      bool require_dns_https_alpn = false);

  int GetSourcePortForNewSessionAndGoAway(
      const url::SchemeHostPort& destination);
  int GetSourcePortForNewSessionInner(const url::SchemeHostPort& destination,
                                      bool goaway_received);

  static ProofVerifyDetailsChromium DefaultProofVerifyDetails();

  void NotifyIPAddressChanged();

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerConnectionClosePacket(uint64_t num);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructGetRequestPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructConnectUdpRequestPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      std::string authority,
      std::string path,
      bool fin);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructConnectUdpRequestPacket(
      QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      std::string authority,
      std::string path,
      bool fin);
  std::string ConstructClientH3DatagramFrame(
      uint64_t quarter_stream_id,
      uint64_t context_id,
      std::unique_ptr<quic::QuicEncryptedPacket> inner);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientH3DatagramPacket(
      uint64_t packet_number,
      uint64_t quarter_stream_id,
      uint64_t context_id,
      std::unique_ptr<quic::QuicEncryptedPacket> inner);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructOkResponsePacket(
      QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructOkResponsePacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin);
  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket();
  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number);
  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      QuicTestPacketMaker& packet_maker,
      uint64_t packet_number);
  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerSettingsPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructAckPacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      uint64_t packet_num_received,
      uint64_t smallest_received,
      uint64_t largest_received);
  std::string ConstructDataHeader(size_t body_len);

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      std::string_view data);

  std::string ConstructH3Datagram(
      uint64_t stream_id,
      uint64_t context_id,
      std::unique_ptr<quic::QuicEncryptedPacket> packet);

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) const;
  quic::QuicStreamId GetQpackDecoderStreamId() const;
  std::string StreamCancellationQpackDecoderInstruction(int n) const;
  std::string StreamCancellationQpackDecoderInstruction(
      int n,
      bool create_stream) const;
  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n);

  void OnFailedOnDefaultNetwork(int rv);

  const quic::QuicConnectionId kNewCID = quic::test::TestConnectionId(12345678);
  const url::SchemeHostPort kDefaultDestination{
      url::kHttpsScheme, kDefaultServerHostName, kDefaultServerPort};

  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<MockHostResolverBase> host_resolver_;
  TestSSLConfigService ssl_config_service_{SSLContextConfig()};
  std::unique_ptr<MockClientSocketFactory> socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockQuicContext context_;
  const quic::ParsedQuicVersion version_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<TestProxyDelegate> proxy_delegate_;
  std::unique_ptr<ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier_;
  std::unique_ptr<QuicSessionPool> factory_;

  NetLogWithSource net_log_;
  TestCompletionCallback callback_;
  const CompletionRepeatingCallback failed_on_default_network_callback_;
  bool failed_on_default_network_ = false;
  NetErrorDetails net_error_details_;
  StaticHttpUserAgentSettings http_user_agent_settings_ = {"test-lang",
                                                           "test-ua"};

  raw_ptr<QuicParams> quic_params_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace net::test

#endif  // NET_QUIC_QUIC_SESSION_POOL_TEST_BASE_H_
