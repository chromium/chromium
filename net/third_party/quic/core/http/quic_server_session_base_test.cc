// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_server_session_base.h"

#include <cstdint>
#include <memory>

#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/proto/cached_network_parameters.pb.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/fake_proof_source.h"
#include "net/third_party/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_crypto_server_config_peer.h"
#include "net/third_party/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/third_party/quic/test_tools/quic_server_session_base_peer.h"
#include "net/third_party/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_sustained_bandwidth_recorder_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quic/tools/quic_simple_server_stream.h"

using testing::_;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class TestServerSession : public QuicServerSessionBase {
 public:
  TestServerSession(const QuicConfig& config,
                    QuicConnection* connection,
                    QuicSession::Visitor* visitor,
                    QuicCryptoServerStream::Helper* helper,
                    const QuicCryptoServerConfig* crypto_config,
                    QuicCompressedCertsCache* compressed_certs_cache,
                    QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicServerSessionBase(config,
                              CurrentSupportedVersions(),
                              connection,
                              visitor,
                              helper,
                              crypto_config,
                              compressed_certs_cache),
        quic_simple_server_backend_(quic_simple_server_backend) {}

  ~TestServerSession() override { delete connection(); };

 protected:
  QuicSpdyStream* CreateIncomingStream(QuicStreamId id) override {
    if (!ShouldCreateIncomingStream(id)) {
      return nullptr;
    }
    QuicSpdyStream* stream = new QuicSimpleServerStream(
        id, this, BIDIRECTIONAL, quic_simple_server_backend_);
    ActivateStream(QuicWrapUnique(stream));
    return stream;
  }

  QuicSpdyStream* CreateOutgoingBidirectionalStream() override {
    DCHECK(false);
    return nullptr;
  }

  QuicSpdyStream* CreateOutgoingUnidirectionalStream() override {
    if (!ShouldCreateOutgoingStream()) {
      return nullptr;
    }

    QuicSpdyStream* stream = new QuicSimpleServerStream(
        GetNextOutgoingStreamId(), this, WRITE_UNIDIRECTIONAL,
        quic_simple_server_backend_);
    ActivateStream(QuicWrapUnique(stream));
    return stream;
  }

  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override {
    return new QuicCryptoServerStream(
        crypto_config, compressed_certs_cache,
        GetQuicReloadableFlag(enable_quic_stateless_reject_support), this,
        stream_helper());
  }

 private:
  QuicSimpleServerBackend*
      quic_simple_server_backend_;  // Owned by QuicServerSessionBaseTest
};

const size_t kMaxStreamsForTest = 10;

class QuicServerSessionBaseTest : public QuicTestWithParam<ParsedQuicVersion> {
 protected:
  QuicServerSessionBaseTest()
      : QuicServerSessionBaseTest(crypto_test_utils::ProofSourceForTesting()) {}

  explicit QuicServerSessionBaseTest(std::unique_ptr<ProofSource> proof_source)
      : crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(),
                       std::move(proof_source),
                       KeyExchangeSource::Default(),
                       TlsServerHandshaker::CreateSslCtx()),
        compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize) {
    config_.SetMaxIncomingDynamicStreamsToSend(kMaxStreamsForTest);
    QuicConfigPeer::SetReceivedMaxIncomingDynamicStreams(&config_,
                                                         kMaxStreamsForTest);
    config_.SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    config_.SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);

    ParsedQuicVersionVector supported_versions = SupportedVersions(GetParam());
    connection_ = new StrictMock<MockQuicConnection>(
        &helper_, &alarm_factory_, Perspective::IS_SERVER, supported_versions);
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    session_ = QuicMakeUnique<TestServerSession>(
        config_, connection_, &owner_, &stream_helper_, &crypto_config_,
        &compressed_certs_cache_, &memory_cache_backend_);
    MockClock clock;
    handshake_message_.reset(crypto_config_.AddDefaultConfig(
        QuicRandom::GetInstance(), &clock,
        QuicCryptoServerConfig::ConfigOptions()));
    session_->Initialize();
    QuicSessionPeer::GetMutableCryptoStream(session_.get())
        ->OnSuccessfulVersionNegotiation(supported_versions.front());
    visitor_ = QuicConnectionPeer::GetVisitor(connection_);
  }

  QuicStreamId GetNthClientInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthClientInitiatedStreamId(*session_, n);
  }

  QuicStreamId GetNthServerInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthServerInitiatedStreamId(*session_, n);
  }

  StrictMock<MockQuicSessionVisitor> owner_;
  StrictMock<MockQuicCryptoServerStreamHelper> stream_helper_;
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  QuicCompressedCertsCache compressed_certs_cache_;
  QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<TestServerSession> session_;
  std::unique_ptr<CryptoHandshakeMessage> handshake_message_;
  QuicConnectionVisitorInterface* visitor_;
};

// Compares CachedNetworkParameters.
MATCHER_P(EqualsProto, network_params, "") {
  CachedNetworkParameters reference(network_params);
  return (arg->bandwidth_estimate_bytes_per_second() ==
              reference.bandwidth_estimate_bytes_per_second() &&
          arg->bandwidth_estimate_bytes_per_second() ==
              reference.bandwidth_estimate_bytes_per_second() &&
          arg->max_bandwidth_estimate_bytes_per_second() ==
              reference.max_bandwidth_estimate_bytes_per_second() &&
          arg->max_bandwidth_timestamp_seconds() ==
              reference.max_bandwidth_timestamp_seconds() &&
          arg->min_rtt_ms() == reference.min_rtt_ms() &&
          arg->previous_connection_state() ==
              reference.previous_connection_state());
}

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicServerSessionBaseTest,
                        ::testing::ValuesIn(AllSupportedVersions()));
TEST_P(QuicServerSessionBaseTest, CloseStreamDueToReset) {
  // Open a stream, then reset it.
  // Send two bytes of payload to open it.
  QuicStreamFrame data1(GetNthClientInitiatedId(0), false, 0,
                        QuicStringPiece("HT"));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Send a reset (and expect the peer to send a RST in response).
  QuicRstStreamFrame rst1(kInvalidControlFrameId, GetNthClientInitiatedId(0),
                          QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(owner_, OnRstStreamReceived(_)).Times(1);
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(GetNthClientInitiatedId(0),
                                          QUIC_RST_ACKNOWLEDGEMENT));
  visitor_->OnRstStream(rst1);
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());

  // Send the same two bytes of payload in a new packet.
  visitor_->OnStreamFrame(data1);

  // The stream should not be re-opened.
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicServerSessionBaseTest, NeverOpenStreamDueToReset) {
  // Send a reset (and expect the peer to send a RST in response).
  QuicRstStreamFrame rst1(kInvalidControlFrameId, GetNthClientInitiatedId(0),
                          QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(owner_, OnRstStreamReceived(_)).Times(1);
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(GetNthClientInitiatedId(0),
                                          QUIC_RST_ACKNOWLEDGEMENT));
  visitor_->OnRstStream(rst1);
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());

  // Send two bytes of payload.
  QuicStreamFrame data1(GetNthClientInitiatedId(0), false, 0,
                        QuicStringPiece("HT"));
  visitor_->OnStreamFrame(data1);

  // The stream should never be opened, now that the reset is received.
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicServerSessionBaseTest, AcceptClosedStream) {
  // Send (empty) compressed headers followed by two bytes of data.
  QuicStreamFrame frame1(GetNthClientInitiatedId(0), false, 0,
                         QuicStringPiece("\1\0\0\0\0\0\0\0HT"));
  QuicStreamFrame frame2(GetNthClientInitiatedId(1), false, 0,
                         QuicStringPiece("\2\0\0\0\0\0\0\0HT"));
  visitor_->OnStreamFrame(frame1);
  visitor_->OnStreamFrame(frame2);
  EXPECT_EQ(2u, session_->GetNumOpenIncomingStreams());

  // Send a reset (and expect the peer to send a RST in response).
  QuicRstStreamFrame rst(kInvalidControlFrameId, GetNthClientInitiatedId(0),
                         QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(owner_, OnRstStreamReceived(_)).Times(1);
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(GetNthClientInitiatedId(0),
                                          QUIC_RST_ACKNOWLEDGEMENT));
  visitor_->OnRstStream(rst);

  // If we were tracking, we'd probably want to reject this because it's data
  // past the reset point of stream 3.  As it's a closed stream we just drop the
  // data on the floor, but accept the packet because it has data for stream 5.
  QuicStreamFrame frame3(GetNthClientInitiatedId(0), false, 2,
                         QuicStringPiece("TP"));
  QuicStreamFrame frame4(GetNthClientInitiatedId(1), false, 2,
                         QuicStringPiece("TP"));
  visitor_->OnStreamFrame(frame3);
  visitor_->OnStreamFrame(frame4);
  // The stream should never be opened, now that the reset is received.
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicServerSessionBaseTest, MaxOpenStreams) {
  // Test that the server refuses if a client attempts to open too many data
  // streams.  The server accepts slightly more than the negotiated stream limit
  // to deal with rare cases where a client FIN/RST is lost.

  // The slightly increased stream limit is set during config negotiation.  It
  // is either an increase of 10 over negotiated limit, or a fixed percentage
  // scaling, whichever is larger. Test both before continuing.
  session_->OnConfigNegotiated();
  EXPECT_LT(kMaxStreamsMultiplier * kMaxStreamsForTest,
            kMaxStreamsForTest + kMaxStreamsMinimumIncrement);
  EXPECT_EQ(kMaxStreamsForTest + kMaxStreamsMinimumIncrement,
            session_->max_open_incoming_streams());
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
  QuicStreamId stream_id = GetNthClientInitiatedId(0);
  // Open the max configured number of streams, should be no problem.
  for (size_t i = 0; i < kMaxStreamsForTest; ++i) {
    EXPECT_TRUE(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
        session_.get(), stream_id));
    stream_id += QuicSpdySessionPeer::NextStreamId(*session_);
  }

  // Open more streams: server should accept slightly more than the limit.
  for (size_t i = 0; i < kMaxStreamsMinimumIncrement; ++i) {
    EXPECT_TRUE(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
        session_.get(), stream_id));
    stream_id += QuicSpdySessionPeer::NextStreamId(*session_);
  }

  // Now violate the server's internal stream limit.
  stream_id += QuicSpdySessionPeer::NextStreamId(*session_);
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream_id, QUIC_REFUSED_STREAM));
  // Even if the connection remains open, the stream creation should fail.
  EXPECT_FALSE(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
      session_.get(), stream_id));
}

TEST_P(QuicServerSessionBaseTest, MaxAvailableStreams) {
  // Test that the server closes the connection if a client makes too many data
  // streams available.  The server accepts slightly more than the negotiated
  // stream limit to deal with rare cases where a client FIN/RST is lost.

  session_->OnConfigNegotiated();
  const size_t kAvailableStreamLimit = session_->MaxAvailableStreams();
  EXPECT_EQ(
      session_->max_open_incoming_streams() * kMaxAvailableStreamsMultiplier,
      session_->MaxAvailableStreams());
  // The protocol specification requires that there can be at least 10 times
  // as many available streams as the connection's maximum open streams.
  EXPECT_LE(10 * kMaxStreamsForTest, kAvailableStreamLimit);

  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
      session_.get(), GetNthClientInitiatedId(0)));

  // Establish available streams up to the server's limit.
  QuicStreamId next_id = QuicSpdySessionPeer::NextStreamId(*session_);
  const int kLimitingStreamId =
      GetNthClientInitiatedId(kAvailableStreamLimit + 1);
  EXPECT_TRUE(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
      session_.get(), kLimitingStreamId));

  // A further available stream will result in connection close.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_TOO_MANY_AVAILABLE_STREAMS, _, _));
  // This forces stream kLimitingStreamId + 2 to become available, which
  // violates the quota.
  EXPECT_FALSE(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
      session_.get(), kLimitingStreamId + 2 * next_id));
}

TEST_P(QuicServerSessionBaseTest, GetEvenIncomingError) {
  // Incoming streams on the server session must be odd.
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_STREAM_ID, _, _));
  EXPECT_EQ(nullptr, QuicServerSessionBasePeer::GetOrCreateDynamicStream(
                         session_.get(), GetNthServerInitiatedId(0)));
}

TEST_P(QuicServerSessionBaseTest, GetStreamDisconnected) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  // Don't create new streams if the connection is disconnected.
  QuicConnectionPeer::TearDownLocalConnectionState(connection_);
  EXPECT_QUIC_BUG(QuicServerSessionBasePeer::GetOrCreateDynamicStream(
                      session_.get(), GetNthClientInitiatedId(0)),
                  "ShouldCreateIncomingStream called when disconnected");
}

class MockQuicCryptoServerStream : public QuicCryptoServerStream {
 public:
  explicit MockQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      QuicServerSessionBase* session,
      QuicCryptoServerStream::Helper* helper)
      : QuicCryptoServerStream(
            crypto_config,
            compressed_certs_cache,
            GetQuicReloadableFlag(enable_quic_stateless_reject_support),
            session,
            helper) {}
  MockQuicCryptoServerStream(const MockQuicCryptoServerStream&) = delete;
  MockQuicCryptoServerStream& operator=(const MockQuicCryptoServerStream&) =
      delete;
  ~MockQuicCryptoServerStream() override {}

  MOCK_METHOD1(SendServerConfigUpdate,
               void(const CachedNetworkParameters* cached_network_parameters));
};

TEST_P(QuicServerSessionBaseTest, BandwidthEstimates) {
  // Test that bandwidth estimate updates are sent to the client, only when
  // bandwidth resumption is enabled, the bandwidth estimate has changed
  // sufficiently, enough time has passed,
  // and we don't have any other data to write.

  // Client has sent kBWRE connection option to trigger bandwidth resumption.
  QuicTagVector copt;
  copt.push_back(kBWRE);
  QuicConfigPeer::SetReceivedConnectionOptions(session_->config(), copt);
  session_->OnConfigNegotiated();
  EXPECT_TRUE(
      QuicServerSessionBasePeer::IsBandwidthResumptionEnabled(session_.get()));

  int32_t bandwidth_estimate_kbytes_per_second = 123;
  int32_t max_bandwidth_estimate_kbytes_per_second = 134;
  int32_t max_bandwidth_estimate_timestamp = 1122334455;
  const QuicString serving_region = "not a real region";
  session_->set_serving_region(serving_region);

  session_->UnregisterStreamPriority(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()),
      /*is_static=*/true);
  QuicServerSessionBasePeer::SetCryptoStream(session_.get(), nullptr);
  MockQuicCryptoServerStream* crypto_stream =
      new MockQuicCryptoServerStream(&crypto_config_, &compressed_certs_cache_,
                                     session_.get(), &stream_helper_);
  QuicServerSessionBasePeer::SetCryptoStream(session_.get(), crypto_stream);
  session_->RegisterStreamPriority(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()),
      /*is_static=*/true, QuicStream::kDefaultPriority);

  // Set some initial bandwidth values.
  QuicSentPacketManager* sent_packet_manager =
      QuicConnectionPeer::GetSentPacketManager(session_->connection());
  QuicSustainedBandwidthRecorder& bandwidth_recorder =
      QuicSentPacketManagerPeer::GetBandwidthRecorder(sent_packet_manager);
  // Seed an rtt measurement equal to the initial default rtt.
  RttStats* rtt_stats =
      const_cast<RttStats*>(sent_packet_manager->GetRttStats());
  rtt_stats->UpdateRtt(rtt_stats->initial_rtt(), QuicTime::Delta::Zero(),
                       QuicTime::Zero());
  QuicSustainedBandwidthRecorderPeer::SetBandwidthEstimate(
      &bandwidth_recorder, bandwidth_estimate_kbytes_per_second);
  QuicSustainedBandwidthRecorderPeer::SetMaxBandwidthEstimate(
      &bandwidth_recorder, max_bandwidth_estimate_kbytes_per_second,
      max_bandwidth_estimate_timestamp);
  // Queue up some pending data.
  session_->MarkConnectionLevelWriteBlocked(QuicUtils::GetCryptoStreamId(
      session_->connection()->transport_version()));
  EXPECT_TRUE(session_->HasDataToWrite());

  // There will be no update sent yet - not enough time has passed.
  QuicTime now = QuicTime::Zero();
  session_->OnCongestionWindowChange(now);

  // Bandwidth estimate has now changed sufficiently but not enough time has
  // passed to send a Server Config Update.
  bandwidth_estimate_kbytes_per_second =
      bandwidth_estimate_kbytes_per_second * 1.6;
  session_->OnCongestionWindowChange(now);

  // Bandwidth estimate has now changed sufficiently and enough time has passed,
  // but not enough packets have been sent.
  int64_t srtt_ms =
      sent_packet_manager->GetRttStats()->smoothed_rtt().ToMilliseconds();
  now = now + QuicTime::Delta::FromMilliseconds(
                  kMinIntervalBetweenServerConfigUpdatesRTTs * srtt_ms);
  session_->OnCongestionWindowChange(now);

  // The connection no longer has pending data to be written.
  session_->OnCanWrite();
  EXPECT_FALSE(session_->HasDataToWrite());
  session_->OnCongestionWindowChange(now);

  // Bandwidth estimate has now changed sufficiently, enough time has passed,
  // and enough packets have been sent.
  SerializedPacket packet(1 + kMinPacketsBetweenServerConfigUpdates,
                          PACKET_4BYTE_PACKET_NUMBER, nullptr, 1000, false,
                          false);
  sent_packet_manager->OnPacketSent(&packet, 0, now, NOT_RETRANSMISSION,
                                    HAS_RETRANSMITTABLE_DATA);

  // Verify that the proto has exactly the values we expect.
  CachedNetworkParameters expected_network_params;
  expected_network_params.set_bandwidth_estimate_bytes_per_second(
      bandwidth_recorder.BandwidthEstimate().ToBytesPerSecond());
  expected_network_params.set_max_bandwidth_estimate_bytes_per_second(
      bandwidth_recorder.MaxBandwidthEstimate().ToBytesPerSecond());
  expected_network_params.set_max_bandwidth_timestamp_seconds(
      bandwidth_recorder.MaxBandwidthTimestamp());
  expected_network_params.set_min_rtt_ms(session_->connection()
                                             ->sent_packet_manager()
                                             .GetRttStats()
                                             ->min_rtt()
                                             .ToMilliseconds());
  expected_network_params.set_previous_connection_state(
      CachedNetworkParameters::CONGESTION_AVOIDANCE);
  expected_network_params.set_timestamp(
      session_->connection()->clock()->WallNow().ToUNIXSeconds());
  expected_network_params.set_serving_region(serving_region);

  EXPECT_CALL(*crypto_stream,
              SendServerConfigUpdate(EqualsProto(expected_network_params)))
      .Times(1);
  EXPECT_CALL(*connection_, OnSendConnectionState(_)).Times(1);
  session_->OnCongestionWindowChange(now);
}

TEST_P(QuicServerSessionBaseTest, BandwidthResumptionExperiment) {
  if (GetParam().handshake_protocol == PROTOCOL_TLS1_3) {
    // This test relies on resumption, which is not currently supported by the
    // TLS handshake.
    // TODO(nharper): Add support for resumption to the TLS handshake.
    return;
  }
  // Test that if a client provides a CachedNetworkParameters with the same
  // serving region as the current server, and which was made within an hour of
  // now, that this data is passed down to the send algorithm.

  // Client has sent kBWRE connection option to trigger bandwidth resumption.
  QuicTagVector copt;
  copt.push_back(kBWRE);
  QuicConfigPeer::SetReceivedConnectionOptions(session_->config(), copt);

  const QuicString kTestServingRegion = "a serving region";
  session_->set_serving_region(kTestServingRegion);

  // Set the time to be one hour + one second from the 0 baseline.
  connection_->AdvanceTime(
      QuicTime::Delta::FromSeconds(kNumSecondsPerHour + 1));

  QuicCryptoServerStream* crypto_stream = static_cast<QuicCryptoServerStream*>(
      QuicSessionPeer::GetMutableCryptoStream(session_.get()));

  // No effect if no CachedNetworkParameters provided.
  EXPECT_CALL(*connection_, ResumeConnectionState(_, _)).Times(0);
  session_->OnConfigNegotiated();

  // No effect if CachedNetworkParameters provided, but different serving
  // regions.
  CachedNetworkParameters cached_network_params;
  cached_network_params.set_bandwidth_estimate_bytes_per_second(1);
  cached_network_params.set_serving_region("different serving region");
  crypto_stream->SetPreviousCachedNetworkParams(cached_network_params);
  EXPECT_CALL(*connection_, ResumeConnectionState(_, _)).Times(0);
  session_->OnConfigNegotiated();

  // Same serving region, but timestamp is too old, should have no effect.
  cached_network_params.set_serving_region(kTestServingRegion);
  cached_network_params.set_timestamp(0);
  crypto_stream->SetPreviousCachedNetworkParams(cached_network_params);
  EXPECT_CALL(*connection_, ResumeConnectionState(_, _)).Times(0);
  session_->OnConfigNegotiated();

  // Same serving region, and timestamp is recent: estimate is stored.
  cached_network_params.set_timestamp(
      connection_->clock()->WallNow().ToUNIXSeconds());
  crypto_stream->SetPreviousCachedNetworkParams(cached_network_params);
  EXPECT_CALL(*connection_, ResumeConnectionState(_, _)).Times(1);
  session_->OnConfigNegotiated();
}

TEST_P(QuicServerSessionBaseTest, BandwidthMaxEnablesResumption) {
  EXPECT_FALSE(
      QuicServerSessionBasePeer::IsBandwidthResumptionEnabled(session_.get()));

  // Client has sent kBWMX connection option to trigger bandwidth resumption.
  QuicTagVector copt;
  copt.push_back(kBWMX);
  QuicConfigPeer::SetReceivedConnectionOptions(session_->config(), copt);
  session_->OnConfigNegotiated();
  EXPECT_TRUE(
      QuicServerSessionBasePeer::IsBandwidthResumptionEnabled(session_.get()));
}

TEST_P(QuicServerSessionBaseTest, NoBandwidthResumptionByDefault) {
  EXPECT_FALSE(
      QuicServerSessionBasePeer::IsBandwidthResumptionEnabled(session_.get()));
  session_->OnConfigNegotiated();
  EXPECT_FALSE(
      QuicServerSessionBasePeer::IsBandwidthResumptionEnabled(session_.get()));
}

// Tests which check the lifetime management of data members of
// QuicCryptoServerStream objects when async GetProof is in use.
class StreamMemberLifetimeTest : public QuicServerSessionBaseTest {
 public:
  StreamMemberLifetimeTest()
      : QuicServerSessionBaseTest(
            std::unique_ptr<FakeProofSource>(new FakeProofSource())),
        crypto_config_peer_(&crypto_config_) {
    GetFakeProofSource()->Activate();
  }

  FakeProofSource* GetFakeProofSource() const {
    return static_cast<FakeProofSource*>(crypto_config_peer_.GetProofSource());
  }

 private:
  QuicCryptoServerConfigPeer crypto_config_peer_;
};

INSTANTIATE_TEST_CASE_P(StreamMemberLifetimeTests,
                        StreamMemberLifetimeTest,
                        ::testing::ValuesIn(AllSupportedVersions()));

// Trigger an operation which causes an async invocation of
// ProofSource::GetProof.  Delay the completion of the operation until after the
// stream has been destroyed, and verify that there are no memory bugs.
TEST_P(StreamMemberLifetimeTest, Basic) {
  if (GetParam().handshake_protocol == PROTOCOL_TLS1_3) {
    // This test depends on the QUIC crypto protocol, so it is disabled for the
    // TLS handshake.
    // TODO(nharper): Fix this test so it doesn't rely on QUIC crypto.
    return;
  }
  SetQuicReloadableFlag(enable_quic_stateless_reject_support, true);
  SetQuicReloadableFlag(quic_use_cheap_stateless_rejects, true);

  const QuicClock* clock = helper_.GetClock();
  ParsedQuicVersion version = AllSupportedVersions().front();
  CryptoHandshakeMessage chlo = crypto_test_utils::GenerateDefaultInchoateCHLO(
      clock, version.transport_version, &crypto_config_);
  chlo.SetVector(kCOPT, QuicTagVector{kSREJ});
  std::vector<ParsedQuicVersion> packet_version_list = {version};
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      1, 0, true, false, 1, QuicString(chlo.GetSerialized().AsStringPiece()),
      PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID,
      PACKET_4BYTE_PACKET_NUMBER, &packet_version_list));

  EXPECT_CALL(stream_helper_, CanAcceptClientHello(_, _, _, _, _))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(stream_helper_, GenerateConnectionIdForReject(_))
      .WillOnce(testing::Return(12345));

  // Set the current packet
  QuicConnectionPeer::SetCurrentPacket(session_->connection(),
                                       packet->AsStringPiece());

  // Yes, this is horrible.  But it's the easiest way to trigger the behavior we
  // need to exercise.
  QuicCryptoServerStreamBase* crypto_stream =
      const_cast<QuicCryptoServerStreamBase*>(session_->crypto_stream());

  // Feed the CHLO into the crypto stream, which will trigger a call to
  // ProofSource::GetProof
  crypto_test_utils::SendHandshakeMessageToStream(crypto_stream, chlo,
                                                  Perspective::IS_CLIENT);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Destroy the stream
  session_.reset();

  // Allow the async ProofSource::GetProof call to complete.  Verify (under
  // memory access checkers) that this does not result in accesses to any
  // freed memory from the session or its subobjects.
  GetFakeProofSource()->InvokePendingCallback(0);
}

}  // namespace
}  // namespace test
}  // namespace quic
