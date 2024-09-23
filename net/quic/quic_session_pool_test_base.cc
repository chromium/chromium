// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_test_base.h"

#include <sys/types.h>

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_proxy_delegate.h"
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
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/common/quiche_data_writer.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
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
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using std::string;

namespace net::test {

QuicSessionPoolTestBase::RequestBuilder::RequestBuilder(
    QuicSessionPoolTestBase* test,
    QuicSessionPool* pool)
    : quic_version(test->version_),
      net_log(test->net_log_),
      failed_on_default_network_callback(
          test->failed_on_default_network_callback_),
      callback(test->callback_.callback()),
      request(pool) {}
QuicSessionPoolTestBase::RequestBuilder::RequestBuilder(
    QuicSessionPoolTestBase* test)
    : RequestBuilder(test, test->factory_.get()) {}
QuicSessionPoolTestBase::RequestBuilder::~RequestBuilder() = default;

int QuicSessionPoolTestBase::RequestBuilder::CallRequest() {
  return request.Request(
      std::move(destination), quic_version, proxy_chain,
      std::move(proxy_annotation_tag), http_user_agent_settings, session_usage,
      privacy_mode, priority, socket_tag, network_anonymization_key,
      secure_dns_policy, require_dns_https_alpn, cert_verify_flags, url,
      net_log, &net_error_details,
      std::move(failed_on_default_network_callback), std::move(callback));
}
QuicSessionPoolTestBase::QuicSessionPoolTestBase(
    quic::ParsedQuicVersion version,
    std::vector<base::test::FeatureRef> enabled_features,
    std::vector<base::test::FeatureRef> disabled_features)
    : host_resolver_(std::make_unique<MockHostResolver>(
          /*default_result=*/MockHostResolverBase::RuleResolver::
              GetLocalhostResult())),
      socket_factory_(std::make_unique<MockClientSocketFactory>()),
      version_(version),
      client_maker_(version_,
                    quic::QuicUtils::CreateRandomConnectionId(
                        context_.random_generator()),
                    context_.clock(),
                    kDefaultServerHostName,
                    quic::Perspective::IS_CLIENT,
                    /*client_priority_uses_incremental=*/true,
                    /*use_priority_header=*/true),
      server_maker_(version_,
                    quic::QuicUtils::CreateRandomConnectionId(
                        context_.random_generator()),
                    context_.clock(),
                    kDefaultServerHostName,
                    quic::Perspective::IS_SERVER,
                    /*client_priority_uses_incremental=*/false,
                    /*use_priority_header=*/false),
      http_server_properties_(std::make_unique<HttpServerProperties>()),
      cert_verifier_(std::make_unique<MockCertVerifier>()),
      net_log_(NetLogWithSource::Make(NetLog::Get(),
                                      NetLogSourceType::QUIC_SESSION_POOL)),
      failed_on_default_network_callback_(base::BindRepeating(
          &QuicSessionPoolTestBase::OnFailedOnDefaultNetwork,
          base::Unretained(this))),
      quic_params_(context_.params()) {
  enabled_features.push_back(features::kAsyncQuicSession);
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  FLAGS_quic_enable_http3_grease_randomness = false;
  context_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));

  // It's important that different proxies have different IPs, to avoid
  // pooling them together.
  host_resolver_->rules()->AddRule(kProxy1HostName, "127.0.1.1");
  host_resolver_->rules()->AddRule(kProxy2HostName, "127.0.1.2");
}

QuicSessionPoolTestBase::~QuicSessionPoolTestBase() = default;
void QuicSessionPoolTestBase::Initialize() {
  DCHECK(!factory_);
  factory_ = std::make_unique<QuicSessionPool>(
      net_log_.net_log(), host_resolver_.get(), &ssl_config_service_,
      socket_factory_.get(), http_server_properties_.get(),
      cert_verifier_.get(), &transport_security_state_, proxy_delegate_.get(),
      /*sct_auditing_delegate=*/nullptr,
      /*SocketPerformanceWatcherFactory*/ nullptr,
      &crypto_client_stream_factory_, &context_);
}

void QuicSessionPoolTestBase::MaybeMakeNewConnectionIdAvailableToSession(
    const quic::QuicConnectionId& new_cid,
    quic::QuicSession* session,
    uint64_t sequence_number) {
  quic::QuicNewConnectionIdFrame new_cid_frame;
  new_cid_frame.connection_id = new_cid;
  new_cid_frame.sequence_number = sequence_number;
  new_cid_frame.retire_prior_to = 0u;
  new_cid_frame.stateless_reset_token =
      quic::QuicUtils::GenerateStatelessResetToken(new_cid_frame.connection_id);
  session->connection()->OnNewConnectionIdFrame(new_cid_frame);
}

std::unique_ptr<HttpStream> QuicSessionPoolTestBase::CreateStream(
    QuicSessionRequest* request) {
  std::unique_ptr<QuicChromiumClientSession::Handle> session =
      request->ReleaseSessionHandle();
  if (!session || !session->IsConnected()) {
    return nullptr;
  }

  std::set<std::string> dns_aliases =
      session->GetDnsAliasesForSessionKey(request->session_key());
  return std::make_unique<QuicHttpStream>(std::move(session),
                                          std::move(dns_aliases));
}

bool QuicSessionPoolTestBase::HasActiveSession(
    const url::SchemeHostPort& scheme_host_port,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    bool require_dns_https_alpn) {
  quic::QuicServerId server_id(scheme_host_port.host(),
                               scheme_host_port.port());
  return QuicSessionPoolPeer::HasActiveSession(
      factory_.get(), server_id, privacy_mode, network_anonymization_key,
      proxy_chain, session_usage, require_dns_https_alpn);
}

bool QuicSessionPoolTestBase::HasActiveJob(
    const url::SchemeHostPort& scheme_host_port,
    const PrivacyMode privacy_mode,
    bool require_dns_https_alpn) {
  quic::QuicServerId server_id(scheme_host_port.host(),
                               scheme_host_port.port());
  return QuicSessionPoolPeer::HasActiveJob(
      factory_.get(), server_id, privacy_mode, require_dns_https_alpn);
}

// Get the pending, not activated session, if there is only one session alive.
QuicChromiumClientSession* QuicSessionPoolTestBase::GetPendingSession(
    const url::SchemeHostPort& scheme_host_port) {
  quic::QuicServerId server_id(scheme_host_port.host(),
                               scheme_host_port.port());
  return QuicSessionPoolPeer::GetPendingSession(
      factory_.get(), server_id, PRIVACY_MODE_DISABLED, scheme_host_port);
}

QuicChromiumClientSession* QuicSessionPoolTestBase::GetActiveSession(
    const url::SchemeHostPort& scheme_host_port,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    bool require_dns_https_alpn) {
  quic::QuicServerId server_id(scheme_host_port.host(),
                               scheme_host_port.port());
  return QuicSessionPoolPeer::GetActiveSession(
      factory_.get(), server_id, privacy_mode, network_anonymization_key,
      proxy_chain, session_usage, require_dns_https_alpn);
}

int QuicSessionPoolTestBase::GetSourcePortForNewSessionAndGoAway(
    const url::SchemeHostPort& destination) {
  return GetSourcePortForNewSessionInner(destination, true);
}

int QuicSessionPoolTestBase::GetSourcePortForNewSessionInner(
    const url::SchemeHostPort& destination,
    bool goaway_received) {
  // Should only be called if there is no active session for this destination.
  EXPECT_FALSE(HasActiveSession(destination));
  size_t socket_count = socket_factory_->udp_client_socket_ports().size();

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  GURL url("https://" + destination.host() + "/");
  RequestBuilder builder(this);
  builder.destination = destination;
  builder.url = url;

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
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
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
  return socket_factory_->udp_client_socket_ports()[socket_count];
}

ProofVerifyDetailsChromium
QuicSessionPoolTestBase::DefaultProofVerifyDetails() {
  // Load a certificate that is valid for *.example.org
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  EXPECT_TRUE(test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  return verify_details;
}

void QuicSessionPoolTestBase::NotifyIPAddressChanged() {
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructServerConnectionClosePacket(uint64_t num) {
  return server_maker_.Packet(num)
      .AddConnectionCloseFrame(quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED,
                               "Time to panic!")
      .Build();
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructClientRstPacket(
    uint64_t packet_number,
    quic::QuicRstStreamErrorCode error_code) {
  quic::QuicStreamId stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  return client_maker_.Packet(packet_number)
      .AddStopSendingFrame(stream_id, error_code)
      .AddRstStreamFrame(stream_id, error_code)
      .Build();
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructGetRequestPacket(uint64_t packet_number,
                                                   quic::QuicStreamId stream_id,
                                                   bool fin) {
  quiche::HttpHeaderBlock headers =
      client_maker_.GetRequestHeaders("GET", "https", "/");
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_len;
  return client_maker_.MakeRequestHeadersPacket(packet_number, stream_id, fin,
                                                priority, std::move(headers),
                                                &spdy_headers_frame_len);
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructConnectUdpRequestPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    std::string authority,
    std::string path,
    bool fin) {
  return ConstructConnectUdpRequestPacket(client_maker_, packet_number,
                                          stream_id, authority, path, fin);
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructConnectUdpRequestPacket(
    QuicTestPacketMaker& packet_maker,
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    std::string authority,
    std::string path,
    bool fin) {
  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":path"] = path;
  headers[":protocol"] = "connect-udp";
  headers[":method"] = "CONNECT";
  headers[":authority"] = authority;
  headers["user-agent"] = "test-ua";
  headers["capsule-protocol"] = "?1";
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_len;
  auto rv = packet_maker.MakeRequestHeadersPacket(
      packet_number, stream_id, fin, priority, std::move(headers),
      &spdy_headers_frame_len, /*should_include_priority_frame=*/false);
  return rv;
}

std::string QuicSessionPoolTestBase::ConstructClientH3DatagramFrame(
    uint64_t quarter_stream_id,
    uint64_t context_id,
    std::unique_ptr<quic::QuicEncryptedPacket> inner) {
  std::string data;
  // Allow enough space for payload and two varint-62's.
  data.resize(inner->length() + 2 * 8);
  quiche::QuicheDataWriter writer(data.capacity(), data.data());
  CHECK(writer.WriteVarInt62(quarter_stream_id));
  CHECK(writer.WriteVarInt62(context_id));
  CHECK(writer.WriteBytes(inner->data(), inner->length()));
  data.resize(writer.length());
  return data;
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructClientH3DatagramPacket(
    uint64_t packet_number,
    uint64_t quarter_stream_id,
    uint64_t context_id,
    std::unique_ptr<quic::QuicEncryptedPacket> inner) {
  std::string data = ConstructClientH3DatagramFrame(
      quarter_stream_id, context_id, std::move(inner));
  return client_maker_.Packet(packet_number).AddMessageFrame(data).Build();
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructOkResponsePacket(uint64_t packet_number,
                                                   quic::QuicStreamId stream_id,
                                                   bool fin) {
  return ConstructOkResponsePacket(server_maker_, packet_number, stream_id,
                                   fin);
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructOkResponsePacket(
    QuicTestPacketMaker& packet_maker,
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool fin) {
  quiche::HttpHeaderBlock headers = packet_maker.GetResponseHeaders("200");
  size_t spdy_headers_frame_len;
  return packet_maker.MakeResponseHeadersPacket(packet_number, stream_id, fin,
                                                std::move(headers),
                                                &spdy_headers_frame_len);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicSessionPoolTestBase::ConstructInitialSettingsPacket() {
  return client_maker_.MakeInitialSettingsPacket(1);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicSessionPoolTestBase::ConstructInitialSettingsPacket(
    uint64_t packet_number) {
  return client_maker_.MakeInitialSettingsPacket(packet_number);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicSessionPoolTestBase::ConstructInitialSettingsPacket(
    QuicTestPacketMaker& packet_maker,
    uint64_t packet_number) {
  return packet_maker.MakeInitialSettingsPacket(packet_number);
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructServerSettingsPacket(uint64_t packet_number) {
  return server_maker_.MakeInitialSettingsPacket(packet_number);
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructAckPacket(
    test::QuicTestPacketMaker& packet_maker,
    uint64_t packet_number,
    uint64_t packet_num_received,
    uint64_t smallest_received,
    uint64_t largest_received) {
  return packet_maker.Packet(packet_number)
      .AddAckFrame(packet_num_received, smallest_received, largest_received)
      .Build();
}

std::string QuicSessionPoolTestBase::ConstructDataHeader(size_t body_len) {
  quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
      body_len, quiche::SimpleBufferAllocator::Get());
  return std::string(buffer.data(), buffer.size());
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicSessionPoolTestBase::ConstructServerDataPacket(uint64_t packet_number,
                                                   quic::QuicStreamId stream_id,
                                                   bool fin,
                                                   std::string_view data) {
  return server_maker_.Packet(packet_number)
      .AddStreamFrame(stream_id, fin, data)
      .Build();
}

std::string QuicSessionPoolTestBase::ConstructH3Datagram(
    uint64_t stream_id,
    uint64_t context_id,
    std::unique_ptr<quic::QuicEncryptedPacket> packet) {
  std::string data;
  // Allow enough space for payload and two varint-62's.
  data.resize(packet->length() + 2 * 8);
  quiche::QuicheDataWriter writer(data.capacity(), data.data());
  CHECK(writer.WriteVarInt62(stream_id >> 2));
  CHECK(writer.WriteVarInt62(context_id));
  CHECK(writer.WriteBytes(packet->data(), packet->length()));
  data.resize(writer.length());
  return data;
}

quic::QuicStreamId
QuicSessionPoolTestBase::GetNthClientInitiatedBidirectionalStreamId(
    int n) const {
  return quic::test::GetNthClientInitiatedBidirectionalStreamId(
      version_.transport_version, n);
}

quic::QuicStreamId QuicSessionPoolTestBase::GetQpackDecoderStreamId() const {
  return quic::test::GetNthClientInitiatedUnidirectionalStreamId(
      version_.transport_version, 1);
}

std::string QuicSessionPoolTestBase::StreamCancellationQpackDecoderInstruction(
    int n) const {
  return StreamCancellationQpackDecoderInstruction(n, true);
}

std::string QuicSessionPoolTestBase::StreamCancellationQpackDecoderInstruction(
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

quic::QuicStreamId
QuicSessionPoolTestBase::GetNthServerInitiatedUnidirectionalStreamId(int n) {
  return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
      version_.transport_version, n);
}

void QuicSessionPoolTestBase::OnFailedOnDefaultNetwork(int rv) {
  failed_on_default_network_ = true;
}

}  // namespace net::test
