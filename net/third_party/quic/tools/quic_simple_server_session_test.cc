// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_simple_server_session.h"

#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/proto/cached_network_parameters.pb.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/third_party/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_sustained_bandwidth_recorder_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/tools/quic_backend_response.h"
#include "net/third_party/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quic/tools/quic_simple_server_stream.h"

using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {
typedef QuicSimpleServerSession::PromisedStreamInfo PromisedStreamInfo;
}  // namespace

class QuicSimpleServerSessionPeer {
 public:
  static void SetCryptoStream(QuicSimpleServerSession* s,
                              QuicCryptoServerStream* crypto_stream) {
    s->crypto_stream_.reset(crypto_stream);
    s->RegisterStaticStream(
        QuicUtils::GetCryptoStreamId(s->connection()->transport_version()),
        crypto_stream);
  }

  static QuicSpdyStream* CreateIncomingStream(QuicSimpleServerSession* s,
                                              QuicStreamId id) {
    return s->CreateIncomingStream(id);
  }

  static QuicSimpleServerStream* CreateOutgoingUnidirectionalStream(
      QuicSimpleServerSession* s) {
    return s->CreateOutgoingUnidirectionalStream();
  }
};

namespace {

const size_t kMaxStreamsForTest = 10;

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
            GetQuicReloadableFlag(
                enable_quic_stateless_reject_support),  // NOLINT
            session,
            helper) {}
  MockQuicCryptoServerStream(const MockQuicCryptoServerStream&) = delete;
  MockQuicCryptoServerStream& operator=(const MockQuicCryptoServerStream&) =
      delete;
  ~MockQuicCryptoServerStream() override {}

  MOCK_METHOD1(SendServerConfigUpdate,
               void(const CachedNetworkParameters* cached_network_parameters));

  void set_encryption_established(bool has_established) {
    encryption_established_override_ = has_established;
  }

  bool encryption_established() const override {
    return QuicCryptoServerStream::encryption_established() ||
           encryption_established_override_;
  }

 private:
  bool encryption_established_override_ = false;
};

class MockQuicConnectionWithSendStreamData : public MockQuicConnection {
 public:
  MockQuicConnectionWithSendStreamData(
      MockQuicConnectionHelper* helper,
      MockAlarmFactory* alarm_factory,
      Perspective perspective,
      const ParsedQuicVersionVector& supported_versions)
      : MockQuicConnection(helper,
                           alarm_factory,
                           perspective,
                           supported_versions) {}

  MOCK_METHOD4(SendStreamData,
               QuicConsumedData(QuicStreamId id,
                                size_t write_length,
                                QuicStreamOffset offset,
                                StreamSendingState state));
};

class MockQuicSimpleServerSession : public QuicSimpleServerSession {
 public:
  MockQuicSimpleServerSession(
      const QuicConfig& config,
      QuicConnection* connection,
      QuicSession::Visitor* visitor,
      QuicCryptoServerStream::Helper* helper,
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerSession(config,
                                CurrentSupportedVersions(),
                                connection,
                                visitor,
                                helper,
                                crypto_config,
                                compressed_certs_cache,
                                quic_simple_server_backend) {}
  // Methods taking non-copyable types like spdy::SpdyHeaderBlock by value
  // cannot be mocked directly.
  size_t WritePushPromise(QuicStreamId original_stream_id,
                          QuicStreamId promised_stream_id,
                          spdy::SpdyHeaderBlock headers) override {
    return WritePushPromiseMock(original_stream_id, promised_stream_id,
                                headers);
  }
  MOCK_METHOD3(WritePushPromiseMock,
               size_t(QuicStreamId original_stream_id,
                      QuicStreamId promised_stream_id,
                      const spdy::SpdyHeaderBlock& headers));

  size_t WriteHeaders(QuicStreamId stream_id,
                      spdy::SpdyHeaderBlock headers,
                      bool fin,
                      spdy::SpdyPriority priority,
                      QuicReferenceCountedPointer<QuicAckListenerInterface>
                          ack_listener) override {
    return WriteHeadersMock(stream_id, headers, fin, priority, ack_listener);
  }
  MOCK_METHOD5(
      WriteHeadersMock,
      size_t(QuicStreamId stream_id,
             const spdy::SpdyHeaderBlock& headers,
             bool fin,
             spdy::SpdyPriority priority,
             const QuicReferenceCountedPointer<QuicAckListenerInterface>&
                 ack_listener));
  MOCK_METHOD1(SendBlocked, void(QuicStreamId));
};

class QuicSimpleServerSessionTest
    : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  bool ClearControlFrame(const QuicFrame& frame) {
    DeleteFrame(&const_cast<QuicFrame&>(frame));
    return true;
  }

 protected:
  QuicSimpleServerSessionTest()
      : crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(),
                       crypto_test_utils::ProofSourceForTesting(),
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
    connection_ = new StrictMock<MockQuicConnectionWithSendStreamData>(
        &helper_, &alarm_factory_, Perspective::IS_SERVER, supported_versions);
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    session_ = QuicMakeUnique<MockQuicSimpleServerSession>(
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

    session_->OnConfigNegotiated();
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
  StrictMock<MockQuicConnectionWithSendStreamData>* connection_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  QuicCompressedCertsCache compressed_certs_cache_;
  QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<MockQuicSimpleServerSession> session_;
  std::unique_ptr<CryptoHandshakeMessage> handshake_message_;
  QuicConnectionVisitorInterface* visitor_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicSimpleServerSessionTest,
                        ::testing::ValuesIn(AllSupportedVersions()));

TEST_P(QuicSimpleServerSessionTest, CloseStreamDueToReset) {
  // Open a stream, then reset it.
  // Send two bytes of payload to open it.
  QuicStreamFrame data1(GetNthClientInitiatedId(0), false, 0,
                        QuicStringPiece("HT"));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Receive a reset (and send a RST in response).
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

TEST_P(QuicSimpleServerSessionTest, NeverOpenStreamDueToReset) {
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

TEST_P(QuicSimpleServerSessionTest, AcceptClosedStream) {
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

TEST_P(QuicSimpleServerSessionTest, CreateIncomingStreamDisconnected) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  // Tests that incoming stream creation fails when connection is not connected.
  size_t initial_num_open_stream = session_->GetNumOpenIncomingStreams();
  QuicConnectionPeer::TearDownLocalConnectionState(connection_);
  EXPECT_QUIC_BUG(QuicSimpleServerSessionPeer::CreateIncomingStream(
                      session_.get(), GetNthClientInitiatedId(0)),
                  "ShouldCreateIncomingStream called when disconnected");
  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenIncomingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateEvenIncomingDynamicStream) {
  // Tests that incoming stream creation fails when given stream id is even.
  size_t initial_num_open_stream = session_->GetNumOpenIncomingStreams();
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_STREAM_ID,
                              "Client created even numbered stream", _));
  QuicSimpleServerSessionPeer::CreateIncomingStream(session_.get(),
                                                    GetNthServerInitiatedId(0));
  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenIncomingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateIncomingStream) {
  QuicSpdyStream* stream = QuicSimpleServerSessionPeer::CreateIncomingStream(
      session_.get(), GetNthClientInitiatedId(0));
  EXPECT_NE(nullptr, stream);
  EXPECT_EQ(GetNthClientInitiatedId(0), stream->id());
}

TEST_P(QuicSimpleServerSessionTest, CreateOutgoingDynamicStreamDisconnected) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  // Tests that outgoing stream creation fails when connection is not connected.
  size_t initial_num_open_stream = session_->GetNumOpenOutgoingStreams();
  QuicConnectionPeer::TearDownLocalConnectionState(connection_);
  EXPECT_QUIC_BUG(
      QuicSimpleServerSessionPeer::CreateOutgoingUnidirectionalStream(
          session_.get()),
      "ShouldCreateOutgoingStream called when disconnected");

  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenOutgoingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateOutgoingDynamicStreamUnencrypted) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  // Tests that outgoing stream creation fails when encryption has not yet been
  // established.
  size_t initial_num_open_stream = session_->GetNumOpenOutgoingStreams();
  EXPECT_QUIC_BUG(
      QuicSimpleServerSessionPeer::CreateOutgoingUnidirectionalStream(
          session_.get()),
      "Encryption not established so no outgoing stream created.");
  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenOutgoingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateOutgoingDynamicStreamUptoLimit) {
  // Tests that outgoing stream creation should not be affected by existing
  // incoming stream and vice-versa. But when reaching the limit of max outgoing
  // stream allowed, creation should fail.

  // Receive some data to initiate a incoming stream which should not effect
  // creating outgoing streams.
  QuicStreamFrame data1(GetNthClientInitiatedId(0), false, 0,
                        QuicStringPiece("HT"));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());
  EXPECT_EQ(0u, session_->GetNumOpenOutgoingStreams());

  session_->UnregisterStreamPriority(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()),
      /*is_static=*/true);
  // Assume encryption already established.
  QuicSimpleServerSessionPeer::SetCryptoStream(session_.get(), nullptr);
  MockQuicCryptoServerStream* crypto_stream =
      new MockQuicCryptoServerStream(&crypto_config_, &compressed_certs_cache_,
                                     session_.get(), &stream_helper_);
  crypto_stream->set_encryption_established(true);
  QuicSimpleServerSessionPeer::SetCryptoStream(session_.get(), crypto_stream);
  session_->RegisterStreamPriority(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()),
      /*is_static=*/true, QuicStream::kDefaultPriority);

  // Create push streams till reaching the upper limit of allowed open streams.
  for (size_t i = 0; i < kMaxStreamsForTest; ++i) {
    QuicSpdyStream* created_stream =
        QuicSimpleServerSessionPeer::CreateOutgoingUnidirectionalStream(
            session_.get());
    EXPECT_EQ(GetNthServerInitiatedId(i), created_stream->id());
    EXPECT_EQ(i + 1, session_->GetNumOpenOutgoingStreams());
  }

  // Continuing creating push stream would fail.
  EXPECT_EQ(nullptr,
            QuicSimpleServerSessionPeer::CreateOutgoingUnidirectionalStream(
                session_.get()));
  EXPECT_EQ(kMaxStreamsForTest, session_->GetNumOpenOutgoingStreams());

  // Create peer initiated stream should have no problem.
  QuicStreamFrame data2(GetNthClientInitiatedId(1), false, 0,
                        QuicStringPiece("HT"));
  session_->OnStreamFrame(data2);
  EXPECT_EQ(2u, session_->GetNumOpenIncomingStreams());
}

TEST_P(QuicSimpleServerSessionTest, OnStreamFrameWithEvenStreamId) {
  QuicStreamFrame frame(GetNthServerInitiatedId(0), false, 0,
                        QuicStringPiece());
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_STREAM_ID,
                              "Client sent data on server push stream", _));
  session_->OnStreamFrame(frame);
}

TEST_P(QuicSimpleServerSessionTest, GetEvenIncomingError) {
  // Tests that calling GetOrCreateDynamicStream() on an outgoing stream not
  // promised yet should result close connection.
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_STREAM_ID,
                                            "Data for nonexistent stream", _));
  EXPECT_EQ(nullptr, QuicSessionPeer::GetOrCreateDynamicStream(
                         session_.get(), GetNthServerInitiatedId(1)));
}

// In order to test the case where server push stream creation goes beyond
// limit, server push streams need to be hanging there instead of
// immediately closing after sending back response.
// To achieve this goal, this class resets flow control windows so that large
// responses will not be sent fully in order to prevent push streams from being
// closed immediately.
// Also adjust connection-level flow control window to ensure a large response
// can cause stream-level flow control blocked but not connection-level.
class QuicSimpleServerSessionServerPushTest
    : public QuicSimpleServerSessionTest {
 protected:
  const size_t kStreamFlowControlWindowSize = 32 * 1024;  // 32KB.

  QuicSimpleServerSessionServerPushTest() : QuicSimpleServerSessionTest() {
    // Reset stream level flow control window to be 32KB.
    QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(
        &config_, kStreamFlowControlWindowSize);
    // Reset connection level flow control window to be 1.5 MB which is large
    // enough that it won't block any stream to write before stream level flow
    // control blocks it.
    QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
        &config_, kInitialSessionFlowControlWindowForTest);
    // Enable server push.
    QuicTagVector copt;
    copt.push_back(kSPSH);
    QuicConfigPeer::SetReceivedConnectionOptions(&config_, copt);

    ParsedQuicVersionVector supported_versions = SupportedVersions(GetParam());
    connection_ = new StrictMock<MockQuicConnectionWithSendStreamData>(
        &helper_, &alarm_factory_, Perspective::IS_SERVER, supported_versions);
    session_ = QuicMakeUnique<MockQuicSimpleServerSession>(
        config_, connection_, &owner_, &stream_helper_, &crypto_config_,
        &compressed_certs_cache_, &memory_cache_backend_);
    session_->Initialize();
    QuicSessionPeer::GetMutableCryptoStream(session_.get())
        ->OnSuccessfulVersionNegotiation(supported_versions.front());
    // Needed to make new session flow control window and server push work.
    session_->OnConfigNegotiated();

    visitor_ = QuicConnectionPeer::GetVisitor(connection_);

    session_->UnregisterStreamPriority(
        QuicUtils::GetHeadersStreamId(connection_->transport_version()),
        /*is_static=*/true);
    QuicSimpleServerSessionPeer::SetCryptoStream(session_.get(), nullptr);
    // Assume encryption already established.
    MockQuicCryptoServerStream* crypto_stream = new MockQuicCryptoServerStream(
        &crypto_config_, &compressed_certs_cache_, session_.get(),
        &stream_helper_);

    crypto_stream->set_encryption_established(true);
    QuicSimpleServerSessionPeer::SetCryptoStream(session_.get(), crypto_stream);
    session_->RegisterStreamPriority(
        QuicUtils::GetHeadersStreamId(connection_->transport_version()),
        /*is_static=*/true, QuicStream::kDefaultPriority);
  }

  // Given |num_resources|, create this number of fake push resources and push
  // them by sending PUSH_PROMISE for all and sending push responses for as much
  // as possible(limited by kMaxStreamsForTest).
  // If |num_resources| > kMaxStreamsForTest, the left over will be queued.
  void PromisePushResources(size_t num_resources) {
    // To prevent push streams from being closed the response need to be larger
    // than stream flow control window so stream won't send the full body.
    size_t body_size = 2 * kStreamFlowControlWindowSize;  // 64KB.

    QuicString request_url = "mail.google.com/";
    spdy::SpdyHeaderBlock request_headers;
    QuicString resource_host = "www.google.com";
    QuicString partial_push_resource_path = "/server_push_src";
    std::list<QuicBackendResponse::ServerPushInfo> push_resources;
    QuicString scheme = "http";
    for (unsigned int i = 1; i <= num_resources; ++i) {
      QuicStreamId stream_id = GetNthServerInitiatedId(i - 1);
      QuicString path =
          partial_push_resource_path + QuicTextUtils::Uint64ToString(i);
      QuicString url = scheme + "://" + resource_host + path;
      QuicUrl resource_url = QuicUrl(url);
      QuicString body(body_size, 'a');
      memory_cache_backend_.AddSimpleResponse(resource_host, path, 200, body);
      push_resources.push_back(QuicBackendResponse::ServerPushInfo(
          resource_url, spdy::SpdyHeaderBlock(), QuicStream::kDefaultPriority,
          body));
      // PUSH_PROMISED are sent for all the resources.
      EXPECT_CALL(*session_, WritePushPromiseMock(GetNthClientInitiatedId(0),
                                                  stream_id, _));
      if (i <= kMaxStreamsForTest) {
        // |kMaxStreamsForTest| promised responses should be sent.
        EXPECT_CALL(*session_,
                    WriteHeadersMock(stream_id, _, false,
                                     QuicStream::kDefaultPriority, _));
        // Since flow control window is smaller than response body, not the
        // whole body will be sent.
        EXPECT_CALL(*connection_, SendStreamData(stream_id, _, 0, NO_FIN))
            .WillOnce(
                Return(QuicConsumedData(kStreamFlowControlWindowSize, false)));
        EXPECT_CALL(*session_, SendBlocked(stream_id));
      }
    }
    session_->PromisePushResources(request_url, push_resources,
                                   GetNthClientInitiatedId(0), request_headers);
  }
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicSimpleServerSessionServerPushTest,
                        ::testing::ValuesIn(AllSupportedVersions()));

TEST_P(QuicSimpleServerSessionServerPushTest, TestPromisePushResources) {
  // Tests that given more than kMaxOpenStreamForTest resources, all their
  // PUSH_PROMISE's will be sent out and only |kMaxOpenStreamForTest| streams
  // will be opened and send push response.

  size_t num_resources = kMaxStreamsForTest + 5;
  PromisePushResources(num_resources);
  EXPECT_EQ(kMaxStreamsForTest, session_->GetNumOpenOutgoingStreams());
}

TEST_P(QuicSimpleServerSessionServerPushTest,
       HandlePromisedPushRequestsAfterStreamDraining) {
  // Tests that after promised stream queued up, when an opened stream is marked
  // draining, a queued promised stream will become open and send push response.
  size_t num_resources = kMaxStreamsForTest + 1;
  PromisePushResources(num_resources);
  QuicStreamId next_out_going_stream_id =
      GetNthServerInitiatedId(kMaxStreamsForTest);

  // After an open stream is marked draining, a new stream is expected to be
  // created and a response sent on the stream.
  EXPECT_CALL(*session_, WriteHeadersMock(next_out_going_stream_id, _, false,
                                          QuicStream::kDefaultPriority, _));
  EXPECT_CALL(*connection_,
              SendStreamData(next_out_going_stream_id, _, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(kStreamFlowControlWindowSize, false)));
  EXPECT_CALL(*session_, SendBlocked(next_out_going_stream_id));
  session_->StreamDraining(GetNthServerInitiatedId(0));
  // Number of open outgoing streams should still be the same, because a new
  // stream is opened. And the queue should be empty.
  EXPECT_EQ(kMaxStreamsForTest, session_->GetNumOpenOutgoingStreams());
}

TEST_P(QuicSimpleServerSessionServerPushTest,
       ResetPromisedStreamToCancelServerPush) {
  // Tests that after all resources are promised, a RST frame from client can
  // prevent a promised resource to be send out.

  // Having two extra resources to be send later. One of them will be reset, so
  // when opened stream become close, only one will become open.
  size_t num_resources = kMaxStreamsForTest + 2;
  PromisePushResources(num_resources);

  // Reset the last stream in the queue. It should be marked cancelled.
  QuicStreamId stream_got_reset =
      GetNthServerInitiatedId(kMaxStreamsForTest + 1);
  QuicRstStreamFrame rst(kInvalidControlFrameId, stream_got_reset,
                         QUIC_STREAM_CANCELLED, 0);
  EXPECT_CALL(owner_, OnRstStreamReceived(_)).Times(1);
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(
          this, &QuicSimpleServerSessionServerPushTest::ClearControlFrame));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_got_reset, QUIC_RST_ACKNOWLEDGEMENT));
  visitor_->OnRstStream(rst);

  // When the first 2 streams becomes draining, the two queued up stream could
  // be created. But since one of them was marked cancelled due to RST frame,
  // only one queued resource will be sent out.
  QuicStreamId stream_not_reset = GetNthServerInitiatedId(kMaxStreamsForTest);
  InSequence s;
  EXPECT_CALL(*session_, WriteHeadersMock(stream_not_reset, _, false,
                                          QuicStream::kDefaultPriority, _));
  EXPECT_CALL(*connection_, SendStreamData(stream_not_reset, _, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(kStreamFlowControlWindowSize, false)));
  EXPECT_CALL(*session_, SendBlocked(stream_not_reset));
  EXPECT_CALL(*session_, WriteHeadersMock(stream_got_reset, _, false,
                                          QuicStream::kDefaultPriority, _))
      .Times(0);

  session_->StreamDraining(GetNthServerInitiatedId(0));
  session_->StreamDraining(GetNthServerInitiatedId(1));
}

TEST_P(QuicSimpleServerSessionServerPushTest,
       CloseStreamToHandleMorePromisedStream) {
  // Tests that closing a open outgoing stream can trigger a promised resource
  // in the queue to be send out.
  size_t num_resources = kMaxStreamsForTest + 1;
  PromisePushResources(num_resources);
  QuicStreamId stream_to_open = GetNthServerInitiatedId(kMaxStreamsForTest);

  // Resetting 1st open stream will close the stream and give space for extra
  // stream to be opened.
  QuicStreamId stream_got_reset = GetNthServerInitiatedId(0);
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_got_reset, QUIC_RST_ACKNOWLEDGEMENT));
  EXPECT_CALL(*session_, WriteHeadersMock(stream_to_open, _, false,
                                          QuicStream::kDefaultPriority, _));
  EXPECT_CALL(*connection_, SendStreamData(stream_to_open, _, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(kStreamFlowControlWindowSize, false)));

  EXPECT_CALL(*session_, SendBlocked(stream_to_open));
  EXPECT_CALL(owner_, OnRstStreamReceived(_)).Times(1);
  QuicRstStreamFrame rst(kInvalidControlFrameId, stream_got_reset,
                         QUIC_STREAM_CANCELLED, 0);
  visitor_->OnRstStream(rst);
}

}  // namespace
}  // namespace test
}  // namespace quic
