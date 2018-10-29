// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_client_socket.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_headers.h"
#include "net/http/transport_security_state.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_stream_factory.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

static const char kOriginHost[] = "www.google.com";
static const int kOriginPort = 443;
static const char kProxyUrl[] = "https://myproxy:6121/";
static const char kProxyHost[] = "myproxy";
static const int kProxyPort = 6121;
static const char kUserAgent[] = "Mozilla/1.0";
static const char kRedirectUrl[] = "https://example.com/";

static const char kMsg1[] = "\0hello!\xff";
static const int kLen1 = 8;
static const char kMsg2[] = "\0a2345678\0";
static const int kLen2 = 10;
static const char kMsg3[] = "bye!";
static const int kLen3 = 4;
static const char kMsg33[] = "bye!bye!";
static const int kLen33 = kLen3 + kLen3;
static const char kMsg333[] = "bye!bye!bye!";
static const int kLen333 = kLen3 + kLen3 + kLen3;

}  // anonymous namespace

namespace net {
namespace test {

class QuicProxyClientSocketTest
    : public ::testing::TestWithParam<
          std::tuple<quic::QuicTransportVersion, bool>>,
      public WithScopedTaskEnvironment {
 protected:
  static const bool kFin = true;
  static const bool kIncludeVersion = true;
  static const bool kIncludeDiversificationNonce = true;
  static const bool kIncludeCongestionFeedback = true;
  static const bool kSendFeedback = true;

  static size_t GetStreamFrameDataLengthFromPacketLength(
      quic::QuicByteCount packet_length,
      quic::QuicTransportVersion version,
      bool include_version,
      bool include_diversification_nonce,
      quic::QuicConnectionIdLength connection_id_length,
      quic::QuicPacketNumberLength packet_number_length,
      quic::QuicStreamOffset offset) {
    size_t min_data_length = 1;
    size_t min_packet_length =
        quic::NullEncrypter(quic::Perspective::IS_CLIENT)
            .GetCiphertextSize(min_data_length) +
        quic::QuicPacketCreator::StreamFramePacketOverhead(
            version, quic::PACKET_8BYTE_CONNECTION_ID,
            quic::PACKET_0BYTE_CONNECTION_ID, include_version,
            include_diversification_nonce, packet_number_length, offset);

    DCHECK(packet_length >= min_packet_length);
    return min_data_length + packet_length - min_packet_length;
  }

  QuicProxyClientSocketTest()
      : version_(std::get<0>(GetParam())),
        client_data_stream_id1_(quic::QuicUtils::GetHeadersStreamId(version_) +
                                2),
        client_headers_include_h2_stream_dependency_(std::get<1>(GetParam())),
        crypto_config_(quic::test::crypto_test_utils::ProofVerifierForTesting(),
                       quic::TlsClientHandshaker::CreateSslCtx()),
        connection_id_(2),
        client_maker_(version_,
                      connection_id_,
                      &clock_,
                      kProxyHost,
                      quic::Perspective::IS_CLIENT,
                      client_headers_include_h2_stream_dependency_),
        server_maker_(version_,
                      connection_id_,
                      &clock_,
                      kProxyHost,
                      quic::Perspective::IS_SERVER,
                      false),
        random_generator_(0),
        header_stream_offset_(0),
        response_offset_(0),
        user_agent_(kUserAgent),
        proxy_host_port_(kProxyHost, kProxyPort),
        endpoint_host_port_(kOriginHost, kOriginPort),
        host_resolver_(new MockCachingHostResolver()),
        http_auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(host_resolver_.get())) {
    IPAddress ip(192, 0, 2, 33);
    peer_addr_ = IPEndPoint(ip, 443);
    clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
  }

  void SetUp() override {}

  void TearDown() override {
    sock_.reset();
    EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
  }

  void Initialize() {
    std::unique_ptr<MockUDPClientSocket> socket(new MockUDPClientSocket(
        mock_quic_data_.InitializeAndGetSequencedSocketData(),
        net_log_.bound().net_log()));
    socket->Connect(peer_addr_);
    runner_ = new TestTaskRunner(&clock_);
    send_algorithm_ = new quic::test::MockSendAlgorithm();
    EXPECT_CALL(*send_algorithm_, InRecovery()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
        .WillRepeatedly(Return(quic::kMaxPacketSize));
    EXPECT_CALL(*send_algorithm_, PacingRate(_))
        .WillRepeatedly(Return(quic::QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .WillRepeatedly(Return(quic::QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, OnApplicationLimited(_)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, GetCongestionControlType())
        .Times(AnyNumber());
    helper_.reset(
        new QuicChromiumConnectionHelper(&clock_, &random_generator_));
    alarm_factory_.reset(new QuicChromiumAlarmFactory(runner_.get(), &clock_));

    QuicChromiumPacketWriter* writer = new QuicChromiumPacketWriter(
        socket.get(), base::ThreadTaskRunnerHandle::Get().get());
    quic::QuicConnection* connection = new quic::QuicConnection(
        connection_id_,
        quic::QuicSocketAddress(quic::QuicSocketAddressImpl(peer_addr_)),
        helper_.get(), alarm_factory_.get(), writer, true /* owns_writer */,
        quic::Perspective::IS_CLIENT,
        quic::test::SupportedVersions(
            quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO, version_)));
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
    base::TimeTicks dns_start = dns_end - base::TimeDelta::FromMilliseconds(1);

    session_.reset(new QuicChromiumClientSession(
        connection, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        &transport_security_state_, /*ssl_config_service=*/nullptr,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)),
        QuicSessionKey("mail.example.org", 80, PRIVACY_MODE_DISABLED,
                       SocketTag()),
        /*require_confirmation=*/false, /*migrate_session_early_v2=*/false,
        /*migrate_session_on_network_change_v2=*/false,
        /*go_away_on_path_degrading*/ false,
        /*default_network=*/NetworkChangeNotifier::kInvalidNetworkHandle,
        base::TimeDelta::FromSeconds(kMaxTimeOnNonDefaultNetworkSecs),
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        client_headers_include_h2_stream_dependency_, /*cert_verify_flags=*/0,
        quic::test::DefaultQuicConfig(), &crypto_config_, "CONNECTION_UNKNOWN",
        dns_start, dns_end, &push_promise_index_, nullptr,
        base::ThreadTaskRunnerHandle::Get().get(),
        /*socket_performance_watcher=*/nullptr, net_log_.bound().net_log()));

    writer->set_delegate(session_.get());

    session_handle_ =
        session_->CreateHandle(HostPortPair("mail.example.org", 80));

    session_->Initialize();
    TestCompletionCallback callback;
    EXPECT_THAT(session_->CryptoConnect(callback.callback()), IsOk());
    EXPECT_TRUE(session_->IsCryptoHandshakeConfirmed());

    EXPECT_THAT(session_handle_->RequestStream(true, callback.callback(),
                                               TRAFFIC_ANNOTATION_FOR_TESTS),
                IsOk());
    std::unique_ptr<QuicChromiumClientStream::Handle> stream_handle =
        session_handle_->ReleaseStream();
    EXPECT_TRUE(stream_handle->IsOpen());

    sock_.reset(new QuicProxyClientSocket(
        std::move(stream_handle), std::move(session_handle_), user_agent_,
        endpoint_host_port_, net_log_.bound(),
        new HttpAuthController(HttpAuth::AUTH_PROXY,
                               GURL("https://" + proxy_host_port_.ToString()),
                               &http_auth_cache_,
                               http_auth_handler_factory_.get())));

    session_->StartReading();
  }

  void PopulateConnectRequestIR(spdy::SpdyHeaderBlock* block) {
    (*block)[":method"] = "CONNECT";
    (*block)[":authority"] = endpoint_host_port_.ToString();
    (*block)["user-agent"] = kUserAgent;
  }

  // Helper functions for constructing packets sent by the client

  std::unique_ptr<quic::QuicReceivedPacket> ConstructSettingsPacket(
      quic::QuicPacketNumber packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number,
                                                   &header_stream_offset_);
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

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicPacketNumber largest_received,
      quic::QuicPacketNumber smallest_received,
      quic::QuicPacketNumber least_unacked,
      size_t bytes_written) {
    return client_maker_.MakeAckAndRstPacket(
        packet_number, !kIncludeVersion, client_data_stream_id1_, error_code,
        largest_received, smallest_received, least_unacked, kSendFeedback,
        bytes_written);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructRstPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicRstStreamErrorCode error_code,
      size_t bytes_written) {
    return client_maker_.MakeRstPacket(packet_number, !kIncludeVersion,
                                       client_data_stream_id1_, error_code,
                                       bytes_written);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructConnectRequestPacket(
      quic::QuicPacketNumber packet_number) {
    spdy::SpdyHeaderBlock block;
    PopulateConnectRequestIR(&block);
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, client_data_stream_id1_, kIncludeVersion, !kFin,
        ConvertRequestPriorityToQuicPriority(LOWEST), std::move(block), 0,
        nullptr, &header_stream_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructConnectAuthRequestPacket(
      quic::QuicPacketNumber packet_number) {
    spdy::SpdyHeaderBlock block;
    PopulateConnectRequestIR(&block);
    block["proxy-authorization"] = "Basic Zm9vOmJhcg==";
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, client_data_stream_id1_, kIncludeVersion, !kFin,
        ConvertRequestPriorityToQuicPriority(LOWEST), std::move(block), 0,
        nullptr, &header_stream_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructDataPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamOffset offset,
      const char* data,
      int length) {
    return client_maker_.MakeDataPacket(packet_number, client_data_stream_id1_,
                                        !kIncludeVersion, !kFin, offset,
                                        quic::QuicStringPiece(data, length));
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndDataPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicPacketNumber largest_received,
      quic::QuicPacketNumber smallest_received,
      quic::QuicPacketNumber least_unacked,
      quic::QuicStreamOffset offset,
      const char* data,
      int length) {
    return client_maker_.MakeAckAndDataPacket(
        packet_number, !kIncludeVersion, client_data_stream_id1_,
        largest_received, smallest_received, least_unacked, !kFin, offset,
        quic::QuicStringPiece(data, length));
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicPacketNumber largest_received,
      quic::QuicPacketNumber smallest_received,
      quic::QuicPacketNumber least_unacked) {
    return client_maker_.MakeAckPacket(packet_number, largest_received,
                                       smallest_received, least_unacked,
                                       kSendFeedback);
  }

  // Helper functions for constructing packets sent by the server

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerRstPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicRstStreamErrorCode error_code,
      size_t bytes_written) {
    return server_maker_.MakeRstPacket(packet_number, !kIncludeVersion,
                                       client_data_stream_id1_, error_code,
                                       bytes_written);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDataPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamOffset offset,
      const char* data,
      int length) {
    return server_maker_.MakeDataPacket(packet_number, client_data_stream_id1_,
                                        !kIncludeVersion, !kFin, offset,
                                        quic::QuicStringPiece(data, length));
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDataFinPacket(
      quic::QuicPacketNumber packet_number,
      quic::QuicStreamOffset offset,
      const char* data,
      int length) {
    return server_maker_.MakeDataPacket(packet_number, client_data_stream_id1_,
                                        !kIncludeVersion, kFin, offset,
                                        quic::QuicStringPiece(data, length));
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

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectAuthReplyPacket(quic::QuicPacketNumber packet_number,
                                        bool fin) {
    spdy::SpdyHeaderBlock block;
    block[":status"] = "407";
    block["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
    return server_maker_.MakeResponseHeadersPacket(
        packet_number, client_data_stream_id1_, !kIncludeVersion, fin,
        std::move(block), nullptr, &response_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectRedirectReplyPacket(
      quic::QuicPacketNumber packet_number,
      bool fin) {
    spdy::SpdyHeaderBlock block;
    block[":status"] = "302";
    block["location"] = kRedirectUrl;
    block["set-cookie"] = "foo=bar";
    return server_maker_.MakeResponseHeadersPacket(
        packet_number, client_data_stream_id1_, !kIncludeVersion, fin,
        std::move(block), nullptr, &response_offset_);
  }

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructServerConnectErrorReplyPacket(quic::QuicPacketNumber packet_number,
                                         bool fin) {
    spdy::SpdyHeaderBlock block;
    block[":status"] = "500";

    return server_maker_.MakeResponseHeadersPacket(
        packet_number, client_data_stream_id1_, !kIncludeVersion, fin,
        std::move(block), nullptr, &response_offset_);
  }

  void AssertConnectSucceeds() {
    TestCompletionCallback callback;
    ASSERT_THAT(sock_->Connect(callback.callback()), IsError(ERR_IO_PENDING));
    ASSERT_THAT(callback.WaitForResult(), IsOk());
  }

  void AssertConnectFails(int result) {
    TestCompletionCallback callback;
    ASSERT_THAT(sock_->Connect(callback.callback()), IsError(ERR_IO_PENDING));
    ASSERT_EQ(result, callback.WaitForResult());
  }

  void ResumeAndRun() {
    // Run until the pause, if the provider isn't paused yet.
    SequencedSocketData* data = mock_quic_data_.GetSequencedSocketData();
    data->RunUntilPaused();
    data->Resume();
    base::RunLoop().RunUntilIdle();
  }

  void AssertWriteReturns(const char* data, int len, int rv) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(len);
    memcpy(buf->data(), data, len);
    EXPECT_EQ(rv,
              sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                           TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void AssertSyncWriteSucceeds(const char* data, int len) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(len);
    memcpy(buf->data(), data, len);
    EXPECT_EQ(len,
              sock_->Write(buf.get(), buf->size(), CompletionOnceCallback(),
                           TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void AssertSyncReadEquals(const char* data, int len) {
    scoped_refptr<IOBuffer> buf = base::MakeRefCounted<IOBuffer>(len);
    ASSERT_EQ(len, sock_->Read(buf.get(), len, CompletionOnceCallback()));
    ASSERT_EQ(spdy::SpdyString(data, len), spdy::SpdyString(buf->data(), len));
    ASSERT_TRUE(sock_->IsConnected());
  }

  void AssertAsyncReadEquals(const char* data, int len) {
    scoped_refptr<IOBuffer> buf = base::MakeRefCounted<IOBuffer>(len);
    ASSERT_EQ(ERR_IO_PENDING,
              sock_->Read(buf.get(), len, read_callback_.callback()));
    EXPECT_TRUE(sock_->IsConnected());

    ResumeAndRun();

    EXPECT_EQ(len, read_callback_.WaitForResult());
    EXPECT_TRUE(sock_->IsConnected());
    ASSERT_EQ(spdy::SpdyString(data, len), spdy::SpdyString(buf->data(), len));
  }

  void AssertReadStarts(const char* data, int len) {
    // Issue the read, which will be completed asynchronously.
    read_buf_ = base::MakeRefCounted<IOBuffer>(len);
    ASSERT_EQ(ERR_IO_PENDING,
              sock_->Read(read_buf_.get(), len, read_callback_.callback()));
    EXPECT_TRUE(sock_->IsConnected());
  }

  void AssertReadReturns(const char* data, int len) {
    EXPECT_TRUE(sock_->IsConnected());

    // Now the read will return.
    EXPECT_EQ(len, read_callback_.WaitForResult());
    ASSERT_EQ(spdy::SpdyString(data, len),
              spdy::SpdyString(read_buf_->data(), len));
  }

  const quic::QuicTransportVersion version_;
  const quic::QuicStreamId client_data_stream_id1_;
  const bool client_headers_include_h2_stream_dependency_;

  // order of destruction of these members matter
  quic::MockClock clock_;
  MockQuicData mock_quic_data_;
  std::unique_ptr<QuicChromiumConnectionHelper> helper_;
  std::unique_ptr<QuicChromiumClientSession> session_;
  std::unique_ptr<QuicChromiumClientSession::Handle> session_handle_;
  std::unique_ptr<QuicProxyClientSocket> sock_;

  BoundTestNetLog net_log_;

  quic::test::MockSendAlgorithm* send_algorithm_;
  scoped_refptr<TestTaskRunner> runner_;

  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  testing::StrictMock<quic::test::MockQuicConnectionVisitor> visitor_;
  TransportSecurityState transport_security_state_;
  quic::QuicCryptoClientConfig crypto_config_;
  quic::QuicClientPushPromiseIndex push_promise_index_;

  const quic::QuicConnectionId connection_id_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  IPEndPoint peer_addr_;
  quic::test::MockRandom random_generator_;
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::QuicStreamOffset header_stream_offset_;
  quic::QuicStreamOffset response_offset_;

  std::string user_agent_;
  HostPortPair proxy_host_port_;
  HostPortPair endpoint_host_port_;
  HttpAuthCache http_auth_cache_;
  std::unique_ptr<MockHostResolverBase> host_resolver_;
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory_;

  TestCompletionCallback read_callback_;
  scoped_refptr<IOBuffer> read_buf_;

  TestCompletionCallback write_callback_;

  DISALLOW_COPY_AND_ASSIGN(QuicProxyClientSocketTest);
};

TEST_P(QuicProxyClientSocketTest, ConnectSendsCorrectRequest) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
}

TEST_P(QuicProxyClientSocketTest, ConnectWithAuthRequested) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectAuthReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(407, response->headers->response_code());
}

TEST_P(QuicProxyClientSocketTest, ConnectWithAuthCredentials) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectAuthRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  // Add auth to cache
  const base::string16 kFoo(base::ASCIIToUTF16("foo"));
  const base::string16 kBar(base::ASCIIToUTF16("bar"));
  http_auth_cache_.Add(GURL(kProxyUrl), "MyRealm1", HttpAuth::AUTH_SCHEME_BASIC,
                       "Basic realm=MyRealm1", AuthCredentials(kFoo, kBar),
                       "/");

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
}

TEST_P(QuicProxyClientSocketTest, ConnectRedirects) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectRedirectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  AssertConnectFails(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);

  const HttpResponseHeaders* headers = response->headers.get();
  ASSERT_EQ(302, headers->response_code());
  ASSERT_FALSE(headers->HasHeader("set-cookie"));
  ASSERT_TRUE(headers->HasHeaderValue("content-length", "0"));

  std::string location;
  ASSERT_TRUE(headers->IsRedirect(&location));
  ASSERT_EQ(location, kRedirectUrl);
}

TEST_P(QuicProxyClientSocketTest, ConnectFails) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, 0);  // EOF

  Initialize();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_QUIC_PROTOCOL_ERROR);

  ASSERT_FALSE(sock_->IsConnected());
}

TEST_P(QuicProxyClientSocketTest, WasEverUsedReturnsCorrectValue) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  EXPECT_TRUE(sock_->WasEverUsed());  // Used due to crypto handshake
  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->WasEverUsed());
  sock_->Disconnect();
  EXPECT_TRUE(sock_->WasEverUsed());
}

TEST_P(QuicProxyClientSocketTest, GetPeerAddressReturnsCorrectValues) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data_.AddRead(ASYNC, 0);               // EOF

  Initialize();

  IPEndPoint addr;
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));

  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsOk());

  ResumeAndRun();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));

  sock_->Disconnect();

  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));
}

TEST_P(QuicProxyClientSocketTest, IsConnectedAndIdle) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4, quic::QUIC_STREAM_CANCELLED, 0));

  Initialize();

  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnectedAndIdle());

  // The next read is consumed and buffered.
  ResumeAndRun();

  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  AssertSyncReadEquals(kMsg1, kLen1);

  EXPECT_TRUE(sock_->IsConnectedAndIdle());
}

TEST_P(QuicProxyClientSocketTest, GetTotalReceivedBytes) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(2, 0, kMsg333, kLen333));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4, quic::QUIC_STREAM_CANCELLED, 0));

  Initialize();

  EXPECT_EQ(0, sock_->GetTotalReceivedBytes());

  AssertConnectSucceeds();

  EXPECT_EQ(0, sock_->GetTotalReceivedBytes());

  // The next read is consumed and buffered.
  ResumeAndRun();

  EXPECT_EQ(0, sock_->GetTotalReceivedBytes());

  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33, kLen33);

  EXPECT_EQ((int64_t)kLen33, sock_->GetTotalReceivedBytes());

  AssertSyncReadEquals(kMsg3, kLen3);

  EXPECT_EQ((int64_t)kLen333, sock_->GetTotalReceivedBytes());
}

// ----------- Write

TEST_P(QuicProxyClientSocketTest, WriteSendsDataInDataFrame) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndDataPacket(3, 1, 1, 1, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructDataPacket(4, kLen1, kMsg2, kLen2));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(5, quic::QUIC_STREAM_CANCELLED, kLen1 + kLen2));

  Initialize();

  AssertConnectSucceeds();

  AssertSyncWriteSucceeds(kMsg1, kLen1);
  AssertSyncWriteSucceeds(kMsg2, kLen2);
}

TEST_P(QuicProxyClientSocketTest, WriteSplitsLargeDataIntoMultiplePackets) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndDataPacket(3, 1, 1, 1, 0, kMsg1, kLen1));

  // Expect |kNumDataPackets| data packets, each containing the max possible
  // amount of data.
  const int kNumDataPackets = 3;
  std::string data(kNumDataPackets * quic::kDefaultMaxPacketSize, 'x');
  quic::QuicStreamOffset offset = kLen1;
  size_t total_data_length = 0;
  for (int i = 0; i < kNumDataPackets; ++i) {
    size_t max_packet_data_length = GetStreamFrameDataLengthFromPacketLength(
        quic::kDefaultMaxPacketSize, version_, !kIncludeVersion,
        !kIncludeDiversificationNonce, quic::PACKET_8BYTE_CONNECTION_ID,
        quic::PACKET_1BYTE_PACKET_NUMBER, offset);
    mock_quic_data_.AddWrite(SYNCHRONOUS,
                             ConstructDataPacket(4 + i, offset, data.c_str(),
                                                 max_packet_data_length));
    offset += max_packet_data_length;
    total_data_length += max_packet_data_length;
  }
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4 + kNumDataPackets,
                                      quic::QUIC_STREAM_CANCELLED, offset));

  Initialize();

  AssertConnectSucceeds();

  // Make a small write. An ACK and STOP_WAITING will be bundled. This prevents
  // ACK and STOP_WAITING from being bundled with the subsequent large write.
  // This allows the test code for computing the size of data sent in each
  // packet to not become too complicated.
  AssertSyncWriteSucceeds(kMsg1, kLen1);

  // Make large write that should be split up
  AssertSyncWriteSucceeds(data.c_str(), total_data_length);
}

// ----------- Read

TEST_P(QuicProxyClientSocketTest, ReadReadsDataInDataFrame) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4, quic::QUIC_STREAM_CANCELLED, 0));

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_P(QuicProxyClientSocketTest, ReadDataFromBufferedFrames) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen1, kMsg2, kLen2));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(4, quic::QUIC_STREAM_CANCELLED, 3, 3, 1));

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  ResumeAndRun();
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(QuicProxyClientSocketTest, ReadDataMultipleBufferedFrames) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen1, kMsg2, kLen2));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(4, quic::QUIC_STREAM_CANCELLED, 3, 3, 1));

  Initialize();

  AssertConnectSucceeds();

  // The next two reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(QuicProxyClientSocketTest, LargeReadWillMergeDataFromDifferentFrames) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg3, kLen3));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen3, kMsg3, kLen3));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(4, quic::QUIC_STREAM_CANCELLED, 3, 3, 1));

  Initialize();

  AssertConnectSucceeds();

  // The next two reads are consumed and buffered.
  ResumeAndRun();
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
}

TEST_P(QuicProxyClientSocketTest, MultipleShortReadsThenMoreRead) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  int offset = 0;

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(2, offset, kMsg1, kLen1));
  offset += kLen1;
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, offset, kMsg3, kLen3));
  offset += kLen3;
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(4, offset, kMsg3, kLen3));
  offset += kLen3;
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(4, 4, 3, 1));

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(5, offset, kMsg2, kLen2));
  offset += kLen2;
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(5, quic::QUIC_STREAM_CANCELLED, 5, 5, 1));

  Initialize();

  AssertConnectSucceeds();

  // The next 4 reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(QuicProxyClientSocketTest, ReadWillSplitDataFromLargeFrame) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen1, kMsg33, kLen33));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(4, quic::QUIC_STREAM_CANCELLED, 3, 3, 1));

  Initialize();

  AssertConnectSucceeds();

  // The next 2 reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
}

TEST_P(QuicProxyClientSocketTest, MultipleReadsFromSameLargeFrame) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(2, 0, kMsg333, kLen333));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4, quic::QUIC_STREAM_CANCELLED, 0));

  Initialize();

  AssertConnectSucceeds();

  // The next read is consumed and buffered.
  ResumeAndRun();

  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33, kLen33);

  // Now attempt to do a read of more data than remains buffered
  scoped_refptr<IOBuffer> buf = base::MakeRefCounted<IOBuffer>(kLen33);
  ASSERT_EQ(kLen3, sock_->Read(buf.get(), kLen33, CompletionOnceCallback()));
  ASSERT_EQ(spdy::SpdyString(kMsg3, kLen3),
            spdy::SpdyString(buf->data(), kLen3));
  ASSERT_TRUE(sock_->IsConnected());
}

TEST_P(QuicProxyClientSocketTest, ReadAuthResponseBody) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectAuthReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen1, kMsg2, kLen2));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(4, quic::QUIC_STREAM_CANCELLED, 3, 3, 1));

  Initialize();

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  // The next two reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(QuicProxyClientSocketTest, ReadErrorResponseBody) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectErrorReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS,
                          ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS,
                          ConstructServerDataPacket(3, kLen1, kMsg2, kLen2));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(4, quic::QUIC_STREAM_CANCELLED, 3, 3, 1));
  Initialize();

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
}

// ----------- Reads and Writes

TEST_P(QuicProxyClientSocketTest, AsyncReadAroundWrite) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructDataPacket(4, 0, kMsg2, kLen2));

  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen1, kMsg3, kLen3));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(5, quic::QUIC_STREAM_CANCELLED, 3, 3, 1, kLen2));

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();

  AssertSyncReadEquals(kMsg1, kLen1);

  AssertReadStarts(kMsg3, kLen3);
  // Read should block until after the write succeeds.

  AssertSyncWriteSucceeds(kMsg2, kLen2);

  ASSERT_FALSE(read_callback_.have_result());
  ResumeAndRun();

  // Now the read will return.
  AssertReadReturns(kMsg3, kLen3);
}

TEST_P(QuicProxyClientSocketTest, AsyncWriteAroundReads) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataPacket(3, kLen1, kMsg3, kLen3));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddWrite(ASYNC, ConstructDataPacket(4, 0, kMsg2, kLen2));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndDataPacket(5, 3, 3, 1, kLen2, kMsg2, kLen2));

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(6, quic::QUIC_STREAM_CANCELLED, kLen2 + kLen2));

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  // Write should block until the next read completes.
  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg2, kLen2, kLen2);
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  AssertAsyncReadEquals(kMsg3, kLen3);

  ASSERT_FALSE(write_callback_.have_result());

  // Now the write will complete
  ResumeAndRun();
  EXPECT_EQ(kLen2, write_callback_.WaitForResult());
}

// ----------- Reading/Writing on Closed socket

// Reading from an already closed socket should return 0
TEST_P(QuicProxyClientSocketTest, ReadOnClosedSocketReturnsZero) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, 0);  // EOF

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();

  ASSERT_FALSE(sock_->IsConnected());
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionOnceCallback()));
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

// Read pending when socket is closed should return 0
TEST_P(QuicProxyClientSocketTest, PendingReadOnCloseReturnsZero) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, 0);  // EOF

  Initialize();

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1, kLen1);

  ResumeAndRun();

  ASSERT_EQ(0, read_callback_.WaitForResult());
}

// Reading from a disconnected socket is an error
TEST_P(QuicProxyClientSocketTest, ReadOnDisconnectSocketReturnsNotConnected) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  AssertConnectSucceeds();

  sock_->Disconnect();

  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(nullptr, 1, CompletionOnceCallback()));
}

// Reading data after receiving FIN should return buffered data received before
// FIN, then 0.
TEST_P(QuicProxyClientSocketTest, ReadAfterFinReceivedReturnsBufferedData) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerDataFinPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4, quic::QUIC_STREAM_CANCELLED, 0));

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();

  AssertSyncReadEquals(kMsg1, kLen1);
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionOnceCallback()));

  sock_->Disconnect();
  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(nullptr, 1, CompletionOnceCallback()));
}

// Calling Write() on a closed socket is an error.
TEST_P(QuicProxyClientSocketTest, WriteOnClosedStream) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, 0);  // EOF

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();

  AssertWriteReturns(kMsg1, kLen1, ERR_QUIC_PROTOCOL_ERROR);
}

// Calling Write() on a disconnected socket is an error.
TEST_P(QuicProxyClientSocketTest, WriteOnDisconnectedSocket) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  AssertConnectSucceeds();

  sock_->Disconnect();

  AssertWriteReturns(kMsg1, kLen1, ERR_SOCKET_NOT_CONNECTED);
}

// If the socket is closed with a pending Write(), the callback should be called
// with the same error the session was closed with.
TEST_P(QuicProxyClientSocketTest, WritePendingOnClose) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);

  Initialize();

  AssertConnectSucceeds();

  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg1, kLen1, kLen1);

  // This second write will be async. This is the pending write that's being
  // tested.
  AssertWriteReturns(kMsg1, kLen1, ERR_IO_PENDING);

  // Make sure the write actually starts.
  base::RunLoop().RunUntilIdle();

  session_->CloseSessionOnError(ERR_CONNECTION_CLOSED,
                                quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_THAT(write_callback_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

TEST_P(QuicProxyClientSocketTest, DisconnectWithWritePending) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);

  Initialize();

  AssertConnectSucceeds();

  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg1, kLen1, kLen1);

  // This second write will be async. This is the pending write that's being
  // tested.
  AssertWriteReturns(kMsg1, kLen1, ERR_IO_PENDING);

  // Make sure the write actually starts.
  base::RunLoop().RunUntilIdle();

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(write_callback_.have_result());
}

// If the socket is Disconnected with a pending Read(), the callback
// should not be called.
TEST_P(QuicProxyClientSocketTest, DisconnectWithReadPending) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndRstPacket(3, quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  Initialize();

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  AssertReadStarts(kMsg1, kLen1);

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(read_callback_.have_result());
}

// If the socket is Reset when both a read and write are pending,
// both should be called back.
TEST_P(QuicProxyClientSocketTest, RstWithReadAndWritePending) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(
      ASYNC, ConstructServerRstPacket(2, quic::QUIC_STREAM_CANCELLED, 0));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      ASYNC, ConstructAckAndDataPacket(3, 1, 1, 1, 0, kMsg2, kLen2));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(4, quic::QUIC_RST_ACKNOWLEDGEMENT,
                                            2, 2, 1, kLen2));

  Initialize();

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  AssertReadStarts(kMsg1, kLen1);

  // Write should block until the next read completes.
  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg2, kLen2, kLen2);
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  ResumeAndRun();

  EXPECT_TRUE(read_callback_.have_result());
  EXPECT_TRUE(write_callback_.have_result());
}

// Makes sure the proxy client socket's source gets the expected NetLog events
// and only the expected NetLog events (No SpdySession events).
TEST_P(QuicProxyClientSocketTest, NetLog) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1, 1));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructRstPacket(4, quic::QUIC_STREAM_CANCELLED, 0));

  Initialize();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  NetLogSource sock_source = sock_->NetLog().source();
  sock_.reset();

  TestNetLogEntry::List entry_list;
  net_log_.GetEntriesForSource(sock_source, &entry_list);

  ASSERT_EQ(entry_list.size(), 10u);
  EXPECT_TRUE(
      LogContainsBeginEvent(entry_list, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(entry_list, 1,
                               NetLogEventType::HTTP2_PROXY_CLIENT_SESSION,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsBeginEvent(
      entry_list, 2, NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsEvent(
      entry_list, 3, NetLogEventType::HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entry_list, 4, NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsBeginEvent(
      entry_list, 5, NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(
      entry_list, 6,
      NetLogEventType::HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entry_list, 7, NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 8,
                               NetLogEventType::SOCKET_BYTES_RECEIVED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(
      LogContainsEndEvent(entry_list, 9, NetLogEventType::SOCKET_ALIVE));
}

// A helper class that will delete |sock| when the callback is invoked.
class DeleteSockCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSockCallback(std::unique_ptr<QuicProxyClientSocket>* sock)
      : sock_(sock) {}

  ~DeleteSockCallback() override {}

  CompletionOnceCallback callback() {
    return base::BindOnce(&DeleteSockCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    sock_->reset(NULL);
    SetResult(result);
  }

  std::unique_ptr<QuicProxyClientSocket>* sock_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSockCallback);
};

// If the socket is reset when both a read and write are pending, and the
// read callback causes the socket to be deleted, the write callback should
// not be called.
TEST_P(QuicProxyClientSocketTest, RstWithReadAndWritePendingDelete) {
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructSettingsPacket(1));
  mock_quic_data_.AddWrite(SYNCHRONOUS, ConstructConnectRequestPacket(2));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  mock_quic_data_.AddRead(
      ASYNC, ConstructServerRstPacket(2, quic::QUIC_STREAM_CANCELLED, 0));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      ASYNC, ConstructAckAndDataPacket(3, 1, 1, 1, 0, kMsg1, kLen1));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(4, quic::QUIC_RST_ACKNOWLEDGEMENT,
                                            2, 2, 1, kLen1));

  Initialize();

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  DeleteSockCallback read_callback(&sock_);
  scoped_refptr<IOBuffer> read_buf = base::MakeRefCounted<IOBuffer>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), kLen1, read_callback.callback()));

  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg1, kLen1, kLen1);
  AssertWriteReturns(kMsg1, kLen1, ERR_IO_PENDING);

  ResumeAndRun();

  EXPECT_FALSE(sock_.get());

  EXPECT_EQ(0, read_callback.WaitForResult());
  EXPECT_FALSE(write_callback_.have_result());
}

INSTANTIATE_TEST_CASE_P(
    VersionIncludeStreamDependencySequence,
    QuicProxyClientSocketTest,
    ::testing::Combine(
        ::testing::ValuesIn(quic::AllSupportedTransportVersions()),
        ::testing::Bool()));

}  // namespace test
}  // namespace net
