// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_spdy_session.h"

#include <cstdint>
#include <set>
#include <utility>

#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/quic_crypto_stream.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_stream.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_flow_controller_peer.h"
#include "net/third_party/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_send_buffer_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/spdy/core/spdy_framer.h"

using spdy::kV3HighestPriority;
using spdy::Spdy3PriorityToHttp2Weight;
using spdy::SpdyFramer;
using spdy::SpdyHeaderBlock;
using spdy::SpdyPriority;
using spdy::SpdyPriorityIR;
using spdy::SpdySerializedFrame;
using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class TestCryptoStream : public QuicCryptoStream, public QuicCryptoHandshaker {
 public:
  explicit TestCryptoStream(QuicSession* session)
      : QuicCryptoStream(session),
        QuicCryptoHandshaker(this, session),
        encryption_established_(false),
        handshake_confirmed_(false),
        params_(new QuicCryptoNegotiatedParameters) {}

  void OnHandshakeMessage(const CryptoHandshakeMessage& /*message*/) override {
    encryption_established_ = true;
    handshake_confirmed_ = true;
    CryptoHandshakeMessage msg;
    QuicString error_details;
    session()->config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session()->config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    session()->config()->ToHandshakeMessage(&msg);
    const QuicErrorCode error =
        session()->config()->ProcessPeerHello(msg, CLIENT, &error_details);
    EXPECT_EQ(QUIC_NO_ERROR, error);
    session()->OnConfigNegotiated();
    session()->connection()->SetDefaultEncryptionLevel(
        ENCRYPTION_FORWARD_SECURE);
    session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  }

  // QuicCryptoStream implementation
  QuicLongHeaderType GetLongHeaderType(
      QuicStreamOffset /*offset*/) const override {
    return HANDSHAKE;
  }
  bool encryption_established() const override {
    return encryption_established_;
  }
  bool handshake_confirmed() const override { return handshake_confirmed_; }
  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override {
    return *params_;
  }
  CryptoMessageParser* crypto_message_parser() override {
    return QuicCryptoHandshaker::crypto_message_parser();
  }

  MOCK_METHOD0(OnCanWrite, void());

  MOCK_CONST_METHOD0(HasPendingRetransmission, bool());

 private:
  using QuicCryptoStream::session;

  bool encryption_established_;
  bool handshake_confirmed_;
  QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
};

class TestHeadersStream : public QuicHeadersStream {
 public:
  explicit TestHeadersStream(QuicSpdySession* session)
      : QuicHeadersStream(session) {}

  MOCK_METHOD0(OnCanWrite, void());
};

class TestStream : public QuicSpdyStream {
 public:
  TestStream(QuicStreamId id, QuicSpdySession* session, StreamType type)
      : QuicSpdyStream(id, session, type) {}

  using QuicStream::CloseWriteSide;

  void OnDataAvailable() override {}

  MOCK_METHOD0(OnCanWrite, void());
  MOCK_METHOD3(RetransmitStreamData,
               bool(QuicStreamOffset, QuicByteCount, bool));

  MOCK_CONST_METHOD0(HasPendingRetransmission, bool());
};

class TestSession : public QuicSpdySession {
 public:
  explicit TestSession(QuicConnection* connection)
      : QuicSpdySession(connection,
                        nullptr,
                        DefaultQuicConfig(),
                        CurrentSupportedVersions()),
        crypto_stream_(this),
        writev_consumes_all_data_(false) {
    Initialize();
    this->connection()->SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        QuicMakeUnique<NullEncrypter>(connection->perspective()));
  }

  ~TestSession() override { delete connection(); }

  TestCryptoStream* GetMutableCryptoStream() override {
    return &crypto_stream_;
  }

  const TestCryptoStream* GetCryptoStream() const override {
    return &crypto_stream_;
  }

  TestStream* CreateOutgoingBidirectionalStream() override {
    TestStream* stream =
        new TestStream(GetNextOutgoingStreamId(), this, BIDIRECTIONAL);
    ActivateStream(QuicWrapUnique(stream));
    return stream;
  }

  TestStream* CreateOutgoingUnidirectionalStream() override {
    TestStream* stream =
        new TestStream(GetNextOutgoingStreamId(), this, WRITE_UNIDIRECTIONAL);
    ActivateStream(QuicWrapUnique(stream));
    return stream;
  }

  TestStream* CreateIncomingStream(QuicStreamId id) override {
    // Enforce the limit on the number of open streams.
    if (GetNumOpenIncomingStreams() + 1 > max_open_incoming_streams()) {
      connection()->CloseConnection(
          QUIC_TOO_MANY_OPEN_STREAMS, "Too many streams!",
          ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
      return nullptr;
    } else {
      TestStream* stream = new TestStream(id, this, BIDIRECTIONAL);
      ActivateStream(QuicWrapUnique(stream));
      return stream;
    }
  }

  bool ShouldCreateIncomingStream(QuicStreamId /*id*/) override { return true; }

  bool ShouldCreateOutgoingStream() override { return true; }

  bool IsClosedStream(QuicStreamId id) {
    return QuicSession::IsClosedStream(id);
  }

  QuicStream* GetOrCreateDynamicStream(QuicStreamId stream_id) {
    return QuicSpdySession::GetOrCreateDynamicStream(stream_id);
  }

  QuicConsumedData WritevData(QuicStream* stream,
                              QuicStreamId id,
                              size_t write_length,
                              QuicStreamOffset offset,
                              StreamSendingState state) override {
    bool fin = state != NO_FIN;
    QuicConsumedData consumed(write_length, fin);
    if (!writev_consumes_all_data_) {
      consumed =
          QuicSession::WritevData(stream, id, write_length, offset, state);
    }
    if (fin && consumed.fin_consumed) {
      stream->set_fin_sent(true);
    }
    QuicSessionPeer::GetWriteBlockedStreams(this)->UpdateBytesForStream(
        id, consumed.bytes_consumed);
    return consumed;
  }

  void set_writev_consumes_all_data(bool val) {
    writev_consumes_all_data_ = val;
  }

  QuicConsumedData SendStreamData(QuicStream* stream) {
    struct iovec iov;
    if (stream->id() !=
            QuicUtils::GetCryptoStreamId(connection()->transport_version()) &&
        connection()->encryption_level() != ENCRYPTION_FORWARD_SECURE) {
      this->connection()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    }
    MakeIOVector("not empty", &iov);
    QuicStreamPeer::SendBuffer(stream).SaveStreamData(&iov, 1, 0, 9);
    QuicConsumedData consumed = WritevData(stream, stream->id(), 9, 0, FIN);
    QuicStreamPeer::SendBuffer(stream).OnStreamDataConsumed(
        consumed.bytes_consumed);
    return consumed;
  }

  bool ClearControlFrame(const QuicFrame& frame) {
    DeleteFrame(&const_cast<QuicFrame&>(frame));
    return true;
  }

  QuicConsumedData SendLargeFakeData(QuicStream* stream, int bytes) {
    DCHECK(writev_consumes_all_data_);
    return WritevData(stream, stream->id(), bytes, 0, FIN);
  }

  using QuicSession::closed_streams;
  using QuicSession::next_outgoing_stream_id;
  using QuicSession::PostProcessAfterData;
  using QuicSession::zombie_streams;

 private:
  StrictMock<TestCryptoStream> crypto_stream_;

  bool writev_consumes_all_data_;
};

class QuicSpdySessionTestBase : public QuicTestWithParam<ParsedQuicVersion> {
 protected:
  explicit QuicSpdySessionTestBase(Perspective perspective)
      : connection_(
            new StrictMock<MockQuicConnection>(&helper_,
                                               &alarm_factory_,
                                               perspective,
                                               SupportedVersions(GetParam()))),
        session_(connection_) {
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "http";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    TestCryptoStream* crypto_stream = session_.GetMutableCryptoStream();
    EXPECT_CALL(*crypto_stream, HasPendingRetransmission())
        .Times(testing::AnyNumber());
  }

  void CheckClosedStreams() {
    for (QuicStreamId i =
             QuicUtils::GetCryptoStreamId(connection_->transport_version());
         i < 100; i++) {
      if (!QuicContainsKey(closed_streams_, i)) {
        EXPECT_FALSE(session_.IsClosedStream(i)) << " stream id: " << i;
      } else {
        EXPECT_TRUE(session_.IsClosedStream(i)) << " stream id: " << i;
      }
    }
  }

  void CloseStream(QuicStreamId id) {
    EXPECT_CALL(*connection_, SendControlFrame(_))
        .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
    EXPECT_CALL(*connection_, OnStreamReset(id, _));
    session_.CloseStream(id);
    closed_streams_.insert(id);
  }

  QuicTransportVersion transport_version() const {
    return connection_->transport_version();
  }

  QuicStreamId GetNthClientInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, n);
  }

  QuicStreamId GetNthServerInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthServerInitiatedStreamId(session_, n);
  }

  QuicStreamId NextId() { return QuicSpdySessionPeer::NextStreamId(session_); }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  TestSession session_;
  std::set<QuicStreamId> closed_streams_;
  SpdyHeaderBlock headers_;
};

class QuicSpdySessionTestServer : public QuicSpdySessionTestBase {
 protected:
  QuicSpdySessionTestServer()
      : QuicSpdySessionTestBase(Perspective::IS_SERVER) {}
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicSpdySessionTestServer,
                        ::testing::ValuesIn(AllSupportedVersions()));

TEST_P(QuicSpdySessionTestServer, PeerAddress) {
  EXPECT_EQ(QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort),
            session_.peer_address());
}

TEST_P(QuicSpdySessionTestServer, SelfAddress) {
  EXPECT_EQ(QuicSocketAddress(), session_.self_address());
}

TEST_P(QuicSpdySessionTestServer, IsCryptoHandshakeConfirmed) {
  EXPECT_FALSE(session_.IsCryptoHandshakeConfirmed());
  CryptoHandshakeMessage message;
  session_.GetMutableCryptoStream()->OnHandshakeMessage(message);
  EXPECT_TRUE(session_.IsCryptoHandshakeConfirmed());
}

TEST_P(QuicSpdySessionTestServer, IsClosedStreamDefault) {
  // Ensure that no streams are initially closed.
  for (QuicStreamId i =
           QuicUtils::GetCryptoStreamId(connection_->transport_version());
       i < 100; i++) {
    EXPECT_FALSE(session_.IsClosedStream(i)) << "stream id: " << i;
  }
}

TEST_P(QuicSpdySessionTestServer, AvailableStreams) {
  ASSERT_TRUE(session_.GetOrCreateDynamicStream(GetNthClientInitiatedId(2)) !=
              nullptr);
  // Both client initiated streams with smaller stream IDs are available.
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(&session_,
                                                 GetNthClientInitiatedId(0)));
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(&session_,
                                                 GetNthClientInitiatedId(1)));
  ASSERT_TRUE(session_.GetOrCreateDynamicStream(GetNthClientInitiatedId(1)) !=
              nullptr);
  ASSERT_TRUE(session_.GetOrCreateDynamicStream(GetNthClientInitiatedId(0)) !=
              nullptr);
}

TEST_P(QuicSpdySessionTestServer, IsClosedStreamLocallyCreated) {
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  EXPECT_EQ(GetNthServerInitiatedId(0), stream2->id());
  QuicSpdyStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  EXPECT_EQ(GetNthServerInitiatedId(1), stream4->id());

  CheckClosedStreams();
  CloseStream(GetNthServerInitiatedId(0));
  CheckClosedStreams();
  CloseStream(GetNthServerInitiatedId(1));
  CheckClosedStreams();
}

TEST_P(QuicSpdySessionTestServer, IsClosedStreamPeerCreated) {
  QuicStreamId stream_id1 = GetNthClientInitiatedId(0);
  QuicStreamId stream_id2 = GetNthClientInitiatedId(1);
  session_.GetOrCreateDynamicStream(stream_id1);
  session_.GetOrCreateDynamicStream(stream_id2);

  CheckClosedStreams();
  CloseStream(stream_id1);
  CheckClosedStreams();
  CloseStream(stream_id2);
  // Create a stream, and make another available.
  QuicStream* stream3 = session_.GetOrCreateDynamicStream(stream_id2 + 4);
  CheckClosedStreams();
  // Close one, but make sure the other is still not closed
  CloseStream(stream3->id());
  CheckClosedStreams();
}

TEST_P(QuicSpdySessionTestServer, MaximumAvailableOpenedStreams) {
  QuicStreamId stream_id = GetNthClientInitiatedId(0);
  session_.GetOrCreateDynamicStream(stream_id);
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_NE(nullptr,
            session_.GetOrCreateDynamicStream(
                stream_id + 2 * (session_.max_open_incoming_streams() - 1)));
}

TEST_P(QuicSpdySessionTestServer, TooManyAvailableStreams) {
  QuicStreamId stream_id1 = GetNthClientInitiatedId(0);
  QuicStreamId stream_id2;
  EXPECT_NE(nullptr, session_.GetOrCreateDynamicStream(stream_id1));
  // A stream ID which is too large to create.
  stream_id2 = GetNthClientInitiatedId(2 * session_.MaxAvailableStreams() + 4);
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_TOO_MANY_AVAILABLE_STREAMS, _, _));
  EXPECT_EQ(nullptr, session_.GetOrCreateDynamicStream(stream_id2));
}

TEST_P(QuicSpdySessionTestServer, ManyAvailableStreams) {
  // When max_open_streams_ is 200, should be able to create 200 streams
  // out-of-order, that is, creating the one with the largest stream ID first.
  QuicSessionPeer::SetMaxOpenIncomingStreams(&session_, 200);
  QuicStreamId stream_id = GetNthClientInitiatedId(0);
  // Create one stream.
  session_.GetOrCreateDynamicStream(stream_id);
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  // Create the largest stream ID of a threatened total of 200 streams.
  session_.GetOrCreateDynamicStream(stream_id + 2 * (200 - 1));
}

TEST_P(QuicSpdySessionTestServer,
       DebugDFatalIfMarkingClosedStreamWriteBlocked) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  QuicStreamId closed_stream_id = stream2->id();
  // Close the stream.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(closed_stream_id, _));
  stream2->Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  QuicString msg =
      QuicStrCat("Marking unknown stream ", closed_stream_id, " blocked.");
  EXPECT_QUIC_BUG(session_.MarkConnectionLevelWriteBlocked(closed_stream_id),
                  msg);
}

TEST_P(QuicSpdySessionTestServer, OnCanWrite) {
  session_.set_writev_consumes_all_data(true);
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();

  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream6->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());

  InSequence s;

  // Reregister, to test the loop limit.
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendStreamData(stream2);
    session_.MarkConnectionLevelWriteBlocked(stream2->id());
  }));
  // 2 will get called a second time as it didn't finish its block
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendStreamData(stream2);
  }));
  EXPECT_CALL(*stream6, OnCanWrite()).WillOnce(Invoke([this, stream6]() {
    session_.SendStreamData(stream6);
  }));
  // 4 will not get called, as we exceeded the loop limit.
  session_.OnCanWrite();
  EXPECT_TRUE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, TestBatchedWrites) {
  session_.set_writev_consumes_all_data(true);
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();

  session_.set_writev_consumes_all_data(true);
  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());

  // With two sessions blocked, we should get two write calls.  They should both
  // go to the first stream as it will only write 6k and mark itself blocked
  // again.
  InSequence s;
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendLargeFakeData(stream2, 6000);
    session_.MarkConnectionLevelWriteBlocked(stream2->id());
  }));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendLargeFakeData(stream2, 6000);
    session_.MarkConnectionLevelWriteBlocked(stream2->id());
  }));
  session_.OnCanWrite();

  // We should get one more call for stream2, at which point it has used its
  // write quota and we move over to stream 4.
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendLargeFakeData(stream2, 6000);
    session_.MarkConnectionLevelWriteBlocked(stream2->id());
  }));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(Invoke([this, stream4]() {
    session_.SendLargeFakeData(stream4, 6000);
    session_.MarkConnectionLevelWriteBlocked(stream4->id());
  }));
  session_.OnCanWrite();

  // Now let stream 4 do the 2nd of its 3 writes, but add a block for a high
  // priority stream 6.  4 should be preempted.  6 will write but *not* block so
  // will cede back to 4.
  stream6->SetPriority(kV3HighestPriority);
  EXPECT_CALL(*stream4, OnCanWrite())
      .WillOnce(Invoke([this, stream4, stream6]() {
        session_.SendLargeFakeData(stream4, 6000);
        session_.MarkConnectionLevelWriteBlocked(stream4->id());
        session_.MarkConnectionLevelWriteBlocked(stream6->id());
      }));
  EXPECT_CALL(*stream6, OnCanWrite())
      .WillOnce(Invoke([this, stream4, stream6]() {
        session_.SendStreamData(stream6);
        session_.SendLargeFakeData(stream4, 6000);
      }));
  session_.OnCanWrite();

  // Stream4 alread did 6k worth of writes, so after doing another 12k it should
  // cede and 2 should resume.
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(Invoke([this, stream4]() {
    session_.SendLargeFakeData(stream4, 12000);
    session_.MarkConnectionLevelWriteBlocked(stream4->id());
  }));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendLargeFakeData(stream2, 6000);
    session_.MarkConnectionLevelWriteBlocked(stream2->id());
  }));
  session_.OnCanWrite();
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteBundlesStreams) {
  // Encryption needs to be established before data can be sent.
  CryptoHandshakeMessage msg;
  MockPacketWriter* writer = static_cast<MockPacketWriter*>(
      QuicConnectionPeer::GetWriter(session_.connection()));
  EXPECT_CALL(*writer, WritePacket(_, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  session_.GetMutableCryptoStream()->OnHandshakeMessage(msg);

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);

  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();

  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream6->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());

  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm, GetCongestionWindow())
      .WillRepeatedly(Return(kMaxPacketSize * 10));
  EXPECT_CALL(*send_algorithm, InRecovery()).WillRepeatedly(Return(false));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendStreamData(stream2);
  }));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(Invoke([this, stream4]() {
    session_.SendStreamData(stream4);
  }));
  EXPECT_CALL(*stream6, OnCanWrite()).WillOnce(Invoke([this, stream6]() {
    session_.SendStreamData(stream6);
  }));

  // Expect that we only send one packet, the writes from different streams
  // should be bundled together.
  EXPECT_CALL(*writer, WritePacket(_, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  EXPECT_CALL(*send_algorithm, OnPacketSent(_, _, _, _, _));
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));
  session_.OnCanWrite();
  EXPECT_FALSE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteCongestionControlBlocks) {
  session_.set_writev_consumes_all_data(true);
  InSequence s;

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);

  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();

  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream6->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());

  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendStreamData(stream2);
  }));
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream6, OnCanWrite()).WillOnce(Invoke([this, stream6]() {
    session_.SendStreamData(stream6);
  }));
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(false));
  // stream4->OnCanWrite is not called.

  session_.OnCanWrite();
  EXPECT_TRUE(session_.WillingAndAbleToWrite());

  // Still congestion-control blocked.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(false));
  session_.OnCanWrite();
  EXPECT_TRUE(session_.WillingAndAbleToWrite());

  // stream4->OnCanWrite is called once the connection stops being
  // congestion-control blocked.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(Invoke([this, stream4]() {
    session_.SendStreamData(stream4);
  }));
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));
  session_.OnCanWrite();
  EXPECT_FALSE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteWriterBlocks) {
  // Drive congestion control manually in order to ensure that
  // application-limited signaling is handled correctly.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(true));

  // Drive packet writer manually.
  MockPacketWriter* writer = static_cast<MockPacketWriter*>(
      QuicConnectionPeer::GetWriter(session_.connection()));
  EXPECT_CALL(*writer, IsWriteBlocked()).WillRepeatedly(Return(true));
  EXPECT_CALL(*writer, IsWriteBlockedDataBuffered())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*writer, WritePacket(_, _, _, _, _)).Times(0);

  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();

  session_.MarkConnectionLevelWriteBlocked(stream2->id());

  EXPECT_CALL(*stream2, OnCanWrite()).Times(0);
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_)).Times(0);

  session_.OnCanWrite();
  EXPECT_TRUE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, BufferedHandshake) {
  session_.set_writev_consumes_all_data(true);
  EXPECT_FALSE(session_.HasPendingHandshake());  // Default value.

  // Test that blocking other streams does not change our status.
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  EXPECT_FALSE(session_.HasPendingHandshake());

  TestStream* stream3 = session_.CreateOutgoingBidirectionalStream();
  session_.MarkConnectionLevelWriteBlocked(stream3->id());
  EXPECT_FALSE(session_.HasPendingHandshake());

  // Blocking (due to buffering of) the Crypto stream is detected.
  session_.MarkConnectionLevelWriteBlocked(
      QuicUtils::GetCryptoStreamId(connection_->transport_version()));
  EXPECT_TRUE(session_.HasPendingHandshake());

  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  session_.MarkConnectionLevelWriteBlocked(stream4->id());
  EXPECT_TRUE(session_.HasPendingHandshake());

  InSequence s;
  // Force most streams to re-register, which is common scenario when we block
  // the Crypto stream, and only the crypto stream can "really" write.

  // Due to prioritization, we *should* be asked to write the crypto stream
  // first.
  // Don't re-register the crypto stream (which signals complete writing).
  TestCryptoStream* crypto_stream = session_.GetMutableCryptoStream();
  EXPECT_CALL(*crypto_stream, OnCanWrite());

  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendStreamData(stream2);
  }));
  EXPECT_CALL(*stream3, OnCanWrite()).WillOnce(Invoke([this, stream3]() {
    session_.SendStreamData(stream3);
  }));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(Invoke([this, stream4]() {
    session_.SendStreamData(stream4);
    session_.MarkConnectionLevelWriteBlocked(stream4->id());
  }));

  session_.OnCanWrite();
  EXPECT_TRUE(session_.WillingAndAbleToWrite());
  EXPECT_FALSE(session_.HasPendingHandshake());  // Crypto stream wrote.
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteWithClosedStream) {
  session_.set_writev_consumes_all_data(true);
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();

  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream6->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());
  CloseStream(stream6->id());

  InSequence s;
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(Invoke(&session_, &TestSession::ClearControlFrame));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(Invoke([this, stream2]() {
    session_.SendStreamData(stream2);
  }));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(Invoke([this, stream4]() {
    session_.SendStreamData(stream4);
  }));
  session_.OnCanWrite();
  EXPECT_FALSE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer,
       OnCanWriteLimitsNumWritesIfFlowControlBlocked) {
  // Drive congestion control manually in order to ensure that
  // application-limited signaling is handled correctly.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(true));

  // Ensure connection level flow control blockage.
  QuicFlowControllerPeer::SetSendWindowOffset(session_.flow_controller(), 0);
  EXPECT_TRUE(session_.flow_controller()->IsBlocked());
  EXPECT_TRUE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());

  // Mark the crypto and headers streams as write blocked, we expect them to be
  // allowed to write later.
  session_.MarkConnectionLevelWriteBlocked(
      QuicUtils::GetCryptoStreamId(connection_->transport_version()));

  // Create a data stream, and although it is write blocked we never expect it
  // to be allowed to write as we are connection level flow control blocked.
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();
  session_.MarkConnectionLevelWriteBlocked(stream->id());
  EXPECT_CALL(*stream, OnCanWrite()).Times(0);

  // The crypto and headers streams should be called even though we are
  // connection flow control blocked.
  TestCryptoStream* crypto_stream = session_.GetMutableCryptoStream();
  EXPECT_CALL(*crypto_stream, OnCanWrite());
  QuicSpdySessionPeer::SetHeadersStream(&session_, nullptr);
  TestHeadersStream* headers_stream = new TestHeadersStream(&session_);
  QuicSpdySessionPeer::SetHeadersStream(&session_, headers_stream);
  session_.MarkConnectionLevelWriteBlocked(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()));
  EXPECT_CALL(*headers_stream, OnCanWrite());

  // After the crypto and header streams perform a write, the connection will be
  // blocked by the flow control, hence it should become application-limited.
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));

  session_.OnCanWrite();
  EXPECT_FALSE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, SendGoAway) {
  if (transport_version() == QUIC_VERSION_99) {
    // GoAway frames are not in version 99
    return;
  }
  MockPacketWriter* writer = static_cast<MockPacketWriter*>(
      QuicConnectionPeer::GetWriter(session_.connection()));
  EXPECT_CALL(*writer, WritePacket(_, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallySendControlFrame));
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_.goaway_sent());

  const QuicStreamId kTestStreamId = 5u;
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(0);
  EXPECT_CALL(*connection_,
              OnStreamReset(kTestStreamId, QUIC_STREAM_PEER_GOING_AWAY))
      .Times(0);
  EXPECT_TRUE(session_.GetOrCreateDynamicStream(kTestStreamId));
}

TEST_P(QuicSpdySessionTestServer, DoNotSendGoAwayTwice) {
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_.goaway_sent());
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
}

TEST_P(QuicSpdySessionTestServer, InvalidGoAway) {
  QuicGoAwayFrame go_away(kInvalidControlFrameId, QUIC_PEER_GOING_AWAY,
                          session_.next_outgoing_stream_id(), "");
  session_.OnGoAway(go_away);
}

// Test that server session will send a connectivity probe in response to a
// connectivity probe on the same path.
TEST_P(QuicSpdySessionTestServer, ServerReplyToConnecitivityProbe) {
  QuicSocketAddress old_peer_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort);
  EXPECT_EQ(old_peer_address, session_.peer_address());

  QuicSocketAddress new_peer_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort + 1);

  EXPECT_CALL(*connection_,
              SendConnectivityProbingResponsePacket(new_peer_address));
  if (transport_version() == QUIC_VERSION_99) {
    // Need to explicitly do this to emulate the reception of a PathChallenge,
    // which stores its payload for use in generating the response.
    connection_->OnPathChallengeFrame(
        QuicPathChallengeFrame(0, {{0, 1, 2, 3, 4, 5, 6, 7}}));
  }
  session_.OnConnectivityProbeReceived(session_.self_address(),
                                       new_peer_address);
  EXPECT_EQ(old_peer_address, session_.peer_address());
}

TEST_P(QuicSpdySessionTestServer, IncreasedTimeoutAfterCryptoHandshake) {
  EXPECT_EQ(kInitialIdleTimeoutSecs + 3,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
  CryptoHandshakeMessage msg;
  session_.GetMutableCryptoStream()->OnHandshakeMessage(msg);
  EXPECT_EQ(kMaximumIdleTimeoutSecs + 3,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
}

TEST_P(QuicSpdySessionTestServer, RstStreamBeforeHeadersDecompressed) {
  // Send two bytes of payload.
  QuicStreamFrame data1(GetNthClientInitiatedId(0), false, 0,
                        QuicStringPiece("HT"));
  session_.OnStreamFrame(data1);
  EXPECT_EQ(1u, session_.GetNumOpenIncomingStreams());

  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(GetNthClientInitiatedId(0), _));
  QuicRstStreamFrame rst1(kInvalidControlFrameId, GetNthClientInitiatedId(0),
                          QUIC_ERROR_PROCESSING_STREAM, 0);
  session_.OnRstStream(rst1);
  EXPECT_EQ(0u, session_.GetNumOpenIncomingStreams());
  // Connection should remain alive.
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicSpdySessionTestServer, OnStreamFrameFinStaticStreamId) {
  // Send two bytes of payload.
  QuicStreamFrame data1(
      QuicUtils::GetCryptoStreamId(connection_->transport_version()), true, 0,
      QuicStringPiece("HT"));
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Attempt to close a static stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_.OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestServer, OnRstStreamStaticStreamId) {
  // Send two bytes of payload.
  QuicRstStreamFrame rst1(
      kInvalidControlFrameId,
      QuicUtils::GetCryptoStreamId(connection_->transport_version()),
      QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Attempt to reset a static stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_.OnRstStream(rst1);
}

TEST_P(QuicSpdySessionTestServer, OnStreamFrameInvalidStreamId) {
  // Send two bytes of payload.
  QuicStreamFrame data1(
      QuicUtils::GetInvalidStreamId(connection_->transport_version()), true, 0,
      QuicStringPiece("HT"));
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Recevied data for an invalid stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_.OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestServer, OnRstStreamInvalidStreamId) {
  // Send two bytes of payload.
  QuicRstStreamFrame rst1(
      kInvalidControlFrameId,
      QuicUtils::GetInvalidStreamId(connection_->transport_version()),
      QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Recevied data for an invalid stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_.OnRstStream(rst1);
}

TEST_P(QuicSpdySessionTestServer, HandshakeUnblocksFlowControlBlockedStream) {
  // Test that if a stream is flow control blocked, then on receipt of the SHLO
  // containing a suitable send window offset, the stream becomes unblocked.

  // Ensure that Writev consumes all the data it is given (simulate no socket
  // blocking).
  session_.set_writev_consumes_all_data(true);

  // Create a stream, and send enough data to make it flow control blocked.
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  QuicString body(kMinimumFlowControlSendWindow, '.');
  EXPECT_FALSE(stream2->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(AtLeast(1));
  stream2->WriteOrBufferBody(body, false, nullptr);
  EXPECT_TRUE(stream2->flow_controller()->IsBlocked());
  EXPECT_TRUE(session_.IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_.IsStreamFlowControlBlocked());

  // Now complete the crypto handshake, resulting in an increased flow control
  // send window.
  CryptoHandshakeMessage msg;
  session_.GetMutableCryptoStream()->OnHandshakeMessage(msg);
  EXPECT_TRUE(QuicSessionPeer::IsStreamWriteBlocked(&session_, stream2->id()));
  // Stream is now unblocked.
  EXPECT_FALSE(stream2->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
}

TEST_P(QuicSpdySessionTestServer,
       HandshakeUnblocksFlowControlBlockedCryptoStream) {
  // Test that if the crypto stream is flow control blocked, then if the SHLO
  // contains a larger send window offset, the stream becomes unblocked.
  session_.set_writev_consumes_all_data(true);
  TestCryptoStream* crypto_stream = session_.GetMutableCryptoStream();
  EXPECT_FALSE(crypto_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(&session_);
  EXPECT_FALSE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
  for (QuicStreamId i = 0;
       !crypto_stream->flow_controller()->IsBlocked() && i < 1000u; i++) {
    EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
    EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
    QuicStreamOffset offset = crypto_stream->stream_bytes_written();
    QuicConfig config;
    CryptoHandshakeMessage crypto_message;
    config.ToHandshakeMessage(&crypto_message);
    crypto_stream->SendHandshakeMessage(crypto_message);
    char buf[1000];
    QuicDataWriter writer(1000, buf, NETWORK_BYTE_ORDER);
    crypto_stream->WriteStreamData(offset, crypto_message.size(), &writer);
  }
  EXPECT_TRUE(crypto_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_.IsStreamFlowControlBlocked());
  EXPECT_FALSE(session_.HasDataToWrite());
  EXPECT_TRUE(crypto_stream->HasBufferedData());

  // Now complete the crypto handshake, resulting in an increased flow control
  // send window.
  CryptoHandshakeMessage msg;
  session_.GetMutableCryptoStream()->OnHandshakeMessage(msg);
  EXPECT_TRUE(QuicSessionPeer::IsStreamWriteBlocked(
      &session_,
      QuicUtils::GetCryptoStreamId(connection_->transport_version())));
  // Stream is now unblocked and will no longer have buffered data.
  EXPECT_FALSE(crypto_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
}

#if !defined(OS_IOS)
// This test is failing flakily for iOS bots.
// http://crbug.com/425050
// NOTE: It's not possible to use the standard MAYBE_ convention to disable
// this test on iOS because when this test gets instantiated it ends up with
// various names that are dependent on the parameters passed.
TEST_P(QuicSpdySessionTestServer,
       HandshakeUnblocksFlowControlBlockedHeadersStream) {
  // Test that if the header stream is flow control blocked, then if the SHLO
  // contains a larger send window offset, the stream becomes unblocked.
  session_.set_writev_consumes_all_data(true);
  TestCryptoStream* crypto_stream = session_.GetMutableCryptoStream();
  EXPECT_FALSE(crypto_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(&session_);
  EXPECT_FALSE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
  QuicStreamId stream_id = 5;
  // Write until the header stream is flow control blocked.
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
  SpdyHeaderBlock headers;
  SimpleRandom random;
  while (!headers_stream->flow_controller()->IsBlocked() && stream_id < 2000) {
    EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
    EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
    headers["header"] = QuicStrCat(random.RandUint64(), random.RandUint64(),
                                   random.RandUint64());
    session_.WriteHeaders(stream_id, headers.Clone(), true, 0, nullptr);
    stream_id += 2;
  }
  // Write once more to ensure that the headers stream has buffered data. The
  // random headers may have exactly filled the flow control window.
  session_.WriteHeaders(stream_id, std::move(headers), true, 0, nullptr);
  EXPECT_TRUE(headers_stream->HasBufferedData());

  EXPECT_TRUE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(crypto_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_.IsStreamFlowControlBlocked());
  EXPECT_FALSE(session_.HasDataToWrite());

  // Now complete the crypto handshake, resulting in an increased flow control
  // send window.
  CryptoHandshakeMessage msg;
  session_.GetMutableCryptoStream()->OnHandshakeMessage(msg);

  // Stream is now unblocked and will no longer have buffered data.
  EXPECT_FALSE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
  EXPECT_TRUE(headers_stream->HasBufferedData());
  EXPECT_TRUE(QuicSessionPeer::IsStreamWriteBlocked(
      &session_,
      QuicUtils::GetHeadersStreamId(connection_->transport_version())));
}
#endif  // !defined(OS_IOS)

TEST_P(QuicSpdySessionTestServer,
       ConnectionFlowControlAccountingRstOutOfOrder) {
  // Test that when we receive an out of order stream RST we correctly adjust
  // our connection level flow control receive window.
  // On close, the stream should mark as consumed all bytes between the highest
  // byte consumed so far and the final byte offset from the RST frame.
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();

  const QuicStreamOffset kByteOffset =
      1 + kInitialSessionFlowControlWindowForTest / 2;

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .Times(2)
      .WillRepeatedly(Invoke(&session_, &TestSession::ClearControlFrame));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream->id(),
                               QUIC_STREAM_CANCELLED, kByteOffset);
  session_.OnRstStream(rst_frame);
  if (!session_.deprecate_post_process_after_data()) {
    session_.PostProcessAfterData();
  }
  EXPECT_EQ(kByteOffset, session_.flow_controller()->bytes_consumed());
}

TEST_P(QuicSpdySessionTestServer,
       ConnectionFlowControlAccountingFinAndLocalReset) {
  // Test the situation where we receive a FIN on a stream, and before we fully
  // consume all the data from the sequencer buffer we locally RST the stream.
  // The bytes between highest consumed byte, and the final byte offset that we
  // determined when the FIN arrived, should be marked as consumed at the
  // connection level flow controller when the stream is reset.
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();

  const QuicStreamOffset kByteOffset =
      kInitialSessionFlowControlWindowForTest / 2 - 1;
  QuicStreamFrame frame(stream->id(), true, kByteOffset, ".");
  session_.OnStreamFrame(frame);
  if (!session_.deprecate_post_process_after_data()) {
    session_.PostProcessAfterData();
  }
  EXPECT_TRUE(connection_->connected());

  EXPECT_EQ(0u, stream->flow_controller()->bytes_consumed());
  EXPECT_EQ(kByteOffset + frame.data_length,
            stream->flow_controller()->highest_received_byte_offset());

  // Reset stream locally.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  stream->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_EQ(kByteOffset + frame.data_length,
            session_.flow_controller()->bytes_consumed());
}

TEST_P(QuicSpdySessionTestServer, ConnectionFlowControlAccountingFinAfterRst) {
  // Test that when we RST the stream (and tear down stream state), and then
  // receive a FIN from the peer, we correctly adjust our connection level flow
  // control receive window.

  // Connection starts with some non-zero highest received byte offset,
  // due to other active streams.
  const uint64_t kInitialConnectionBytesConsumed = 567;
  const uint64_t kInitialConnectionHighestReceivedOffset = 1234;
  EXPECT_LT(kInitialConnectionBytesConsumed,
            kInitialConnectionHighestReceivedOffset);
  session_.flow_controller()->UpdateHighestReceivedOffset(
      kInitialConnectionHighestReceivedOffset);
  session_.flow_controller()->AddBytesConsumed(kInitialConnectionBytesConsumed);

  // Reset our stream: this results in the stream being closed locally.
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  stream->Reset(QUIC_STREAM_CANCELLED);

  // Now receive a response from the peer with a FIN. We should handle this by
  // adjusting the connection level flow control receive window to take into
  // account the total number of bytes sent by the peer.
  const QuicStreamOffset kByteOffset = 5678;
  QuicString body = "hello";
  QuicStreamFrame frame(stream->id(), true, kByteOffset, QuicStringPiece(body));
  session_.OnStreamFrame(frame);

  QuicStreamOffset total_stream_bytes_sent_by_peer =
      kByteOffset + body.length();
  EXPECT_EQ(kInitialConnectionBytesConsumed + total_stream_bytes_sent_by_peer,
            session_.flow_controller()->bytes_consumed());
  EXPECT_EQ(
      kInitialConnectionHighestReceivedOffset + total_stream_bytes_sent_by_peer,
      session_.flow_controller()->highest_received_byte_offset());
}

TEST_P(QuicSpdySessionTestServer, ConnectionFlowControlAccountingRstAfterRst) {
  // Test that when we RST the stream (and tear down stream state), and then
  // receive a RST from the peer, we correctly adjust our connection level flow
  // control receive window.

  // Connection starts with some non-zero highest received byte offset,
  // due to other active streams.
  const uint64_t kInitialConnectionBytesConsumed = 567;
  const uint64_t kInitialConnectionHighestReceivedOffset = 1234;
  EXPECT_LT(kInitialConnectionBytesConsumed,
            kInitialConnectionHighestReceivedOffset);
  session_.flow_controller()->UpdateHighestReceivedOffset(
      kInitialConnectionHighestReceivedOffset);
  session_.flow_controller()->AddBytesConsumed(kInitialConnectionBytesConsumed);

  // Reset our stream: this results in the stream being closed locally.
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  stream->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream));

  // Now receive a RST from the peer. We should handle this by adjusting the
  // connection level flow control receive window to take into account the total
  // number of bytes sent by the peer.
  const QuicStreamOffset kByteOffset = 5678;
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream->id(),
                               QUIC_STREAM_CANCELLED, kByteOffset);
  session_.OnRstStream(rst_frame);

  EXPECT_EQ(kInitialConnectionBytesConsumed + kByteOffset,
            session_.flow_controller()->bytes_consumed());
  EXPECT_EQ(kInitialConnectionHighestReceivedOffset + kByteOffset,
            session_.flow_controller()->highest_received_byte_offset());
}

TEST_P(QuicSpdySessionTestServer, InvalidStreamFlowControlWindowInHandshake) {
  // Test that receipt of an invalid (< default) stream flow control window from
  // the peer results in the connection being torn down.
  const uint32_t kInvalidWindow = kMinimumFlowControlSendWindow - 1;
  QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(session_.config(),
                                                            kInvalidWindow);

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_INVALID_WINDOW, _, _));
  session_.OnConfigNegotiated();
}

TEST_P(QuicSpdySessionTestServer, InvalidSessionFlowControlWindowInHandshake) {
  // Test that receipt of an invalid (< default) session flow control window
  // from the peer results in the connection being torn down.
  const uint32_t kInvalidWindow = kMinimumFlowControlSendWindow - 1;
  QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(session_.config(),
                                                             kInvalidWindow);

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_INVALID_WINDOW, _, _));
  session_.OnConfigNegotiated();
}

// Test negotiation of custom server initial flow control window.
TEST_P(QuicSpdySessionTestServer, CustomFlowControlWindow) {
  QuicTagVector copt;
  copt.push_back(kIFW7);
  QuicConfigPeer::SetReceivedConnectionOptions(session_.config(), copt);

  session_.OnConfigNegotiated();
  EXPECT_EQ(192 * 1024u, QuicFlowControllerPeer::ReceiveWindowSize(
                             session_.flow_controller()));
}

TEST_P(QuicSpdySessionTestServer, FlowControlWithInvalidFinalOffset) {
  // Test that if we receive a stream RST with a highest byte offset that
  // violates flow control, that we close the connection.
  const uint64_t kLargeOffset = kInitialSessionFlowControlWindowForTest + 1;
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _))
      .Times(2);

  // Check that stream frame + FIN results in connection close.
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  stream->Reset(QUIC_STREAM_CANCELLED);
  QuicStreamFrame frame(stream->id(), true, kLargeOffset, QuicStringPiece());
  session_.OnStreamFrame(frame);

  // Check that RST results in connection close.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream->id(),
                               QUIC_STREAM_CANCELLED, kLargeOffset);
  session_.OnRstStream(rst_frame);
}

TEST_P(QuicSpdySessionTestServer, WindowUpdateUnblocksHeadersStream) {
  // Test that a flow control blocked headers stream gets unblocked on recipt of
  // a WINDOW_UPDATE frame.

  // Set the headers stream to be flow control blocked.
  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(&session_);
  QuicFlowControllerPeer::SetSendWindowOffset(headers_stream->flow_controller(),
                                              0);
  EXPECT_TRUE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_.IsStreamFlowControlBlocked());

  // Unblock the headers stream by supplying a WINDOW_UPDATE.
  QuicWindowUpdateFrame window_update_frame(kInvalidControlFrameId,
                                            headers_stream->id(),
                                            2 * kMinimumFlowControlSendWindow);
  session_.OnWindowUpdateFrame(window_update_frame);
  EXPECT_FALSE(headers_stream->flow_controller()->IsBlocked());
  EXPECT_FALSE(session_.IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_.IsStreamFlowControlBlocked());
}

TEST_P(QuicSpdySessionTestServer,
       TooManyUnfinishedStreamsCauseServerRejectStream) {
  // If a buggy/malicious peer creates too many streams that are not ended
  // with a FIN or RST then we send an RST to refuse streams.
  const QuicStreamId kMaxStreams = 5;
  QuicSessionPeer::SetMaxOpenIncomingStreams(&session_, kMaxStreams);
  const QuicStreamId kFirstStreamId = GetNthClientInitiatedId(0);
  const QuicStreamId kFinalStreamId = GetNthClientInitiatedId(kMaxStreams);
  // Create kMaxStreams data streams, and close them all without receiving a
  // FIN or a RST_STREAM from the client.
  const QuicStreamId kNextId = QuicSpdySessionPeer::NextStreamId(session_);
  for (QuicStreamId i = kFirstStreamId; i < kFinalStreamId; i += kNextId) {
    QuicStreamFrame data1(i, false, 0, QuicStringPiece("HT"));
    session_.OnStreamFrame(data1);
    // EXPECT_EQ(1u, session_.GetNumOpenStreams());
    EXPECT_CALL(*connection_, SendControlFrame(_))
        .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
    EXPECT_CALL(*connection_, OnStreamReset(i, _));
    session_.CloseStream(i);
  }

  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(1);
  EXPECT_CALL(*connection_, OnStreamReset(kFinalStreamId, QUIC_REFUSED_STREAM))
      .Times(1);
  // Create one more data streams to exceed limit of open stream.
  QuicStreamFrame data1(kFinalStreamId, false, 0, QuicStringPiece("HT"));
  session_.OnStreamFrame(data1);

  // Called after any new data is received by the session, and triggers the
  // call to close the connection.
  if (!session_.deprecate_post_process_after_data()) {
    session_.PostProcessAfterData();
  }
}

TEST_P(QuicSpdySessionTestServer, DrainingStreamsDoNotCountAsOpened) {
  // Verify that a draining stream (which has received a FIN but not consumed
  // it) does not count against the open quota (because it is closed from the
  // protocol point of view).
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(0);
  EXPECT_CALL(*connection_, OnStreamReset(_, QUIC_REFUSED_STREAM)).Times(0);
  const QuicStreamId kMaxStreams = 5;
  QuicSessionPeer::SetMaxOpenIncomingStreams(&session_, kMaxStreams);

  // Create kMaxStreams + 1 data streams, and mark them draining.
  const QuicStreamId kFirstStreamId = GetNthClientInitiatedId(0);
  const QuicStreamId kFinalStreamId =
      GetNthClientInitiatedId(2 * kMaxStreams + 1);
  for (QuicStreamId i = kFirstStreamId; i < kFinalStreamId; i += NextId()) {
    QuicStreamFrame data1(i, true, 0, QuicStringPiece("HT"));
    session_.OnStreamFrame(data1);
    EXPECT_EQ(1u, session_.GetNumOpenIncomingStreams());
    session_.StreamDraining(i);
    EXPECT_EQ(0u, session_.GetNumOpenIncomingStreams());
  }

  // Called after any new data is received by the session, and triggers the call
  // to close the connection.
  if (!session_.deprecate_post_process_after_data()) {
    session_.PostProcessAfterData();
  }
}

TEST_P(QuicSpdySessionTestServer, TestMaxIncomingAndOutgoingStreamsAllowed) {
  // Tests that on server side, the value of max_open_incoming/outgoing streams
  // are setup correctly during negotiation.
  // The value for outgoing stream is limited to negotiated value and for
  // incoming stream it is set to be larger than that.
  session_.OnConfigNegotiated();
  // The max number of open outgoing streams is less than that of incoming
  // streams, and it should be same as negotiated value.
  EXPECT_LT(session_.max_open_outgoing_streams(),
            session_.max_open_incoming_streams());
  EXPECT_EQ(session_.max_open_outgoing_streams(),
            kDefaultMaxStreamsPerConnection);
  EXPECT_GT(session_.max_open_incoming_streams(),
            kDefaultMaxStreamsPerConnection);
}

class QuicSpdySessionTestClient : public QuicSpdySessionTestBase {
 protected:
  QuicSpdySessionTestClient()
      : QuicSpdySessionTestBase(Perspective::IS_CLIENT) {}
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicSpdySessionTestClient,
                        ::testing::ValuesIn(AllSupportedVersions()));

TEST_P(QuicSpdySessionTestClient, AvailableStreamsClient) {
  ASSERT_TRUE(session_.GetOrCreateDynamicStream(GetNthServerInitiatedId(2)) !=
              nullptr);
  // Both server initiated streams with smaller stream IDs should be available.
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(&session_,
                                                 GetNthServerInitiatedId(0)));
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(&session_,
                                                 GetNthServerInitiatedId(1)));
  ASSERT_TRUE(session_.GetOrCreateDynamicStream(GetNthServerInitiatedId(0)) !=
              nullptr);
  ASSERT_TRUE(session_.GetOrCreateDynamicStream(GetNthServerInitiatedId(1)) !=
              nullptr);
  // And client initiated stream ID should be not available.
  EXPECT_FALSE(QuicSessionPeer::IsStreamAvailable(&session_,
                                                  GetNthClientInitiatedId(0)));
}

TEST_P(QuicSpdySessionTestClient, RecordFinAfterReadSideClosed) {
  // Verify that an incoming FIN is recorded in a stream object even if the read
  // side has been closed.  This prevents an entry from being made in
  // locally_closed_streams_highest_offset_ (which will never be deleted).
  TestStream* stream = session_.CreateOutgoingBidirectionalStream();
  QuicStreamId stream_id = stream->id();

  // Close the read side manually.
  QuicStreamPeer::CloseReadSide(stream);

  // Receive a stream data frame with FIN.
  QuicStreamFrame frame(stream_id, true, 0, QuicStringPiece());
  session_.OnStreamFrame(frame);
  EXPECT_TRUE(stream->fin_received());

  // Reset stream locally.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  stream->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream));

  // Allow the session to delete the stream object.
  if (!session_.deprecate_post_process_after_data()) {
    session_.PostProcessAfterData();
  }
  EXPECT_TRUE(connection_->connected());
  EXPECT_TRUE(QuicSessionPeer::IsStreamClosed(&session_, stream_id));
  EXPECT_FALSE(QuicSessionPeer::IsStreamCreated(&session_, stream_id));

  // The stream is not waiting for the arrival of the peer's final offset as it
  // was received with the FIN earlier.
  EXPECT_EQ(
      0u,
      QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(&session_).size());
}

TEST_P(QuicSpdySessionTestClient, TestMaxIncomingAndOutgoingStreamsAllowed) {
  // Tests that on client side, the value of max_open_incoming/outgoing streams
  // are setup correctly during negotiation.
  // When flag is true, the value for outgoing stream is limited to negotiated
  // value and for incoming stream it is set to be larger than that.
  session_.OnConfigNegotiated();
  EXPECT_LT(session_.max_open_outgoing_streams(),
            session_.max_open_incoming_streams());
  EXPECT_EQ(session_.max_open_outgoing_streams(),
            kDefaultMaxStreamsPerConnection);
}

TEST_P(QuicSpdySessionTestClient, WritePriority) {
  QuicSpdySessionPeer::SetHeadersStream(&session_, nullptr);
  TestHeadersStream* headers_stream = new TestHeadersStream(&session_);
  QuicSpdySessionPeer::SetHeadersStream(&session_, headers_stream);

  // Make packet writer blocked so |headers_stream| will buffer its write data.
  MockPacketWriter* writer = static_cast<MockPacketWriter*>(
      QuicConnectionPeer::GetWriter(session_.connection()));
  EXPECT_CALL(*writer, IsWriteBlocked()).WillRepeatedly(Return(true));

  const QuicStreamId id = 4;
  const QuicStreamId parent_stream_id = 9;
  const SpdyPriority priority = kV3HighestPriority;
  const bool exclusive = true;
  session_.WritePriority(id, parent_stream_id,
                         Spdy3PriorityToHttp2Weight(priority), exclusive);

  QuicStreamSendBuffer& send_buffer =
      QuicStreamPeer::SendBuffer(headers_stream);
  if (transport_version() > QUIC_VERSION_39) {
    ASSERT_EQ(1u, send_buffer.size());

    SpdyPriorityIR priority_frame(
        id, parent_stream_id, Spdy3PriorityToHttp2Weight(priority), exclusive);
    SpdyFramer spdy_framer(SpdyFramer::ENABLE_COMPRESSION);
    SpdySerializedFrame frame = spdy_framer.SerializeFrame(priority_frame);

    const QuicMemSlice& slice =
        QuicStreamSendBufferPeer::CurrentWriteSlice(&send_buffer)->slice;
    EXPECT_EQ(QuicStringPiece(frame.data(), frame.size()),
              QuicStringPiece(slice.data(), slice.length()));
  } else {
    EXPECT_EQ(0u, send_buffer.size());
  }
}

TEST_P(QuicSpdySessionTestServer, ZombieStreams) {
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  QuicStreamPeer::SetStreamBytesWritten(3, stream2);
  EXPECT_TRUE(stream2->IsWaitingForAcks());

  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream2->id(), _));
  session_.CloseStream(stream2->id());
  EXPECT_FALSE(QuicContainsKey(session_.zombie_streams(), stream2->id()));
  ASSERT_EQ(1u, session_.closed_streams()->size());
  EXPECT_EQ(stream2->id(), session_.closed_streams()->front()->id());
  session_.OnStreamDoneWaitingForAcks(2);
  EXPECT_FALSE(QuicContainsKey(session_.zombie_streams(), stream2->id()));
  EXPECT_EQ(1u, session_.closed_streams()->size());
  EXPECT_EQ(stream2->id(), session_.closed_streams()->front()->id());
}

TEST_P(QuicSpdySessionTestServer, OnStreamFrameLost) {
  QuicConnectionPeer::SetSessionDecidesWhatToWrite(connection_);
  InSequence s;

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);

  TestCryptoStream* crypto_stream = session_.GetMutableCryptoStream();
  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();

  QuicStreamFrame frame1(
      QuicUtils::GetCryptoStreamId(connection_->transport_version()), false, 0,
      1300);
  QuicStreamFrame frame2(stream2->id(), false, 0, 9);
  QuicStreamFrame frame3(stream4->id(), false, 0, 9);

  // Lost data on cryption stream, streams 2 and 4.
  EXPECT_CALL(*stream4, HasPendingRetransmission()).WillOnce(Return(true));
  EXPECT_CALL(*crypto_stream, HasPendingRetransmission())
      .WillOnce(Return(true));
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(true));
  session_.OnFrameLost(QuicFrame(frame3));
  session_.OnFrameLost(QuicFrame(frame1));
  session_.OnFrameLost(QuicFrame(frame2));
  EXPECT_TRUE(session_.WillingAndAbleToWrite());

  // Mark streams 2 and 4 write blocked.
  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());

  // Lost data is retransmitted before new data, and retransmissions for crypto
  // stream go first.
  // Do not check congestion window when crypto stream has lost data.
  EXPECT_CALL(*send_algorithm, CanSend(_)).Times(0);
  EXPECT_CALL(*crypto_stream, OnCanWrite());
  EXPECT_CALL(*crypto_stream, HasPendingRetransmission())
      .WillOnce(Return(false));
  // Check congestion window for non crypto streams.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream4, OnCanWrite());
  EXPECT_CALL(*stream4, HasPendingRetransmission()).WillOnce(Return(false));
  // Connection is blocked.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(false));

  session_.OnCanWrite();
  EXPECT_TRUE(session_.WillingAndAbleToWrite());

  // Unblock connection.
  // Stream 2 retransmits lost data.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  // Stream 2 sends new data.
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream4, OnCanWrite());
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));

  session_.OnCanWrite();
  EXPECT_FALSE(session_.WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, DonotRetransmitDataOfClosedStreams) {
  QuicConnectionPeer::SetSessionDecidesWhatToWrite(connection_);
  InSequence s;

  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();

  QuicStreamFrame frame1(stream2->id(), false, 0, 9);
  QuicStreamFrame frame2(stream4->id(), false, 0, 9);
  QuicStreamFrame frame3(stream6->id(), false, 0, 9);

  EXPECT_CALL(*stream6, HasPendingRetransmission()).WillOnce(Return(true));
  EXPECT_CALL(*stream4, HasPendingRetransmission()).WillOnce(Return(true));
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(true));
  session_.OnFrameLost(QuicFrame(frame3));
  session_.OnFrameLost(QuicFrame(frame2));
  session_.OnFrameLost(QuicFrame(frame1));

  session_.MarkConnectionLevelWriteBlocked(stream2->id());
  session_.MarkConnectionLevelWriteBlocked(stream4->id());
  session_.MarkConnectionLevelWriteBlocked(stream6->id());

  // Reset stream 4 locally.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream4->id(), _));
  stream4->Reset(QUIC_STREAM_CANCELLED);

  // Verify stream 4 is removed from streams with lost data list.
  EXPECT_CALL(*stream6, OnCanWrite());
  EXPECT_CALL(*stream6, HasPendingRetransmission()).WillOnce(Return(false));
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(false));
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(Invoke(&session_, &TestSession::ClearControlFrame));
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream6, OnCanWrite());
  session_.OnCanWrite();
}

TEST_P(QuicSpdySessionTestServer, RetransmitFrames) {
  QuicConnectionPeer::SetSessionDecidesWhatToWrite(connection_);
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);
  InSequence s;

  TestStream* stream2 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_.CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_.CreateOutgoingBidirectionalStream();
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
  session_.SendWindowUpdate(stream2->id(), 9);

  QuicStreamFrame frame1(stream2->id(), false, 0, 9);
  QuicStreamFrame frame2(stream4->id(), false, 0, 9);
  QuicStreamFrame frame3(stream6->id(), false, 0, 9);
  QuicWindowUpdateFrame window_update(1, stream2->id(), 9);
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1));
  frames.push_back(QuicFrame(&window_update));
  frames.push_back(QuicFrame(frame2));
  frames.push_back(QuicFrame(frame3));
  EXPECT_FALSE(session_.WillingAndAbleToWrite());

  EXPECT_CALL(*stream2, RetransmitStreamData(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(&session_, &TestSession::ClearControlFrame));
  EXPECT_CALL(*stream4, RetransmitStreamData(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(*stream6, RetransmitStreamData(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));
  session_.RetransmitFrames(frames, TLP_RETRANSMISSION);
}

TEST_P(QuicSpdySessionTestServer, OnPriorityFrame) {
  QuicStreamId stream_id = GetNthClientInitiatedId(0);
  TestStream* stream = session_.CreateIncomingStream(stream_id);
  session_.OnPriorityFrame(stream_id, kV3HighestPriority);
  EXPECT_EQ(kV3HighestPriority, stream->priority());
}

}  // namespace
}  // namespace test
}  // namespace quic
