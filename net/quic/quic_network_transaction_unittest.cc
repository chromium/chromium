// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_proxy_delegate.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/test_upload_data_stream_not_allow_http1.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_session_pool_peer.h"
#include "net/quic/quic_socket_data_provider.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_frame_builder.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
using ::testing::Key;

namespace net::test {

namespace {

enum DestinationType {
  // In pooling tests with two requests for different origins to the same
  // destination, the destination should be
  SAME_AS_FIRST,   // the same as the first origin,
  SAME_AS_SECOND,  // the same as the second origin, or
  DIFFERENT,       // different from both.
};

const char kDefaultServerHostName[] = "mail.example.org";
const char kDifferentHostname[] = "different.example.com";

constexpr std::string_view kQuic200RespStatusLine = "HTTP/1.1 200";

// Response data used for QUIC requests in multiple tests.
constexpr std::string_view kQuicRespData = "hello!";
// Response data used for HTTP requests in multiple tests.
// TODO(crbug.com/41496581): Once MockReadWrite accepts a
// std::string_view parameter, we can use "constexpr std::string_view" for this.
const char kHttpRespData[] = "hello world";

struct TestParams {
  quic::ParsedQuicVersion version;
  bool priority_header_enabled;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return base::StrCat({ParsedQuicVersionToString(p.version), "_",
                       p.priority_header_enabled ? "PriorityHeaderEnabled"
                                                 : "PriorityHeaderDisabled"});
}

// Run QuicNetworkTransactionWithDestinationTest instances with all value
// combinations of version and destination_type.
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

std::string GenerateQuicAltSvcHeaderValue(
    const quic::ParsedQuicVersionVector& versions,
    std::string host,
    uint16_t port) {
  std::string value;
  std::string version_string;
  bool first_version = true;
  for (const auto& version : versions) {
    if (first_version) {
      first_version = false;
    } else {
      value.append(", ");
    }
    value.append(base::StrCat({quic::AlpnForVersion(version), "=\"", host, ":",
                               base::NumberToString(port), "\""}));
  }
  return value;
}

std::string GenerateQuicAltSvcHeaderValue(
    const quic::ParsedQuicVersionVector& versions,
    uint16_t port) {
  return GenerateQuicAltSvcHeaderValue(versions, "", port);
}

std::string GenerateQuicAltSvcHeader(
    const quic::ParsedQuicVersionVector& versions) {
  std::string altsvc_header = "Alt-Svc: ";
  altsvc_header.append(GenerateQuicAltSvcHeaderValue(versions, 443));
  altsvc_header.append("\r\n");
  return altsvc_header;
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      AllSupportedQuicVersions();
  for (const quic::ParsedQuicVersion& version : all_supported_versions) {
    params.push_back(TestParams{version, true});
    params.push_back(TestParams{version, false});
  }
  return params;
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

std::string ConstructDataFrameForVersion(std::string_view body,
                                         quic::ParsedQuicVersion version) {
  quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
      body.size(), quiche::SimpleBufferAllocator::Get());
  return base::StrCat({std::string_view(buffer.data(), buffer.size()), body});
}

}  // namespace

class TestSocketPerformanceWatcher : public SocketPerformanceWatcher {
 public:
  TestSocketPerformanceWatcher(bool* should_notify_updated_rtt,
                               bool* rtt_notification_received)
      : should_notify_updated_rtt_(should_notify_updated_rtt),
        rtt_notification_received_(rtt_notification_received) {}

  TestSocketPerformanceWatcher(const TestSocketPerformanceWatcher&) = delete;
  TestSocketPerformanceWatcher& operator=(const TestSocketPerformanceWatcher&) =
      delete;

  ~TestSocketPerformanceWatcher() override = default;

  bool ShouldNotifyUpdatedRTT() const override {
    return *should_notify_updated_rtt_;
  }

  void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) override {
    *rtt_notification_received_ = true;
  }

  void OnConnectionChanged() override {}

 private:
  raw_ptr<bool> should_notify_updated_rtt_;
  raw_ptr<bool> rtt_notification_received_;
};

class TestSocketPerformanceWatcherFactory
    : public SocketPerformanceWatcherFactory {
 public:
  TestSocketPerformanceWatcherFactory() = default;

  TestSocketPerformanceWatcherFactory(
      const TestSocketPerformanceWatcherFactory&) = delete;
  TestSocketPerformanceWatcherFactory& operator=(
      const TestSocketPerformanceWatcherFactory&) = delete;

  ~TestSocketPerformanceWatcherFactory() override = default;

  // SocketPerformanceWatcherFactory implementation:
  std::unique_ptr<SocketPerformanceWatcher> CreateSocketPerformanceWatcher(
      const Protocol protocol,
      const IPAddress& /* address */) override {
    if (protocol != PROTOCOL_QUIC) {
      return nullptr;
    }
    ++watcher_count_;
    return std::make_unique<TestSocketPerformanceWatcher>(
        &should_notify_updated_rtt_, &rtt_notification_received_);
  }

  size_t watcher_count() const { return watcher_count_; }

  bool rtt_notification_received() const { return rtt_notification_received_; }

  void set_should_notify_updated_rtt(bool should_notify_updated_rtt) {
    should_notify_updated_rtt_ = should_notify_updated_rtt;
  }

 private:
  size_t watcher_count_ = 0u;
  bool should_notify_updated_rtt_ = true;
  bool rtt_notification_received_ = false;
};

class QuicNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<TestParams>,
      public WithTaskEnvironment {
 protected:
  QuicNetworkTransactionTest()
      : version_(GetParam().version),
        supported_versions_(quic::test::SupportedVersions(version_)),
        client_maker_(std::make_unique<QuicTestPacketMaker>(
            version_,
            quic::QuicUtils::CreateRandomConnectionId(
                context_.random_generator()),
            context_.clock(),
            kDefaultServerHostName,
            quic::Perspective::IS_CLIENT,
            /*client_priority_uses_incremental=*/true,
            /*use_priority_header=*/true)),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(
                          context_.random_generator()),
                      context_.clock(),
                      kDefaultServerHostName,
                      quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false),
        quic_task_runner_(
            base::MakeRefCounted<TestTaskRunner>(context_.mock_clock())),
        ssl_config_service_(std::make_unique<SSLConfigServiceDefaults>()),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        http_server_properties_(std::make_unique<HttpServerProperties>()),
        ssl_data_(ASYNC, OK) {
    if (GetParam().priority_header_enabled) {
      feature_list_.InitAndEnableFeature(net::features::kPriorityHeader);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kPriorityHeader);
    }
    FLAGS_quic_enable_http3_grease_randomness = false;
    request_.method = "GET";
    std::string url("https://");
    url.append(kDefaultServerHostName);
    request_.url = GURL(url);
    request_.load_flags = 0;
    request_.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    verify_details_.cert_verify_result.verified_cert = cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  }

  void SetUp() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::RunLoop().RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();
    session_.reset();
  }

  void DisablePriorityHeader() {
    // switch client_maker_ to a version that does not add priority headers.
    client_maker_ = std::make_unique<QuicTestPacketMaker>(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        /*client_priority_uses_incremental=*/true,
        /*use_priority_header=*/false);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerConnectionClosePacket(uint64_t num) {
    return server_maker_.Packet(num)
        .AddConnectionCloseFrame(quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED,
                                 "Time to panic!")
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received) {
    return client_maker_->Packet(packet_number)
        .AddAckFrame(1, largest_received, smallest_received)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckAndRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received) {
    return client_maker_->Packet(num)
        .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
        .AddStopSendingFrame(stream_id, error_code)
        .AddRstStreamFrame(stream_id, error_code)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code) {
    return client_maker_->Packet(num)
        .AddStopSendingFrame(stream_id, error_code)
        .AddRstStreamFrame(stream_id, error_code)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientAckAndConnectionClosePacket(
      uint64_t num,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type) {
    return client_maker_->Packet(num)
        .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
        .AddConnectionCloseFrame(quic_error, quic_error_details, frame_type)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code) {
    return server_maker_.Packet(num)
        .AddStopSendingFrame(stream_id, error_code)
        .AddRstStreamFrame(stream_id, error_code)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number) {
    return client_maker_->MakeInitialSettingsPacket(packet_number);
  }

  // Uses default QuicTestPacketMaker.
  quiche::HttpHeaderBlock GetRequestHeaders(const std::string& method,
                                            const std::string& scheme,
                                            const std::string& path) {
    return GetRequestHeaders(method, scheme, path, client_maker_.get());
  }

  // Uses customized QuicTestPacketMaker.
  quiche::HttpHeaderBlock GetRequestHeaders(const std::string& method,
                                            const std::string& scheme,
                                            const std::string& path,
                                            QuicTestPacketMaker* maker) {
    return maker->GetRequestHeaders(method, scheme, path);
  }

  quiche::HttpHeaderBlock ConnectRequestHeaders(const std::string& host_port) {
    return client_maker_->ConnectRequestHeaders(host_port);
  }

  quiche::HttpHeaderBlock GetResponseHeaders(const std::string& status) {
    return server_maker_.GetResponseHeaders(status);
  }

  // Appends alt_svc headers in the response headers.
  quiche::HttpHeaderBlock GetResponseHeaders(const std::string& status,
                                             const std::string& alt_svc) {
    return server_maker_.GetResponseHeaders(status, alt_svc);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      std::string_view data) {
    return server_maker_.Packet(packet_number)
        .AddStreamFrame(stream_id, fin, data)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      std::string_view data) {
    return client_maker_->Packet(packet_number)
        .AddStreamFrame(stream_id, fin, data)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckAndDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      bool fin,
      std::string_view data) {
    return client_maker_->Packet(packet_number)
        .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
        .AddStreamFrame(stream_id, fin, data)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckDataAndRst(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      quic::QuicStreamId data_id,
      bool fin,
      std::string_view data) {
    return client_maker_->Packet(packet_number)
        .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
        .AddStreamFrame(data_id, fin, data)
        .AddStopSendingFrame(stream_id, error_code)
        .AddRstStreamFrame(stream_id, error_code)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      quiche::HttpHeaderBlock headers,
      bool should_include_priority_frame = true) {
    return ConstructClientRequestHeadersPacket(
        packet_number, stream_id, fin, DEFAULT_PRIORITY, std::move(headers),
        should_include_priority_frame);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      RequestPriority request_priority,
      quiche::HttpHeaderBlock headers,
      bool should_include_priority_frame = true) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_->MakeRequestHeadersPacket(
        packet_number, stream_id, fin, priority, std::move(headers), nullptr,
        should_include_priority_frame);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructClientPriorityPacket(
      uint64_t packet_number,
      quic::QuicStreamId id,
      RequestPriority request_priority) {
    spdy::SpdyPriority spdy_priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_->MakePriorityPacket(packet_number, id, spdy_priority);
  }

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructClientRequestHeadersAndDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      RequestPriority request_priority,
      quiche::HttpHeaderBlock headers,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_->MakeRequestHeadersAndMultipleDataFramesPacket(
        packet_number, stream_id, fin, priority, std::move(headers),
        spdy_headers_frame_length, data_writes);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerResponseHeadersPacket(uint64_t packet_number,
                                       quic::QuicStreamId stream_id,
                                       bool fin,
                                       quiche::HttpHeaderBlock headers) {
    return server_maker_.MakeResponseHeadersPacket(
        packet_number, stream_id, fin, std::move(headers), nullptr);
  }

  std::string ConstructDataFrame(std::string_view body) {
    return ConstructDataFrameForVersion(body, version_);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructConnectUdpRequestPacket(
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
    headers["capsule-protocol"] = "?1";
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    auto rv = client_maker_->MakeRequestHeadersPacket(
        packet_number, stream_id, fin, priority, std::move(headers),
        &spdy_headers_frame_len, /*should_include_priority_frame=*/false);
    return rv;
  }

  std::string ConstructH3Datagram(
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

  void CreateSession(const quic::ParsedQuicVersionVector& supported_versions) {
    session_params_.enable_quic = true;
    context_.params()->supported_versions = supported_versions;

    session_context_.quic_context = &context_;
    session_context_.client_socket_factory = &socket_factory_;
    session_context_.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;
    session_context_.host_resolver = &host_resolver_;
    session_context_.cert_verifier = &cert_verifier_;
    session_context_.transport_security_state = &transport_security_state_;
    session_context_.socket_performance_watcher_factory =
        &test_socket_performance_watcher_factory_;
    session_context_.proxy_delegate = proxy_delegate_.get();
    session_context_.proxy_resolution_service = proxy_resolution_service_.get();
    session_context_.ssl_config_service = ssl_config_service_.get();
    session_context_.http_auth_handler_factory = auth_handler_factory_.get();
    session_context_.http_server_properties = http_server_properties_.get();
    session_context_.net_log = NetLog::Get();

    session_ =
        std::make_unique<HttpNetworkSession>(session_params_, session_context_);
    session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
        true);
    SpdySessionPoolPeer spdy_pool_peer(session_->spdy_session_pool());
    spdy_pool_peer.SetEnableSendingInitialData(false);
  }

  void CreateSession() { return CreateSession(supported_versions_); }

  void CheckWasQuicResponse(HttpNetworkTransaction* trans,
                            std::string_view status_line,
                            const quic::ParsedQuicVersion& version) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ(status_line, response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_alpn_negotiated);
    auto connection_info =
        QuicHttpStream::ConnectionInfoFromQuicVersion(version);
    if (connection_info == response->connection_info) {
      return;
    }
    // QUIC v1 and QUIC v2 are considered a match, because they have the same
    // ALPN token.
    if ((connection_info == HttpConnectionInfo::kQUIC_RFC_V1 ||
         connection_info == HttpConnectionInfo::kQUIC_2_DRAFT_8) &&
        (response->connection_info == HttpConnectionInfo::kQUIC_RFC_V1 ||
         response->connection_info == HttpConnectionInfo::kQUIC_2_DRAFT_8)) {
      return;
    }

    // They do not match.  This EXPECT_EQ will fail and print useful
    // information.
    EXPECT_EQ(connection_info, response->connection_info);
  }

  void CheckWasQuicResponse(HttpNetworkTransaction* trans,
                            std::string_view status_line) {
    CheckWasQuicResponse(trans, status_line, version_);
  }

  void CheckWasQuicResponse(HttpNetworkTransaction* trans) {
    CheckWasQuicResponse(trans, kQuic200RespStatusLine, version_);
  }

  void CheckResponsePort(HttpNetworkTransaction* trans, uint16_t port) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(port, response->remote_endpoint.port());
  }

  void CheckWasHttpResponse(HttpNetworkTransaction* trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_FALSE(response->was_alpn_negotiated);
    EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  }

  void CheckWasSpdyResponse(HttpNetworkTransaction* trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    // SPDY and QUIC use the same 200 response format.
    EXPECT_EQ(kQuic200RespStatusLine, response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_alpn_negotiated);
    EXPECT_EQ(HttpConnectionInfo::kHTTP2, response->connection_info);
  }

  void CheckResponseData(HttpNetworkTransaction* trans,
                         std::string_view expected) {
    std::string response_data;
    ASSERT_THAT(ReadTransaction(trans, &response_data), IsOk());
    EXPECT_EQ(expected, response_data);
  }

  void RunTransaction(HttpNetworkTransaction* trans) {
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_with_source_);
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
  }

  void SendRequestAndExpectHttpResponse(std::string_view expected) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasHttpResponse(&trans);
    CheckResponseData(&trans, expected);
  }

  void SendRequestAndExpectHttpResponseFromProxy(
      std::string_view expected,
      uint16_t port,
      const ProxyChain& proxy_chain) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasHttpResponse(&trans);
    CheckResponsePort(&trans, port);
    CheckResponseData(&trans, expected);
    EXPECT_EQ(trans.GetResponseInfo()->proxy_chain, proxy_chain);
    ASSERT_TRUE(proxy_chain.IsValid());
    ASSERT_FALSE(proxy_chain.is_direct());
  }

  void SendRequestAndExpectSpdyResponseFromProxy(
      std::string_view expected,
      uint16_t port,
      const ProxyChain& proxy_chain) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasSpdyResponse(&trans);
    CheckResponsePort(&trans, port);
    CheckResponseData(&trans, expected);
    EXPECT_EQ(trans.GetResponseInfo()->proxy_chain, proxy_chain);
    ASSERT_TRUE(proxy_chain.IsValid());
    ASSERT_FALSE(proxy_chain.is_direct());
  }

  void SendRequestAndExpectQuicResponse(std::string_view expected,
                                        std::string_view status_line) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, 443, status_line,
                                                   version_, std::nullopt);
  }

  void SendRequestAndExpectQuicResponse(std::string_view expected) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(
        expected, 443, kQuic200RespStatusLine, version_, std::nullopt);
  }

  void AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey()) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    url::SchemeHostPort server(request_.url);
    AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
    base::Time expiration = base::Time::Now() + base::Days(1);
    http_server_properties_->SetQuicAlternativeService(
        server, network_anonymization_key, alternative_service, expiration,
        supported_versions_);
  }

  void AddQuicRemoteAlternativeServiceMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode,
      const HostPortPair& alternative) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    url::SchemeHostPort server(request_.url);
    AlternativeService alternative_service(kProtoQUIC, alternative.host(),
                                           alternative.port());
    base::Time expiration = base::Time::Now() + base::Days(1);
    http_server_properties_->SetQuicAlternativeService(
        server, NetworkAnonymizationKey(), alternative_service, expiration,
        supported_versions_);
  }

  void ExpectBrokenAlternateProtocolMapping(
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey()) {
    const url::SchemeHostPort server(request_.url);
    const AlternativeServiceInfoVector alternative_service_info_vector =
        http_server_properties_->GetAlternativeServiceInfos(
            server, network_anonymization_key);
    EXPECT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_TRUE(http_server_properties_->IsAlternativeServiceBroken(
        alternative_service_info_vector[0].alternative_service(),
        network_anonymization_key));
  }

  void ExpectQuicAlternateProtocolMapping(
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey()) {
    const url::SchemeHostPort server(request_.url);
    const AlternativeServiceInfoVector alternative_service_info_vector =
        http_server_properties_->GetAlternativeServiceInfos(
            server, network_anonymization_key);
    EXPECT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_EQ(
        kProtoQUIC,
        alternative_service_info_vector[0].alternative_service().protocol);
    EXPECT_FALSE(http_server_properties_->IsAlternativeServiceBroken(
        alternative_service_info_vector[0].alternative_service(),
        network_anonymization_key));
  }

  void AddHangingNonAlternateProtocolSocketData() {
    auto hanging_data = std::make_unique<StaticSocketDataProvider>();
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data->set_connect_data(hanging_connect);
    hanging_data_.push_back(std::move(hanging_data));
    socket_factory_.AddSocketDataProvider(hanging_data_.back().get());
  }

  // Adds a new socket data provider for an HTTP request, and runs a request,
  // expecting it to be used.
  void AddHttpDataAndRunRequest() {
    MockWrite http_writes[] = {
        MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
        MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
        MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

    MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                             MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                             MockRead(SYNCHRONOUS, 5, "http used"),
                             // Connection closed.
                             MockRead(SYNCHRONOUS, OK, 6)};
    SequencedSocketData http_data(http_reads, http_writes);
    socket_factory_.AddSocketDataProvider(&http_data);
    SSLSocketDataProvider ssl_data(ASYNC, OK);
    socket_factory_.AddSSLSocketDataProvider(&ssl_data);
    SendRequestAndExpectHttpResponse("http used");
    EXPECT_TRUE(http_data.AllWriteDataConsumed());
    EXPECT_TRUE(http_data.AllReadDataConsumed());
  }

  // Adds a new socket data provider for a QUIC request, and runs a request,
  // expecting it to be used. The new QUIC session is not closed.
  void AddQuicDataAndRunRequest() {
    QuicTestPacketMaker client_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        /*client_priority_uses_incremental=*/true,
        /*use_priority_header=*/true);
    QuicTestPacketMaker server_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        /*client_priority_uses_incremental=*/false,
        /*use_priority_header=*/false);
    MockQuicData quic_data(version_);
    int packet_number = 1;
    client_maker.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker.MakeInitialSettingsPacket(packet_number++));
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
            true, ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", "https", "/", &client_maker), nullptr));
    client_maker.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
    quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
    quic_data.AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   server_maker.GetResponseHeaders("200"), nullptr));
    quic_data.AddRead(
        ASYNC,
        server_maker.Packet(2)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0), true,
                            ConstructDataFrame("quic used"))
            .Build());
    // Don't care about the final ack.
    quic_data.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);
    // No more data to read.
    quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    quic_data.AddSocketDataToFactory(&socket_factory_);

    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    TestCompletionCallback callback;
    int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

    // Pump the message loop to get the request started.
    base::RunLoop().RunUntilIdle();
    // Explicitly confirm the handshake.
    crypto_client_stream_factory_.last_stream()
        ->NotifySessionOneRttKeyAvailable();

    ASSERT_FALSE(quic_data.AllReadDataConsumed());
    quic_data.Resume();

    // Run the QUIC session to completion.
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(quic_data.AllReadDataConsumed());
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

  static void AddCertificate(SSLSocketDataProvider* ssl_data) {
    ssl_data->ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
    ASSERT_TRUE(ssl_data->ssl_info.cert);
  }

  void SendRequestAndExpectQuicResponseMaybeFromProxy(
      std::string_view expected,
      uint16_t port,
      std::string_view status_line,
      const quic::ParsedQuicVersion& version,
      std::optional<ProxyChain> proxy_chain) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasQuicResponse(&trans, status_line, version);
    CheckResponsePort(&trans, port);
    CheckResponseData(&trans, expected);
    if (proxy_chain.has_value()) {
      EXPECT_EQ(trans.GetResponseInfo()->proxy_chain, *proxy_chain);
      ASSERT_TRUE(proxy_chain->IsValid());
      ASSERT_FALSE(proxy_chain->is_direct());
      // DNS aliases should be empty when using a proxy.
      EXPECT_TRUE(trans.GetResponseInfo()->dns_aliases.empty());
    } else {
      EXPECT_TRUE(trans.GetResponseInfo()->proxy_chain.is_direct());
    }
  }

  // Verify that the set of QUIC protocols in `alt_svc_info_vector` and
  // `supported_versions` is the same.  Since QUICv1 and QUICv2 have the same
  // ALPN token "h3", they cannot be distinguished when parsing ALPN, so
  // consider them equal.  This is accomplished by comparing the set of ALPN
  // strings (instead of comparing the set of ParsedQuicVersion entities).
  static void VerifyQuicVersionsInAlternativeServices(
      const AlternativeServiceInfoVector& alt_svc_info_vector,
      const quic::ParsedQuicVersionVector& supported_versions) {
    // Process supported versions.
    std::set<std::string> supported_alpn;
    for (const auto& version : supported_versions) {
      if (version.AlpnDeferToRFCv1()) {
        // These versions currently do not support Alt-Svc.
        return;
      }
      supported_alpn.insert(quic::ParsedQuicVersionToString(version));
    }

    // Versions that support the legacy Google-specific Alt-Svc format are sent
    // in a single Alt-Svc entry, therefore they are accumulated in a single
    // AlternativeServiceInfo, whereas more recent versions all have their own
    // Alt-Svc entry and AlternativeServiceInfo entry.  Flatten to compare.
    std::set<std::string> alt_svc_negotiated_alpn;
    for (const auto& alt_svc_info : alt_svc_info_vector) {
      EXPECT_EQ(kProtoQUIC, alt_svc_info.alternative_service().protocol);
      for (const auto& version : alt_svc_info.advertised_versions()) {
        alt_svc_negotiated_alpn.insert(
            quic::ParsedQuicVersionToString(version));
      }
    }

    // Compare.
    EXPECT_EQ(alt_svc_negotiated_alpn, supported_alpn);
  }

  const quic::ParsedQuicVersion version_;
  const std::string alt_svc_header_ =
      GenerateQuicAltSvcHeader({version_}) + "\r\n";
  quic::ParsedQuicVersionVector supported_versions_;
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  MockQuicContext context_;
  std::unique_ptr<QuicTestPacketMaker> client_maker_;
  QuicTestPacketMaker server_maker_;
  scoped_refptr<TestTaskRunner> quic_task_runner_;
  std::unique_ptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockHostResolver host_resolver_{/*default_result=*/MockHostResolverBase::
                                      RuleResolver::GetLocalhostResult()};
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  TestSocketPerformanceWatcherFactory test_socket_performance_watcher_factory_;
  std::unique_ptr<SSLConfigServiceDefaults> ssl_config_service_;
  // `proxy_resolution_service_` may store a pointer to `proxy_delegate_`, so
  // ensure that the latter outlives the former.
  std::unique_ptr<TestProxyDelegate> proxy_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  HttpNetworkSessionParams session_params_;
  HttpNetworkSessionContext session_context_;
  HttpRequestInfo request_;
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::NONE)};
  RecordingNetLogObserver net_log_observer_;
  std::vector<std::unique_ptr<StaticSocketDataProvider>> hanging_data_;
  SSLSocketDataProvider ssl_data_;
  std::unique_ptr<ScopedMockNetworkChangeNotifier> scoped_mock_change_notifier_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicNetworkTransactionTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicNetworkTransactionTest, BasicRequestAndResponse) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData quic_data(version_);
  int sent_packet_num = 0;
  int received_packet_num = 0;
  const quic::QuicStreamId stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  // HTTP/3 SETTINGS are always the first thing sent on a connection
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(++sent_packet_num));
  // The GET request with no body is sent next.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                                      ++sent_packet_num, stream_id, true,
                                      GetRequestHeaders("GET", "https", "/")));
  // Read the response headers.
  quic_data.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                               ++received_packet_num, stream_id, false,
                               GetResponseHeaders("200")));
  // Read the response body.
  quic_data.AddRead(SYNCHRONOUS, ConstructServerDataPacket(
                                     ++received_packet_num, stream_id, true,
                                     ConstructDataFrame(kQuicRespData)));
  // Acknowledge the previous two received packets.
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(++sent_packet_num, received_packet_num, 1));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  // Connection close on shutdown.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientAckAndConnectionClosePacket(
                                      ++sent_packet_num, received_packet_num, 1,
                                      quic::QUIC_CONNECTION_CANCELLED,
                                      "net error", quic::NO_IETF_QUIC_ERROR));

  quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Delete the session while the MockQuicData is still in scope.
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, HeaderDecodingDelayHistogram) {
  base::HistogramTester histograms;

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData quic_data(version_);
  int sent_packet_num = 0;
  int received_packet_num = 0;
  const quic::QuicStreamId stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  // HTTP/3 SETTINGS are always the first thing sent on a connection
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(++sent_packet_num));
  // The GET request with no body is sent next.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                                      ++sent_packet_num, stream_id, true,
                                      GetRequestHeaders("GET", "https", "/")));
  // Read the response headers.
  quic_data.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                               ++received_packet_num, stream_id, false,
                               GetResponseHeaders("200")));
  // Read the response body.
  quic_data.AddRead(SYNCHRONOUS, ConstructServerDataPacket(
                                     ++received_packet_num, stream_id, true,
                                     ConstructDataFrame(kQuicRespData)));
  // Acknowledge the previous two received packets.
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(++sent_packet_num, received_packet_num, 1));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  // Connection close on shutdown.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientAckAndConnectionClosePacket(
                                      ++sent_packet_num, received_packet_num, 1,
                                      quic::QUIC_CONNECTION_CANCELLED,
                                      "net error", quic::NO_IETF_QUIC_ERROR));

  quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Delete the session while the MockQuicData is still in scope.
  session_.reset();

  histograms.ExpectTotalCount(
      "Net.QuicChromiumClientStream.HeaderDecodingDelay", 1);
}

TEST_P(QuicNetworkTransactionTest, BasicRequestAndResponseWithAsycWrites) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData quic_data(version_);
  int sent_packet_num = 0;
  int received_packet_num = 0;
  const quic::QuicStreamId stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  // HTTP/3 SETTINGS are always the first thing sent on a connection
  quic_data.AddWrite(ASYNC, ConstructInitialSettingsPacket(++sent_packet_num));
  // The GET request with no body is sent next.
  quic_data.AddWrite(ASYNC, ConstructClientRequestHeadersPacket(
                                ++sent_packet_num, stream_id, true,
                                GetRequestHeaders("GET", "https", "/")));
  // Read the response headers.
  quic_data.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                               ++received_packet_num, stream_id, false,
                               GetResponseHeaders("200")));
  // Read the response body.
  quic_data.AddRead(SYNCHRONOUS, ConstructServerDataPacket(
                                     ++received_packet_num, stream_id, true,
                                     ConstructDataFrame(kQuicRespData)));
  // Acknowledge the previous two received packets.
  quic_data.AddWrite(ASYNC, ConstructClientAckPacket(++sent_packet_num,
                                                     received_packet_num, 1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  // Connection close on shutdown.
  quic_data.AddWrite(ASYNC, ConstructClientAckAndConnectionClosePacket(
                                ++sent_packet_num, received_packet_num, 1,
                                quic::QUIC_CONNECTION_CANCELLED, "net error",
                                quic::NO_IETF_QUIC_ERROR));

  quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Delete the session while the MockQuicData is still in scope.
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, BasicRequestAndResponseWithTrailers) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData quic_data(version_);
  int sent_packet_num = 0;
  int received_packet_num = 0;
  const quic::QuicStreamId stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  // HTTP/3 SETTINGS are always the first thing sent on a connection
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(++sent_packet_num));
  // The GET request with no body is sent next.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                                      ++sent_packet_num, stream_id, true,
                                      GetRequestHeaders("GET", "https", "/")));
  // Read the response headers.
  quic_data.AddRead(ASYNC, server_maker_.MakeResponseHeadersPacket(
                               ++received_packet_num, stream_id, false,
                               GetResponseHeaders("200"), nullptr));
  // Read the response body.
  quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(++received_packet_num, stream_id, false,
                                       ConstructDataFrame(kQuicRespData)));
  // Acknowledge the previous two received packets.
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(++sent_packet_num, received_packet_num, 1));
  // Read the response trailers.
  quiche::HttpHeaderBlock trailers;
  trailers.AppendValueOrAddHeader("foo", "bar");
  quic_data.AddRead(ASYNC, server_maker_.MakeResponseHeadersPacket(
                               ++received_packet_num, stream_id, true,
                               std::move(trailers), nullptr));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  // Connection close on shutdown.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientAckAndConnectionClosePacket(
                                      ++sent_packet_num, received_packet_num, 1,
                                      quic::QUIC_CONNECTION_CANCELLED,
                                      "net error", quic::NO_IETF_QUIC_ERROR));

  quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Delete the session while the MockQuicData is still in scope.
  session_.reset();
}

// Regression test for crbug.com/332587381
TEST_P(QuicNetworkTransactionTest, BasicRequestAndResponseWithEmptyTrailers) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData quic_data(version_);
  int sent_packet_num = 0;
  int received_packet_num = 0;
  const quic::QuicStreamId stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  // HTTP/3 SETTINGS are always the first thing sent on a connection
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(++sent_packet_num));
  // The GET request with no body is sent next.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                                      ++sent_packet_num, stream_id, true,
                                      GetRequestHeaders("GET", "https", "/")));
  // Read the response headers.
  quic_data.AddRead(ASYNC, server_maker_.MakeResponseHeadersPacket(
                               ++received_packet_num, stream_id, false,
                               GetResponseHeaders("200"), nullptr));
  // Read the response body.
  quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(++received_packet_num, stream_id, false,
                                       ConstructDataFrame(kQuicRespData)));
  // Acknowledge the previous two received packets.
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(++sent_packet_num, received_packet_num, 1));
  // Read the empty response trailers.
  quic_data.AddRead(ASYNC, server_maker_.MakeResponseHeadersPacket(
                               ++received_packet_num, stream_id, true,
                               quiche::HttpHeaderBlock(), nullptr));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  // Connection close on shutdown.
  quic_data.AddWrite(SYNCHRONOUS, ConstructClientAckAndConnectionClosePacket(
                                      ++sent_packet_num, received_packet_num, 1,
                                      quic::QUIC_CONNECTION_CANCELLED,
                                      "net error", quic::NO_IETF_QUIC_ERROR));

  quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Delete the session while the MockQuicData is still in scope.
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, WriteErrorHandshakeConfirmed) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  base::HistogramTester histograms;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  mock_quic_data.AddWrite(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  histograms.ExpectBucketCount("Net.QuicSession.WriteError",
                               -ERR_INTERNET_DISCONNECTED, 1);
  histograms.ExpectBucketCount("Net.QuicSession.WriteError.HandshakeConfirmed",
                               -ERR_INTERNET_DISCONNECTED, 1);
}

TEST_P(QuicNetworkTransactionTest, WriteErrorHandshakeConfirmedAsync) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  base::HistogramTester histograms;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  mock_quic_data.AddWrite(ASYNC, ERR_INTERNET_DISCONNECTED);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  histograms.ExpectBucketCount("Net.QuicSession.WriteError",
                               -ERR_INTERNET_DISCONNECTED, 1);
  histograms.ExpectBucketCount("Net.QuicSession.WriteError.HandshakeConfirmed",
                               -ERR_INTERNET_DISCONNECTED, 1);
}

TEST_P(QuicNetworkTransactionTest, SocketWatcherEnabled) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  test_socket_performance_watcher_factory_.set_should_notify_updated_rtt(true);

  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  SendRequestAndExpectQuicResponse(kQuicRespData);
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

TEST_P(QuicNetworkTransactionTest, SocketWatcherDisabled) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  test_socket_performance_watcher_factory_.set_should_notify_updated_rtt(false);

  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  SendRequestAndExpectQuicResponse(kQuicRespData);
  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

TEST_P(QuicNetworkTransactionTest, ForceQuic) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Check that the NetLog was filled reasonably.
  auto entries = net_log_observer_.GetEntries();
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_SESSION_PACKET_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_PACKET_SENT.
  pos = ExpectLogContainsSomewhere(entries, 0,
                                   NetLogEventType::QUIC_SESSION_PACKET_SENT,
                                   NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0,
      NetLogEventType::QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  EXPECT_EQ(1, GetIntegerValueFromParams(entries[pos], "packet_number"));

  // ... and also a TYPE_QUIC_SESSION_PACKET_AUTHENTICATED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_SESSION_PACKET_AUTHENTICATED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  // ... and also a QUIC_SESSION_STREAM_FRAME_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_SESSION_STREAM_FRAME_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  int log_stream_id = GetIntegerValueFromParams(entries[pos], "stream_id");
  EXPECT_EQ(GetNthClientInitiatedBidirectionalStreamId(0),
            static_cast<quic::QuicStreamId>(log_stream_id));
}

// Regression test for https://crbug.com/1043531.
TEST_P(QuicNetworkTransactionTest, ResetOnEmptyResponseHeaders) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int write_packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          write_packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, GetRequestHeaders("GET", "https", "/")));

  const quic::QuicStreamId request_stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  quiche::HttpHeaderBlock empty_response_headers;
  const std::string response_data = server_maker_.QpackEncodeHeaders(
      request_stream_id, std::move(empty_response_headers), nullptr);
  uint64_t read_packet_num = 1;
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(read_packet_num++, request_stream_id,
                                       false, response_data));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      ASYNC, ConstructClientAckDataAndRst(
                 write_packet_num++, request_stream_id,
                 quic::QUIC_STREAM_GENERAL_PROTOCOL_ERROR, 1, 1,
                 GetQpackDecoderStreamId(), false,
                 StreamCancellationQpackDecoderInstruction(request_stream_id)));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(net::ERR_QUIC_PROTOCOL_ERROR));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, LargeResponseHeaders) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  quiche::HttpHeaderBlock response_headers = GetResponseHeaders("200");
  response_headers["key1"] = std::string(30000, 'A');
  response_headers["key2"] = std::string(30000, 'A');
  response_headers["key3"] = std::string(30000, 'A');
  response_headers["key4"] = std::string(30000, 'A');
  response_headers["key5"] = std::string(30000, 'A');
  response_headers["key6"] = std::string(30000, 'A');
  response_headers["key7"] = std::string(30000, 'A');
  response_headers["key8"] = std::string(30000, 'A');
  quic::QuicStreamId stream_id;
  std::string response_data;
  stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  response_data = server_maker_.QpackEncodeHeaders(
      stream_id, std::move(response_headers), nullptr);

  uint64_t packet_number = 1;
  size_t chunk_size = 1200;
  for (size_t offset = 0; offset < response_data.length();
       offset += chunk_size) {
    size_t len = std::min(chunk_size, response_data.length() - offset);
    mock_quic_data.AddRead(
        ASYNC, ConstructServerDataPacket(
                   packet_number++, stream_id, false,
                   std::string_view(response_data).substr(offset, len)));
  }

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 packet_number, GetNthClientInitiatedBidirectionalStreamId(0),
                 true, ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(ASYNC, ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddWrite(
      ASYNC, ConstructClientAckPacket(packet_num++, packet_number, 3));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, TooLargeResponseHeaders) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));

  quiche::HttpHeaderBlock response_headers = GetResponseHeaders("200");
  response_headers["key1"] = std::string(30000, 'A');
  response_headers["key2"] = std::string(30000, 'A');
  response_headers["key3"] = std::string(30000, 'A');
  response_headers["key4"] = std::string(30000, 'A');
  response_headers["key5"] = std::string(30000, 'A');
  response_headers["key6"] = std::string(30000, 'A');
  response_headers["key7"] = std::string(30000, 'A');
  response_headers["key8"] = std::string(30000, 'A');
  response_headers["key9"] = std::string(30000, 'A');

  quic::QuicStreamId stream_id;
  std::string response_data;
  stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  response_data = server_maker_.QpackEncodeHeaders(
      stream_id, std::move(response_headers), nullptr);

  uint64_t packet_number = 1;
  size_t chunk_size = 1200;
  for (size_t offset = 0; offset < response_data.length();
       offset += chunk_size) {
    size_t len = std::min(chunk_size, response_data.length() - offset);
    mock_quic_data.AddRead(
        ASYNC, ConstructServerDataPacket(
                   packet_number++, stream_id, false,
                   std::string_view(response_data).substr(offset, len)));
  }

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 packet_number, GetNthClientInitiatedBidirectionalStreamId(0),
                 true, ConstructDataFrame("hello!")));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(ASYNC, ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddWrite(
      ASYNC, ConstructClientAckAndRstPacket(
                 packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                 quic::QUIC_HEADERS_TOO_LARGE, packet_number, 3));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_P(QuicNetworkTransactionTest, RedirectMultipleLocations) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));

  quiche::HttpHeaderBlock response_headers = GetResponseHeaders("301");
  response_headers.AppendValueOrAddHeader("location", "https://example1.test");
  response_headers.AppendValueOrAddHeader("location", "https://example2.test");

  const quic::QuicStreamId stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  const std::string response_data = server_maker_.QpackEncodeHeaders(
      stream_id, std::move(response_headers), nullptr);
  ASSERT_LT(response_data.size(), 1200u);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(/*packet_number=*/1, stream_id,
                                       /*fin=*/true, response_data));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      ASYNC, ConstructClientAckAndRstPacket(
                 packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                 quic::QUIC_STREAM_CANCELLED,
                 /*largest_received=*/1, /*smallest_received=*/1));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_P(QuicNetworkTransactionTest, ForceQuicForAll) {
  context_.params()->origins_to_force_quic_on.insert(HostPortPair());

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

// Regression test for https://crbug.com/695225
TEST_P(QuicNetworkTransactionTest, 408Response) {
  context_.params()->origins_to_force_quic_on.insert(HostPortPair());

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("408")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData, "HTTP/1.1 408");
}

TEST_P(QuicNetworkTransactionTest, QuicProxy) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:80"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), 1, 1,
          false, ConstructDataFrame(kGetRequest)));

  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 11\r\n\r\n";
  ASSERT_EQ(strlen(kHttpRespData), 11u);

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kGetResponse)));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kHttpRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(0))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  // There is no need to set up an alternate protocol job, because
  // no attempt will be made to speak to the proxy over TCP.

  request_.url = GURL("http://mail.example.org/");
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectHttpResponseFromProxy(
      kHttpRespData, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());

  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

// Regression test for https://crbug.com/492458.  Test that for an HTTP
// connection through a QUIC proxy, the certificate exhibited by the proxy is
// checked against the proxy hostname, not the origin hostname.
TEST_P(QuicNetworkTransactionTest, QuicProxyWithCert) {
  DisablePriorityHeader();
  const std::string kOriginHost = "mail.example.com";
  const std::string kProxyHost = "proxy.example.org";

  session_params_.enable_quic = true;
  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, kProxyHost, 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  client_maker_->set_hostname(kOriginHost);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.com:80"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.com\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), 1, 1,
          false, ConstructDataFrame(kGetRequest)));

  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 11\r\n\r\n";
  ASSERT_EQ(strlen(kHttpRespData), 11u);

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kGetResponse)));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kHttpRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientDataPacket(packet_num++, GetQpackDecoderStreamId(), false,
                                StreamCancellationQpackDecoderInstruction(0)));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert.get());
  // This certificate is valid for the proxy, but not for the origin.
  EXPECT_TRUE(cert->VerifyNameMatch(kProxyHost));
  EXPECT_FALSE(cert->VerifyNameMatch(kOriginHost));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  request_.url = GURL("http://" + kOriginHost);
  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectHttpResponseFromProxy(
      kHttpRespData, kQuicProxyChain.First().GetPort(), kQuicProxyChain);
}

TEST_P(QuicNetworkTransactionTest, AlternativeServicesDifferentHost) {
  context_.params()->allow_remote_alt_svc = true;
  HostPortPair origin("www.example.org", 443);
  HostPortPair alternative("mail.example.org", 443);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert.get());
  // TODO(rch): the connection should be "to" the origin, so if the cert is
  // valid for the origin but not the alternative, that should work too.
  EXPECT_TRUE(cert->VerifyNameMatch(origin.host()));
  EXPECT_TRUE(cert->VerifyNameMatch(alternative.host()));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  client_maker_->set_hostname(origin.host());
  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  request_.url = GURL("https://" + origin.host());
  AddQuicRemoteAlternativeServiceMapping(
      MockCryptoClientStream::CONFIRM_HANDSHAKE, alternative);
  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, DoNotUseQuicForUnsupportedVersion) {
  quic::ParsedQuicVersion unsupported_version =
      quic::ParsedQuicVersion::Unsupported();
  // Add support for another QUIC version besides |version_|. Also find an
  // unsupported version.
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (version == version_) {
      continue;
    }
    if (supported_versions_.size() != 2) {
      supported_versions_.push_back(version);
      continue;
    }
    unsupported_version = version;
    break;
  }
  ASSERT_EQ(2u, supported_versions_.size());
  ASSERT_NE(quic::ParsedQuicVersion::Unsupported(), unsupported_version);

  // Set up alternative service to use QUIC with a version that is not
  // supported.
  url::SchemeHostPort server(request_.url);
  AlternativeService alternative_service(kProtoQUIC, kDefaultServerHostName,
                                         443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      {unsupported_version});

  AlternativeServiceInfoVector alt_svc_info_vector =
      http_server_properties_->GetAlternativeServiceInfos(
          server, NetworkAnonymizationKey());
  EXPECT_EQ(1u, alt_svc_info_vector.size());
  EXPECT_EQ(kProtoQUIC, alt_svc_info_vector[0].alternative_service().protocol);
  EXPECT_EQ(1u, alt_svc_info_vector[0].advertised_versions().size());
  EXPECT_EQ(unsupported_version,
            alt_svc_info_vector[0].advertised_versions()[0]);

  // First request should still be sent via TCP as the QUIC version advertised
  // in the stored AlternativeService is not supported by the client. However,
  // the response from the server will advertise new Alt-Svc with supported
  // versions.
  std::string altsvc_header = GenerateQuicAltSvcHeader(supported_versions_);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(altsvc_header.c_str()),
      MockRead("\r\n"),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Second request should be sent via QUIC as a new list of verions supported
  // by the client has been advertised by the server.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();

  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Check alternative service list is updated with new versions.
  alt_svc_info_vector =
      session_->http_server_properties()->GetAlternativeServiceInfos(
          server, NetworkAnonymizationKey());
  VerifyQuicVersionsInAlternativeServices(alt_svc_info_vector,
                                          supported_versions_);
}

// Regression test for https://crbug.com/546991.
// The server might not be able to serve a request on an alternative connection,
// and might send a 421 Misdirected Request response status to indicate this.
// HttpNetworkTransaction should reset the request and retry without using
// alternative services.
TEST_P(QuicNetworkTransactionTest, RetryMisdirectedRequest) {
  // Set up alternative service to use QUIC.
  // Note that |origins_to_force_quic_on| cannot be used in this test, because
  // that overrides |enable_alternative_services|.
  url::SchemeHostPort server(request_.url);
  AlternativeService alternative_service(kProtoQUIC, kDefaultServerHostName,
                                         443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      supported_versions_);

  // First try: The alternative job uses QUIC and reports an HTTP 421
  // Misdirected Request error.  The main job uses TCP, but |http_data| below is
  // paused at Connect(), so it will never exit the socket pool. This ensures
  // that the alternate job always wins the race and keeps whether the
  // |http_data| exits the socket pool before the main job is aborted
  // deterministic. The first main job gets aborted without the socket pool ever
  // dispensing the socket, making it available for the second try.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 GetResponseHeaders("421")));
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // Second try: The main job uses TCP, and there is no alternate job. Once the
  // Connect() is unblocked, |http_data| will leave the socket pool, binding to
  // the main job of the second request. It then succeeds over HTTP/1.1.
  // Note that if there was an alternative QUIC Job created for the second try,
  // that would read these data, and would fail with ERR_QUIC_PROTOCOL_ERROR.
  // Therefore this test ensures that no alternative Job is created on retry.
  MockWrite writes[] = {MockWrite(ASYNC, 0, "GET / HTTP/1.1\r\n"),
                        MockWrite(ASYNC, 1, "Host: mail.example.org\r\n"),
                        MockWrite(ASYNC, 2, "Connection: keep-alive\r\n\r\n")};
  MockRead reads[] = {MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n\r\n"),
                      MockRead(ASYNC, 4, kHttpRespData),
                      MockRead(ASYNC, OK, 5)};
  SequencedSocketData http_data(MockConnect(ASYNC, ERR_IO_PENDING) /* pause */,
                                reads, writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());

  // Run until |mock_quic_data| has failed and |http_data| has paused.
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();

  // |mock_quic_data| must have run to completion.
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());

  // Now that the QUIC data has been consumed, unblock |http_data|.
  http_data.socket()->OnConnectComplete(MockConnect());

  // The retry logic must hide the 421 status. The transaction succeeds on
  // |http_data|.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckWasHttpResponse(&trans);
  CheckResponsePort(&trans, 443);
  CheckResponseData(&trans, kHttpRespData);
}

TEST_P(QuicNetworkTransactionTest, ForceQuicWithErrorConnecting) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data1(version_);
  mock_quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  mock_quic_data1.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  client_maker_->Reset();
  MockQuicData mock_quic_data2(version_);
  mock_quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  mock_quic_data2.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);

  mock_quic_data1.AddSocketDataToFactory(&socket_factory_);
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  EXPECT_EQ(0U, test_socket_performance_watcher_factory_.watcher_count());
  for (size_t i = 0; i < 2; ++i) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    TestCompletionCallback callback;
    int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
    EXPECT_EQ(1 + i, test_socket_performance_watcher_factory_.watcher_count());

    NetErrorDetails details;
    trans.PopulateNetErrorDetails(&details);
    EXPECT_EQ(quic::QUIC_PACKET_READ_ERROR, details.quic_connection_error);
  }
}

TEST_P(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("www.google.com:443"));

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  EXPECT_EQ(0U, test_socket_performance_watcher_factory_.watcher_count());
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceForQuic) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, UseIetfAlternativeServiceForQuic) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  std::string alt_svc_header =
      "Alt-Svc: " + quic::AlpnForVersion(version_) + "=\":443\"\r\n\r\n";
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

// Much like above, but makes sure NetworkAnonymizationKey is respected.
TEST_P(QuicNetworkTransactionTest,
       UseAlternativeServiceForQuicWithNetworkAnonymizationKey) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);

  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  AddCertificate(&ssl_data_);

  // Request with empty NetworkAnonymizationKey.
  StaticSocketDataProvider http_data1(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data1);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // First request with kNetworkIsolationKey1.
  StaticSocketDataProvider http_data2(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Request with kNetworkIsolationKey2.
  StaticSocketDataProvider http_data3(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data3);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Second request with kNetworkIsolationKey1, can finally use QUIC, since
  // alternative service infrmation has been received in this context before.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  // This is first so that the test fails if alternative service info is
  // written with the right NetworkAnonymizationKey, but always queried with an
  // empty one.
  request_.network_isolation_key = NetworkIsolationKey();
  request_.network_anonymization_key = NetworkAnonymizationKey();
  SendRequestAndExpectHttpResponse(kHttpRespData);
  request_.network_isolation_key = kNetworkIsolationKey1;
  request_.network_anonymization_key = kNetworkAnonymizationKey1;
  SendRequestAndExpectHttpResponse(kHttpRespData);
  request_.network_isolation_key = kNetworkIsolationKey2;
  request_.network_anonymization_key = kNetworkAnonymizationKey2;
  SendRequestAndExpectHttpResponse(kHttpRespData);

  // Only use QUIC when using a NetworkAnonymizationKey which has been used when
  // alternative service information was received.
  request_.network_isolation_key = kNetworkIsolationKey1;
  request_.network_anonymization_key = kNetworkAnonymizationKey1;
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceWithVersionForQuic1) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  // Both client and server supports two QUIC versions:
  // Client supports |supported_versions_[0]| and |supported_versions_[1]|,
  // server supports |version_| and |advertised_version_2|.
  // Only |version_| (same as |supported_versions_[0]|) is supported by both.
  // The QuicStreamFactoy will pick up |version_|, which is verified as the
  // PacketMakers are using |version_|.

  // Compare ALPN strings instead of ParsedQuicVersions because QUIC v1 and v2
  // have the same ALPN string.
  ASSERT_EQ(1u, supported_versions_.size());
  ASSERT_EQ(supported_versions_[0], version_);
  quic::ParsedQuicVersion advertised_version_2 =
      quic::ParsedQuicVersion::Unsupported();
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (quic::AlpnForVersion(version) == quic::AlpnForVersion(version_)) {
      continue;
    }
    if (supported_versions_.size() != 2) {
      supported_versions_.push_back(version);
      continue;
    }
    if (supported_versions_.size() == 2 &&
        quic::AlpnForVersion(supported_versions_[1]) ==
            quic::AlpnForVersion(version)) {
      continue;
    }
    advertised_version_2 = version;
    break;
  }
  ASSERT_EQ(2u, supported_versions_.size());
  ASSERT_NE(quic::ParsedQuicVersion::Unsupported(), advertised_version_2);

  std::string QuicAltSvcWithVersionHeader =
      base::StringPrintf("Alt-Svc: %s=\":443\", %s=\":443\"\r\n\r\n",
                         quic::AlpnForVersion(advertised_version_2).c_str(),
                         quic::AlpnForVersion(version_).c_str());

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(QuicAltSvcWithVersionHeader.c_str()), MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest,
       PickQuicVersionWhenMultipleVersionsAreSupported) {
  // Client and server both support more than one QUIC_VERSION.
  // Client prefers common_version_2, and then |version_|.
  // Server prefers |version_| common_version_2.
  // We should honor the server's preference.
  // The picked version is verified via checking the version used by the
  // TestPacketMakers and the response.
  // Since Chrome only supports one ALPN-negotiated version, common_version_2
  // will be another version that the common library supports even though
  // Chrome may consider it obsolete.

  // Find an alternative commonly supported version other than |version_|.
  quic::ParsedQuicVersion common_version_2 =
      quic::ParsedQuicVersion::Unsupported();
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (version != version_ && !version.AlpnDeferToRFCv1()) {
      common_version_2 = version;
      break;
    }
  }
  ASSERT_NE(common_version_2, quic::ParsedQuicVersion::Unsupported());

  // Setting up client's preference list: {|version_|, |common_version_2|}.
  supported_versions_.clear();
  supported_versions_.push_back(common_version_2);
  supported_versions_.push_back(version_);

  // Setting up server's Alt-Svc header in the following preference order:
  // |version_|, |common_version_2|.
  std::string QuicAltSvcWithVersionHeader;
  quic::ParsedQuicVersion picked_version =
      quic::ParsedQuicVersion::Unsupported();
  QuicAltSvcWithVersionHeader =
      "Alt-Svc: " + quic::AlpnForVersion(version_) + "=\":443\"; ma=3600, " +
      quic::AlpnForVersion(common_version_2) + "=\":443\"; ma=3600\r\n\r\n";
  picked_version = version_;  // Use server's preference.

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(QuicAltSvcWithVersionHeader.c_str()), MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(picked_version);

  // Reset QuicTestPacket makers as the version picked may not be |version_|.
  client_maker_ = std::make_unique<QuicTestPacketMaker>(
      picked_version,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true, /*use_priority_header=*/true);
  QuicTestPacketMaker server_maker(
      picked_version,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/false);

  int packet_num = 1;
  if (VersionUsesHttp3(picked_version.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }

  quic::QuicStreamId client_stream_0 =
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          picked_version.transport_version, 0);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientRequestHeadersPacket(
                              packet_num++, client_stream_0, true,
                              GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ASYNC,
                         server_maker.MakeResponseHeadersPacket(
                             1, client_stream_0, false,
                             server_maker.GetResponseHeaders("200"), nullptr));
  mock_quic_data.AddRead(
      ASYNC, server_maker.Packet(2)
                 .AddStreamFrame(client_stream_0, true,
                                 ConstructDataFrameForVersion(kQuicRespData,
                                                              picked_version))
                 .Build());
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponseMaybeFromProxy(
      kQuicRespData, 443, kQuic200RespStatusLine, picked_version, std::nullopt);
}

TEST_P(QuicNetworkTransactionTest, SetAlternativeServiceWithScheme) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  std::string alt_svc_header = base::StrCat(
      {"Alt-Svc: ",
       GenerateQuicAltSvcHeaderValue({version_}, "foo.example.com", 443), ",",
       GenerateQuicAltSvcHeaderValue({version_}, 444), "\r\n\r\n"});
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  // Send https request, ignore alternative service advertising if response
  // header advertises alternative service for mail.example.org.
  request_.url = GURL("https://mail.example.org:443");
  SendRequestAndExpectHttpResponse(kHttpRespData);
  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  url::SchemeHostPort http_server("http", "mail.example.org", 443);
  url::SchemeHostPort https_server("https", "mail.example.org", 443);
  // Check alternative service is set for the correct origin.
  EXPECT_EQ(2u, http_server_properties
                    ->GetAlternativeServiceInfos(https_server,
                                                 NetworkAnonymizationKey())
                    .size());
  EXPECT_TRUE(
      http_server_properties
          ->GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .empty());
}

TEST_P(QuicNetworkTransactionTest, DoNotGetAltSvcForDifferentOrigin) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  std::string alt_svc_header = base::StrCat(
      {"Alt-Svc: ",
       GenerateQuicAltSvcHeaderValue({version_}, "foo.example.com", 443), ",",
       GenerateQuicAltSvcHeaderValue({version_}, 444), "\r\n\r\n"});
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  AddCertificate(&ssl_data_);

  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  // Send https request and set alternative services if response header
  // advertises alternative service for mail.example.org.
  SendRequestAndExpectHttpResponse(kHttpRespData);
  HttpServerProperties* http_server_properties =
      session_->http_server_properties();

  const url::SchemeHostPort https_server(request_.url);
  // Check alternative service is set.
  EXPECT_EQ(2u, http_server_properties
                    ->GetAlternativeServiceInfos(https_server,
                                                 NetworkAnonymizationKey())
                    .size());

  // Send http request to the same origin but with diffrent scheme, should not
  // use QUIC.
  request_.url = GURL("http://mail.example.org:443");
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

TEST_P(QuicNetworkTransactionTest,
       StoreMutuallySupportedVersionsWhenProcessAltSvc) {
  // Add support for another QUIC version besides |version_|.
  for (const quic::ParsedQuicVersion& version : AllSupportedQuicVersions()) {
    if (version != version_) {
      supported_versions_.push_back(version);
      break;
    }
  }

  std::string altsvc_header = GenerateQuicAltSvcHeader(supported_versions_);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(altsvc_header.c_str()),
      MockRead("\r\n"),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();

  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Alt-Svc header contains all possible versions, so alternative services
  // should contain all of |supported_versions_|.
  const url::SchemeHostPort https_server(request_.url);
  const AlternativeServiceInfoVector alt_svc_info_vector =
      session_->http_server_properties()->GetAlternativeServiceInfos(
          https_server, NetworkAnonymizationKey());
  VerifyQuicVersionsInAlternativeServices(alt_svc_info_vector,
                                          supported_versions_);
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceAllSupportedVersion) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  std::string altsvc_header = base::StringPrintf(
      "Alt-Svc: %s=\":443\"\r\n\r\n", quic::AlpnForVersion(version_).c_str());
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(altsvc_header.c_str()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

// Verify that if a QUIC connection times out, the QuicHttpStream will
// return QUIC_PROTOCOL_ERROR.
TEST_P(QuicNetworkTransactionTest, TimeoutAfterHandshakeConfirmed) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->idle_connection_timeout = base::Seconds(5);
  // Turn off port migration to avoid dealing with unnecessary complexity in
  // this test.
  context_.params()->allow_port_migration = false;

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  client_maker_->set_save_packet_frames(true);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          priority, GetRequestHeaders("GET", "https", "/"), nullptr));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  // QuicConnection::OnRetransmissionTimeout skips a packet number when
  // sending PTO packets.
  packet_num++;
  // PTO 1
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeRetransmissionPacket(1, packet_num++));
  // QuicConnection::OnRetransmissionTimeout skips a packet number when
  // sending PTO packets.
  packet_num++;
  // PTO 2
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeRetransmissionPacket(2, packet_num++));
  // QuicConnection::OnRetransmissionTimeout skips a packet number when
  // sending PTO packets.
  packet_num++;
  // PTO 3
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeRetransmissionPacket(1, packet_num++));

  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->Packet(packet_num++)
                         .AddConnectionCloseFrame(
                             quic::QUIC_NETWORK_IDLE_TIMEOUT,
                             "No recent network activity after 4s. Timeout:4s")
                         .Build());

  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  // Run the QUIC session to completion.
  quic_task_runner_->RunUntilIdle();

  ExpectQuicAlternateProtocolMapping();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

// TODO(fayang): Add time driven TOO_MANY_RTOS test.

// Verify that if a QUIC protocol error occurs after the handshake is confirmed
// the request fails with QUIC_PROTOCOL_ERROR.
TEST_P(QuicNetworkTransactionTest, ProtocolErrorAfterHandshakeConfirmed) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(ASYNC,
                    ConstructServerRstPacket(
                        1, GetNthClientInitiatedBidirectionalStreamId(47),
                        quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(
          packet_num++, 1, 1, quic::QUIC_HTTP_STREAM_WRONG_DIRECTION,
          quic_error_details, quic::IETF_STOP_SENDING));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  ASSERT_FALSE(quic_data.AllReadDataConsumed());
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_TRUE(quic_data.AllReadDataConsumed());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  ExpectQuicAlternateProtocolMapping();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// connection times out, then QUIC will be marked as broken and the request
// retried over TCP.
TEST_P(QuicNetworkTransactionTest, TimeoutAfterHandshakeConfirmedThenBroken2) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  context_.params()->idle_connection_timeout = base::Seconds(5);
  // Turn off port migration to avoid dealing with unnecessary complexity in
  // this test.
  context_.params()->allow_port_migration = false;

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  client_maker_->set_save_packet_frames(true);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          priority, GetRequestHeaders("GET", "https", "/"), nullptr));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  // QuicConnection::OnRetransmissionTimeout skips a packet number when
  // sending PTO packets.
  packet_num++;
  // PTO 1
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeRetransmissionPacket(1, packet_num++));

  // QuicConnection::OnRetransmissionTimeout skips a packet number when
  // sending PTO packets.
  packet_num++;
  // PTO 2
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeRetransmissionPacket(2, packet_num++));

  // QuicConnection::OnRetransmissionTimeout skips a packet number when
  // sending PTO packets.
  packet_num++;
  // PTO 3
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeRetransmissionPacket(1, packet_num++));

  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->Packet(packet_num++)
                         .AddConnectionCloseFrame(
                             quic::QUIC_NETWORK_IDLE_TIMEOUT,
                             "No recent network activity after 4s. Timeout:4s")
                         .Build());

  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  // Run the QUIC session to completion.
  quic_task_runner_->RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, kHttpRespData);
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// protocol error occurs after the handshake is confirmed, the request
// retried over TCP and the QUIC will be marked as broken.
TEST_P(QuicNetworkTransactionTest,
       ProtocolErrorAfterHandshakeConfirmedThenBroken) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  context_.params()->idle_connection_timeout = base::Seconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(ASYNC,
                    ConstructServerRstPacket(
                        1, GetNthClientInitiatedBidirectionalStreamId(47),
                        quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(
          packet_num++, 1, 1, quic::QUIC_HTTP_STREAM_WRONG_DIRECTION,
          quic_error_details, quic::IETF_STOP_SENDING));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, kHttpRespData);
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Much like above test, but verifies that NetworkAnonymizationKey is respected.
TEST_P(QuicNetworkTransactionTest,
       ProtocolErrorAfterHandshakeConfirmedThenBrokenWithNetworkIsolationKey) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  context_.params()->idle_connection_timeout = base::Seconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  uint64_t packet_number = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructInitialSettingsPacket(packet_number++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(ASYNC,
                    ConstructServerRstPacket(
                        1, GetNthClientInitiatedBidirectionalStreamId(47),
                        quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic::QuicErrorCode quic_error_code = quic::QUIC_INVALID_STREAM_ID;
  quic_error_code = quic::QUIC_HTTP_STREAM_WRONG_DIRECTION;
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructClientAckAndConnectionClosePacket(
                         packet_number++, 1, 1, quic_error_code,
                         quic_error_details, quic::IETF_STOP_SENDING));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkAnonymizationKey1);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkAnonymizationKey2);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  request_.network_isolation_key = kNetworkIsolationKey1;
  request_.network_anonymization_key = kNetworkAnonymizationKey1;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, kHttpRespData);
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());

  // The alternative service shouldhave been marked as broken under
  // kNetworkIsolationKey1 but not kNetworkIsolationKey2.
  ExpectBrokenAlternateProtocolMapping(kNetworkAnonymizationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkAnonymizationKey2);

  // Subsequent requests using kNetworkIsolationKey1 should not use QUIC.
  AddHttpDataAndRunRequest();
  // Requests using other NetworkIsolationKeys can still use QUIC.
  request_.network_isolation_key = kNetworkIsolationKey2;
  request_.network_anonymization_key = kNetworkAnonymizationKey2;

  AddQuicDataAndRunRequest();

  // The last two requests should not have changed the alternative service
  // mappings.
  ExpectBrokenAlternateProtocolMapping(kNetworkAnonymizationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkAnonymizationKey2);
}

TEST_P(QuicNetworkTransactionTest,
       ProtocolErrorAfterHandshakeConfirmedThenBrokenWithUseDnsHttpsSvcbAlpn) {
  session_params_.use_dns_https_svcb_alpn = true;
  context_.params()->idle_connection_timeout = base::Seconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(ASYNC,
                    ConstructServerRstPacket(
                        1, GetNthClientInitiatedBidirectionalStreamId(47),
                        quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(
          packet_num++, 1, 1, quic::QUIC_HTTP_STREAM_WRONG_DIRECTION,
          quic_error_details, quic::IETF_STOP_SENDING));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  HostResolverEndpointResult endpoint_result1;
  endpoint_result1.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoint_result1.metadata.supported_protocol_alpns = {
      quic::QuicVersionLabelToString(quic::CreateQuicVersionLabel(version_))};
  HostResolverEndpointResult endpoint_result2;
  endpoint_result2.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.push_back(endpoint_result1);
  endpoints.push_back(endpoint_result2);
  host_resolver_.rules()->AddRule(
      "mail.example.org",
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{"mail.example.org"}));

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, kHttpRespData);
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// request is reset from, then QUIC will be marked as broken and the request
// retried over TCP.
TEST_P(QuicNetworkTransactionTest, ResetAfterHandshakeConfirmedThenBroken) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          priority, GetRequestHeaders("GET", "https", "/"), nullptr));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  quic_data.AddRead(ASYNC, ConstructServerRstPacket(
                               1, GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_HEADERS_TOO_LARGE));

  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddAckFrame(/*first_received=*/1, /*largest_received=*/1,
                       /*smallest_received=*/1)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_HEADERS_TOO_LARGE)
          .Build());

  quic_data.AddRead(ASYNC, OK);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, kHttpRespData);
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Verify that when an origin has two alt-svc advertisements, one local and one
// remote, that when the local is broken the request will go over QUIC via
// the remote Alt-Svc.
// This is a regression test for crbug/825646.
TEST_P(QuicNetworkTransactionTest, RemoteAltSvcWorkingWhileLocalAltSvcBroken) {
  context_.params()->allow_remote_alt_svc = true;

  GURL origin1 = request_.url;  // mail.example.org
  GURL origin2("https://www.example.org/");
  ASSERT_NE(origin1.host(), origin2.host());

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch("www.example.org"));
  ASSERT_TRUE(cert->VerifyNameMatch("mail.example.org"));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  // Set up alternative service for |origin1|.
  AlternativeService local_alternative(kProtoQUIC, "mail.example.org", 443);
  AlternativeService remote_alternative(kProtoQUIC, "www.example.org", 443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  AlternativeServiceInfoVector alternative_services;
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          local_alternative, expiration,
          context_.params()->supported_versions));
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          remote_alternative, expiration,
          context_.params()->supported_versions));
  http_server_properties_->SetAlternativeServices(url::SchemeHostPort(origin1),
                                                  NetworkAnonymizationKey(),
                                                  alternative_services);

  http_server_properties_->MarkAlternativeServiceBroken(
      local_alternative, NetworkAnonymizationKey());

  SendRequestAndExpectQuicResponse(kQuicRespData);
}

// Verify that when multiple alternatives are broken,
// ALTERNATE_PROTOCOL_USAGE_BROKEN is only logged once.
// This is a regression test for crbug/1024613.
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeOnlyRecordedOnce) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  base::HistogramTester histogram_tester;

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  GURL origin1 = request_.url;  // mail.example.org

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch("mail.example.org"));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  CreateSession();

  // Set up alternative service for |origin1|.
  AlternativeService local_alternative(kProtoQUIC, "mail.example.org", 443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  AlternativeServiceInfoVector alternative_services;
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          local_alternative, expiration,
          context_.params()->supported_versions));
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          local_alternative, expiration,
          context_.params()->supported_versions));
  http_server_properties_->SetAlternativeServices(url::SchemeHostPort(origin1),
                                                  NetworkAnonymizationKey(),
                                                  alternative_services);

  http_server_properties_->MarkAlternativeServiceBroken(
      local_alternative, NetworkAnonymizationKey());

  SendRequestAndExpectHttpResponse(kHttpRespData);

  histogram_tester.ExpectBucketCount("Net.AlternateProtocolUsage",
                                     ALTERNATE_PROTOCOL_USAGE_BROKEN, 1);
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// request is reset from, then QUIC will be marked as broken and the request
// retried over TCP. Then, subsequent requests will go over a new TCP
// connection instead of going back to the broken QUIC connection.
// This is a regression tests for crbug/731303.
TEST_P(QuicNetworkTransactionTest,
       ResetPooledAfterHandshakeConfirmedThenBroken) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  context_.params()->allow_remote_alt_svc = true;

  GURL origin1 = request_.url;
  GURL origin2("https://www.example.org/");
  ASSERT_NE(origin1.host(), origin2.host());

  MockQuicData mock_quic_data(version_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch("www.example.org"));
  ASSERT_TRUE(cert->VerifyNameMatch("mail.example.org"));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  // First request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));

  // Second request will go over the pooled QUIC connection, but will be
  // reset by the server.
  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_CLIENT, true);
  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_SERVER, false);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/", &client_maker2)));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerRstPacket(3, GetNthClientInitiatedBidirectionalStreamId(1),
                               quic::QUIC_HEADERS_TOO_LARGE));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddAckFrame(/*first_received=*/1, /*largest_received=*/3,
                       /*smallest_received=*/2)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                             quic::QUIC_HEADERS_TOO_LARGE)
          .Build());

  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: www.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Then the next request to the second origin will be sent over TCP.
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  // Set up alternative service for |origin1|.
  base::Time expiration = base::Time::Now() + base::Days(1);
  AlternativeService alternative1(kProtoQUIC, origin1.host(), 443);
  http_server_properties_->SetQuicAlternativeService(
      url::SchemeHostPort(origin1), NetworkAnonymizationKey(), alternative1,
      expiration, supported_versions_);

  // Set up alternative service for |origin2|.
  AlternativeService alternative2(kProtoQUIC, origin2.host(), 443);
  http_server_properties_->SetQuicAlternativeService(
      url::SchemeHostPort(origin2), NetworkAnonymizationKey(), alternative2,
      expiration, supported_versions_);

  // First request opens connection to `kDestination1`
  // with quic::QuicServerId.host() == origin1.host().
  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Second request pools to existing connection with same destination,
  // because certificate matches, even though quic::QuicServerId is different.
  // After it is reset, it will fail back to TCP and mark QUIC as broken.
  request_.url = origin2;
  SendRequestAndExpectHttpResponse(kHttpRespData);
  EXPECT_FALSE(http_server_properties_->IsAlternativeServiceBroken(
      alternative1, NetworkAnonymizationKey()))
      << alternative1.ToString();
  EXPECT_TRUE(http_server_properties_->IsAlternativeServiceBroken(
      alternative2, NetworkAnonymizationKey()))
      << alternative2.ToString();

  // The third request should use a new TCP connection, not the broken
  // QUIC connection.
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

TEST_P(QuicNetworkTransactionTest,
       DoNotUseAlternativeServiceQuicUnsupportedVersion) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  std::string altsvc_header =
      base::StringPrintf("Alt-Svc: quic=\":443\"; v=\"%u\"\r\n\r\n",
                         version_.transport_version - 1);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(altsvc_header.c_str()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

// When multiple alternative services are advertised, HttpStreamFactory should
// select the alternative service which uses existing QUIC session if available.
// If no existing QUIC session can be used, use the first alternative service
// from the list.
TEST_P(QuicNetworkTransactionTest, UseExistingAlternativeServiceForQuic) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  context_.params()->allow_remote_alt_svc = true;
  std::string alt_svc_header = base::StrCat(
      {"Alt-Svc: ",
       GenerateQuicAltSvcHeaderValue({version_}, "foo.example.org", 443), ",",
       GenerateQuicAltSvcHeaderValue({version_}, 444), "\r\n\r\n"});
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // First QUIC request data.
  // Open a session to foo.example.org:443 using the first entry of the
  // alternative service list.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));

  std::string alt_svc_list = base::StrCat(
      {GenerateQuicAltSvcHeaderValue({version_}, "mail.example.org", 444), ",",
       GenerateQuicAltSvcHeaderValue({version_}, "foo.example.org", 443), ",",
       GenerateQuicAltSvcHeaderValue({version_}, "bar.example.org", 445)});
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200", alt_svc_list)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));

  // Second QUIC request data.
  // Connection pooling, using existing session, no need to include version
  // as version negotiation has been completed.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  SendRequestAndExpectHttpResponse(kHttpRespData);

  SendRequestAndExpectQuicResponse(kQuicRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

// Pool to existing session with matching quic::QuicServerId
// even if alternative service destination is different.
TEST_P(QuicNetworkTransactionTest, PoolByOrigin) {
  context_.params()->allow_remote_alt_svc = true;
  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  // First request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));

  // Second request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  const char kDestination1[] = "first.example.com";
  const char kDestination2[] = "second.example.com";

  // Set up alternative service entry to `kDestination1`.
  url::SchemeHostPort server(request_.url);
  AlternativeService alternative_service(kProtoQUIC, kDestination1, 443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      supported_versions_);
  // First request opens connection to `kDestination1`
  // with quic::QuicServerId.host() == kDefaultServerHostName.
  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Set up alternative service entry to a different destination.
  alternative_service = AlternativeService(kProtoQUIC, kDestination2, 443);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      supported_versions_);
  // Second request pools to existing connection with same quic::QuicServerId,
  // even though alternative service destination is different.
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

// Pool to existing session with matching destination and matching certificate
// even if origin is different, and even if the alternative service with
// matching destination is not the first one on the list.
TEST_P(QuicNetworkTransactionTest, PoolByDestination) {
  context_.params()->allow_remote_alt_svc = true;
  GURL origin1 = request_.url;
  GURL origin2("https://www.example.org/");
  ASSERT_NE(origin1.host(), origin2.host());

  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  // First request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));

  // Second request.
  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_CLIENT, true);
  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_SERVER, false);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/", &client_maker2)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  const char kDestination1[] = "first.example.com";
  const char kDestination2[] = "second.example.com";

  // Set up alternative service for |origin1|.
  AlternativeService alternative_service1(kProtoQUIC, kDestination1, 443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  http_server_properties_->SetQuicAlternativeService(
      url::SchemeHostPort(origin1), NetworkAnonymizationKey(),
      alternative_service1, expiration, supported_versions_);

  // Set up multiple alternative service entries for |origin2|,
  // the first one with a different destination as for |origin1|,
  // the second one with the same.  The second one should be used,
  // because the request can be pooled to that one.
  AlternativeService alternative_service2(kProtoQUIC, kDestination2, 443);
  AlternativeServiceInfoVector alternative_services;
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service2, expiration,
          context_.params()->supported_versions));
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service1, expiration,
          context_.params()->supported_versions));
  http_server_properties_->SetAlternativeServices(url::SchemeHostPort(origin2),
                                                  NetworkAnonymizationKey(),
                                                  alternative_services);
  // First request opens connection to `kDestination1`
  // with quic::QuicServerId.host() == origin1.host().
  SendRequestAndExpectQuicResponse(kQuicRespData);

  // Second request pools to existing connection with same destination,
  // because certificate matches, even though quic::QuicServerId is different.
  request_.url = origin2;

  SendRequestAndExpectQuicResponse(kQuicRespData);
}

// Multiple origins have listed the same alternative services. When there's a
// existing QUIC session opened by a request to other origin,
// if the cert is valid, should select this QUIC session to make the request
// if this is also the first existing QUIC session.
TEST_P(QuicNetworkTransactionTest,
       UseSharedExistingAlternativeServiceForQuicWithValidCert) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  context_.params()->allow_remote_alt_svc = true;
  // Default cert is valid for *.example.org

  // HTTP data for request to www.example.org.
  const char kWwwHttpRespData[] = "hello world from www.example.org";
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kWwwHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // HTTP data for request to mail.example.org.
  std::string alt_svc_header2 = base::StrCat(
      {"Alt-Svc: ", GenerateQuicAltSvcHeaderValue({version_}, 444), ",",
       GenerateQuicAltSvcHeaderValue({version_}, "www.example.org", 443),
       "\r\n\r\n"});
  const char kMailHttpRespData[] = "hello world from mail.example.org";
  MockRead http_reads2[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header2.data()),
      MockRead(kMailHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data2(http_reads2, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  QuicTestPacketMaker client_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), "mail.example.org", quic::Perspective::IS_CLIENT, true);
  server_maker_.set_hostname("www.example.org");
  client_maker_->set_hostname("www.example.org");
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  // First QUIC request data.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  const char kMailQuicRespData[] = "hello from mail QUIC!";
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kMailQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  // Second QUIC request data.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/", &client_maker)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), true,
                 ConstructDataFrame(kMailQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  // Send two HTTP requests, responses set up alt-svc lists for the origins.
  request_.url = GURL("https://www.example.org/");
  SendRequestAndExpectHttpResponse(kWwwHttpRespData);
  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponse(kMailHttpRespData);

  // Open a QUIC session to mail.example.org:443 when making request
  // to mail.example.org.
  request_.url = GURL("https://www.example.org/");
  SendRequestAndExpectQuicResponse(kMailQuicRespData);

  // Uses the existing QUIC session when making request to www.example.org.
  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectQuicResponse(kMailQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, AlternativeServiceDifferentPort) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  std::string alt_svc_header =
      base::StrCat({"Alt-Svc: ", GenerateQuicAltSvcHeaderValue({version_}, 137),
                    "\r\n\r\n"});
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);

  url::SchemeHostPort http_server("https", kDefaultServerHostName, 443);
  AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_properties_->GetAlternativeServiceInfos(
          http_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  const AlternativeService alternative_service =
      alternative_service_info_vector[0].alternative_service();
  EXPECT_EQ(kProtoQUIC, alternative_service.protocol);
  EXPECT_EQ(kDefaultServerHostName, alternative_service.host);
  EXPECT_EQ(137, alternative_service.port);
}

TEST_P(QuicNetworkTransactionTest, ConfirmAlternativeService) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  AlternativeService alternative_service(kProtoQUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey());
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);

  mock_quic_data.Resume();

  EXPECT_FALSE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));
  EXPECT_NE(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         NetworkAnonymizationKey()));
}

TEST_P(QuicNetworkTransactionTest,
       ConfirmAlternativeServiceWithNetworkIsolationKey) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  AlternativeService alternative_service(kProtoQUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkAnonymizationKey1);
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkAnonymizationKey2);
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkAnonymizationKey1));
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkAnonymizationKey2));

  request_.network_isolation_key = kNetworkIsolationKey1;
  request_.network_anonymization_key = kNetworkAnonymizationKey1;
  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);

  mock_quic_data.Resume();

  EXPECT_FALSE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkAnonymizationKey1));
  EXPECT_NE(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         kNetworkAnonymizationKey1));
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkAnonymizationKey2));
  EXPECT_EQ(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         kNetworkAnonymizationKey2));
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceForQuicForHttps) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame("hello!")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

TEST_P(QuicNetworkTransactionTest, HungAlternativeService) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};

  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // The QUIC transaction will not be allowed to complete.
  MockWrite quic_writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  MockRead quic_reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };
  SequencedSocketData quic_data(quic_reads, quic_writes);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // The HTTP transaction will complete.
  SequencedSocketData http_data2(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  // Run the first request.
  SendRequestAndExpectHttpResponse(kHttpRespData);
  ASSERT_TRUE(http_data.AllReadDataConsumed());
  ASSERT_TRUE(http_data.AllWriteDataConsumed());

  // Now run the second request in which the QUIC socket hangs,
  // and verify the the transaction continues over HTTP.
  SendRequestAndExpectHttpResponse(kHttpRespData);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(http_data2.AllReadDataConsumed());
  ASSERT_TRUE(http_data2.AllWriteDataConsumed());
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithHttpRace) {
  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  EXPECT_THAT(trans.Start(&request_, callback.callback(), net_log_with_source_),
              IsError(ERR_IO_PENDING));
  // Complete host resolution in next message loop so that QUIC job could
  // proceed.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  ASSERT_FALSE(mock_quic_data.AllReadDataConsumed());
  mock_quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, kQuicRespData);

  EXPECT_EQ(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         NetworkAnonymizationKey()));
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithNoHttpRace) {
  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS, client_maker_->MakeInitialSettingsPacket(packet_number++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_number++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  EXPECT_THAT(trans.Start(&request_, callback.callback(), net_log_with_source_),
              IsError(ERR_IO_PENDING));
  // Complete host resolution in next message loop so that QUIC job could
  // proceed.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  ASSERT_FALSE(mock_quic_data.AllReadDataConsumed());
  mock_quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithProxy) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP,
                                             "myproxy", 70)},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  // Since we are using a proxy, the QUIC job will not succeed.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET http://mail.example.org/ HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Proxy-Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
                           MockRead(SYNCHRONOUS, 4, alt_svc_header_.data()),
                           MockRead(SYNCHRONOUS, 5, kHttpRespData),
                           MockRead(SYNCHRONOUS, OK, 6)};

  StaticSocketDataProvider http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  request_.url = GURL("http://mail.example.org/");
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithConfirmationRequired) {
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithTooEarlyResponse) {
  uint64_t packet_number = 1;
  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(
      SYNCHRONOUS, client_maker_->MakeInitialSettingsPacket(packet_number++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("425")));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckDataAndRst(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED, 1, 1, GetQpackDecoderStreamId(), false,
          StreamCancellationQpackDecoderInstruction(0)));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_number++, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Confirm the handshake after the 425 Too Early.
  base::RunLoop().RunUntilIdle();

  // The handshake hasn't been confirmed yet, so the retry should not have
  // succeeded.
  EXPECT_FALSE(callback.have_result());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithMultipleTooEarlyResponse) {
  uint64_t packet_number = 1;
  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(
      SYNCHRONOUS, client_maker_->MakeInitialSettingsPacket(packet_number++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("425")));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckDataAndRst(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED, 1, 1, GetQpackDecoderStreamId(), false,
          StreamCancellationQpackDecoderInstruction(0)));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(1), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("425")));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckDataAndRst(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(1),
          quic::QUIC_STREAM_CANCELLED, 2, 1, GetQpackDecoderStreamId(), false,
          StreamCancellationQpackDecoderInstruction(1, false)));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Confirm the handshake after the 425 Too Early.
  base::RunLoop().RunUntilIdle();

  // The handshake hasn't been confirmed yet, so the retry should not have
  // succeeded.
  EXPECT_FALSE(callback.have_result());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  const HttpResponseInfo* response = trans.GetResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_TRUE(response->headers.get() != nullptr);
  EXPECT_EQ("HTTP/1.1 425", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version_),
            response->connection_info);
}

TEST_P(QuicNetworkTransactionTest,
       LogGranularQuicErrorCodeOnQuicProtocolErrorLocal) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  // Read a close connection packet with
  // quic::QuicErrorCode: quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED from the peer.
  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  NetErrorDetails details;
  EXPECT_EQ(quic::QUIC_NO_ERROR, details.quic_connection_error);

  trans.PopulateNetErrorDetails(&details);
  // Verify the error code logged is what sent by the peer.
  EXPECT_EQ(quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED,
            details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest,
       LogGranularQuicErrorCodeOnQuicProtocolErrorRemote) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  mock_quic_data.AddRead(ASYNC,
                         ConstructServerRstPacket(
                             1, GetNthClientInitiatedBidirectionalStreamId(47),
                             quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(
          packet_num++, 1, 1, quic::QUIC_HTTP_STREAM_WRONG_DIRECTION,
          quic_error_details, quic::IETF_STOP_SENDING));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  NetErrorDetails details;
  EXPECT_EQ(quic::QUIC_NO_ERROR, details.quic_connection_error);

  trans.PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QUIC_HTTP_STREAM_WRONG_DIRECTION,
            details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest, RstStreamErrorHandling) {
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  // Read the response headers, then a RST_STREAM frame.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerRstPacket(2, GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddAckFrame(/*first_received=*/1, /*largest_received=*/2,
                       /*smallest_received=*/1)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Read the headers.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  const HttpResponseInfo* response = trans.GetResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_TRUE(response->headers.get() != nullptr);
  EXPECT_EQ(kQuic200RespStatusLine, response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version_),
            response->connection_info);

  std::string response_data;
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR, ReadTransaction(&trans, &response_data));
}

TEST_P(QuicNetworkTransactionTest, RstStreamBeforeHeaders) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerRstPacket(1, GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddAckFrame(/*first_received=*/1, /*largest_received=*/1,
                       /*smallest_received=*/1)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Read the headers.
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocol) {
  // Alternate-protocol job
  std::unique_ptr<quic::QuicEncryptedPacket> close(
      ConstructServerConnectionClosePacket(1));
  MockRead quic_reads[] = {
      MockRead(ASYNC, close->data(), close->length()),
      MockRead(ASYNC, ERR_IO_PENDING),  // No more data to read
      MockRead(ASYNC, OK),              // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest,
       BrokenAlternateProtocolWithNetworkIsolationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  // Alternate-protocol job
  std::unique_ptr<quic::QuicEncryptedPacket> close(
      ConstructServerConnectionClosePacket(1));
  MockRead quic_reads[] = {
      MockRead(ASYNC, close->data(), close->length()),
      MockRead(ASYNC, ERR_IO_PENDING),  // No more data to read
      MockRead(ASYNC, OK),              // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT,
      kNetworkAnonymizationKey1);
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT,
      kNetworkAnonymizationKey2);
  request_.network_isolation_key = kNetworkIsolationKey1;
  request_.network_anonymization_key = kNetworkAnonymizationKey1;
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping(kNetworkAnonymizationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkAnonymizationKey2);
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolReadError) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, NoBrokenAlternateProtocolIfTcpFails) {
  // Alternate-protocol job will fail when the session attempts to read.
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job will also fail.
  MockRead http_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_SOCKET_NOT_CONNECTED));
  ExpectQuicAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, DelayTCPOnStartWithQuicSupportOnSameIP) {
  // Tests that TCP job is delayed and QUIC job does not require confirmation
  // if QUIC was recently supported on the same IP on start.

  // Set QUIC support on the last IP address, which is same with the local IP
  // address. Require confirmation mode will be turned off immediately when
  // local IP address is sorted out after we configure the UDP socket.
  http_server_properties_->SetLastLocalAddressWhenQuicWorked(
      IPAddress(192, 0, 2, 33));

  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_number++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_number++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  // No HTTP data is mocked as TCP job never starts in this case.

  CreateSession();
  // QuicSessionPool by default requires confirmation on construction.
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  // Stall host resolution so that QUIC job will not succeed synchronously.
  // Socket will not be configured immediately and QUIC support is not sorted
  // out, TCP job will still be delayed as server properties indicates QUIC
  // support on last IP address.
  host_resolver_.set_synchronous_mode(false);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  EXPECT_THAT(trans.Start(&request_, callback.callback(), net_log_with_source_),
              IsError(ERR_IO_PENDING));
  // Complete host resolution in next message loop so that QUIC job could
  // proceed.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  ASSERT_FALSE(mock_quic_data.AllReadDataConsumed());
  mock_quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest,
       DelayTCPOnStartWithQuicSupportOnDifferentIP) {
  // Tests that TCP job is delayed and QUIC job requires confirmation if QUIC
  // was recently supported on a different IP address on start.

  // Set QUIC support on the last IP address, which is different with the local
  // IP address. Require confirmation mode will remain when local IP address is
  // sorted out after we configure the UDP socket.
  http_server_properties_->SetLastLocalAddressWhenQuicWorked(
      IPAddress(1, 2, 3, 4));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  // No HTTP data is mocked as TCP job will be delayed and never starts.

  CreateSession();
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  // Stall host resolution so that QUIC job could not proceed and unblocks TCP.
  // Socket will not be configured immediately and QUIC support is not sorted
  // out, TCP job will still be delayed as server properties indicates QUIC
  // support on last IP address.
  host_resolver_.set_synchronous_mode(false);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  EXPECT_THAT(trans.Start(&request_, callback.callback(), net_log_with_source_),
              IsError(ERR_IO_PENDING));

  // Complete host resolution in next message loop so that QUIC job could
  // proceed.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake so that QUIC job could succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, NetErrorDetailsSetBeforeHandshake) {
  // Test that NetErrorDetails is correctly populated, even if the
  // handshake has not yet been confirmed and no stream has been created.

  // QUIC job will pause. When resumed, it will fail.
  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // Main job will also fail.
  MockRead http_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  // Require handshake confirmation to ensure that no QUIC streams are
  // created, and to ensure that the TCP job does not wait for the QUIC
  // job to fail before it starts.
  session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
      false);

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Allow the TCP job to fail.
  base::RunLoop().RunUntilIdle();
  // Now let the QUIC job fail.
  mock_quic_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  ExpectQuicAlternateProtocolMapping();
  NetErrorDetails details;
  trans.PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QUIC_PACKET_READ_ERROR, details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest, FailedZeroRttBrokenAlternateProtocol) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest,
       FailedZeroRttBrokenAlternateProtocolWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkAnonymizationKey1);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkAnonymizationKey2);

  request_.network_isolation_key = kNetworkIsolationKey1;
  request_.network_anonymization_key = kNetworkAnonymizationKey1;
  SendRequestAndExpectHttpResponse("hello from http");
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectBrokenAlternateProtocolMapping(kNetworkAnonymizationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkAnonymizationKey2);

  // Subsequent requests using kNetworkIsolationKey1 should not use QUIC.
  AddHttpDataAndRunRequest();
  // Requests using other NetworkIsolationKeys can still use QUIC.
  request_.network_isolation_key = kNetworkIsolationKey2;
  request_.network_anonymization_key = kNetworkAnonymizationKey2;

  AddQuicDataAndRunRequest();

  // The last two requests should not have changed the alternative service
  // mappings.
  ExpectBrokenAlternateProtocolMapping(kNetworkAnonymizationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkAnonymizationKey2);
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolOnConnectFailure) {
  // Alternate-protocol job will fail before creating a QUIC session.
  StaticSocketDataProvider quic_data;
  quic_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnect) {
  FLAGS_quic_enable_chaos_protection = false;
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeDummyCHLOPacket(1));
  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  // TODO(rch): Check if we need a 0RTT version of ConnectionCloseDuringConnect
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnectProxy) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeDummyCHLOPacket(1));
  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(1));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                       1, GetNthClientInitiatedBidirectionalStreamId(0), true,
                       GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddWrite(SYNCHRONOUS, ConstructClientAckPacket(2, 1, 1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  const char kProxyHost[] = "myproxy.org";
  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, kProxyHost, 443)});
  const auto kHttpsProxyChain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTPS, kProxyHost, 443);
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain, kHttpsProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_.url = GURL("http://mail.example.org/");

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(kProxyHost, "192.168.0.1", "");

  CreateSession();
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  SendRequestAndExpectHttpResponseFromProxy(
      kHttpRespData, kHttpsProxyChain.First().GetPort(), kHttpsProxyChain);
  EXPECT_THAT(session_->proxy_resolution_service()->proxy_retry_info(),
              ElementsAre(Key(kQuicProxyChain)));
}

TEST_P(QuicNetworkTransactionTest, SecureResourceOverSecureQuic) {
  client_maker_->set_hostname("www.example.org");
  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  request_.url = GURL("https://www.example.org:443");
  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponse(kQuicRespData);
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

TEST_P(QuicNetworkTransactionTest, QuicUpload) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData(base::byte_span_from_cstring("1"), true);

  request_.upload_data_stream = &upload_data;

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_NE(OK, callback.WaitForResult());
}

TEST_P(QuicNetworkTransactionTest, QuicUploadWriteError) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  ScopedMockNetworkChangeNotifier network_change_notifier;
  MockNetworkChangeNotifier* mock_ncn =
      network_change_notifier.mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  context_.params()->migrate_sessions_on_network_change_v2 = true;

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          GetRequestHeaders("POST", "https", "/")));
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(&socket_factory_);

  MockQuicData socket_data2(version_);
  socket_data2.AddConnect(SYNCHRONOUS, ERR_ADDRESS_INVALID);
  socket_data2.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);

  request_.upload_data_stream = &upload_data;

  auto trans = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                        session_.get());
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();
  upload_data.AppendData(base::byte_span_from_cstring("1"), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(OK, callback.WaitForResult());
  trans.reset();
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, RetryAfterAsyncNoBufferSpace) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(ASYNC, ERR_NO_BUFFER_SPACE);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  socket_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  socket_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructClientAckPacket(packet_num++, 2, 1));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddAckFrame(/*first_received=*/1, /*largest_received=*/2,
                       /*smallest_received=*/1)
          .AddConnectionCloseFrame(quic::QUIC_CONNECTION_CANCELLED, "net error",
                                   0)
          .Build());

  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, RetryAfterSynchronousNoBufferSpace) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS, ERR_NO_BUFFER_SPACE);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  socket_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  socket_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructClientAckPacket(packet_num++, 2, 1));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddAckFrame(/*first_received=*/1, /*largest_received=*/2,
                       /*smallest_received=*/1)
          .AddConnectionCloseFrame(quic::QUIC_CONNECTION_CANCELLED, "net error",
                                   0)
          .Build());

  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse(kQuicRespData);
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, MaxRetriesAfterAsyncNoBufferSpace) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  for (int i = 0; i < 13; ++i) {  // 12 retries then one final failure.
    socket_data.AddWrite(ASYNC, ERR_NO_BUFFER_SPACE);
  }
  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicSessionPoolPeer::SetTaskRunner(session_->quic_session_pool(),
                                     quic_task_runner_.get());

  quic::QuicTime start = context_.clock()->Now();
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  while (!callback.have_result()) {
    base::RunLoop().RunUntilIdle();
    quic_task_runner_->RunUntilIdle();
  }
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  // Backoff should take between 4 - 5 seconds.
  EXPECT_TRUE(context_.clock()->Now() - start >
              quic::QuicTime::Delta::FromSeconds(4));
  EXPECT_TRUE(context_.clock()->Now() - start <
              quic::QuicTime::Delta::FromSeconds(5));
}

TEST_P(QuicNetworkTransactionTest, MaxRetriesAfterSynchronousNoBufferSpace) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  for (int i = 0; i < 13; ++i) {  // 12 retries then one final failure.
    socket_data.AddWrite(ASYNC, ERR_NO_BUFFER_SPACE);
  }
  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicSessionPoolPeer::SetTaskRunner(session_->quic_session_pool(),
                                     quic_task_runner_.get());

  quic::QuicTime start = context_.clock()->Now();
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  while (!callback.have_result()) {
    base::RunLoop().RunUntilIdle();
    quic_task_runner_->RunUntilIdle();
  }
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  // Backoff should take between 4 - 5 seconds.
  EXPECT_TRUE(context_.clock()->Now() - start >
              quic::QuicTime::Delta::FromSeconds(4));
  EXPECT_TRUE(context_.clock()->Now() - start <
              quic::QuicTime::Delta::FromSeconds(5));
}

TEST_P(QuicNetworkTransactionTest, NoMigrationForMsgTooBig) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  const std::string error_details = base::StrCat(
      {"Write failed with error: ", base::NumberToString(ERR_MSG_TOO_BIG), " (",
       strerror(ERR_MSG_TOO_BIG), ")"});

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  socket_data.AddWrite(SYNCHRONOUS, ERR_MSG_TOO_BIG);
  // Connection close packet will be sent for MSG_TOO_BIG.
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num + 1)
          .AddConnectionCloseFrame(quic::QUIC_PACKET_WRITE_ERROR, error_details)
          .Build());
  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, QuicForceHolBlocking) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);

  int write_packet_index = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_index++));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersAndDataFramesPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, DEFAULT_PRIORITY, GetRequestHeaders("POST", "https", "/"),
          nullptr, {ConstructDataFrame("1")}));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(write_packet_index++, 2, 1));

  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData(base::byte_span_from_cstring("1"), true);

  request_.upload_data_stream = &upload_data;

  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, HostInAllowlist) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  session_params_.quic_host_allowlist.insert("mail.example.org");

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, HostNotInAllowlist) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  session_params_.quic_host_allowlist.insert("mail.example.com");

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectHttpResponse(kHttpRespData);
}

class QuicNetworkTransactionWithDestinationTest
    : public PlatformTest,
      public ::testing::WithParamInterface<PoolingTestParams>,
      public WithTaskEnvironment {
 protected:
  QuicNetworkTransactionWithDestinationTest()
      : version_(GetParam().version),
        supported_versions_(quic::test::SupportedVersions(version_)),
        destination_type_(GetParam().destination_type),
        ssl_config_service_(std::make_unique<SSLConfigServiceDefaults>()),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        ssl_data_(ASYNC, OK) {
    FLAGS_quic_enable_http3_grease_randomness = false;
  }

  void SetUp() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();

    HttpNetworkSessionParams session_params;
    session_params.enable_quic = true;
    // To simplefy tests, we disable UseDnsHttpsSvcbAlpn feature. If this is
    // enabled, we need to prepare mock sockets for `dns_alpn_h3_job_`. Also
    // AsyncQuicSession feature makes it more complecated because it changes the
    // socket call order.
    session_params.use_dns_https_svcb_alpn = false;

    context_.params()->allow_remote_alt_svc = true;
    context_.params()->supported_versions = supported_versions_;

    HttpNetworkSessionContext session_context;

    context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));

    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);
    session_context.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;

    session_context.quic_context = &context_;
    session_context.client_socket_factory = &socket_factory_;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = &cert_verifier_;
    session_context.transport_security_state = &transport_security_state_;
    session_context.socket_performance_watcher_factory =
        &test_socket_performance_watcher_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.http_auth_handler_factory = auth_handler_factory_.get();
    session_context.http_server_properties = &http_server_properties_;

    session_ =
        std::make_unique<HttpNetworkSession>(session_params, session_context);
    session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
        false);
  }

  void TearDown() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::RunLoop().RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();
    session_.reset();
  }

  void SetQuicAlternativeService(const std::string& origin) {
    HostPortPair destination;
    switch (destination_type_) {
      case SAME_AS_FIRST:
        destination = HostPortPair(origin1_, 443);
        break;
      case SAME_AS_SECOND:
        destination = HostPortPair(origin2_, 443);
        break;
      case DIFFERENT:
        destination = HostPortPair(kDifferentHostname, 443);
        break;
    }
    AlternativeService alternative_service(kProtoQUIC, destination);
    base::Time expiration = base::Time::Now() + base::Days(1);
    http_server_properties_.SetQuicAlternativeService(
        url::SchemeHostPort("https", origin, 443), NetworkAnonymizationKey(),
        alternative_service, expiration, supported_versions_);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(uint64_t packet_number,
                                      quic::QuicStreamId stream_id,
                                      QuicTestPacketMaker* maker) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    quiche::HttpHeaderBlock headers(
        maker->GetRequestHeaders("GET", "https", "/"));
    return maker->MakeRequestHeadersPacket(
        packet_number, stream_id, true, priority, std::move(headers), nullptr);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerResponseHeadersPacket(uint64_t packet_number,
                                       quic::QuicStreamId stream_id,
                                       QuicTestPacketMaker* maker) {
    quiche::HttpHeaderBlock headers(maker->GetResponseHeaders("200"));
    return maker->MakeResponseHeadersPacket(packet_number, stream_id, false,
                                            std::move(headers), nullptr);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      QuicTestPacketMaker* maker) {
    return maker->Packet(packet_number)
        .AddStreamFrame(stream_id, true,
                        ConstructDataFrameForVersion("hello", version_))
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      QuicTestPacketMaker* maker) {
    return maker->Packet(packet_number)
        .AddAckFrame(1, largest_received, smallest_received)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number,
      QuicTestPacketMaker* maker) {
    return maker->MakeInitialSettingsPacket(packet_number);
  }

  void AddRefusedSocketData() {
    auto refused_data = std::make_unique<StaticSocketDataProvider>();
    MockConnect refused_connect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
    refused_data->set_connect_data(refused_connect);
    socket_factory_.AddSocketDataProvider(refused_data.get());
    static_socket_data_provider_vector_.push_back(std::move(refused_data));
  }

  void AddHangingSocketData() {
    auto hanging_data = std::make_unique<StaticSocketDataProvider>();
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data->set_connect_data(hanging_connect);
    socket_factory_.AddSocketDataProvider(hanging_data.get());
    static_socket_data_provider_vector_.push_back(std::move(hanging_data));
    socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  }

  bool AllDataConsumed() {
    for (const auto& socket_data_ptr : static_socket_data_provider_vector_) {
      if (!socket_data_ptr->AllReadDataConsumed() ||
          !socket_data_ptr->AllWriteDataConsumed()) {
        return false;
      }
    }
    return true;
  }

  void SendRequestAndExpectQuicResponse(const std::string& host) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    HttpRequestInfo request;
    std::string url("https://");
    url.append(host);
    request.url = GURL(url);
    request.load_flags = 0;
    request.method = "GET";
    request.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    TestCompletionCallback callback;
    int rv = trans.Start(&request, callback.callback(), net_log_with_source_);
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    std::string response_data;
    ASSERT_THAT(ReadTransaction(&trans, &response_data), IsOk());
    EXPECT_EQ("hello", response_data);

    const HttpResponseInfo* response = trans.GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ(kQuic200RespStatusLine, response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_alpn_negotiated);
    EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version_),
              response->connection_info);
    EXPECT_EQ(443, response->remote_endpoint.port());
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  const quic::ParsedQuicVersion version_;
  quic::ParsedQuicVersionVector supported_versions_;
  DestinationType destination_type_;
  std::string origin1_;
  std::string origin2_;
  MockQuicContext context_;
  std::unique_ptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver_{/*default_result=*/MockHostResolverBase::
                                      RuleResolver::GetLocalhostResult()};
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  TestSocketPerformanceWatcherFactory test_socket_performance_watcher_factory_;
  std::unique_ptr<SSLConfigServiceDefaults> ssl_config_service_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  HttpServerProperties http_server_properties_;
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::NONE)};
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  std::vector<std::unique_ptr<StaticSocketDataProvider>>
      static_socket_data_provider_vector_;
  SSLSocketDataProvider ssl_data_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicNetworkTransactionWithDestinationTest,
                         ::testing::ValuesIn(GetPoolingTestParams()),
                         ::testing::PrintToStringParamName());

// A single QUIC request fails because the certificate does not match the origin
// hostname, regardless of whether it matches the alternative service hostname.
TEST_P(QuicNetworkTransactionWithDestinationTest, InvalidCertificate) {
  if (destination_type_ == DIFFERENT) {
    return;
  }

  GURL url("https://mail.example.com/");
  origin1_ = url.host();

  // Not used for requests, but this provides a test case where the certificate
  // is valid for the hostname of the alternative service.
  origin2_ = "mail.example.org";

  SetQuicAlternativeService(origin1_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_FALSE(cert->VerifyNameMatch(origin1_));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddRefusedSocketData();

  HttpRequestInfo request;
  request.url = url;
  request.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request, callback.callback(), net_log_with_source_);
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(AllDataConsumed());
}

// First request opens QUIC session to alternative service.  Second request
// pools to it, because destination matches and certificate is valid, even
// though quic::QuicServerId is different.
TEST_P(QuicNetworkTransactionWithDestinationTest, PoolIfCertificateValid) {
  origin1_ = "mail.example.org";
  origin2_ = "news.example.org";

  SetQuicAlternativeService(origin1_);
  SetQuicAlternativeService(origin2_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicTestPacketMaker client_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true, /*use_priority_header=*/true);
  QuicTestPacketMaker server_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/false);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(packet_num++, &client_maker));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          &client_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(packet_num++, 2, 1, &client_maker));

  client_maker.set_hostname(origin2_);
  server_maker.set_hostname(origin2_);

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
          &client_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          3, GetNthClientInitiatedBidirectionalStreamId(1), &server_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          4, GetNthClientInitiatedBidirectionalStreamId(1), &server_maker));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(packet_num++, 4, 3, &client_maker));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingSocketData();
  AddHangingSocketData();

  auto quic_task_runner =
      base::MakeRefCounted<TestTaskRunner>(context_.mock_clock());
  QuicSessionPoolPeer::SetAlarmFactory(
      session_->quic_session_pool(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner.get(),
                                                 context_.clock()));

  SendRequestAndExpectQuicResponse(origin1_);
  SendRequestAndExpectQuicResponse(origin2_);

  EXPECT_TRUE(AllDataConsumed());
}

// First request opens QUIC session to alternative service.  Second request does
// not pool to it, even though destination matches, because certificate is not
// valid.  Instead, a new QUIC session is opened to the same destination with a
// different quic::QuicServerId.
TEST_P(QuicNetworkTransactionWithDestinationTest,
       DoNotPoolIfCertificateInvalid) {
  origin1_ = "news.example.org";
  origin2_ = "mail.example.com";

  SetQuicAlternativeService(origin1_);
  SetQuicAlternativeService(origin2_);

  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert1->VerifyNameMatch(origin1_));
  ASSERT_FALSE(cert1->VerifyNameMatch(origin2_));
  ASSERT_FALSE(cert1->VerifyNameMatch(kDifferentHostname));

  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert2->VerifyNameMatch(origin2_));
  ASSERT_FALSE(cert2->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert1;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert2;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  QuicTestPacketMaker client_maker1(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true, /*use_priority_header=*/true);
  QuicTestPacketMaker server_maker1(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/false);

  MockQuicData mock_quic_data1(version_);
  int packet_num = 1;
  mock_quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                            packet_num++, &client_maker1));
  mock_quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          &client_maker1));
  mock_quic_data1.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker1));
  mock_quic_data1.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker1));
  mock_quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(packet_num++, 2, 1, &client_maker1));
  mock_quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data1.AddRead(ASYNC, 0);               // EOF

  mock_quic_data1.AddSocketDataToFactory(&socket_factory_);

  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2_, quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true, /*use_priority_header=*/true);
  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2_, quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/false);

  MockQuicData mock_quic_data2(version_);
  int packet_num2 = 1;
  mock_quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                            packet_num2++, &client_maker2));
  mock_quic_data2.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num2++, GetNthClientInitiatedBidirectionalStreamId(0),
          &client_maker2));
  mock_quic_data2.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker2));
  mock_quic_data2.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker2));
  mock_quic_data2.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(packet_num2++, 2, 1, &client_maker2));
  mock_quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data2.AddRead(ASYNC, 0);               // EOF

  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  SendRequestAndExpectQuicResponse(origin1_);
  SendRequestAndExpectQuicResponse(origin2_);

  EXPECT_TRUE(AllDataConsumed());
}

// Performs an HTTPS/1.1 request over QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectHttpsServer) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  QuicSocketDataProvider socket_data(version_);
  int packet_num = 1;
  socket_data
      .AddWrite("initial-settings",
                ConstructInitialSettingsPacket(packet_num++))
      .Sync();
  socket_data
      .AddWrite("priority",
                ConstructClientPriorityPacket(
                    packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                    DEFAULT_PRIORITY))
      .Sync();
  socket_data
      .AddWrite("connect-request",
                ConstructClientRequestHeadersPacket(
                    packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                    false, DEFAULT_PRIORITY,
                    ConnectRequestHeaders("mail.example.org:443"), false))
      .Sync();
  socket_data.AddRead("connect-response",
                      ConstructServerResponseHeadersPacket(
                          1, GetNthClientInitiatedBidirectionalStreamId(0),
                          false, GetResponseHeaders("200")));

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  socket_data
      .AddWrite("get-request",
                ConstructClientAckAndDataPacket(
                    packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                    1, 1, false, ConstructDataFrame(kGetRequest)))
      .Sync();

  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kRespData[] = "0123456789";

  socket_data.AddRead("get-response",
                      ConstructServerDataPacket(
                          2, GetNthClientInitiatedBidirectionalStreamId(0),
                          false, ConstructDataFrame(kGetResponse)));

  socket_data
      .AddRead("response-data",
               ConstructServerDataPacket(
                   3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   ConstructDataFrame(kRespData)))
      .Sync();

  socket_data
      .AddWrite("response-ack", ConstructClientAckPacket(packet_num++, 3, 2))
      .Sync();

  socket_data
      .AddWrite(
          "qpack-cancel-rst",
          client_maker_->Packet(packet_num++)
              .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                              StreamCancellationQpackDecoderInstruction(0))
              .AddStopSendingFrame(
                  GetNthClientInitiatedBidirectionalStreamId(0),
                  quic::QUIC_STREAM_CANCELLED)
              .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                                 quic::QUIC_STREAM_CANCELLED)
              .Build())
      .Sync();

  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponseFromProxy(
      kRespData, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  socket_data.RunUntilAllConsumed();
}

// Performs an HTTP/2 request over QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectSpdyServer) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  SpdyTestUtil spdy_util(/*use_priority_header=*/true);

  spdy::SpdySerializedFrame get_frame =
      spdy_util.ConstructSpdyGet("https://mail.example.org/", 1, LOWEST);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), 1, 1,
          false, ConstructDataFrame({get_frame.data(), get_frame.size()})));
  spdy::SpdySerializedFrame resp_frame =
      spdy_util.ConstructSpdyGetReply(nullptr, 0, 1);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame({resp_frame.data(), resp_frame.size()})));

  const char kRespData[] = "0123456789";
  spdy::SpdySerializedFrame data_frame =
      spdy_util.ConstructSpdyDataFrame(1, kRespData, true);
  mock_quic_data.AddRead(
      SYNCHRONOUS,
      ConstructServerDataPacket(
          3, GetNthClientInitiatedBidirectionalStreamId(0), false,
          ConstructDataFrame({data_frame.data(), data_frame.size()})));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(0))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectSpdyResponseFromProxy(
      kRespData, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Performs an HTTP/3 request over QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectQuicServer) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  QuicTestPacketMaker to_endpoint_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), "mail.example.org", quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true,
      /*use_priority_header=*/true);
  QuicTestPacketMaker from_endpoint_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), "mail.example.org", quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/true);

  QuicSocketDataProvider socket_data(version_);
  const char kRespData[] = "0123456789";
  int to_proxy_packet_num = 1;
  int from_proxy_packet_num = 1;
  int to_endpoint_packet_num = 1;
  int from_endpoint_packet_num = 1;
  socket_data
      .AddWrite("inital-client-settings",
                ConstructInitialSettingsPacket(to_proxy_packet_num++))
      .Sync();

  socket_data
      .AddWrite("connect-udp",
                ConstructConnectUdpRequestPacket(
                    to_proxy_packet_num++,
                    GetNthClientInitiatedBidirectionalStreamId(0),
                    "proxy.example.org:70",
                    "/.well-known/masque/udp/mail.example.org/443/", false))
      .Sync();

  socket_data
      .AddRead("inital-proxy-settings",
               server_maker_.MakeInitialSettingsPacket(from_proxy_packet_num++))
      .Sync();

  socket_data.AddRead("connect-udp-response",
                      ConstructServerResponseHeadersPacket(
                          from_proxy_packet_num++,
                          GetNthClientInitiatedBidirectionalStreamId(0), false,
                          GetResponseHeaders("200")));

  socket_data
      .AddWrite("ack-connect-udp-response",
                ConstructClientAckPacket(to_proxy_packet_num++,
                                         from_proxy_packet_num - 1,
                                         from_proxy_packet_num - 1))
      .Sync();

  socket_data
      .AddWrite("endpoint-initial-client-settings",
                client_maker_->Packet(to_proxy_packet_num++)
                    .AddMessageFrame(ConstructH3Datagram(
                        GetNthClientInitiatedBidirectionalStreamId(0), 0,
                        to_endpoint_maker.MakeInitialSettingsPacket(
                            to_endpoint_packet_num++)))
                    .Build())
      .Sync();

  socket_data
      .AddWrite(
          "get-request-to-ep",
          client_maker_->Packet(to_proxy_packet_num++)
              .AddMessageFrame(ConstructH3Datagram(
                  GetNthClientInitiatedBidirectionalStreamId(0), 0,
                  to_endpoint_maker.MakeRequestHeadersPacket(
                      to_endpoint_packet_num++,
                      GetNthClientInitiatedBidirectionalStreamId(0), true,
                      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
                      GetRequestHeaders("GET", "https", "/",
                                        &to_endpoint_maker),
                      nullptr,
                      /*should_include_priority_frame=*/true)))
              .Build())
      .Sync();

  socket_data
      .AddRead("endpoint-response",
               server_maker_
                   .Packet(from_proxy_packet_num++)
                   // Response headers
                   .AddMessageFrame(ConstructH3Datagram(
                       GetNthClientInitiatedBidirectionalStreamId(0), 0,
                       from_endpoint_maker.MakeResponseHeadersPacket(
                           from_endpoint_packet_num++,
                           GetNthClientInitiatedBidirectionalStreamId(0), false,
                           GetResponseHeaders("200"), nullptr)))
                   // Response data
                   .AddMessageFrame(ConstructH3Datagram(
                       GetNthClientInitiatedBidirectionalStreamId(0), 0,
                       from_endpoint_maker.Packet(from_endpoint_packet_num++)
                           .AddStreamFrame(
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               true, ConstructDataFrame(kRespData))
                           .Build()))
                   .Build())
      .Sync();

  socket_data
      .AddWrite("ack-endpoint-response",
                client_maker_
                    ->Packet(to_proxy_packet_num++)
                    // Ack to proxy
                    .AddAckFrame(1, from_proxy_packet_num - 1,
                                 from_proxy_packet_num - 1)
                    // Ack to endpoint
                    .AddMessageFrame(ConstructH3Datagram(
                        GetNthClientInitiatedBidirectionalStreamId(0), 0,
                        to_endpoint_maker.Packet(to_endpoint_packet_num++)
                            .AddAckFrame(1, from_endpoint_packet_num - 1,
                                         from_endpoint_packet_num - 1)
                            .Build()))
                    .Build())
      .Sync();

  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  // Add an alternate-protocol mapping so that the transaction
  // uses QUIC to the endpoint.
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans);
  CheckResponsePort(&trans, kQuicProxyChain.First().GetPort());
  CheckResponseData(&trans, kRespData);
  EXPECT_EQ(trans.GetResponseInfo()->proxy_chain, kQuicProxyChain);
  EXPECT_TRUE(socket_data.AllDataConsumed());
}

// Performs an 'http://' request over QUIC proxy tunnel, where the endpoint
// has AlternativeService info, but that is not used for HTTP
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectHttpServer) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  QuicSocketDataProvider socket_data(version_);
  int packet_num = 1;
  socket_data
      .AddWrite("initial-setttings",
                ConstructInitialSettingsPacket(packet_num++))
      .Sync();
  socket_data
      .AddWrite("priority",
                ConstructClientPriorityPacket(
                    packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                    DEFAULT_PRIORITY))
      .Sync();
  socket_data
      .AddWrite("connect-request",
                ConstructClientRequestHeadersPacket(
                    packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                    false, DEFAULT_PRIORITY,
                    ConnectRequestHeaders("mail.example.org:80"), false))
      .Sync();
  socket_data.AddRead("connect-response",
                      ConstructServerResponseHeadersPacket(
                          1, GetNthClientInitiatedBidirectionalStreamId(0),
                          false, GetResponseHeaders("200")));

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  socket_data
      .AddWrite("get-request",
                ConstructClientAckAndDataPacket(
                    packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                    1, 1, false, ConstructDataFrame(kGetRequest)))
      .Sync();

  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kRespData[] = "0123456789";

  socket_data.AddRead("get-response",
                      ConstructServerDataPacket(
                          2, GetNthClientInitiatedBidirectionalStreamId(0),
                          false, ConstructDataFrame(kGetResponse)));

  socket_data
      .AddRead("response-data",
               ConstructServerDataPacket(
                   3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   ConstructDataFrame(kRespData)))
      .Sync();

  socket_data
      .AddWrite("response-ack", ConstructClientAckPacket(packet_num++, 3, 2))
      .Sync();

  socket_data
      .AddWrite(
          "qpack-cancel-rst",
          client_maker_->Packet(packet_num++)
              .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                              StreamCancellationQpackDecoderInstruction(0))
              .AddStopSendingFrame(
                  GetNthClientInitiatedBidirectionalStreamId(0),
                  quic::QUIC_STREAM_CANCELLED)
              .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                                 quic::QUIC_STREAM_CANCELLED)
              .Build())
      .Sync();

  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("http://mail.example.org/");
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectHttpResponseFromProxy(
      kRespData, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  socket_data.RunUntilAllConsumed();
}

// Make two HTTP/1.1 requests to the same host over a QUIC proxy tunnel and
// check that the proxy socket is reused for the second request.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectReuseTransportSocket) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int write_packet_index = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_index++));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          false, DEFAULT_PRIORITY,
          ConnectRequestHeaders("mail.example.org:443"), false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  const char kGetRequest1[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          1, 1, false, ConstructDataFrame(kGetRequest1)));

  const char kGetResponse1[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kRespData1[] = "0123456789";

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kGetResponse1)));

  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       ConstructDataFrame(kRespData1)));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(write_packet_index++, 3, 2));

  const char kGetRequest2[] =
      "GET /2 HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientDataPacket(write_packet_index++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                false, ConstructDataFrame(kGetRequest2)));

  const char kGetResponse2[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  const char kRespData2[] = "0123456";

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kGetResponse2)));

  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       5, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       ConstructDataFrame(kRespData2)));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(write_packet_index++, 5, 4));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(write_packet_index++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(0))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponseFromProxy(
      kRespData1, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  request_.url = GURL("https://mail.example.org/2");
  SendRequestAndExpectHttpResponseFromProxy(
      kRespData2, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Make an HTTP/1.1 request to one host and an HTTP/2 request to a different
// host over a QUIC proxy tunnel. Check that the QUIC session to the proxy
// server is reused for the second request.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectReuseQuicSession) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));

  // CONNECT request and response for first request
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  // GET request, response, and data over QUIC tunnel for first request
  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), 1, 1,
          false, ConstructDataFrame(kGetRequest)));

  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kRespData1[] = "0123456789";

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kGetResponse)));
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       ConstructDataFrame(kRespData1)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2));

  // CONNECT request and response for second request
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("different.example.org:443"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));

  // GET request, response, and data over QUIC tunnel for second request
  SpdyTestUtil spdy_util(/*use_priority_header=*/true);
  spdy::SpdySerializedFrame get_frame =
      spdy_util.ConstructSpdyGet("https://different.example.org/", 1, LOWEST);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), 4, 4,
          false, ConstructDataFrame({get_frame.data(), get_frame.size()})));

  spdy::SpdySerializedFrame resp_frame =
      spdy_util.ConstructSpdyGetReply(nullptr, 0, 1);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 5, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 ConstructDataFrame({resp_frame.data(), resp_frame.size()})));

  const char kRespData2[] = "0123456";
  spdy::SpdySerializedFrame data_frame =
      spdy_util.ConstructSpdyDataFrame(1, kRespData2, true);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 6, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 ConstructDataFrame({data_frame.data(), data_frame.size()})));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 6, 5));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(0))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(1, false))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponseFromProxy(
      kRespData1, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  request_.url = GURL("https://different.example.org/");
  SendRequestAndExpectSpdyResponseFromProxy(
      kRespData2, kQuicProxyChain.First().GetPort(), kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Make two HTTP/1.1 requests, one to a host through a QUIC proxy and another
// directly to the proxy. The proxy socket should not be reused for the second
// request.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectNoReuseDifferentChains) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const ProxyServer kQuicProxyServer{ProxyServer::SCHEME_QUIC,
                                     HostPortPair("proxy.example.org", 443)};
  const ProxyChain kQuicProxyChain =
      ProxyChain::ForIpProtection({kQuicProxyServer});

  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->set_proxy_chain(kQuicProxyChain);

  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://not-used:70", TRAFFIC_ANNOTATION_FOR_TESTS);
  proxy_resolution_service_->SetProxyDelegate(proxy_delegate_.get());

  MockQuicData mock_quic_data_1(version_);
  size_t write_packet_index = 1;

  mock_quic_data_1.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(write_packet_index++));

  mock_quic_data_1.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data_1.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          false, DEFAULT_PRIORITY,
          ConnectRequestHeaders("mail.example.org:443"), false));
  mock_quic_data_1.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  const char kGetRequest1[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data_1.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          1, 1, false, ConstructDataFrame(kGetRequest1)));

  const char kGetResponse1[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kTrans1RespData[] = "0123456789";

  mock_quic_data_1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 ConstructDataFrame(kGetResponse1)));

  mock_quic_data_1.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       ConstructDataFrame(kTrans1RespData)));

  mock_quic_data_1.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(write_packet_index++, 3, 2));
  mock_quic_data_1.AddRead(SYNCHRONOUS,
                           ERR_IO_PENDING);  // No more data to read

  mock_quic_data_1.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(write_packet_index++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(0))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data_1.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponseFromProxy(kTrans1RespData, 443,
                                            kQuicProxyChain);

  proxy_delegate_->set_proxy_chain(ProxyChain::Direct());

  context_.params()->origins_to_force_quic_on.insert(
      kQuicProxyServer.host_port_pair());

  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kQuicProxyServer.GetHost(),
      quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true,
      /*use_priority_header=*/GetParam().priority_header_enabled);

  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kQuicProxyServer.GetHost(),
      quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/false);

  MockQuicData mock_quic_data_2(version_);
  write_packet_index = 1;

  mock_quic_data_2.AddWrite(
      SYNCHRONOUS,
      client_maker2.MakeInitialSettingsPacket(write_packet_index++));

  mock_quic_data_2.AddWrite(
      SYNCHRONOUS,
      client_maker2.MakeRequestHeadersPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
          GetRequestHeaders("GET", "https", "/", &client_maker2), nullptr));
  mock_quic_data_2.AddRead(
      ASYNC, server_maker2.MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200"), nullptr));
  const char kTrans2RespData[] = "0123456";
  mock_quic_data_2.AddRead(
      ASYNC, server_maker2.Packet(2)
                 .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                                 true, ConstructDataFrame(kTrans2RespData))
                 .Build());
  mock_quic_data_2.AddWrite(
      SYNCHRONOUS,
      client_maker2.Packet(write_packet_index++).AddAckFrame(1, 2, 1).Build());
  mock_quic_data_2.AddRead(SYNCHRONOUS,
                           ERR_IO_PENDING);  // No more data to read

  mock_quic_data_2.AddSocketDataToFactory(&socket_factory_);

  SSLSocketDataProvider ssl_2(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_2);

  request_.url =
      GURL(base::StrCat({"https://", kQuicProxyServer.GetHost(), "/"}));
  SendRequestAndExpectQuicResponse(kTrans2RespData);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data_1.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data_1.AllWriteDataConsumed());
  EXPECT_TRUE(mock_quic_data_2.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data_2.AllWriteDataConsumed());
}

// Sends a CONNECT request to a QUIC proxy and receive a 500 response.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectFailure) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 GetResponseHeaders("500")));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndRstPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED, 1, 1));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Sends a CONNECT request to a QUIC proxy and get a UDP socket read error.
TEST_P(QuicNetworkTransactionTest, QuicProxyQuicConnectionError) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Sends an HTTP/1.1 request over QUIC proxy tunnel and gets a bad cert from the
// host. Retries request and succeeds.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectBadCertificate) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;

  const auto kQuicProxyChain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)});
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {kQuicProxyChain}, TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckDataAndRst(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED, 1, 1, GetQpackDecoderStreamId(), false,
          StreamCancellationQpackDecoderInstruction(0)));

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 GetResponseHeaders("200")));

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndDataPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), 2, 2,
          false, ConstructDataFrame(kGetRequest)));

  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kRespData[] = "0123456789";

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                 ConstructDataFrame(kGetResponse)));

  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       4, GetNthClientInitiatedBidirectionalStreamId(1), false,
                       ConstructDataFrame(kRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->Packet(packet_num++)
          .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                          StreamCancellationQpackDecoderInstruction(1, false))
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  SSLSocketDataProvider ssl_data_bad_cert(ASYNC, ERR_CERT_AUTHORITY_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_bad_cert);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, callback.WaitForResult());

  rv = trans.RestartIgnoringLastError(callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  CheckWasHttpResponse(&trans);
  CheckResponsePort(&trans, kQuicProxyChain.First().GetPort());
  CheckResponseData(&trans, kRespData);
  EXPECT_EQ(trans.GetResponseInfo()->proxy_chain, kQuicProxyChain);

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Checks if a request's specified "user-agent" header shows up correctly in the
// CONNECT request to a QUIC proxy.
TEST_P(QuicNetworkTransactionTest, QuicProxyUserAgent) {
  DisablePriorityHeader();
  const char kConfiguredUserAgent[] = "Configured User-Agent";
  const char kRequestUserAgent[] = "Request User-Agent";
  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));

  quiche::HttpHeaderBlock headers =
      ConnectRequestHeaders("mail.example.org:443");
  headers["user-agent"] = kConfiguredUserAgent;
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, std::move(headers), false));
  // Return an error, so the transaction stops here (this test isn't interested
  // in the rest).
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  StaticHttpUserAgentSettings http_user_agent_settings(
      std::string() /* accept_language */, kConfiguredUserAgent);
  session_context_.http_user_agent_settings = &http_user_agent_settings;
  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  request_.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                   kRequestUserAgent);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Makes sure the CONNECT request packet for a QUIC proxy contains the correct
// HTTP/2 stream dependency and weights given the request priority.
TEST_P(QuicNetworkTransactionTest, QuicProxyRequestPriority) {
  DisablePriorityHeader();
  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  const RequestPriority request_priority = MEDIUM;

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientPriorityPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          DEFAULT_PRIORITY));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
          DEFAULT_PRIORITY, ConnectRequestHeaders("mail.example.org:443"),
          false));
  // Return an error, so the transaction stops here (this test isn't interested
  // in the rest).
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(request_priority, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Makes sure the CONNECT request packet for a QUIC proxy contains the correct
// HTTP/2 stream dependency and weights given the request priority.
TEST_P(QuicNetworkTransactionTest, QuicProxyMultipleRequestsError) {
  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  const RequestPriority kRequestPriority = MEDIUM;
  const RequestPriority kRequestPriority2 = LOWEST;

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(ASYNC, ConstructInitialSettingsPacket(1));
  mock_quic_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  // This should never be reached.
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // Second connection attempt just fails - result doesn't really matter.
  MockQuicData mock_quic_data2(version_);
  mock_quic_data2.AddConnect(SYNCHRONOUS, ERR_FAILED);
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  int original_max_sockets_per_group =
      ClientSocketPoolManager::max_sockets_per_group(
          HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL, 1);
  int original_max_sockets_per_pool =
      ClientSocketPoolManager::max_sockets_per_pool(
          HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL, 1);
  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(kRequestPriority, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  HttpRequestInfo request2;
  request2.url = GURL("https://mail.example.org/some/other/path/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  HttpNetworkTransaction trans2(kRequestPriority2, session_.get());
  TestCompletionCallback callback2;
  int rv2 = trans2.Start(&request2, callback2.callback(), net_log_with_source_);
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());

  EXPECT_EQ(ERR_FAILED, callback2.WaitForResult());

  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
      original_max_sockets_per_pool);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
      original_max_sockets_per_group);
}

// Test the request-challenge-retry sequence for basic auth, over a QUIC
// connection when setting up a QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyAuth) {
  const std::u16string kBaz(u"baz");
  const std::u16string kFoo(u"foo");

  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  // On the second pass, the body read of the auth challenge is synchronous, so
  // IsConnectedAndIdle returns false.  The socket should still be drained and
  // reused. See http://crbug.com/544255.
  for (int i = 0; i < 2; ++i) {
    QuicTestPacketMaker client_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        true);
    QuicTestPacketMaker server_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        false);

    MockQuicData mock_quic_data(version_);

    int packet_num = 1;
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker.MakeInitialSettingsPacket(packet_num++));

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakePriorityPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::HttpStreamPriority::kDefaultUrgency));

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
            quic::HttpStreamPriority::kDefaultUrgency,
            client_maker.ConnectRequestHeaders("mail.example.org:443"), nullptr,
            false));

    quiche::HttpHeaderBlock headers = server_maker.GetResponseHeaders("407");
    headers["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
    headers["content-length"] = "10";
    mock_quic_data.AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   std::move(headers), nullptr));

    if (i == 0) {
      mock_quic_data.AddRead(
          ASYNC,
          server_maker.Packet(2)
              .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                              false, "0123456789")
              .Build());
    } else {
      mock_quic_data.AddRead(
          SYNCHRONOUS,
          server_maker.Packet(2)
              .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                              false, "0123456789")
              .Build());
    }

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.Packet(packet_num++).AddAckFrame(1, 2, 1).Build());

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.Packet(packet_num++)
            .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                            StreamCancellationQpackDecoderInstruction(0))
            .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                                 quic::QUIC_STREAM_CANCELLED)
            .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
            .Build());

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakePriorityPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
            quic::HttpStreamPriority::kDefaultUrgency));

    headers = client_maker.ConnectRequestHeaders("mail.example.org:443");
    headers["proxy-authorization"] = "Basic Zm9vOmJheg==";
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
            quic::HttpStreamPriority::kDefaultUrgency, std::move(headers),
            nullptr, false));

    // Response to wrong password
    headers = server_maker.GetResponseHeaders("407");
    headers["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
    headers["content-length"] = "10";
    mock_quic_data.AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                   std::move(headers), nullptr));
    mock_quic_data.AddRead(SYNCHRONOUS,
                           ERR_IO_PENDING);  // No more data to read

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.Packet(packet_num++)
            .AddAckFrame(/*first_received=*/1, /*largest_received=*/3,
                         /*smallest_received=*/3)
            .AddStreamFrame(GetQpackDecoderStreamId(), /*fin=*/false,
                            StreamCancellationQpackDecoderInstruction(1, false))
            .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                                 quic::QUIC_STREAM_CANCELLED)
            .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                               quic::QUIC_STREAM_CANCELLED)
            .Build());

    mock_quic_data.AddSocketDataToFactory(&socket_factory_);
    mock_quic_data.GetSequencedSocketData()->set_busy_before_sync_reads(true);

    CreateSession();

    request_.url = GURL("https://mail.example.org/");
    // Ensure that proxy authentication is attempted even
    // when privacy mode is enabled.
    request_.privacy_mode = PRIVACY_MODE_ENABLED;
    {
      HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
      RunTransaction(&trans);

      const HttpResponseInfo* response = trans.GetResponseInfo();
      ASSERT_TRUE(response != nullptr);
      ASSERT_TRUE(response->headers.get() != nullptr);
      EXPECT_EQ("HTTP/1.1 407", response->headers->GetStatusLine());
      EXPECT_TRUE(response->headers->IsKeepAlive());
      EXPECT_EQ(407, response->headers->response_code());
      EXPECT_EQ(10, response->headers->GetContentLength());
      EXPECT_EQ(HttpVersion(1, 1), response->headers->GetHttpVersion());
      std::optional<AuthChallengeInfo> auth_challenge =
          response->auth_challenge;
      ASSERT_TRUE(auth_challenge.has_value());
      EXPECT_TRUE(auth_challenge->is_proxy);
      EXPECT_EQ("https://proxy.example.org:70",
                auth_challenge->challenger.Serialize());
      EXPECT_EQ("MyRealm1", auth_challenge->realm);
      EXPECT_EQ("basic", auth_challenge->scheme);

      TestCompletionCallback callback;
      int rv = trans.RestartWithAuth(AuthCredentials(kFoo, kBaz),
                                     callback.callback());
      EXPECT_EQ(ERR_IO_PENDING, rv);
      EXPECT_EQ(OK, callback.WaitForResult());

      response = trans.GetResponseInfo();
      ASSERT_TRUE(response != nullptr);
      ASSERT_TRUE(response->headers.get() != nullptr);
      EXPECT_EQ("HTTP/1.1 407", response->headers->GetStatusLine());
      EXPECT_TRUE(response->headers->IsKeepAlive());
      EXPECT_EQ(407, response->headers->response_code());
      EXPECT_EQ(10, response->headers->GetContentLength());
      EXPECT_EQ(HttpVersion(1, 1), response->headers->GetHttpVersion());
      auth_challenge = response->auth_challenge;
      ASSERT_TRUE(auth_challenge.has_value());
      EXPECT_TRUE(auth_challenge->is_proxy);
      EXPECT_EQ("https://proxy.example.org:70",
                auth_challenge->challenger.Serialize());
      EXPECT_EQ("MyRealm1", auth_challenge->realm);
      EXPECT_EQ("basic", auth_challenge->scheme);
    }
    // HttpNetworkTransaction is torn down now that it's out of scope, causing
    // the QUIC stream to be cleaned up (since the proxy socket cannot be
    // reused because it's not connected).
    EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
  }
}

// Test that NetworkAnonymizationKey is respected by QUIC connections, when
// kPartitionConnectionsByNetworkIsolationKey is enabled.
TEST_P(QuicNetworkTransactionTest, NetworkIsolation) {
  const SchemefulSite kSite1(GURL("http://origin1/"));
  const SchemefulSite kSite2(GURL("http://origin2/"));
  NetworkIsolationKey network_isolation_key1(kSite1, kSite1);
  NetworkIsolationKey network_isolation_key2(kSite2, kSite2);
  const auto network_anonymization_key1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const auto network_anonymization_key2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  GURL url1 = GURL("https://mail.example.org/1");
  GURL url2 = GURL("https://mail.example.org/2");
  GURL url3 = GURL("https://mail.example.org/3");

  for (bool partition_connections : {false, true}) {
    SCOPED_TRACE(partition_connections);

    base::test::ScopedFeatureList feature_list;
    if (partition_connections) {
      feature_list.InitAndEnableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    }

    // Reads and writes for the unpartitioned case, where only one socket is
    // used.

    context_.params()->origins_to_force_quic_on.insert(
        HostPortPair::FromString("mail.example.org:443"));

    MockQuicData unpartitioned_mock_quic_data(version_);
    QuicTestPacketMaker client_maker1(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        /*client_priority_uses_incremental=*/true,
        /*use_priority_header=*/true);
    QuicTestPacketMaker server_maker1(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        /*client_priority_uses_incremental=*/false,
        /*use_priority_header=*/false);

    int packet_num = 1;
    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker1.MakeInitialSettingsPacket(packet_num++));

    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker1.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", url1.scheme(), "/1"), nullptr));
    unpartitioned_mock_quic_data.AddRead(
        ASYNC, server_maker1.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   GetResponseHeaders("200"), nullptr));
    const char kRespData1[] = "1";
    unpartitioned_mock_quic_data.AddRead(
        ASYNC,
        server_maker1.Packet(2)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0), true,
                            ConstructDataFrame(kRespData1))
            .Build());
    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientAckPacket(packet_num++, 2, 1));

    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker1.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), true,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", url2.scheme(), "/2"), nullptr));
    unpartitioned_mock_quic_data.AddRead(
        ASYNC, server_maker1.MakeResponseHeadersPacket(
                   3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                   GetResponseHeaders("200"), nullptr));
    const char kRespData2[] = "2";
    unpartitioned_mock_quic_data.AddRead(
        ASYNC,
        server_maker1.Packet(4)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1), true,
                            ConstructDataFrame(kRespData2))
            .Build());
    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientAckPacket(packet_num++, 4, 3));

    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker1.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(2), true,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", url3.scheme(), "/3"), nullptr));
    unpartitioned_mock_quic_data.AddRead(
        ASYNC, server_maker1.MakeResponseHeadersPacket(
                   5, GetNthClientInitiatedBidirectionalStreamId(2), false,
                   GetResponseHeaders("200"), nullptr));
    const char kRespData3[] = "3";
    unpartitioned_mock_quic_data.AddRead(
        ASYNC,
        server_maker1.Packet(6)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(2), true,
                            ConstructDataFrame(kRespData3))
            .Build());
    unpartitioned_mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientAckPacket(packet_num++, 6, 5));

    unpartitioned_mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

    // Reads and writes for the partitioned case, where two sockets are used.

    MockQuicData partitioned_mock_quic_data1(version_);
    QuicTestPacketMaker client_maker2(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        /*client_priority_uses_incremental=*/true,
        /*use_priority_header=*/true);
    QuicTestPacketMaker server_maker2(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        /*client_priority_uses_incremental=*/false,
        /*use_priority_header=*/false);

    int packet_num2 = 1;
    partitioned_mock_quic_data1.AddWrite(
        SYNCHRONOUS, client_maker2.MakeInitialSettingsPacket(packet_num2++));

    partitioned_mock_quic_data1.AddWrite(
        SYNCHRONOUS,
        client_maker2.MakeRequestHeadersPacket(
            packet_num2++, GetNthClientInitiatedBidirectionalStreamId(0), true,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", url1.scheme(), "/1"), nullptr));
    partitioned_mock_quic_data1.AddRead(
        ASYNC, server_maker2.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   GetResponseHeaders("200"), nullptr));
    partitioned_mock_quic_data1.AddRead(
        ASYNC,
        server_maker2.Packet(2)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0), true,
                            ConstructDataFrame(kRespData1))
            .Build());
    partitioned_mock_quic_data1.AddWrite(
        SYNCHRONOUS,
        client_maker2.Packet(packet_num2++).AddAckFrame(1, 2, 1).Build());

    partitioned_mock_quic_data1.AddWrite(
        SYNCHRONOUS,
        client_maker2.MakeRequestHeadersPacket(
            packet_num2++, GetNthClientInitiatedBidirectionalStreamId(1), true,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", url3.scheme(), "/3"), nullptr));
    partitioned_mock_quic_data1.AddRead(
        ASYNC, server_maker2.MakeResponseHeadersPacket(
                   3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                   GetResponseHeaders("200"), nullptr));
    partitioned_mock_quic_data1.AddRead(
        ASYNC,
        server_maker2.Packet(4)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1), true,
                            ConstructDataFrame(kRespData3))
            .Build());
    partitioned_mock_quic_data1.AddWrite(
        SYNCHRONOUS,
        client_maker2.Packet(packet_num2++).AddAckFrame(1, 4, 3).Build());

    partitioned_mock_quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

    MockQuicData partitioned_mock_quic_data2(version_);
    QuicTestPacketMaker client_maker3(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        /*client_priority_uses_incremental=*/true,
        /*use_priority_header=*/true);
    QuicTestPacketMaker server_maker3(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        /*client_priority_uses_incremental=*/false,
        /*use_priority_header=*/false);

    int packet_num3 = 1;
    partitioned_mock_quic_data2.AddWrite(
        SYNCHRONOUS, client_maker3.MakeInitialSettingsPacket(packet_num3++));

    partitioned_mock_quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker3.MakeRequestHeadersPacket(
            packet_num3++, GetNthClientInitiatedBidirectionalStreamId(0), true,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", url2.scheme(), "/2"), nullptr));
    partitioned_mock_quic_data2.AddRead(
        ASYNC, server_maker3.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   GetResponseHeaders("200"), nullptr));
    partitioned_mock_quic_data2.AddRead(
        ASYNC,
        server_maker3.Packet(2)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0), true,
                            ConstructDataFrame(kRespData2))
            .Build());
    partitioned_mock_quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker3.Packet(packet_num3++).AddAckFrame(1, 2, 1).Build());

    partitioned_mock_quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

    if (partition_connections) {
      partitioned_mock_quic_data1.AddSocketDataToFactory(&socket_factory_);
      partitioned_mock_quic_data2.AddSocketDataToFactory(&socket_factory_);
    } else {
      unpartitioned_mock_quic_data.AddSocketDataToFactory(&socket_factory_);
    }

    CreateSession();

    TestCompletionCallback callback;
    HttpRequestInfo request1;
    request1.method = "GET";
    request1.url = GURL(url1);
    request1.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    request1.network_isolation_key = network_isolation_key1;
    request1.network_anonymization_key = network_anonymization_key1;
    HttpNetworkTransaction trans1(LOWEST, session_.get());
    int rv = trans1.Start(&request1, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    std::string response_data1;
    EXPECT_THAT(ReadTransaction(&trans1, &response_data1), IsOk());
    EXPECT_EQ(kRespData1, response_data1);

    HttpRequestInfo request2;
    request2.method = "GET";
    request2.url = GURL(url2);
    request2.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    request2.network_isolation_key = network_isolation_key2;
    request2.network_anonymization_key = network_anonymization_key2;
    HttpNetworkTransaction trans2(LOWEST, session_.get());
    rv = trans2.Start(&request2, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    std::string response_data2;
    EXPECT_THAT(ReadTransaction(&trans2, &response_data2), IsOk());
    EXPECT_EQ(kRespData2, response_data2);

    HttpRequestInfo request3;
    request3.method = "GET";
    request3.url = GURL(url3);
    request3.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    request3.network_isolation_key = network_isolation_key1;
    request3.network_anonymization_key = network_anonymization_key1;

    HttpNetworkTransaction trans3(LOWEST, session_.get());
    rv = trans3.Start(&request3, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    std::string response_data3;
    EXPECT_THAT(ReadTransaction(&trans3, &response_data3), IsOk());
    EXPECT_EQ(kRespData3, response_data3);

    if (partition_connections) {
      EXPECT_TRUE(partitioned_mock_quic_data1.AllReadDataConsumed());
      EXPECT_TRUE(partitioned_mock_quic_data1.AllWriteDataConsumed());
      EXPECT_TRUE(partitioned_mock_quic_data2.AllReadDataConsumed());
      EXPECT_TRUE(partitioned_mock_quic_data2.AllWriteDataConsumed());
    } else {
      EXPECT_TRUE(unpartitioned_mock_quic_data.AllReadDataConsumed());
      EXPECT_TRUE(unpartitioned_mock_quic_data.AllWriteDataConsumed());
    }
  }
}

// Test that two requests to the same origin over QUIC tunnels use different
// QUIC sessions if their NetworkIsolationKeys don't match, and
// kPartitionConnectionsByNetworkIsolationKey is enabled.
TEST_P(QuicNetworkTransactionTest, NetworkIsolationTunnel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "proxy.example.org", 70)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  const char kRespData[] = "0123456789";

  std::unique_ptr<MockQuicData> mock_quic_data[2] = {
      std::make_unique<MockQuicData>(version_),
      std::make_unique<MockQuicData>(version_)};

  for (int index : {0, 1}) {
    QuicTestPacketMaker client_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        true);
    QuicTestPacketMaker server_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        false);

    int packet_num = 1;
    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS, client_maker.MakeInitialSettingsPacket(packet_num++));

    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS,
        client_maker.MakePriorityPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::HttpStreamPriority::kDefaultUrgency));

    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), false,
            quic::HttpStreamPriority::kDefaultUrgency,
            ConnectRequestHeaders("mail.example.org:80"), nullptr, false));
    mock_quic_data[index]->AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   GetResponseHeaders("200"), nullptr));

    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS,
        client_maker.Packet(packet_num++)
            .AddAckFrame(/*first_received=*/1, /*largest_received=*/1,
                         /*smallest_received=*/1)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                            false, ConstructDataFrame(kGetRequest))
            .Build());

    mock_quic_data[index]->AddRead(
        ASYNC,
        server_maker.Packet(2)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                            false, ConstructDataFrame(kGetResponse))
            .Build());
    mock_quic_data[index]->AddRead(
        SYNCHRONOUS,
        server_maker.Packet(3)
            .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                            false, ConstructDataFrame(kRespData))
            .Build());
    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS,
        client_maker.Packet(packet_num++).AddAckFrame(1, 3, 2).Build());
    mock_quic_data[index]->AddRead(SYNCHRONOUS,
                                   ERR_IO_PENDING);  // No more data to read

    mock_quic_data[index]->AddSocketDataToFactory(&socket_factory_);
  }

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  SSLSocketDataProvider ssl_data2(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data2);

  CreateSession();

  request_.url = GURL("http://mail.example.org/");
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans);
  CheckResponseData(&trans, kRespData);

  const SchemefulSite kSite1(GURL("http://origin1/"));
  request_.network_isolation_key = NetworkIsolationKey(kSite1, kSite1);
  request_.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans2);
  CheckResponseData(&trans2, kRespData);

  EXPECT_TRUE(mock_quic_data[0]->AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data[0]->AllWriteDataConsumed());
  EXPECT_TRUE(mock_quic_data[1]->AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data[1]->AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, AllowHTTP1FalseProhibitsH1) {
  MockRead http_reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING),
                           MockRead(ASYNC, OK)};
  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.method = "POST";
  UploadDataStreamNotAllowHTTP1 upload_data("");
  request_.upload_data_stream = &upload_data;

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_H2_OR_QUIC_REQUIRED));
}

// Confirm mock class UploadDataStreamNotAllowHTTP1 can upload content over
// QUIC.
TEST_P(QuicNetworkTransactionTest, AllowHTTP1MockTest) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int write_packet_index = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_index++));
  const std::string kUploadContent = "foo";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersAndDataFramesPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, DEFAULT_PRIORITY, GetRequestHeaders("POST", "https", "/"),
          nullptr, {ConstructDataFrame(kUploadContent)}));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(write_packet_index++, 2, 1));

  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  UploadDataStreamNotAllowHTTP1 upload_data(kUploadContent);
  request_.upload_data_stream = &upload_data;

  SendRequestAndExpectQuicResponse(kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, AllowHTTP1UploadPauseAndResume) {
  FLAGS_quic_enable_chaos_protection = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Hanging read
  int write_packet_index = 1;
  mock_quic_data.AddWrite(
      ASYNC, client_maker_->MakeDummyCHLOPacket(write_packet_index++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_index++));
  const std::string kUploadContent = "foo";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersAndDataFramesPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, DEFAULT_PRIORITY, GetRequestHeaders("POST", "https", "/"),
          nullptr, {ConstructDataFrame(kUploadContent)}));
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerResponseHeadersPacket(
                       1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                       ConstructDataFrame(kQuicRespData)));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(write_packet_index++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  SequencedSocketData* socket_data = mock_quic_data.GetSequencedSocketData();

  CreateSession();

  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // Set up request.
  request_.method = "POST";
  UploadDataStreamNotAllowHTTP1 upload_data(kUploadContent);
  request_.upload_data_stream = &upload_data;

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  // Resume QUIC job
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  socket_data->Resume();

  base::RunLoop().RunUntilIdle();
  CheckResponseData(&trans, kQuicRespData);
}

TEST_P(QuicNetworkTransactionTest, AllowHTTP1UploadFailH1AndResumeQuic) {
  FLAGS_quic_enable_chaos_protection = false;
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  // This test confirms failed main job should not bother quic job.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead("1.1 Body"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};
  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Hanging read
  int write_packet_index = 1;
  mock_quic_data.AddWrite(
      ASYNC, client_maker_->MakeDummyCHLOPacket(write_packet_index++));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_index++));
  const std::string kUploadContent = "foo";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersAndDataFramesPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, DEFAULT_PRIORITY, GetRequestHeaders("POST", "https", "/"),
          nullptr, {ConstructDataFrame(kUploadContent)}));
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerResponseHeadersPacket(
                       1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                       ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(write_packet_index++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  SequencedSocketData* socket_data = mock_quic_data.GetSequencedSocketData();

  // This packet won't be read because AllowHTTP1:false doesn't allow H/1
  // connection.
  MockRead http_reads2[] = {MockRead("HTTP/1.1 200 OK\r\n")};
  StaticSocketDataProvider http_data2(http_reads2, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  // Send the first request via TCP and set up alternative service (QUIC) for
  // the origin.
  SendRequestAndExpectHttpResponse("1.1 Body");

  // Settings to resume main H/1 job quickly while pausing quic job.
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  ServerNetworkStats stats1;
  stats1.srtt = base::Microseconds(10);
  http_server_properties_->SetServerNetworkStats(
      url::SchemeHostPort(request_.url), NetworkAnonymizationKey(), stats1);

  // Set up request.
  request_.method = "POST";
  UploadDataStreamNotAllowHTTP1 upload_data(kUploadContent);
  request_.upload_data_stream = &upload_data;

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Confirm TCP job was resumed.
  // We can not check its failure because HttpStreamFactory::JobController.
  // main_job_net_error is not exposed.
  while (socket_factory_.mock_data().next_index() < 3u) {
    base::RunLoop().RunUntilIdle();
  }
  // Resume QUIC job.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  socket_data->Resume();
  rv = callback.WaitForResult();
  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    // This test depends heavily on the internal behavior of
    // HttpStreamFactory's JobController and Jobs, which aren't used when
    // the HappyEyeballsV3 is enabled, and when the HappyEyeballsV3 is enabled
    // we create an HttpStream on HTTP/1.1 for the request. Just check we get
    // an appropriate error.
    EXPECT_THAT(rv, IsError(ERR_H2_OR_QUIC_REQUIRED));
  } else {
    EXPECT_THAT(rv, IsOk());
    CheckResponseData(&trans, kQuicRespData);
  }
}

TEST_P(QuicNetworkTransactionTest, IncorrectHttp3GoAway) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;

  MockQuicData mock_quic_data(version_);
  int write_packet_number = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(write_packet_number++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          write_packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, GetRequestHeaders("GET", "https", "/")));

  int read_packet_number = 1;
  mock_quic_data.AddRead(
      ASYNC, server_maker_.MakeInitialSettingsPacket(read_packet_number++));
  // The GOAWAY frame sent by the server MUST have a stream ID corresponding to
  // a client-initiated bidirectional stream.  Any other kind of stream ID
  // should cause the client to close the connection.
  quic::GoAwayFrame goaway{3};
  auto goaway_buffer = quic::HttpEncoder::SerializeGoAwayFrame(goaway);
  const quic::QuicStreamId control_stream_id =
      quic::QuicUtils::GetFirstUnidirectionalStreamId(
          version_.transport_version, quic::Perspective::IS_SERVER);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(read_packet_number++, control_stream_id,
                                       false, goaway_buffer));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(
          write_packet_number++, 2, 4, quic::QUIC_HTTP_GOAWAY_INVALID_STREAM_ID,
          "GOAWAY with invalid stream ID", 0));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ASYNC_ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());

  NetErrorDetails details;
  trans.PopulateNetErrorDetails(&details);
  EXPECT_THAT(details.quic_connection_error,
              quic::test::IsError(quic::QUIC_HTTP_GOAWAY_INVALID_STREAM_ID));
}

TEST_P(QuicNetworkTransactionTest, RetryOnHttp3GoAway) {
  MockQuicData mock_quic_data1(version_);
  int write_packet_number1 = 1;
  mock_quic_data1.AddWrite(
      SYNCHRONOUS, ConstructInitialSettingsPacket(write_packet_number1++));
  const quic::QuicStreamId stream_id1 =
      GetNthClientInitiatedBidirectionalStreamId(0);
  mock_quic_data1.AddWrite(SYNCHRONOUS,
                           ConstructClientRequestHeadersPacket(
                               write_packet_number1++, stream_id1, true,
                               GetRequestHeaders("GET", "https", "/")));
  const quic::QuicStreamId stream_id2 =
      GetNthClientInitiatedBidirectionalStreamId(1);
  mock_quic_data1.AddWrite(SYNCHRONOUS,
                           ConstructClientRequestHeadersPacket(
                               write_packet_number1++, stream_id2, true,
                               GetRequestHeaders("GET", "https", "/foo")));

  int read_packet_number1 = 1;
  mock_quic_data1.AddRead(
      ASYNC, server_maker_.MakeInitialSettingsPacket(read_packet_number1++));

  // GOAWAY with stream_id2 informs the client that stream_id2 (and streams with
  // larger IDs) have not been processed and can safely be retried.
  quic::GoAwayFrame goaway{stream_id2};
  auto goaway_buffer = quic::HttpEncoder::SerializeGoAwayFrame(goaway);
  const quic::QuicStreamId control_stream_id =
      quic::QuicUtils::GetFirstUnidirectionalStreamId(
          version_.transport_version, quic::Perspective::IS_SERVER);
  mock_quic_data1.AddRead(
      ASYNC, ConstructServerDataPacket(read_packet_number1++, control_stream_id,
                                       false, goaway_buffer));
  mock_quic_data1.AddWrite(
      ASYNC, ConstructClientAckPacket(write_packet_number1++, 2, 1));

  // Response to first request is accepted after GOAWAY.
  mock_quic_data1.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                                     read_packet_number1++, stream_id1, false,
                                     GetResponseHeaders("200")));
  const char kRespData1[] = "response on the first connection";
  mock_quic_data1.AddRead(
      ASYNC, ConstructServerDataPacket(read_packet_number1++, stream_id1, true,
                                       ConstructDataFrame(kRespData1)));
  mock_quic_data1.AddWrite(
      ASYNC, ConstructClientAckPacket(write_packet_number1++, 4, 1));
  // Make socket hang to make sure connection stays in connection pool.
  // This should not prevent the retry from opening a new connection.
  mock_quic_data1.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data1.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data1.AddSocketDataToFactory(&socket_factory_);

  // Second request is retried on a new connection.
  MockQuicData mock_quic_data2(version_);
  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true, /*use_priority_header=*/true);
  int write_packet_number2 = 1;
  mock_quic_data2.AddWrite(SYNCHRONOUS, client_maker2.MakeInitialSettingsPacket(
                                            write_packet_number2++));
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  mock_quic_data2.AddWrite(
      SYNCHRONOUS, client_maker2.MakeRequestHeadersPacket(
                       write_packet_number2++, stream_id1, true, priority,
                       GetRequestHeaders("GET", "https", "/foo"), nullptr));

  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
      /*client_priority_uses_incremental=*/false,
      /*use_priority_header=*/false);
  int read_packet_number2 = 1;
  mock_quic_data2.AddRead(ASYNC, server_maker2.MakeResponseHeadersPacket(
                                     read_packet_number2++, stream_id1, false,
                                     GetResponseHeaders("200"), nullptr));
  const char kRespData2[] = "response on the second connection";
  mock_quic_data2.AddRead(
      ASYNC,
      server_maker2.Packet(read_packet_number2++)
          .AddStreamFrame(stream_id1, true, ConstructDataFrame(kRespData2))
          .Build());
  mock_quic_data2.AddWrite(ASYNC, client_maker2.Packet(write_packet_number2++)
                                      .AddAckFrame(1, 2, 1)
                                      .Build());
  mock_quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data2.AddRead(ASYNC, 0);               // EOF
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback1;
  int rv = trans1.Start(&request_, callback1.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();

  HttpRequestInfo request2;
  request2.method = "GET";
  std::string url("https://");
  url.append(kDefaultServerHostName);
  url.append("/foo");
  request2.url = GURL(url);
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), net_log_with_source_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  CheckResponseData(&trans1, kRespData1);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  CheckResponseData(&trans2, kRespData2);

  mock_quic_data1.Resume();
  mock_quic_data2.Resume();
  EXPECT_TRUE(mock_quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(mock_quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data2.AllWriteDataConsumed());
  EXPECT_TRUE(mock_quic_data2.AllReadDataConsumed());
}

// TODO(yoichio):  Add the TCP job reuse case. See crrev.com/c/2174099.

#if BUILDFLAG(ENABLE_WEBSOCKETS)

// This test verifies that when there is an HTTP/3 connection open to a server,
// a WebSocket request does not use it, but instead opens a new connection with
// HTTP/1.
TEST_P(QuicNetworkTransactionTest, WebsocketOpensNewConnectionWithHttp1) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  context_.params()->retry_without_alt_svc_on_quic_errors = false;

  MockQuicData mock_quic_data(version_);

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));

  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  // The request will initially go out over HTTP/3.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          priority, GetRequestHeaders("GET", "https", "/"), nullptr));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 server_maker_.GetResponseHeaders("200"), nullptr));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(2)
                 .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                                 true, ConstructDataFrame(kQuicRespData))
                 .Build());
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read.
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0,
                "GET / HTTP/1.1\r\n"
                "Host: mail.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://mail.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 1,
               "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};

  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSession();

  TestCompletionCallback callback1;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session_.get());
  int rv = trans1.Start(&request_, callback1.callback(), net_log_with_source_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ(kQuic200RespStatusLine, response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(&trans1, &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ(kQuicRespData, response_data);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://mail.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://mail.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session_.get());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), net_log_with_source_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  ASSERT_FALSE(mock_quic_data.AllReadDataConsumed());
  mock_quic_data.Resume();
  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Much like above, but for Alt-Svc QUIC.
TEST_P(QuicNetworkTransactionTest,
       WebsocketOpensNewConnectionWithHttp1AfterAltSvcQuic) {
  if (version_.AlpnDeferToRFCv1()) {
    // These versions currently do not support Alt-Svc.
    return;
  }
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header_.data()),
      MockRead(kHttpRespData),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 GetResponseHeaders("200")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), true,
                 ConstructDataFrame(kQuicRespData)));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockWrite http_writes2[] = {
      MockWrite(SYNCHRONOUS, 0,
                "GET / HTTP/1.1\r\n"
                "Host: mail.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://mail.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead http_reads2[] = {
      MockRead(SYNCHRONOUS, 1,
               "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};

  SequencedSocketData http_data2(http_reads2, http_writes2);
  socket_factory_.AddSocketDataProvider(&http_data2);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  SendRequestAndExpectHttpResponse(kHttpRespData);
  SendRequestAndExpectQuicResponse(kQuicRespData);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://mail.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://mail.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session_.get());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  int rv = trans2.Start(&request2, callback2.callback(), net_log_with_source_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  ASSERT_FALSE(mock_quic_data.AllReadDataConsumed());
  mock_quic_data.Resume();
  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Much like above, but for DnsHttpsSvcbAlpn QUIC.
TEST_P(QuicNetworkTransactionTest,
       WebsocketOpensNewConnectionWithHttp1AfterDnsHttpsSvcbAlpn) {
  session_params_.use_dns_https_svcb_alpn = true;

  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));

  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  // The request will initially go out over HTTP/3.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          priority, GetRequestHeaders("GET", "https", "/"), nullptr));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 server_maker_.GetResponseHeaders("200"), nullptr));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(2)
                 .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                                 true, ConstructDataFrame(kQuicRespData))
                 .Build());
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read.
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0,
                "GET / HTTP/1.1\r\n"
                "Host: mail.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://mail.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 1,
               "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};

  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  HostResolverEndpointResult endpoint_result1;
  endpoint_result1.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoint_result1.metadata.supported_protocol_alpns = {
      quic::QuicVersionLabelToString(quic::CreateQuicVersionLabel(version_))};
  HostResolverEndpointResult endpoint_result2;
  endpoint_result2.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.push_back(endpoint_result1);
  endpoints.push_back(endpoint_result2);
  host_resolver_.rules()->AddRule(
      "mail.example.org",
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{"mail.example.org"}));

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  TestCompletionCallback callback1;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session_.get());
  int rv = trans1.Start(&request_, callback1.callback(), net_log_with_source_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ(kQuic200RespStatusLine, response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(&trans1, &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ(kQuicRespData, response_data);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://mail.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://mail.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session_.get());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), net_log_with_source_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  ASSERT_FALSE(mock_quic_data.AllReadDataConsumed());
  mock_quic_data.Resume();
  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

}  // namespace net::test
