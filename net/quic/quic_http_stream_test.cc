// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/quic/quic_http_stream.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_response_headers.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_frame_builder.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_write_blocked_list.h"
#include "net/third_party/quiche/src/quiche/quic/core/tls_client_handshaker.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net::test {
namespace {

const char kUploadData[] = "Really nifty data!";
const char kDefaultServerHostName[] = "www.example.org";
const uint16_t kDefaultServerPort = 443;

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

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      AllSupportedQuicVersions();
  for (const auto& version : all_supported_versions) {
    params.push_back(TestParams{version, true});
    params.push_back(TestParams{version, false});
  }
  return params;
}

// Returns true if |params| is a dict, has an entry with key "headers", that
// entry is a list of strings, which when interpreted as colon-separated
// key-value pairs has exactly one entry with |key| and that entry has value
// |expected_value|.
bool CheckHeader(const base::Value::Dict& params,
                 std::string_view key,
                 std::string_view expected_value) {
  const base::Value::List* headers = params.FindList("headers");
  if (!headers) {
    return false;
  }

  std::string header_prefix = base::StrCat({key, ": "});
  std::string expected_header = base::StrCat({header_prefix, expected_value});

  bool header_found = false;
  for (const auto& header_value : *headers) {
    const std::string* header = header_value.GetIfString();
    if (!header) {
      return false;
    }
    if (header->starts_with(header_prefix)) {
      if (header_found) {
        return false;
      }
      if (*header != expected_header) {
        return false;
      }
      header_found = true;
    }
  }
  return header_found;
}

class TestQuicConnection : public quic::QuicConnection {
 public:
  TestQuicConnection(const quic::ParsedQuicVersionVector& versions,
                     quic::QuicConnectionId connection_id,
                     IPEndPoint address,
                     QuicChromiumConnectionHelper* helper,
                     QuicChromiumAlarmFactory* alarm_factory,
                     quic::QuicPacketWriter* writer,
                     quic::ConnectionIdGeneratorInterface& generator)
      : quic::QuicConnection(connection_id,
                             quic::QuicSocketAddress(),
                             ToQuicSocketAddress(address),
                             helper,
                             alarm_factory,
                             writer,
                             true /* owns_writer */,
                             quic::Perspective::IS_CLIENT,
                             versions,
                             generator) {}

  void SetSendAlgorithm(quic::SendAlgorithmInterface* send_algorithm) {
    quic::test::QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }
};

// UploadDataStream that always returns errors on data read.
class ReadErrorUploadDataStream : public UploadDataStream {
 public:
  enum class FailureMode { SYNC, ASYNC };

  explicit ReadErrorUploadDataStream(FailureMode mode)
      : UploadDataStream(true, 0), async_(mode) {}

  ReadErrorUploadDataStream(const ReadErrorUploadDataStream&) = delete;
  ReadErrorUploadDataStream& operator=(const ReadErrorUploadDataStream&) =
      delete;

  ~ReadErrorUploadDataStream() override = default;

 private:
  void CompleteRead() { UploadDataStream::OnReadCompleted(ERR_FAILED); }

  // UploadDataStream implementation:
  int InitInternal(const NetLogWithSource& net_log) override { return OK; }

  int ReadInternal(IOBuffer* buf, int buf_len) override {
    if (async_ == FailureMode::ASYNC) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ReadErrorUploadDataStream::CompleteRead,
                                    weak_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
    }
    return ERR_FAILED;
  }

  void ResetInternal() override {}

  const FailureMode async_;

  base::WeakPtrFactory<ReadErrorUploadDataStream> weak_factory_{this};
};

// A helper class that will delete |stream| when the callback is invoked.
class DeleteStreamCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteStreamCallback(std::unique_ptr<QuicHttpStream> stream)
      : stream_(std::move(stream)) {}

  CompletionOnceCallback callback() {
    return base::BindOnce(&DeleteStreamCallback::DeleteStream,
                          base::Unretained(this));
  }

 private:
  void DeleteStream(int result) {
    stream_.reset();
    SetResult(result);
  }

  std::unique_ptr<QuicHttpStream> stream_;
};

}  // namespace

class QuicHttpStreamPeer {
 public:
  static QuicChromiumClientStream::Handle* GetQuicChromiumClientStream(
      QuicHttpStream* stream) {
    return stream->stream_.get();
  }
};

class QuicHttpStreamTest : public ::testing::TestWithParam<TestParams>,
                           public WithTaskEnvironment {
 public:
  void CloseStream(QuicHttpStream* stream, int /*rv*/) { stream->Close(false); }

 protected:
  static const bool kFin = true;

  // Holds a packet to be written to the wire, and the IO mode that should
  // be used by the mock socket when performing the write.
  struct PacketToWrite {
    PacketToWrite(IoMode mode, std::unique_ptr<quic::QuicReceivedPacket> packet)
        : mode(mode), packet(std::move(packet)) {}
    PacketToWrite(IoMode mode, int rv) : mode(mode), rv(rv) {}

    IoMode mode;
    int rv;
    std::unique_ptr<quic::QuicReceivedPacket> packet;
  };

  QuicHttpStreamTest()
      : version_(GetParam().version),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        read_buffer_(base::MakeRefCounted<IOBufferWithSize>(4096)),
        stream_id_(GetNthClientInitiatedBidirectionalStreamId(0)),
        connection_id_(quic::test::TestConnectionId(2)),
        client_maker_(version_,
                      connection_id_,
                      &clock_,
                      kDefaultServerHostName,
                      quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/true),
        server_maker_(version_,
                      connection_id_,
                      &clock_,
                      kDefaultServerHostName,
                      quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false),
        printer_(version_) {
    if (GetParam().priority_header_enabled) {
      feature_list_.InitAndEnableFeature(net::features::kPriorityHeader);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kPriorityHeader);
    }
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic::QuicEnableVersion(version_);
    IPAddress ip(192, 0, 2, 33);
    peer_addr_ = IPEndPoint(ip, 443);
    self_addr_ = IPEndPoint(ip, 8435);
    clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    request_.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  ~QuicHttpStreamTest() override {
    session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
  }

  // Adds a packet to the list of expected writes.
  void AddWrite(std::unique_ptr<quic::QuicReceivedPacket> packet) {
    writes_.emplace_back(SYNCHRONOUS, std::move(packet));
  }

  void AddWrite(IoMode mode, int rv) { writes_.emplace_back(mode, rv); }

  // Returns the packet to be written at position |pos|.
  quic::QuicReceivedPacket* GetWrite(size_t pos) {
    return writes_[pos].packet.get();
  }

  bool AtEof() {
    return socket_data_->AllReadDataConsumed() &&
           socket_data_->AllWriteDataConsumed();
  }

  void ProcessPacket(std::unique_ptr<quic::QuicReceivedPacket> packet) {
    connection_->ProcessUdpPacket(ToQuicSocketAddress(self_addr_),
                                  ToQuicSocketAddress(peer_addr_), *packet);
  }

  // Configures the test fixture to use the list of expected writes.
  void Initialize() {
    mock_writes_ = std::make_unique<MockWrite[]>(writes_.size());
    for (size_t i = 0; i < writes_.size(); i++) {
      if (writes_[i].packet == nullptr) {
        mock_writes_[i] = MockWrite(writes_[i].mode, writes_[i].rv, i);
      } else {
        mock_writes_[i] = MockWrite(writes_[i].mode, writes_[i].packet->data(),
                                    writes_[i].packet->length());
      }
    }

    socket_data_ = std::make_unique<StaticSocketDataProvider>(
        base::span<MockRead>(),
        base::make_span(mock_writes_.get(), writes_.size()));
    socket_data_->set_printer(&printer_);

    auto socket = std::make_unique<MockUDPClientSocket>(socket_data_.get(),
                                                        NetLog::Get());
    socket->Connect(peer_addr_);
    runner_ = base::MakeRefCounted<TestTaskRunner>(&clock_);
    send_algorithm_ = new quic::test::MockSendAlgorithm();
    EXPECT_CALL(*send_algorithm_, InRecovery()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(*send_algorithm_, OnCongestionEvent(_, _, _, _, _, _, _))
        .Times(AnyNumber());
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

    connection_ = new TestQuicConnection(
        quic::test::SupportedVersions(version_), connection_id_, peer_addr_,
        helper_.get(), alarm_factory_.get(),
        new QuicChromiumPacketWriter(
            socket.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get()),
        connection_id_generator_);
    connection_->set_visitor(&visitor_);
    connection_->SetSendAlgorithm(send_algorithm_);

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
        connection_, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        &transport_security_state_, &ssl_config_service_,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)),
        QuicSessionAliasKey(
            url::SchemeHostPort(),
            QuicSessionKey(kDefaultServerHostName, kDefaultServerPort,
                           PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                           SessionUsage::kDestination, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false)),
        /*require_confirmation=*/false,
        /*migrate_session_early_v2=*/false,
        /*migrate_session_on_network_change_v2=*/false,
        /*default_network=*/handles::kInvalidNetworkHandle,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/false, /*allow_port_migration=*/false,
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
    session_->Initialize();

    // Blackhole QPACK decoder stream instead of constructing mock writes.
    session_->qpack_decoder()->set_qpack_stream_sender_delegate(
        &noop_qpack_stream_sender_delegate_);

    TestCompletionCallback callback;
    session_->CryptoConnect(callback.callback());
    stream_ = std::make_unique<QuicHttpStream>(
        session_->CreateHandle(
            url::SchemeHostPort(url::kHttpsScheme, "www.example.org", 443)),
        /*dns_aliases=*/std::set<std::string>());
  }

  void SetRequest(const string& method,
                  const string& path,
                  RequestPriority priority) {
    request_headers_ = client_maker_.GetRequestHeaders(method, "https", path);
  }

  void SetResponse(const string& status, const string& body) {
    response_headers_ = server_maker_.GetResponseHeaders(status);
    response_data_ = body;
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructClientDataPacket(
      uint64_t packet_number,
      bool fin,
      std::string_view data) {
    return client_maker_.Packet(packet_number)
        .AddStreamFrame(stream_id_, fin, data)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      bool fin,
      std::string_view data) {
    return server_maker_.Packet(packet_number)
        .AddStreamFrame(stream_id_, fin, data)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> InnerConstructRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      RequestPriority request_priority,
      size_t* spdy_headers_frame_length) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, stream_id, fin, priority, std::move(request_headers_),
        spdy_headers_frame_length);
  }

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructRequestHeadersAndDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      RequestPriority request_priority,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_.MakeRequestHeadersAndMultipleDataFramesPacket(
        packet_number, stream_id, fin, priority, std::move(request_headers_),
        spdy_headers_frame_length, data_writes);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructRequestAndRstPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      RequestPriority request_priority,
      size_t* spdy_headers_frame_length,
      quic::QuicRstStreamErrorCode error_code) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_.MakeRequestHeadersAndRstPacket(
        packet_number, stream_id, fin, priority, std::move(request_headers_),
        spdy_headers_frame_length, error_code);
  }

  std::unique_ptr<quic::QuicReceivedPacket> InnerConstructResponseHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin,
      size_t* spdy_headers_frame_length) {
    return server_maker_.MakeResponseHeadersPacket(
        packet_number, stream_id, fin, std::move(response_headers_),
        spdy_headers_frame_length);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructResponseHeadersPacket(
      uint64_t packet_number,
      bool fin,
      size_t* spdy_headers_frame_length) {
    return InnerConstructResponseHeadersPacket(packet_number, stream_id_, fin,
                                               spdy_headers_frame_length);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructResponseTrailersPacket(
      uint64_t packet_number,
      bool fin,
      quiche::HttpHeaderBlock trailers,
      size_t* spdy_headers_frame_length) {
    return server_maker_.MakeResponseHeadersPacket(packet_number, stream_id_,
                                                   fin, std::move(trailers),
                                                   spdy_headers_frame_length);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructClientRstStreamErrorPacket(
      uint64_t packet_number) {
    return client_maker_.Packet(packet_number)
        .AddStopSendingFrame(stream_id_, quic::QUIC_ERROR_PROCESSING_STREAM)
        .AddRstStreamFrame(stream_id_, quic::QUIC_ERROR_PROCESSING_STREAM)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstStreamPacket(
      uint64_t packet_number) {
    return client_maker_.Packet(packet_number)
        .AddAckFrame(/*first_received=*/1, /*largest_received=*/2,
                     /*smallest_received=*/1)
        .AddStopSendingFrame(stream_id_, quic::QUIC_STREAM_CANCELLED)
        .AddRstStreamFrame(stream_id_, quic::QUIC_STREAM_CANCELLED)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructClientAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received) {
    return client_maker_.Packet(packet_number)
        .AddAckFrame(1, largest_received, smallest_received)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked) {
    return server_maker_.Packet(packet_number)
        .AddAckFrame(largest_received, smallest_received, least_unacked)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket() {
    return client_maker_.MakeInitialSettingsPacket(1);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      int packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number);
  }

  std::string ConstructDataHeader(size_t body_len) {
    quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
        body_len, quiche::SimpleBufferAllocator::Get());
    return std::string(buffer.data(), buffer.size());
  }

  void ExpectLoadTimingValid(const LoadTimingInfo& load_timing_info,
                             bool session_reused) {
    EXPECT_EQ(session_reused, load_timing_info.socket_reused);
    if (session_reused) {
      ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
    } else {
      ExpectConnectTimingHasTimes(
          load_timing_info.connect_timing,
          CONNECT_TIMING_HAS_SSL_TIMES | CONNECT_TIMING_HAS_DNS_TIMES);
    }
    ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::test::QuicFlagSaver saver_;

  const quic::ParsedQuicVersion version_;

  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLog::Get(), NetLogSourceType::NONE)};
  RecordingNetLogObserver net_log_observer_;
  scoped_refptr<TestTaskRunner> runner_;
  std::unique_ptr<MockWrite[]> mock_writes_;
  quic::MockClock clock_;
  std::unique_ptr<QuicChromiumConnectionHelper> helper_;
  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  testing::StrictMock<quic::test::MockQuicConnectionVisitor> visitor_;
  std::unique_ptr<UploadDataStream> upload_data_stream_;
  std::unique_ptr<QuicHttpStream> stream_;
  TransportSecurityState transport_security_state_;
  SSLConfigServiceDefaults ssl_config_service_;

  // Must outlive `send_algorithm_` and `connection_`.
  std::unique_ptr<QuicChromiumClientSession> session_;
  raw_ptr<quic::test::MockSendAlgorithm> send_algorithm_;
  raw_ptr<TestQuicConnection> connection_;

  quic::QuicCryptoClientConfig crypto_config_;
  TestCompletionCallback callback_;
  HttpRequestInfo request_;
  HttpRequestHeaders headers_;
  HttpResponseInfo response_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  quiche::HttpHeaderBlock request_headers_;
  quiche::HttpHeaderBlock response_headers_;
  string request_data_;
  string response_data_;

  const quic::QuicStreamId stream_id_;

  const quic::QuicConnectionId connection_id_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  IPEndPoint self_addr_;
  IPEndPoint peer_addr_;
  quic::test::MockRandom random_generator_{0};
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  std::unique_ptr<StaticSocketDataProvider> socket_data_;
  QuicPacketPrinter printer_;
  std::vector<PacketToWrite> writes_;
  quic::test::MockConnectionIdGenerator connection_id_generator_;
  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicHttpStreamTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicHttpStreamTest, RenewStreamForAuth) {
  Initialize();
  EXPECT_EQ(nullptr, stream_->RenewStreamForAuth());
}

TEST_P(QuicHttpStreamTest, CanReuseConnection) {
  Initialize();
  EXPECT_FALSE(stream_->CanReuseConnection());
}

TEST_P(QuicHttpStreamTest, DisableConnectionMigrationForStream) {
  request_.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  Initialize();
  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  QuicChromiumClientStream::Handle* client_stream =
      QuicHttpStreamPeer::GetQuicChromiumClientStream(stream_.get());
  EXPECT_FALSE(client_stream->can_migrate_to_cellular_network());
}

TEST_P(QuicHttpStreamTest, GetRequest) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_header_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_header_frame_length));

  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  // Make sure getting load timing from the stream early does not crash.
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream_->GetLoadTimingInfo(&load_timing_info));
  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));

  SetResponse("404", string());
  size_t spdy_response_header_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, kFin, &spdy_response_header_frame_length));

  // Now that the headers have been processed, the callback will return.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  EXPECT_FALSE(response_.response_time.is_null());
  EXPECT_FALSE(response_.request_time.is_null());

  // There is no body, so this should return immediately.
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  EXPECT_TRUE(stream_->GetLoadTimingInfo(&load_timing_info));
  ExpectLoadTimingValid(load_timing_info, /*session_reused=*/false);

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_header_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_header_frame_length),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, LoadTimingTwoRequests) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_header_frame_length;

  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_header_frame_length));

  // SetRequest() again for second request as |request_headers_| was moved.
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(1), kFin,
      DEFAULT_PRIORITY, &spdy_request_header_frame_length));
  AddWrite(
      ConstructClientAckPacket(packet_number++, 3, 1));  // Ack the responses.

  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");
  // Start first request.
  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Start a second request.
  QuicHttpStream stream2(session_->CreateHandle(url::SchemeHostPort(
                             url::kHttpsScheme, "www.example.org", 443)),
                         {} /* dns_aliases */);
  TestCompletionCallback callback2;
  stream2.RegisterRequest(&request_);
  EXPECT_EQ(
      OK, stream2.InitializeStream(true, DEFAULT_PRIORITY, net_log_with_source_,
                                   callback2.callback()));
  EXPECT_EQ(OK,
            stream2.SendRequest(headers_, &response_, callback2.callback()));

  // Ack both requests.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));
  size_t spdy_response_header_frame_length;
  SetResponse("200", string());
  ProcessPacket(InnerConstructResponseHeadersPacket(
      2, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      &spdy_response_header_frame_length));

  // Now that the headers have been processed, the callback will return.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response_.headers->response_code());

  // There is no body, so this should return immediately.
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream_->GetLoadTimingInfo(&load_timing_info));
  ExpectLoadTimingValid(load_timing_info, /*session_reused=*/false);

  // SetResponse() again for second request as |response_headers_| was moved.
  SetResponse("200", string());
  EXPECT_THAT(stream2.ReadResponseHeaders(callback2.callback()),
              IsError(ERR_IO_PENDING));

  ProcessPacket(InnerConstructResponseHeadersPacket(
      3, GetNthClientInitiatedBidirectionalStreamId(1), kFin,
      &spdy_response_header_frame_length));

  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  // There is no body, so this should return immediately.
  EXPECT_EQ(0,
            stream2.ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                     callback2.callback()));
  EXPECT_TRUE(stream2.IsResponseBodyComplete());

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(stream2.GetLoadTimingInfo(&load_timing_info2));
  ExpectLoadTimingValid(load_timing_info2, /*session_reused=*/true);
}

// QuicHttpStream does not currently support trailers. It should ignore
// trailers upon receiving them.
TEST_P(QuicHttpStreamTest, GetRequestWithTrailers) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_header_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_header_frame_length));
  AddWrite(
      ConstructClientAckPacket(packet_number++, 3, 1));  // Ack the data packet.

  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");
  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));
  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));

  SetResponse("200", string());

  // Send the response headers.
  size_t spdy_response_header_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_header_frame_length));
  // Now that the headers have been processed, the callback will return.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  EXPECT_FALSE(response_.response_time.is_null());
  EXPECT_FALSE(response_.request_time.is_null());

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, !kFin, header + kResponseBody));
  quiche::HttpHeaderBlock trailers;
  size_t spdy_trailers_frame_length;
  trailers["foo"] = "bar";
  ProcessPacket(ConstructResponseTrailersPacket(4, kFin, std::move(trailers),
                                                &spdy_trailers_frame_length));

  // Make sure trailers are processed.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());

  EXPECT_EQ(OK,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_header_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_header_frame_length +
                                 strlen(kResponseBody) + header.length() +
                                 +spdy_trailers_frame_length),
            stream_->GetTotalReceivedBytes());
  // Check that NetLog was filled as expected.
  auto entries = net_log_observer_.GetEntries();
  size_t pos = ExpectLogContainsSomewhere(
      entries, /*min_offset=*/0,
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_SEND_REQUEST_HEADERS,
      NetLogEventPhase::NONE);
  pos = ExpectLogContainsSomewhere(
      entries, /*min_offset=*/pos,
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_SEND_REQUEST_HEADERS,
      NetLogEventPhase::NONE);
  ExpectLogContainsSomewhere(
      entries, /*min_offset=*/pos,
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_SEND_REQUEST_HEADERS,
      NetLogEventPhase::NONE);
}

TEST_P(QuicHttpStreamTest, ElideHeadersInNetLog) {
  Initialize();

  net_log_observer_.SetObserverCaptureMode(NetLogCaptureMode::kDefault);

  // Send first request.
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");
  headers_.SetHeader(HttpRequestHeaders::kCookie, "secret");

  size_t spdy_request_header_frame_length;
  int outgoing_packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(outgoing_packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      outgoing_packet_number++, stream_id_, kFin, DEFAULT_PRIORITY,
      &spdy_request_header_frame_length));

  stream_->RegisterRequest(&request_);
  EXPECT_THAT(
      stream_->InitializeStream(true, DEFAULT_PRIORITY, net_log_with_source_,
                                callback_.callback()),
      IsOk());
  EXPECT_THAT(stream_->SendRequest(headers_, &response_, callback_.callback()),
              IsOk());
  int incoming_packet_number = 1;
  ProcessPacket(ConstructServerAckPacket(incoming_packet_number++, 1, 1,
                                         1));  // Ack the request.

  // Process first response.
  SetResponse("200", string());
  response_headers_["set-cookie"] = "secret";
  size_t spdy_response_header_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      incoming_packet_number++, kFin, &spdy_response_header_frame_length));
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());

  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  EXPECT_TRUE(response_.headers->HasHeaderValue("set-cookie", "secret"));

  net_log_observer_.SetObserverCaptureMode(
      NetLogCaptureMode::kIncludeSensitive);

  // Send second request.
  quic::QuicStreamId stream_id = GetNthClientInitiatedBidirectionalStreamId(1);
  request_.url = GURL("https://www.example.org/foo");

  AddWrite(InnerConstructRequestHeadersPacket(
      outgoing_packet_number++, stream_id, kFin, DEFAULT_PRIORITY,
      &spdy_request_header_frame_length));

  auto stream = std::make_unique<QuicHttpStream>(
      session_->CreateHandle(
          url::SchemeHostPort(url::kHttpsScheme, "www.example.org/foo", 443)),
      /*dns_aliases=*/std::set<std::string>());
  stream->RegisterRequest(&request_);
  EXPECT_THAT(
      stream->InitializeStream(true, DEFAULT_PRIORITY, net_log_with_source_,
                               callback_.callback()),
      IsOk());
  EXPECT_THAT(stream->SendRequest(headers_, &response_, callback_.callback()),
              IsOk());
  ProcessPacket(ConstructServerAckPacket(incoming_packet_number++, 1, 1,
                                         1));  // Ack the request.

  // Process second response.
  SetResponse("200", string());
  response_headers_["set-cookie"] = "secret";
  ProcessPacket(InnerConstructResponseHeadersPacket(
      incoming_packet_number++, stream_id, kFin,
      &spdy_response_header_frame_length));
  EXPECT_THAT(stream->ReadResponseHeaders(callback_.callback()), IsOk());

  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  EXPECT_TRUE(response_.headers->HasHeaderValue("set-cookie", "secret"));

  EXPECT_TRUE(AtEof());

  // Check that sensitive header value were stripped
  // for the first transaction (logged with NetLogCaptureMode::kDefault)
  // but not for the second (logged with NetLogCaptureMode::kIncludeSensitive).
  auto entries =
      net_log_observer_.GetEntriesWithType(NetLogEventType::HTTP3_HEADERS_SENT);
  ASSERT_EQ(2u, entries.size());
  EXPECT_TRUE(
      CheckHeader(entries[0].params, "cookie", "[6 bytes were stripped]"));
  EXPECT_TRUE(CheckHeader(entries[1].params, "cookie", "secret"));

  entries = net_log_observer_.GetEntriesWithType(
      NetLogEventType::HTTP3_HEADERS_DECODED);
  ASSERT_EQ(2u, entries.size());
  EXPECT_TRUE(
      CheckHeader(entries[0].params, "set-cookie", "[6 bytes were stripped]"));
  EXPECT_TRUE(CheckHeader(entries[1].params, "set-cookie", "secret"));
}

// Regression test for http://crbug.com/288128
TEST_P(QuicHttpStreamTest, GetRequestLargeResponse) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));

  response_headers_[":status"] = "200";
  response_headers_[":version"] = "HTTP/1.1";
  response_headers_["content-type"] = "text/plain";
  response_headers_["big6"] = string(1000, 'x');  // Lots of x's.

  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, kFin, &spdy_response_headers_frame_length));

  // Now that the headers have been processed, the callback will return.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length),
            stream_->GetTotalReceivedBytes());
}

// Regression test for http://crbug.com/409101
TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendRequest) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  session_->connection()->CloseConnection(
      quic::QUIC_NO_ERROR, "test", quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  EXPECT_EQ(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

// Regression test for http://crbug.com/584441
TEST_P(QuicHttpStreamTest, GetSSLInfoAfterSessionClosed) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  SSLInfo ssl_info;
  EXPECT_FALSE(ssl_info.is_valid());
  stream_->GetSSLInfo(&ssl_info);
  EXPECT_TRUE(ssl_info.is_valid());

  session_->connection()->CloseConnection(
      quic::QUIC_NO_ERROR, "test", quic::ConnectionCloseBehavior::SILENT_CLOSE);

  SSLInfo ssl_info2;
  stream_->GetSSLInfo(&ssl_info2);
  EXPECT_TRUE(ssl_info2.is_valid());
}

TEST_P(QuicHttpStreamTest, GetAlternativeService) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  AlternativeService alternative_service;
  EXPECT_TRUE(stream_->GetAlternativeService(&alternative_service));
  EXPECT_EQ(AlternativeService(kProtoQUIC, "www.example.org", 443),
            alternative_service);

  session_->connection()->CloseConnection(
      quic::QUIC_NO_ERROR, "test", quic::ConnectionCloseBehavior::SILENT_CLOSE);

  AlternativeService alternative_service2;
  EXPECT_TRUE(stream_->GetAlternativeService(&alternative_service2));
  EXPECT_EQ(AlternativeService(kProtoQUIC, "www.example.org", 443),
            alternative_service2);
}

TEST_P(QuicHttpStreamTest, LogGranularQuicConnectionError) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  AddWrite(ConstructAckAndRstStreamPacket(3));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));

  quic::QuicConnectionCloseFrame frame;
  frame.quic_error_code = quic::QUIC_PEER_GOING_AWAY;
  session_->connection()->OnConnectionCloseFrame(frame);

  NetErrorDetails details;
  EXPECT_EQ(quic::QUIC_NO_ERROR, details.quic_connection_error);
  stream_->PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QUIC_PEER_GOING_AWAY, details.quic_connection_error);
}

TEST_P(QuicHttpStreamTest, LogGranularQuicErrorIfHandshakeNotConfirmed) {
  // By default the test setup defaults handshake to be confirmed. Manually set
  // it to be not confirmed.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));

  quic::QuicConnectionCloseFrame frame;
  frame.quic_error_code = quic::QUIC_PEER_GOING_AWAY;
  session_->connection()->OnConnectionCloseFrame(frame);

  NetErrorDetails details;
  stream_->PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QUIC_PEER_GOING_AWAY, details.quic_connection_error);
}

// Regression test for http://crbug.com/409871
TEST_P(QuicHttpStreamTest, SessionClosedBeforeReadResponseHeaders) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  session_->connection()->CloseConnection(
      quic::QUIC_NO_ERROR, "test", quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_NE(OK, stream_->ReadResponseHeaders(callback_.callback()));

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendPostRequest) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));

  std::string header = ConstructDataHeader(strlen(kUploadData));
  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));

  AddWrite(ConstructClientAckPacket(packet_number++, 3, 1));

  Initialize();

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring(kUploadData)));
  upload_data_stream_ =
      std::make_unique<ElementsUploadDataStream>(std::move(element_readers), 0);
  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_THAT(request_.upload_data_stream->Init(CompletionOnceCallback(),
                                                NetLogWithSource()),
              IsOk());

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack both packets in the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  // Send the response headers (but not the body).
  SetResponse("200", string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have already arrived.
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header2 = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, kFin, header2 + kResponseBody));
  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData) + header.length()),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody) + header2.length()),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendPostRequestAndReceiveSoloFin) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  std::string header = ConstructDataHeader(strlen(kUploadData));
  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));

  AddWrite(ConstructClientAckPacket(packet_number++, 3, 1));

  Initialize();

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring(kUploadData)));
  upload_data_stream_ =
      std::make_unique<ElementsUploadDataStream>(std::move(element_readers), 0);
  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_THAT(request_.upload_data_stream->Init(CompletionOnceCallback(),
                                                NetLogWithSource()),
              IsOk());

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack both packets in the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  // Send the response headers (but not the body).
  SetResponse("200", string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have already arrived.
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header2 = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, !kFin, header2 + kResponseBody));
  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  ProcessPacket(ConstructServerDataPacket(4, kFin, ""));
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData) + header.length()),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody) + header2.length()),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequest) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  std::string header = ConstructDataHeader(chunk_size);
  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));
  AddWrite(
      ConstructClientDataPacket(packet_number++, kFin, {header + kUploadData}));

  AddWrite(ConstructClientAckPacket(packet_number++, 3, 1));
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());
  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    false);

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  // Ack both packets in the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  // Send the response headers (but not the body).
  SetResponse("200", string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have already arrived.
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header2 = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, kFin, header2 + kResponseBody));

  // Since the body has already arrived, this should return immediately.
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData) * 2 + header.length() * 2),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody) + header2.length()),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestWithFinalEmptyDataPacket) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  std::string header = ConstructDataHeader(chunk_size);

  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));
  AddWrite(ConstructClientDataPacket(packet_number++, kFin, ""));
  AddWrite(ConstructClientAckPacket(packet_number++, 3, 1));
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());
  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    false);

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(""), true);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  // Send the response headers (but not the body).
  SetResponse("200", string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have already arrived.
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header2 = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, kFin, header2 + kResponseBody));

  // The body has arrived, but it is delivered asynchronously
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData) + header.length()),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody) + header2.length()),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestWithOneEmptyDataPacket) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  AddWrite(ConstructClientDataPacket(packet_number++, kFin, ""));
  AddWrite(ConstructClientAckPacket(packet_number++, 3, 1));
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(""), true);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  // Send the response headers (but not the body).
  SetResponse("200", string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have already arrived.
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, kFin, header + kResponseBody));

  // The body has arrived, but it is delivered asynchronously
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody) + header.length()),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestAbortedByResetStream) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;

  AddWrite(ConstructInitialSettingsPacket(packet_number++));

  std::string header = ConstructDataHeader(chunk_size);
  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));
  AddWrite(ConstructClientAckPacket(packet_number++, 3, 1));
  AddWrite(client_maker_.Packet(packet_number++)
               .AddAckFrame(/*first_received=*/1, /*largest_received=*/4,
                            /*smallest_received=*/1)
               .AddRstStreamFrame(stream_id_, quic::QUIC_STREAM_NO_ERROR)
               .Build());

  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());
  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    false);

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_THAT(request_.upload_data_stream->Init(
                  TestCompletionCallback().callback(), NetLogWithSource()),
              IsOk());
  stream_->RegisterRequest(&request_);
  ASSERT_THAT(
      stream_->InitializeStream(false, DEFAULT_PRIORITY, net_log_with_source_,
                                callback_.callback()),
      IsOk());
  ASSERT_THAT(stream_->SendRequest(headers_, &response_, callback_.callback()),
              IsError(ERR_IO_PENDING));

  // Ack both packets in the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));

  // Send the response headers (but not the body).
  SetResponse("200", string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  std::string header2 = ConstructDataHeader(strlen(kResponseBody));
  ProcessPacket(ConstructServerDataPacket(3, kFin, header2 + kResponseBody));

  // The server uses a STOP_SENDING frame to notify the client that it does not
  // need any further data to fully process the request.
  ProcessPacket(server_maker_.Packet(4)
                    .AddStopSendingFrame(stream_id_, quic::QUIC_STREAM_NO_ERROR)
                    .Build());

  // Finish feeding request body to QuicHttpStream.  Data will be discarded.
  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  // Verify response.
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()), IsOk());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData) + header.length()),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody) + header2.length()),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, DestroyedEarly) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  AddWrite(ConstructAckAndRstStreamPacket(packet_number++));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));
  EXPECT_THAT(stream_->ReadResponseHeaders(
                  base::BindOnce(&QuicHttpStreamTest::CloseStream,
                                 base::Unretained(this), stream_.get())),
              IsError(ERR_IO_PENDING));

  // Send the response with a body.
  SetResponse("404", "hello world!");
  // In the course of processing this packet, the QuicHttpStream close itself.
  size_t response_size = 0;
  ProcessPacket(ConstructResponseHeadersPacket(2, !kFin, &response_size));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  // The stream was closed after receiving the headers.
  EXPECT_EQ(static_cast<int64_t>(response_size),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, Priority) {
  SetRequest("GET", "/", MEDIUM);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), kFin,
      MEDIUM, &spdy_request_headers_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, MEDIUM, net_log_with_source_,
                                          callback_.callback()));

  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));
  EXPECT_THAT(stream_->ReadResponseHeaders(callback_.callback()),
              IsError(ERR_IO_PENDING));

  // Send the response with a body.
  SetResponse("404", "hello world!");
  size_t response_size = 0;
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin, &response_size));

  EXPECT_EQ(OK, callback_.WaitForResult());

  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(response_size),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SessionClosedDuringDoLoop) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  std::string header = ConstructDataHeader(strlen(kUploadData));
  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));

  // Second data write will result in a synchronous failure which will close
  // the session.
  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    false);
  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  QuicHttpStream* stream = stream_.get();
  DeleteStreamCallback delete_stream_callback(std::move(stream_));
  // SendRequest() completes asynchronously after the final chunk is added.
  // Error does not surface yet since packet write is triggered by a packet
  // flusher that tries to bundle request body writes.
  ASSERT_EQ(ERR_IO_PENDING,
            stream->SendRequest(headers_, &response_, callback_.callback()));
  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);
  int rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  // Error will be surfaced once an attempt to read the response occurs.
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream->ReadResponseHeaders(callback_.callback()));
}

TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendHeadersComplete) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructInitialSettingsPacket());
  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Error will be surfaced once |upload_data_stream| triggers the next write.
  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());

  EXPECT_LE(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendHeadersCompleteReadResponse) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  AddWrite(ConstructInitialSettingsPacket());
  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);

  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Error will be surfaced once an attempt to read the response occurs.
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream_->ReadResponseHeaders(callback_.callback()));

  EXPECT_LE(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendBodyComplete) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);
  // Error does not surface yet since packet write is triggered by a packet
  // flusher that tries to bundle request body writes.
  ASSERT_EQ(OK, callback_.WaitForResult());
  // Error will be surfaced once an attempt to read the response occurs.
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream_->ReadResponseHeaders(callback_.callback()));

  EXPECT_LE(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendBundledBodyComplete) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  std::string header = ConstructDataHeader(strlen(kUploadData));
  AddWrite(ConstructRequestHeadersAndDataFramesPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      {header, kUploadData}));

  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  upload_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
  auto* chunked_upload_stream =
      static_cast<ChunkedUploadDataStream*>(upload_data_stream_.get());

  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    false);

  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  ASSERT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  chunked_upload_stream->AppendData(base::byte_span_from_cstring(kUploadData),
                                    true);

  // Error does not surface yet since packet write is triggered by a packet
  // flusher that tries to bundle request body writes.
  ASSERT_EQ(OK, callback_.WaitForResult());
  // Error will be surfaced once an attempt to read the response occurs.
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream_->ReadResponseHeaders(callback_.callback()));

  EXPECT_LE(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, DataReadErrorSynchronous) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(ConstructRequestAndRstPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length,
      quic::QUIC_ERROR_PROCESSING_STREAM));

  Initialize();

  upload_data_stream_ = std::make_unique<ReadErrorUploadDataStream>(
      ReadErrorUploadDataStream::FailureMode::SYNC);
  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  int result = stream_->SendRequest(headers_, &response_, callback_.callback());
  EXPECT_THAT(result, IsError(ERR_FAILED));

  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes includes only headers.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, DataReadErrorAsynchronous) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  int packet_number = 1;
  AddWrite(ConstructInitialSettingsPacket(packet_number++));
  AddWrite(InnerConstructRequestHeadersPacket(
      packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), !kFin,
      DEFAULT_PRIORITY, &spdy_request_headers_frame_length));
  AddWrite(ConstructClientRstStreamErrorPacket(packet_number++));

  Initialize();

  upload_data_stream_ = std::make_unique<ReadErrorUploadDataStream>(
      ReadErrorUploadDataStream::FailureMode::ASYNC);
  request_.method = "POST";
  request_.url = GURL("https://www.example.org/");
  request_.upload_data_stream = upload_data_stream_.get();
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback(), NetLogWithSource()));

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(false, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));

  int result = stream_->SendRequest(headers_, &response_, callback_.callback());

  ProcessPacket(ConstructServerAckPacket(1, 1, 1, 1));
  SetResponse("200", string());

  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.GetResult(result), IsError(ERR_FAILED));

  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes includes only headers.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, GetAcceptChViaAlps) {
  AddWrite(ConstructInitialSettingsPacket());
  Initialize();

  base::HistogramTester histogram_tester;

  session_->OnAcceptChFrameReceivedViaAlps(
      {{{"https://www.example.org", "Sec-CH-UA-Platform"}}});

  request_.method = "GET";
  request_.url = GURL("https://www.example.org/foo");

  stream_->RegisterRequest(&request_);
  EXPECT_EQ(OK, stream_->InitializeStream(true, DEFAULT_PRIORITY,
                                          net_log_with_source_,
                                          callback_.callback()));
  EXPECT_EQ("Sec-CH-UA-Platform", stream_->GetAcceptChViaAlps());
  EXPECT_TRUE(AtEof());

  histogram_tester.ExpectBucketCount(
      "Net.QuicSession.AcceptChFrameReceivedViaAlps", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Net.QuicSession.AcceptChFrameReceivedViaAlps", 1);
  histogram_tester.ExpectBucketCount("Net.QuicSession.AcceptChForOrigin", 1, 1);
  histogram_tester.ExpectTotalCount("Net.QuicSession.AcceptChForOrigin", 1);
}

}  // namespace net::test
