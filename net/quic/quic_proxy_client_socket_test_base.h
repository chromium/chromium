// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROXY_CLIENT_SOCKET_TEST_BASE_H_
#define NET_QUIC_QUIC_PROXY_CLIENT_SOCKET_TEST_BASE_H_

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/default_tick_clock.h"
#include "mock_quic_data.h"
#include "net/base/test_proxy_delegate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_factory.h"
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
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_proxy_client_socket.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace {

inline constexpr char kOriginHost[] = "www.google.com";
inline constexpr int kOriginPort = 443;
inline constexpr char kProxyUrl[] = "https://myproxy:6121/";
inline constexpr char kProxyHost[] = "myproxy";
inline constexpr int kProxyPort = 6121;
inline constexpr char kUserAgent[] = "Mozilla/1.0";
inline constexpr char kRedirectUrl[] = "https://example.com/";

inline constexpr char kMsg1[] = "\0hello!\xff";
inline constexpr int kLen1 = 8;
inline constexpr char kMsg2[] = "\0a2345678\0";
inline constexpr int kLen2 = 10;
inline constexpr char kMsg3[] = "bye!";
inline constexpr int kLen3 = 4;
inline constexpr char kMsg33[] = "bye!bye!";
inline constexpr int kLen33 = kLen3 + kLen3;
inline constexpr char kMsg333[] = "bye!bye!bye!";
inline constexpr int kLen333 = kLen3 + kLen3 + kLen3;

inline constexpr char kDatagramPayload[] = "youveGotMail";
inline constexpr int kDatagramLen = 12;

static inline constexpr int k0ByteConnectionId = 0;
static inline constexpr int k8ByteConnectionId = 8;

inline constexpr char kTestHeaderName[] = "Foo";
// Note: `kTestQuicHeaderName` should be a lowercase version of
// `kTestHeaderName`.
inline constexpr char kTestQuicHeaderName[] = "foo";

}  // anonymous namespace

namespace net {

class QuicProxyClientSocketTestBase
    : public ::testing::TestWithParam<quic::ParsedQuicVersion>,
      public WithTaskEnvironment {
 public:
  QuicProxyClientSocketTestBase();
  QuicProxyClientSocketTestBase(const QuicProxyClientSocketTestBase&) = delete;
  QuicProxyClientSocketTestBase& operator=(
      const QuicProxyClientSocketTestBase&) = delete;

  ~QuicProxyClientSocketTestBase() override;

  static size_t GetStreamFrameDataLengthFromPacketLength(
      quic::QuicByteCount packet_length,
      quic::ParsedQuicVersion version,
      bool include_version,
      bool include_diversification_nonce,
      int connection_id_length,
      quic::QuicPacketNumberLength packet_number_length,
      quic::QuicStreamOffset offset);

  void SetUp() override {}

  void TearDown() override = 0;

  void InitializeSession();

  virtual void InitializeClientSocket() = 0;

  virtual void PopulateConnectRequestIR(
      quiche::HttpHeaderBlock* block,
      std::optional<const HttpRequestHeaders> extra_headers) = 0;

  std::unique_ptr<quic::QuicReceivedPacket> ConstructSettingsPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstOnlyPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructConnectRequestPacket(
      uint64_t packet_number,
      std::optional<const HttpRequestHeaders> extra_headers = std::nullopt,
      RequestPriority request_priority = LOWEST);

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructConnectRequestPacketWithExtraHeaders(
      uint64_t packet_number,
      std::vector<std::pair<std::string, std::string>> extra_headers,
      RequestPriority request_priority = LOWEST);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructConnectAuthRequestPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructDataPacket(
      uint64_t packet_number,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructDatagramPacket(
      uint64_t packet_number,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndDataPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndDatagramPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDatagramPacket(
      uint64_t packet_number,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDataFinPacket(
      uint64_t packet_number,
      std::string_view data);

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerConnectReplyPacket(
      uint64_t packet_number,
      bool fin,
      size_t* header_length = nullptr,
      std::optional<const HttpRequestHeaders> extra_headers = std::nullopt);

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectReplyPacketWithExtraHeaders(
      uint64_t packet_number,
      bool fin,
      std::vector<std::pair<std::string, std::string>> extra_headers);

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectAuthReplyPacket(uint64_t packet_number, bool fin);

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectRedirectReplyPacket(uint64_t packet_number, bool fin);
  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectErrorReplyPacket(uint64_t packet_number, bool fin);

  void ResumeAndRun();

  virtual void AssertConnectSucceeds() = 0;

  virtual void AssertConnectFails(int result) = 0;

  virtual void AssertWriteReturns(const char* data, int len, int rv) = 0;

  virtual void AssertSyncWriteSucceeds(const char* data, int len) = 0;

  virtual void AssertSyncReadEquals(const char* data, int len) = 0;
  virtual void AssertAsyncReadEquals(const char* data, int len) = 0;

  virtual void AssertReadStarts(const char* data, int len) = 0;

  virtual void AssertReadReturns(const char* data, int len) = 0;

  std::string ConstructDataHeader(size_t body_len);

 protected:
  static const bool kFin = true;
  static const bool kIncludeVersion = true;
  static const bool kIncludeDiversificationNonce = true;

  RecordingNetLogObserver net_log_observer_;
  quic::test::QuicFlagSaver saver_;
  const quic::ParsedQuicVersion version_;
  const quic::QuicStreamId client_data_stream_id1_;

  // order of destruction of these members matter
  quic::MockClock clock_;
  test::MockQuicData mock_quic_data_;
  std::unique_ptr<QuicChromiumConnectionHelper> helper_;
  std::unique_ptr<QuicChromiumClientSession> session_;
  std::unique_ptr<QuicChromiumClientSession::Handle> session_handle_;
  std::unique_ptr<QuicChromiumClientStream::Handle> stream_handle_;
  std::unique_ptr<TestProxyDelegate> proxy_delegate_;

  raw_ptr<quic::test::MockSendAlgorithm> send_algorithm_;
  scoped_refptr<test::TestTaskRunner> runner_;

  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  testing::StrictMock<quic::test::MockQuicConnectionVisitor> visitor_;
  TransportSecurityState transport_security_state_;
  SSLConfigServiceDefaults ssl_config_service_;
  quic::QuicCryptoClientConfig crypto_config_;

  const quic::QuicConnectionId connection_id_;
  test::QuicTestPacketMaker client_maker_;
  test::QuicTestPacketMaker server_maker_;
  IPEndPoint peer_addr_;
  IPEndPoint local_addr_;
  quic::test::MockRandom random_generator_{0};
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::test::MockConnectionIdGenerator connection_id_generator_;

  ProxyChain proxy_chain_ = ProxyChain::ForIpProtection(
      {{ProxyServer::SCHEME_QUIC, HostPortPair("proxy.example.com", 443)}});

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

  base::HistogramTester histogram_tester_;
};
}  // namespace net

#endif  // NET_QUIC_QUIC_PROXY_CLIENT_SOCKET_TEST_BASE_H_
