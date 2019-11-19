// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_stream.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quiche/src/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace net {
namespace test {
namespace {

class MockQuicClientSessionBase : public quic::QuicSpdyClientSessionBase {
 public:
  explicit MockQuicClientSessionBase(quic::QuicConnection* connection,
                                     quic::QuicClientPushPromiseIndex* index);
  ~MockQuicClientSessionBase() override;

  const quic::QuicCryptoStream* GetCryptoStream() const override {
    return crypto_stream_.get();
  }

  quic::QuicCryptoStream* GetMutableCryptoStream() override {
    return crypto_stream_.get();
  }

  // From quic::QuicSession.
  MOCK_METHOD2(OnConnectionClosed,
               void(const quic::QuicConnectionCloseFrame& frame,
                    quic::ConnectionCloseSource source));
  MOCK_METHOD1(CreateIncomingStream,
               quic::QuicSpdyStream*(quic::QuicStreamId id));
  MOCK_METHOD1(CreateIncomingStream,
               quic::QuicSpdyStream*(quic::PendingStream* pending));
  MOCK_METHOD0(CreateOutgoingBidirectionalStream, QuicChromiumClientStream*());
  MOCK_METHOD0(CreateOutgoingUnidirectionalStream, QuicChromiumClientStream*());
  MOCK_METHOD5(WritevData,
               quic::QuicConsumedData(quic::QuicStream* stream,
                                      quic::QuicStreamId id,
                                      size_t write_length,
                                      quic::QuicStreamOffset offset,
                                      quic::StreamSendingState fin));
  MOCK_METHOD3(SendRstStream,
               void(quic::QuicStreamId stream_id,
                    quic::QuicRstStreamErrorCode error,
                    quic::QuicStreamOffset bytes_written));

  MOCK_METHOD2(OnStreamHeaders,
               void(quic::QuicStreamId stream_id,
                    quic::QuicStringPiece headers_data));
  MOCK_METHOD2(OnStreamHeadersPriority,
               void(quic::QuicStreamId stream_id,
                    const spdy::SpdyStreamPrecedence& precedence));
  MOCK_METHOD3(OnStreamHeadersComplete,
               void(quic::QuicStreamId stream_id, bool fin, size_t frame_len));
  MOCK_METHOD2(OnPromiseHeaders,
               void(quic::QuicStreamId stream_id,
                    quic::QuicStringPiece headers_data));
  MOCK_METHOD3(OnPromiseHeadersComplete,
               void(quic::QuicStreamId stream_id,
                    quic::QuicStreamId promised_stream_id,
                    size_t frame_len));
  MOCK_CONST_METHOD0(IsCryptoHandshakeConfirmed, bool());
  // Methods taking non-copyable types like spdy::SpdyHeaderBlock by value
  // cannot be mocked directly.
  size_t WriteHeadersOnHeadersStream(
      quic::QuicStreamId id,
      spdy::SpdyHeaderBlock headers,
      bool fin,
      const spdy::SpdyStreamPrecedence& precedence,
      quic::QuicReferenceCountedPointer<quic::QuicAckListenerInterface>
          ack_listener) override {
    return WriteHeadersOnHeadersStreamMock(id, headers, fin, precedence,
                                           std::move(ack_listener));
  }
  MOCK_METHOD5(WriteHeadersOnHeadersStreamMock,
               size_t(quic::QuicStreamId id,
                      const spdy::SpdyHeaderBlock& headers,
                      bool fin,
                      const spdy::SpdyStreamPrecedence& precedence,
                      const quic::QuicReferenceCountedPointer<
                          quic::QuicAckListenerInterface>& ack_listener));
  MOCK_METHOD1(OnHeadersHeadOfLineBlocking, void(quic::QuicTime::Delta delta));

  using quic::QuicSession::ActivateStream;

  // Returns a quic::QuicConsumedData that indicates all of |write_length| (and
  // |fin| if set) has been consumed.
  static quic::QuicConsumedData ConsumeAllData(
      quic::QuicStreamId id,
      size_t write_length,
      quic::QuicStreamOffset offset,
      bool fin,
      quic::QuicAckListenerInterface* ack_listener);

  void OnProofValid(
      const quic::QuicCryptoClientConfig::CachedState& cached) override {}
  void OnProofVerifyDetailsAvailable(
      const quic::ProofVerifyDetails& verify_details) override {}
  bool IsAuthorized(const std::string& hostname) override { return true; }

 protected:
  MOCK_METHOD1(ShouldCreateIncomingStream, bool(quic::QuicStreamId id));
  MOCK_METHOD0(ShouldCreateOutgoingBidirectionalStream, bool());
  MOCK_METHOD0(ShouldCreateOutgoingUnidirectionalStream, bool());

 private:
  std::unique_ptr<quic::QuicCryptoStream> crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicClientSessionBase);
};

MockQuicClientSessionBase::MockQuicClientSessionBase(
    quic::QuicConnection* connection,
    quic::QuicClientPushPromiseIndex* push_promise_index)
    : quic::QuicSpdyClientSessionBase(connection,
                                      push_promise_index,
                                      quic::test::DefaultQuicConfig(),
                                      connection->supported_versions()) {
  crypto_stream_.reset(new quic::test::MockQuicCryptoStream(this));
  Initialize();
  ON_CALL(*this, WritevData(_, _, _, _, _))
      .WillByDefault(testing::Return(quic::QuicConsumedData(0, false)));
}

MockQuicClientSessionBase::~MockQuicClientSessionBase() {}

class QuicChromiumClientStreamTest
    : public ::testing::TestWithParam<quic::ParsedQuicVersion>,
      public WithTaskEnvironment {
 public:
  QuicChromiumClientStreamTest()
      : version_(GetParam()),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        session_(new quic::test::MockQuicConnection(
                     &helper_,
                     &alarm_factory_,
                     quic::Perspective::IS_CLIENT,
                     quic::test::SupportedVersions(version_)),
                 &push_promise_index_) {
    quic::test::QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
        session_.config(), quic::kMinimumFlowControlSendWindow);
    quic::test::QuicConfigPeer::
        SetReceivedInitialMaxStreamDataBytesOutgoingBidirectional(
            session_.config(), quic::kMinimumFlowControlSendWindow);
    quic::test::QuicConfigPeer::SetReceivedMaxIncomingUnidirectionalStreams(
        session_.config(), 10);
    session_.OnConfigNegotiated();
    stream_ = new QuicChromiumClientStream(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        &session_, quic::BIDIRECTIONAL, NetLogWithSource(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    session_.ActivateStream(base::WrapUnique(stream_));
    handle_ = stream_->CreateHandle();
    helper_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void InitializeHeaders() {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
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
  }

  void ReadData(quic::QuicStringPiece expected_data) {
    scoped_refptr<IOBuffer> buffer =
        base::MakeRefCounted<IOBuffer>(expected_data.length() + 1);
    EXPECT_EQ(static_cast<int>(expected_data.length()),
              stream_->Read(buffer.get(), expected_data.length() + 1));
    EXPECT_EQ(expected_data,
              quic::QuicStringPiece(buffer->data(), expected_data.length()));
  }

  quic::QuicHeaderList ProcessHeaders(const spdy::SpdyHeaderBlock& headers) {
    quic::QuicHeaderList h = quic::test::AsHeaderList(headers);
    stream_->OnStreamHeaderList(false, h.uncompressed_header_bytes(), h);
    return h;
  }

  quic::QuicHeaderList ProcessTrailers(const spdy::SpdyHeaderBlock& headers) {
    quic::QuicHeaderList h = quic::test::AsHeaderList(headers);
    stream_->OnStreamHeaderList(true, h.uncompressed_header_bytes(), h);
    return h;
  }

  quic::QuicHeaderList ProcessHeadersFull(
      const spdy::SpdyHeaderBlock& headers) {
    quic::QuicHeaderList h = ProcessHeaders(headers);
    TestCompletionCallback callback;
    EXPECT_EQ(static_cast<int>(h.uncompressed_header_bytes()),
              handle_->ReadInitialHeaders(&headers_, callback.callback()));
    EXPECT_EQ(headers, headers_);
    EXPECT_TRUE(stream_->header_list().empty());
    return h;
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        session_.connection()->transport_version(), n);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        session_.connection()->transport_version(), n);
  }

  void ResetStreamCallback(QuicChromiumClientStream* stream, int /*rv*/) {
    stream->Reset(quic::QUIC_STREAM_CANCELLED);
  }

  std::string ConstructDataHeader(size_t body_len) {
    if (version_.transport_version != quic::QUIC_VERSION_99) {
      return "";
    }
    std::unique_ptr<char[]> buffer;
    auto header_length =
        quic::HttpEncoder::SerializeDataFrameHeader(body_len, &buffer);
    return std::string(buffer.get(), header_length);
  }

  const quic::ParsedQuicVersion version_;
  quic::QuicCryptoClientConfig crypto_config_;
  std::unique_ptr<QuicChromiumClientStream::Handle> handle_;
  std::unique_ptr<QuicChromiumClientStream::Handle> handle2_;
  quic::test::MockQuicConnectionHelper helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  MockQuicClientSessionBase session_;
  QuicChromiumClientStream* stream_;
  spdy::SpdyHeaderBlock headers_;
  spdy::SpdyHeaderBlock trailers_;
  quic::QuicClientPushPromiseIndex push_promise_index_;
};

INSTANTIATE_TEST_SUITE_P(Version,
                         QuicChromiumClientStreamTest,
                         ::testing::ValuesIn(quic::AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicChromiumClientStreamTest, Handle) {
  testing::InSequence seq;
  EXPECT_TRUE(handle_->IsOpen());
  EXPECT_EQ(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                version_.transport_version, 0),
            handle_->id());
  EXPECT_EQ(quic::QUIC_NO_ERROR, handle_->connection_error());
  EXPECT_EQ(quic::QUIC_STREAM_NO_ERROR, handle_->stream_error());
  EXPECT_TRUE(handle_->IsFirstStream());
  EXPECT_FALSE(handle_->IsDoneReading());
  EXPECT_FALSE(handle_->fin_sent());
  EXPECT_FALSE(handle_->fin_received());
  EXPECT_EQ(0u, handle_->stream_bytes_read());
  EXPECT_EQ(0u, handle_->stream_bytes_written());
  EXPECT_EQ(0u, handle_->NumBytesConsumed());

  InitializeHeaders();
  quic::QuicStreamOffset offset = 0;
  ProcessHeadersFull(headers_);
  quic::QuicStreamFrame frame2(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      true, offset, quic::QuicStringPiece());
  stream_->OnStreamFrame(frame2);
  EXPECT_TRUE(handle_->fin_received());
  handle_->OnFinRead();

  const char kData1[] = "hello world";
  const size_t kDataLen = base::size(kData1);

  // All data written.
  std::string header = ConstructDataHeader(kDataLen);
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(kDataLen, true)));
  TestCompletionCallback callback;
  EXPECT_EQ(OK,
            handle_->WriteStreamData(quic::QuicStringPiece(kData1, kDataLen),
                                     true, callback.callback()));

  EXPECT_FALSE(handle_->IsOpen());
  EXPECT_EQ(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                version_.transport_version, 0),
            handle_->id());
  EXPECT_EQ(quic::QUIC_NO_ERROR, handle_->connection_error());
  EXPECT_EQ(quic::QUIC_STREAM_NO_ERROR, handle_->stream_error());
  EXPECT_TRUE(handle_->IsFirstStream());
  EXPECT_TRUE(handle_->IsDoneReading());
  EXPECT_TRUE(handle_->fin_sent());
  EXPECT_TRUE(handle_->fin_received());
  EXPECT_EQ(0u, handle_->stream_bytes_read());
  EXPECT_EQ(header.length() + kDataLen, handle_->stream_bytes_written());
  EXPECT_EQ(0u, handle_->NumBytesConsumed());

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            handle_->WriteStreamData(quic::QuicStringPiece(kData1, kDataLen),
                                     true, callback.callback()));

  std::vector<scoped_refptr<IOBuffer>> buffers = {
      base::MakeRefCounted<IOBuffer>(10)};
  std::vector<int> lengths = {10};
  EXPECT_EQ(
      ERR_CONNECTION_CLOSED,
      handle_->WritevStreamData(buffers, lengths, true, callback.callback()));

  spdy::SpdyHeaderBlock headers;
  EXPECT_EQ(0, handle_->WriteHeaders(std::move(headers), true, nullptr));
}

TEST_P(QuicChromiumClientStreamTest, HandleAfterConnectionClose) {
  EXPECT_CALL(
      session_,
      SendRstStream(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_RST_ACKNOWLEDGEMENT, 0));
  stream_->OnConnectionClosed(quic::QUIC_INVALID_FRAME_DATA,
                              quic::ConnectionCloseSource::FROM_PEER);

  EXPECT_FALSE(handle_->IsOpen());
  EXPECT_EQ(quic::QUIC_INVALID_FRAME_DATA, handle_->connection_error());
}

TEST_P(QuicChromiumClientStreamTest, HandleAfterStreamReset) {
  // Verify that the Handle still behaves correctly after the stream is reset.
  quic::QuicRstStreamFrame rst(
      quic::kInvalidControlFrameId,
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      quic::QUIC_STREAM_CANCELLED, 0);
  if (version_.transport_version != quic::QUIC_VERSION_99) {
    EXPECT_CALL(
        session_,
        SendRstStream(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                          version_.transport_version, 0),
                      quic::QUIC_RST_ACKNOWLEDGEMENT, 0));
  } else {
    // Intercept & check that the call to the QuicConnection's OnStreamReast
    // has correct stream ID and error code -- for V99/IETF Quic, it should
    // have the STREAM_CANCELLED error code, not RST_ACK... Capture
    // OnStreamReset (rather than SendRstStream) because the V99 path bypasses
    // SendRstStream, calling SendRstStreamInner directly. Mocking
    // SendRstStreamInner is problematic since the test relies on it to perform
    // the closing operations and getting the stream in the correct state.
    EXPECT_CALL(
        *(static_cast<quic::test::MockQuicConnection*>(session_.connection())),
        OnStreamReset(stream_->id(), quic::QUIC_STREAM_CANCELLED));
  }

  stream_->OnStreamReset(rst);
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    // Make a STOP_SENDING frame and pass it to QUIC. For V99/IETF QUIC,
    // we need both a REST_STREAM and a STOP_SENDING to effect a closed
    // stream.
    quic::QuicStopSendingFrame stop_sending_frame(
        quic::kInvalidControlFrameId,
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        quic::QUIC_STREAM_CANCELLED);
    session_.OnStopSendingFrame(stop_sending_frame);
  }
  EXPECT_FALSE(handle_->IsOpen());
  EXPECT_EQ(quic::QUIC_STREAM_CANCELLED, handle_->stream_error());
}

TEST_P(QuicChromiumClientStreamTest, OnFinRead) {
  InitializeHeaders();
  quic::QuicStreamOffset offset = 0;
  ProcessHeadersFull(headers_);
  quic::QuicStreamFrame frame2(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      true, offset, quic::QuicStringPiece());
  stream_->OnStreamFrame(frame2);
}

TEST_P(QuicChromiumClientStreamTest, OnDataAvailable) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);
  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(data_len);
    stream_->OnStreamFrame(quic::QuicStreamFrame(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        /*fin=*/false,
        /*offset=*/offset, header));
    offset += header.length();
  }
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(quic::QuicStringPiece(data),
            quic::QuicStringPiece(buffer->data(), data_len));
}

TEST_P(QuicChromiumClientStreamTest, OnDataAvailableAfterReadBody) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);

  // Start to read the body.
  TestCompletionCallback callback;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(2 * data_len);
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));

  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(data_len);
    stream_->OnStreamFrame(quic::QuicStreamFrame(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        /*fin=*/false,
        /*offset=*/offset, header));
    offset += header.length();
  }

  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  EXPECT_EQ(data_len, callback.WaitForResult());
  EXPECT_EQ(quic::QuicStringPiece(data),
            quic::QuicStringPiece(buffer->data(), data_len));
  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, ProcessHeadersWithError) {
  spdy::SpdyHeaderBlock bad_headers;
  bad_headers["NAME"] = "...";
  EXPECT_CALL(
      session_,
      SendRstStream(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_BAD_APPLICATION_PAYLOAD, 0));

  auto headers = quic::test::AsHeaderList(bad_headers);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);

  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, OnDataAvailableWithError) {
  InitializeHeaders();
  auto headers = quic::test::AsHeaderList(headers_);
  ProcessHeadersFull(headers_);
  EXPECT_CALL(
      session_,
      SendRstStream(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_STREAM_CANCELLED, 0));

  const char data[] = "hello world!";
  int data_len = strlen(data);

  // Start to read the body.
  TestCompletionCallback callback;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(2 * data_len);
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle_->ReadBody(
          buffer.get(), 2 * data_len,
          base::BindOnce(&QuicChromiumClientStreamTest::ResetStreamCallback,
                         base::Unretained(this), stream_)));

  // Receive the data and close the stream during the callback.
  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(data_len);
    stream_->OnStreamFrame(quic::QuicStreamFrame(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        /*fin=*/false,
        /*offset=*/offset, header));
    offset += header.length();
  }
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/0, data));

  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, OnError) {
  //  EXPECT_CALL(delegate_, OnError(ERR_INTERNET_DISCONNECTED)).Times(1);

  stream_->OnError(ERR_INTERNET_DISCONNECTED);
  stream_->OnError(ERR_INTERNET_DISCONNECTED);
}

TEST_P(QuicChromiumClientStreamTest, OnTrailers) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);
  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(data_len);
    stream_->OnStreamFrame(quic::QuicStreamFrame(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        /*fin=*/false,
        /*offset=*/offset, header));
    offset += header.length();
  }
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(quic::QuicStringPiece(data),
            quic::QuicStringPiece(buffer->data(), data_len));

  spdy::SpdyHeaderBlock trailers;
  trailers["bar"] = "foo";
  if (version_.transport_version != quic::QUIC_VERSION_99) {
    trailers[quic::kFinalOffsetHeaderKey] = base::NumberToString(strlen(data));
  }

  auto t = ProcessTrailers(trailers);

  TestCompletionCallback trailers_callback;
  EXPECT_EQ(
      static_cast<int>(t.uncompressed_header_bytes()),
      handle_->ReadTrailingHeaders(&trailers_, trailers_callback.callback()));

  // Read the body and verify that it arrives correctly.
  EXPECT_EQ(0,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));

  // Make sure quic::kFinalOffsetHeaderKey is gone from the delivered actual
  // trailers.
  trailers.erase(quic::kFinalOffsetHeaderKey);
  EXPECT_EQ(trailers, trailers_);
  base::RunLoop().RunUntilIdle();
}

// Tests that trailers are marked as consumed only before delegate is to be
// immediately notified about trailers.
TEST_P(QuicChromiumClientStreamTest, MarkTrailersConsumedWhenNotifyDelegate) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);
  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(data_len);
    stream_->OnStreamFrame(quic::QuicStreamFrame(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        /*fin=*/false,
        /*offset=*/offset, header));
    offset += header.length();
  }
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(quic::QuicStringPiece(data),
            quic::QuicStringPiece(buffer->data(), data_len));

  // Read again, and it will be pending.
  EXPECT_THAT(
      handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()),
      IsError(ERR_IO_PENDING));

  spdy::SpdyHeaderBlock trailers;
  trailers["bar"] = "foo";
  if (version_.transport_version != quic::QUIC_VERSION_99) {
    trailers[quic::kFinalOffsetHeaderKey] = base::NumberToString(strlen(data));
  }
  quic::QuicHeaderList t = ProcessTrailers(trailers);
  EXPECT_FALSE(stream_->IsDoneReading());

  EXPECT_EQ(static_cast<int>(t.uncompressed_header_bytes()),
            handle_->ReadTrailingHeaders(&trailers_, callback.callback()));

  // Read the body and verify that it arrives correctly.
  EXPECT_EQ(0, callback.WaitForResult());

  // Make sure the stream is properly closed since trailers and data are all
  // consumed.
  EXPECT_TRUE(stream_->IsDoneReading());
  // Make sure quic::kFinalOffsetHeaderKey is gone from the delivered actual
  // trailers.
  trailers.erase(quic::kFinalOffsetHeaderKey);
  EXPECT_EQ(trailers, trailers_);

  base::RunLoop().RunUntilIdle();
}

// Test that if Read() is called after response body is read and after trailers
// are received but not yet delivered, Read() will return ERR_IO_PENDING instead
// of 0 (EOF).
TEST_P(QuicChromiumClientStreamTest, ReadAfterTrailersReceivedButNotDelivered) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);
  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(data_len);
    stream_->OnStreamFrame(quic::QuicStreamFrame(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        /*fin=*/false,
        /*offset=*/offset, header));
    offset += header.length();
  }
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(quic::QuicStringPiece(data),
            quic::QuicStringPiece(buffer->data(), data_len));

  // Deliver trailers. Delegate notification is posted asynchronously.
  spdy::SpdyHeaderBlock trailers;
  trailers["bar"] = "foo";
  if (version_.transport_version != quic::QUIC_VERSION_99) {
    trailers[quic::kFinalOffsetHeaderKey] = base::NumberToString(strlen(data));
  }

  quic::QuicHeaderList t = ProcessTrailers(trailers);

  EXPECT_FALSE(stream_->IsDoneReading());
  // Read again, it return ERR_IO_PENDING.
  EXPECT_THAT(
      handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()),
      IsError(ERR_IO_PENDING));

  // Trailers are not delivered
  EXPECT_FALSE(stream_->IsDoneReading());

  TestCompletionCallback callback2;
  EXPECT_EQ(static_cast<int>(t.uncompressed_header_bytes()),
            handle_->ReadTrailingHeaders(&trailers_, callback2.callback()));

  // Read the body and verify that it arrives correctly.
  // OnDataAvailable() should follow right after and Read() will return 0.
  EXPECT_EQ(0, callback.WaitForResult());

  // Make sure the stream is properly closed since trailers and data are all
  // consumed.
  EXPECT_TRUE(stream_->IsDoneReading());

  // Make sure quic::kFinalOffsetHeaderKey is gone from the delivered actual
  // trailers.
  trailers.erase(quic::kFinalOffsetHeaderKey);
  EXPECT_EQ(trailers, trailers_);

  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, WriteStreamData) {
  testing::InSequence seq;
  const char kData1[] = "hello world";
  const size_t kDataLen = base::size(kData1);

  // All data written.
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(kDataLen);
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(kDataLen, true)));
  TestCompletionCallback callback;
  EXPECT_EQ(OK,
            handle_->WriteStreamData(quic::QuicStringPiece(kData1, kDataLen),
                                     true, callback.callback()));
}

TEST_P(QuicChromiumClientStreamTest, WriteStreamDataAsync) {
  testing::InSequence seq;
  const char kData1[] = "hello world";
  const size_t kDataLen = base::size(kData1);

  // No data written.
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(0, false)));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->WriteStreamData(quic::QuicStringPiece(kData1, kDataLen),
                                     true, callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // All data written.
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(kDataLen);
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(kDataLen, true)));
  stream_->OnCanWrite();
  // Do 2 writes in version 99.
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    stream_->OnCanWrite();
  }
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

TEST_P(QuicChromiumClientStreamTest, WritevStreamData) {
  testing::InSequence seq;
  scoped_refptr<StringIOBuffer> buf1 =
      base::MakeRefCounted<StringIOBuffer>("hello world!");
  scoped_refptr<StringIOBuffer> buf2 =
      base::MakeRefCounted<StringIOBuffer>("Just a small payload");

  // All data written.
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(buf1->size());
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(buf1->size(), false)));
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(buf2->size());
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(buf2->size(), true)));
  TestCompletionCallback callback;
  EXPECT_EQ(
      OK, handle_->WritevStreamData({buf1, buf2}, {buf1->size(), buf2->size()},
                                    true, callback.callback()));
}

TEST_P(QuicChromiumClientStreamTest, WritevStreamDataAsync) {
  testing::InSequence seq;
  scoped_refptr<StringIOBuffer> buf1 =
      base::MakeRefCounted<StringIOBuffer>("hello world!");
  scoped_refptr<StringIOBuffer> buf2 =
      base::MakeRefCounted<StringIOBuffer>("Just a small payload");

  // Only a part of the data is written.
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(buf1->size());
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      // First piece of data is written.
      .WillOnce(Return(quic::QuicConsumedData(buf1->size(), false)));
  // Second piece of data is queued.
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(0, false)));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->WritevStreamData({buf1.get(), buf2.get()},
                                      {buf1->size(), buf2->size()}, true,
                                      callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // The second piece of data is written.
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(buf2->size());
    EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
        .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  }
  EXPECT_CALL(session_, WritevData(stream_, stream_->id(), _, _, _))
      .WillOnce(Return(quic::QuicConsumedData(buf2->size(), true)));
  stream_->OnCanWrite();
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    stream_->OnCanWrite();
  }
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

TEST_P(QuicChromiumClientStreamTest, HeadersBeforeHandle) {
  // We don't use stream_ because we want an incoming server push
  // stream.
  quic::QuicStreamId stream_id = GetNthServerInitiatedUnidirectionalStreamId(0);
  QuicChromiumClientStream* stream2 = new QuicChromiumClientStream(
      stream_id, &session_, quic::READ_UNIDIRECTIONAL, NetLogWithSource(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  session_.ActivateStream(base::WrapUnique(stream2));

  InitializeHeaders();

  // Receive the headers before the delegate is set.
  quic::QuicHeaderList header_list = quic::test::AsHeaderList(headers_);
  stream2->OnStreamHeaderList(true, header_list.uncompressed_header_bytes(),
                              header_list);

  // Now set the delegate and verify that the headers are delivered.
  handle2_ = stream2->CreateHandle();
  TestCompletionCallback callback;
  EXPECT_EQ(static_cast<int>(header_list.uncompressed_header_bytes()),
            handle2_->ReadInitialHeaders(&headers_, callback.callback()));
  EXPECT_EQ(headers_, headers_);
}

TEST_P(QuicChromiumClientStreamTest, HeadersAndDataBeforeHandle) {
  // We don't use stream_ because we want an incoming server push
  // stream.
  quic::QuicStreamId stream_id = GetNthServerInitiatedUnidirectionalStreamId(0);
  QuicChromiumClientStream* stream2 = new QuicChromiumClientStream(
      stream_id, &session_, quic::READ_UNIDIRECTIONAL, NetLogWithSource(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  session_.ActivateStream(base::WrapUnique(stream2));

  InitializeHeaders();

  // Receive the headers and data before the delegate is set.
  quic::QuicHeaderList header_list = quic::test::AsHeaderList(headers_);
  stream2->OnStreamHeaderList(false, header_list.uncompressed_header_bytes(),
                              header_list);
  const char data[] = "hello world!";

  size_t offset = 0;
  if (version_.transport_version == quic::QUIC_VERSION_99) {
    std::string header = ConstructDataHeader(strlen(data));
    stream2->OnStreamFrame(quic::QuicStreamFrame(stream_id,
                                                 /*fin=*/false,
                                                 /*offset=*/offset, header));
    offset += header.length();
  }
  stream2->OnStreamFrame(quic::QuicStreamFrame(stream_id, /*fin=*/false,
                                               /*offset=*/offset, data));

  // Now set the delegate and verify that the headers are delivered, but
  // not the data, which needs to be read explicitly.
  handle2_ = stream2->CreateHandle();
  TestCompletionCallback callback;
  EXPECT_EQ(static_cast<int>(header_list.uncompressed_header_bytes()),
            handle2_->ReadInitialHeaders(&headers_, callback.callback()));
  EXPECT_EQ(headers_, headers_);
  base::RunLoop().RunUntilIdle();

  // Now explicitly read the data.
  int data_len = base::size(data) - 1;
  scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<IOBuffer>(data_len + 1);
  ASSERT_EQ(data_len, stream2->Read(buffer.get(), data_len + 1));
  EXPECT_EQ(quic::QuicStringPiece(data),
            quic::QuicStringPiece(buffer->data(), data_len));
}

}  // namespace
}  // namespace test
}  // namespace net
