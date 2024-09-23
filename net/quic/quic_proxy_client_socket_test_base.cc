// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_client_socket_test_base.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/session_usage.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/transport_security_state.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
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
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "url/scheme_host_port.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net {

QuicProxyClientSocketTestBase::~QuicProxyClientSocketTestBase() = default;

QuicProxyClientSocketTestBase::QuicProxyClientSocketTestBase()
    : version_(GetParam()),
      client_data_stream_id1_(quic::QuicUtils::GetFirstBidirectionalStreamId(
          version_.transport_version,
          quic::Perspective::IS_CLIENT)),
      mock_quic_data_(version_),
      crypto_config_(quic::test::crypto_test_utils::ProofVerifierForTesting()),
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
  peer_addr_ = IPEndPoint(ip, 443);
  clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
  quic::QuicEnableVersion(version_);
}

size_t QuicProxyClientSocketTestBase::GetStreamFrameDataLengthFromPacketLength(
    quic::QuicByteCount packet_length,
    quic::ParsedQuicVersion version,
    bool include_version,
    bool include_diversification_nonce,
    int connection_id_length,
    quic::QuicPacketNumberLength packet_number_length,
    quic::QuicStreamOffset offset) {
  quiche::QuicheVariableLengthIntegerLength retry_token_length_length =
      quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0;
  quiche::QuicheVariableLengthIntegerLength length_length =
      include_version ? quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2
                      : quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0;
  size_t min_data_length = 1;
  size_t min_packet_length =
      quic::test::TaggingEncrypter(quic::ENCRYPTION_FORWARD_SECURE)
          .GetCiphertextSize(min_data_length) +
      quic::QuicPacketCreator::StreamFramePacketOverhead(
          version.transport_version, k8ByteConnectionId, k0ByteConnectionId,
          include_version, include_diversification_nonce, packet_number_length,
          retry_token_length_length, length_length, offset);

  DCHECK(packet_length >= min_packet_length);
  return min_data_length + packet_length - min_packet_length;
}

void QuicProxyClientSocketTestBase::InitializeSession() {
  auto socket = std::make_unique<MockUDPClientSocket>(
      mock_quic_data_.InitializeAndGetSequencedSocketData(), NetLog::Get());
  socket->Connect(peer_addr_);
  runner_ = base::MakeRefCounted<test::TestTaskRunner>(&clock_);
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
  EXPECT_CALL(*send_algorithm_, GetCongestionControlType()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm_, PopulateConnectionStats(_)).Times(AnyNumber());
  helper_ = std::make_unique<QuicChromiumConnectionHelper>(&clock_,
                                                           &random_generator_);
  alarm_factory_ =
      std::make_unique<QuicChromiumAlarmFactory>(runner_.get(), &clock_);

  QuicChromiumPacketWriter* writer = new QuicChromiumPacketWriter(
      socket.get(), base::SingleThreadTaskRunner::GetCurrentDefault().get());
  quic::QuicConnection* connection = new quic::QuicConnection(
      connection_id_, quic::QuicSocketAddress(),
      net::ToQuicSocketAddress(peer_addr_), helper_.get(), alarm_factory_.get(),
      writer, true /* owns_writer */, quic::Perspective::IS_CLIENT,
      quic::test::SupportedVersions(version_), connection_id_generator_);
  connection->set_visitor(&visitor_);
  connection->SetEncrypter(quic::ENCRYPTION_FORWARD_SECURE,
                           std::make_unique<quic::test::TaggingEncrypter>(
                               quic::ENCRYPTION_FORWARD_SECURE));
  quic::test::QuicConnectionPeer::SetSendAlgorithm(connection, send_algorithm_);

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
      QuicSessionAliasKey(
          url::SchemeHostPort(),
          QuicSessionKey("mail.example.org", 80, PRIVACY_MODE_DISABLED,
                         proxy_chain_, SessionUsage::kDestination, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                         /*require_dns_https_alpn=*/false)),
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
      "CONNECTION_UNKNOWN", dns_start, dns_end,
      base::DefaultTickClock::GetInstance(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      /*socket_performance_watcher=*/nullptr, ConnectionEndpointMetadata(),
      /*report_ecn=*/true, /*enable_origin_frame=*/true,
      NetLogWithSource::Make(NetLogSourceType::NONE));

  writer->set_delegate(session_.get());

  session_->Initialize();

  // Blackhole QPACK decoder stream instead of constructing mock writes.
  session_->qpack_decoder()->set_qpack_stream_sender_delegate(
      &noop_qpack_stream_sender_delegate_);

  TestCompletionCallback callback;
  EXPECT_THAT(session_->CryptoConnect(callback.callback()), test::IsOk());
  EXPECT_TRUE(session_->OneRttKeysAvailable());

  session_handle_ = session_->CreateHandle(
      url::SchemeHostPort(url::kHttpsScheme, "mail.example.org", 80));
  EXPECT_THAT(session_handle_->RequestStream(true, callback.callback(),
                                             TRAFFIC_ANNOTATION_FOR_TESTS),
              test::IsOk());

  stream_handle_ = session_handle_->ReleaseStream();
  EXPECT_TRUE(stream_handle_->IsOpen());
}

// Helper functions for constructing packets sent by the client

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructSettingsPacket(uint64_t packet_number) {
  return client_maker_.MakeInitialSettingsPacket(packet_number);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructAckAndRstOnlyPacket(
    uint64_t packet_number,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received) {
  return client_maker_.Packet(packet_number++)
      .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
      .AddRstStreamFrame(client_data_stream_id1_, error_code)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructAckAndRstPacket(
    uint64_t packet_number,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received) {
  return client_maker_.Packet(packet_number++)
      .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
      .AddStopSendingFrame(client_data_stream_id1_, error_code)
      .AddRstStreamFrame(client_data_stream_id1_, error_code)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructRstPacket(
    uint64_t packet_number,
    quic::QuicRstStreamErrorCode error_code) {
  return client_maker_.Packet(packet_number)
      .AddStopSendingFrame(client_data_stream_id1_, error_code)
      .AddRstStreamFrame(client_data_stream_id1_, error_code)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructConnectRequestPacket(
    uint64_t packet_number,
    std::optional<const HttpRequestHeaders> extra_headers,
    RequestPriority request_priority) {
  quiche::HttpHeaderBlock block;
  PopulateConnectRequestIR(&block, extra_headers);
  return client_maker_.MakeRequestHeadersPacket(
      packet_number, client_data_stream_id1_, !kFin,
      ConvertRequestPriorityToQuicPriority(request_priority), std::move(block),
      nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructConnectRequestPacketWithExtraHeaders(
    uint64_t packet_number,
    std::vector<std::pair<std::string, std::string>> extra_headers,
    RequestPriority request_priority) {
  quiche::HttpHeaderBlock block;
  block[":method"] = "CONNECT";
  block[":authority"] =
      HostPortPair::FromSchemeHostPort(destination_endpoint_).ToString();
  for (const auto& header : extra_headers) {
    block[header.first] = header.second;
  }
  return client_maker_.MakeRequestHeadersPacket(
      packet_number, client_data_stream_id1_, !kFin,
      ConvertRequestPriorityToQuicPriority(request_priority), std::move(block),
      nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructConnectAuthRequestPacket(
    uint64_t packet_number) {
  RequestPriority request_priority = LOWEST;
  quiche::HttpHeaderBlock block;
  PopulateConnectRequestIR(&block, /*extra_headers=*/std::nullopt);
  block["proxy-authorization"] = "Basic Zm9vOmJhcg==";
  return client_maker_.MakeRequestHeadersPacket(
      packet_number, client_data_stream_id1_, !kFin,
      ConvertRequestPriorityToQuicPriority(request_priority), std::move(block),
      nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructDataPacket(uint64_t packet_number,
                                                   std::string_view data) {
  return client_maker_.Packet(packet_number)
      .AddStreamFrame(client_data_stream_id1_, !kFin, data)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructDatagramPacket(uint64_t packet_number,
                                                       std::string_view data) {
  return client_maker_.Packet(packet_number).AddMessageFrame(data).Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructAckAndDataPacket(
    uint64_t packet_number,
    uint64_t largest_received,
    uint64_t smallest_received,
    std::string_view data) {
  return client_maker_.Packet(packet_number)
      .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
      .AddStreamFrame(client_data_stream_id1_, !kFin, data)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructAckAndDatagramPacket(
    uint64_t packet_number,
    uint64_t largest_received,
    uint64_t smallest_received,
    std::string_view data) {
  return client_maker_.MakeAckAndDatagramPacket(packet_number, largest_received,
                                                smallest_received, data);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructAckPacket(uint64_t packet_number,
                                                  uint64_t largest_received,
                                                  uint64_t smallest_received) {
  return client_maker_.Packet(packet_number)
      .AddAckFrame(1, largest_received, smallest_received)
      .Build();
}

// Helper functions for constructing packets sent by the server

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerRstPacket(
    uint64_t packet_number,
    quic::QuicRstStreamErrorCode error_code) {
  return server_maker_.Packet(packet_number)
      .AddStopSendingFrame(client_data_stream_id1_, error_code)
      .AddRstStreamFrame(client_data_stream_id1_, error_code)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerDataPacket(
    uint64_t packet_number,
    std::string_view data) {
  return server_maker_.Packet(packet_number)
      .AddStreamFrame(client_data_stream_id1_, !kFin, data)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerDatagramPacket(
    uint64_t packet_number,
    std::string_view data) {
  return server_maker_.Packet(packet_number).AddMessageFrame(data).Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerDataFinPacket(
    uint64_t packet_number,
    std::string_view data) {
  return server_maker_.Packet(packet_number)
      .AddStreamFrame(client_data_stream_id1_, kFin, data)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerConnectReplyPacket(
    uint64_t packet_number,
    bool fin,
    size_t* header_length,
    std::optional<const HttpRequestHeaders> extra_headers) {
  quiche::HttpHeaderBlock block;
  block[":status"] = "200";

  if (extra_headers) {
    HttpRequestHeaders::Iterator it(*extra_headers);
    while (it.GetNext()) {
      std::string name = base::ToLowerASCII(it.name());
      block[name] = it.value();
    }
  }

  return server_maker_.MakeResponseHeadersPacket(
      packet_number, client_data_stream_id1_, fin, std::move(block),
      header_length);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicProxyClientSocketTestBase::
    ConstructServerConnectReplyPacketWithExtraHeaders(
        uint64_t packet_number,
        bool fin,
        std::vector<std::pair<std::string, std::string>> extra_headers) {
  quiche::HttpHeaderBlock block;
  block[":status"] = "200";
  for (const auto& header : extra_headers) {
    block[header.first] = header.second;
  }

  return server_maker_.MakeResponseHeadersPacket(
      packet_number, client_data_stream_id1_, fin, std::move(block), nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerConnectAuthReplyPacket(
    uint64_t packet_number,
    bool fin) {
  quiche::HttpHeaderBlock block;
  block[":status"] = "407";
  block["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
  return server_maker_.MakeResponseHeadersPacket(
      packet_number, client_data_stream_id1_, fin, std::move(block), nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerConnectRedirectReplyPacket(
    uint64_t packet_number,
    bool fin) {
  quiche::HttpHeaderBlock block;
  block[":status"] = "302";
  block["location"] = kRedirectUrl;
  block["set-cookie"] = "foo=bar";
  return server_maker_.MakeResponseHeadersPacket(
      packet_number, client_data_stream_id1_, fin, std::move(block), nullptr);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicProxyClientSocketTestBase::ConstructServerConnectErrorReplyPacket(
    uint64_t packet_number,
    bool fin) {
  quiche::HttpHeaderBlock block;
  block[":status"] = "500";

  return server_maker_.MakeResponseHeadersPacket(
      packet_number, client_data_stream_id1_, fin, std::move(block), nullptr);
}

void QuicProxyClientSocketTestBase::ResumeAndRun() {
  // Run until the pause, if the provider isn't paused yet.
  SequencedSocketData* data = mock_quic_data_.GetSequencedSocketData();
  data->RunUntilPaused();
  data->Resume();
  base::RunLoop().RunUntilIdle();
}

std::string QuicProxyClientSocketTestBase::ConstructDataHeader(
    size_t body_len) {
  quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
      body_len, quiche::SimpleBufferAllocator::Get());
  return std::string(buffer.data(), buffer.size());
}
}  // namespace net
