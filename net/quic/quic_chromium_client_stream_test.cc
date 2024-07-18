// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_stream.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_context.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace net::test {
namespace {

class EstablishedCryptoStream : public quic::test::MockQuicCryptoStream {
 public:
  using quic::test::MockQuicCryptoStream::MockQuicCryptoStream;

  bool encryption_established() const override { return true; }
};

class MockQuicClientSessionBase : public quic::QuicSpdyClientSessionBase {
 public:
  explicit MockQuicClientSessionBase(quic::QuicConnection* connection);

  MockQuicClientSessionBase(const MockQuicClientSessionBase&) = delete;
  MockQuicClientSessionBase& operator=(const MockQuicClientSessionBase&) =
      delete;

  ~MockQuicClientSessionBase() override;

  const quic::QuicCryptoStream* GetCryptoStream() const override {
    return crypto_stream_.get();
  }

  quic::QuicCryptoStream* GetMutableCryptoStream() override {
    return crypto_stream_.get();
  }

  void SetCryptoStream(quic::QuicCryptoStream* crypto_stream) {
    crypto_stream_.reset(crypto_stream);
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
  MOCK_METHOD6(WritevData,
               quic::QuicConsumedData(quic::QuicStreamId id,
                                      size_t write_length,
                                      quic::QuicStreamOffset offset,
                                      quic::StreamSendingState state,
                                      quic::TransmissionType type,
                                      quic::EncryptionLevel level));
  MOCK_METHOD2(WriteControlFrame,
               bool(const quic::QuicFrame&, quic::TransmissionType));
  MOCK_METHOD4(SendRstStream,
               void(quic::QuicStreamId stream_id,
                    quic::QuicRstStreamErrorCode error,
                    quic::QuicStreamOffset bytes_written,
                    bool send_rst_only));

  MOCK_METHOD2(OnStreamHeaders,
               void(quic::QuicStreamId stream_id,
                    std::string_view headers_data));
  MOCK_METHOD2(OnStreamHeadersPriority,
               void(quic::QuicStreamId stream_id,
                    const spdy::SpdyStreamPrecedence& precedence));
  MOCK_METHOD3(OnStreamHeadersComplete,
               void(quic::QuicStreamId stream_id, bool fin, size_t frame_len));
  MOCK_CONST_METHOD0(OneRttKeysAvailable, bool());
  // Methods taking non-copyable types like quiche::HttpHeaderBlock by value
  // cannot be mocked directly.
  size_t WriteHeadersOnHeadersStream(
      quic::QuicStreamId id,
      quiche::HttpHeaderBlock headers,
      bool fin,
      const spdy::SpdyStreamPrecedence& precedence,
      quiche::QuicheReferenceCountedPointer<quic::QuicAckListenerInterface>
          ack_listener) override {
    return WriteHeadersOnHeadersStreamMock(id, headers, fin, precedence,
                                           std::move(ack_listener));
  }
  MOCK_METHOD5(WriteHeadersOnHeadersStreamMock,
               size_t(quic::QuicStreamId id,
                      const quiche::HttpHeaderBlock& headers,
                      bool fin,
                      const spdy::SpdyStreamPrecedence& precedence,
                      const quiche::QuicheReferenceCountedPointer<
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

 protected:
  MOCK_METHOD1(ShouldCreateIncomingStream, bool(quic::QuicStreamId id));
  MOCK_METHOD0(ShouldCreateOutgoingBidirectionalStream, bool());
  MOCK_METHOD0(ShouldCreateOutgoingUnidirectionalStream, bool());

 private:
  std::unique_ptr<quic::QuicCryptoStream> crypto_stream_;
};

MockQuicClientSessionBase::MockQuicClientSessionBase(
    quic::QuicConnection* connection)
    : quic::QuicSpdyClientSessionBase(connection,
                                      /*visitor=*/nullptr,
                                      quic::test::DefaultQuicConfig(),
                                      connection->supported_versions()) {
  crypto_stream_ = std::make_unique<quic::test::MockQuicCryptoStream>(this);
  Initialize();
  ON_CALL(*this, WritevData(_, _, _, _, _, _))
      .WillByDefault(testing::Return(quic::QuicConsumedData(0, false)));
}

MockQuicClientSessionBase::~MockQuicClientSessionBase() = default;

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
            quic::test::SupportedVersions(version_))) {
    quic::test::QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
        session_.config(), quic::kMinimumFlowControlSendWindow);
    quic::test::QuicConfigPeer::
        SetReceivedInitialMaxStreamDataBytesOutgoingBidirectional(
            session_.config(), quic::kMinimumFlowControlSendWindow);
    quic::test::QuicConfigPeer::SetReceivedMaxUnidirectionalStreams(
        session_.config(), 10);
    session_.OnConfigNegotiated();
    stream_ = new QuicChromiumClientStream(
        quic::test::GetNthClientInitiatedBidirectionalStreamId(
            version_.transport_version, 0),
        &session_, quic::QuicServerId(), quic::BIDIRECTIONAL,
        NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS);
    session_.ActivateStream(base::WrapUnique(stream_.get()));
    handle_ = stream_->CreateHandle();
    helper_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
    session_.SetCryptoStream(new EstablishedCryptoStream(&session_));
    session_.connection()->SetEncrypter(
        quic::ENCRYPTION_FORWARD_SECURE,
        std::make_unique<quic::test::TaggingEncrypter>(
            quic::ENCRYPTION_FORWARD_SECURE));
  }

  void InitializeHeaders() {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
    headers_[":status"] = "200";
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

  quiche::HttpHeaderBlock CreateResponseHeaders(
      const std::string& status_code) {
    quiche::HttpHeaderBlock headers;
    headers[":status"] = status_code;
    return headers;
  }

  void ReadData(std::string_view expected_data) {
    auto buffer =
        base::MakeRefCounted<IOBufferWithSize>(expected_data.length() + 1);
    EXPECT_EQ(static_cast<int>(expected_data.length()),
              stream_->Read(buffer.get(), expected_data.length() + 1));
    EXPECT_EQ(expected_data,
              std::string_view(buffer->data(), expected_data.length()));
  }

  quic::QuicHeaderList ProcessHeaders(const quiche::HttpHeaderBlock& headers) {
    quic::QuicHeaderList h = quic::test::AsHeaderList(headers);
    stream_->OnStreamHeaderList(false, h.uncompressed_header_bytes(), h);
    return h;
  }

  quic::QuicHeaderList ProcessTrailers(const quiche::HttpHeaderBlock& headers) {
    quic::QuicHeaderList h = quic::test::AsHeaderList(headers);
    stream_->OnStreamHeaderList(true, h.uncompressed_header_bytes(), h);
    return h;
  }

  quic::QuicHeaderList ProcessHeadersFull(
      const quiche::HttpHeaderBlock& headers) {
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
    quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
        body_len, quiche::SimpleBufferAllocator::Get());
    return std::string(buffer.data(), buffer.size());
  }

  const quic::ParsedQuicVersion version_;
  quic::QuicCryptoClientConfig crypto_config_;
  std::unique_ptr<QuicChromiumClientStream::Handle> handle_;
  std::unique_ptr<QuicChromiumClientStream::Handle> handle2_;
  quic::test::MockQuicConnectionHelper helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  MockQuicClientSessionBase session_;
  raw_ptr<QuicChromiumClientStream> stream_;
  quiche::HttpHeaderBlock headers_;
  quiche::HttpHeaderBlock trailers_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(Version,
                         QuicChromiumClientStreamTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
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
      true, offset, std::string_view());
  stream_->OnStreamFrame(frame2);
  EXPECT_TRUE(handle_->fin_received());
  handle_->OnFinRead();

  const char kData1[] = "hello world";
  const size_t kDataLen = std::size(kData1);

  // All data written.
  std::string header = ConstructDataHeader(kDataLen);
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(kDataLen, true)));
  TestCompletionCallback callback;
  EXPECT_EQ(OK, handle_->WriteStreamData(std::string_view(kData1, kDataLen),
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
            handle_->WriteStreamData(std::string_view(kData1, kDataLen), true,
                                     callback.callback()));

  std::vector<scoped_refptr<IOBuffer>> buffers = {
      base::MakeRefCounted<IOBufferWithSize>(10)};
  std::vector<int> lengths = {10};
  EXPECT_EQ(
      ERR_CONNECTION_CLOSED,
      handle_->WritevStreamData(buffers, lengths, true, callback.callback()));

  quiche::HttpHeaderBlock headers;
  EXPECT_EQ(0, handle_->WriteHeaders(std::move(headers), true, nullptr));
}

TEST_P(QuicChromiumClientStreamTest, HandleAfterConnectionClose) {
  quic::test::QuicConnectionPeer::TearDownLocalConnectionState(
      session_.connection());
  quic::QuicConnectionCloseFrame frame;
  frame.quic_error_code = quic::QUIC_INVALID_FRAME_DATA;
  stream_->OnConnectionClosed(frame, quic::ConnectionCloseSource::FROM_PEER);

  EXPECT_FALSE(handle_->IsOpen());
  EXPECT_EQ(quic::QUIC_INVALID_FRAME_DATA, handle_->connection_error());
}

TEST_P(QuicChromiumClientStreamTest, HandleAfterStreamReset) {
  // Make a STOP_SENDING frame and pass it to QUIC. We need both a REST_STREAM
  // and a STOP_SENDING to effect a closed stream.
  quic::QuicStopSendingFrame stop_sending_frame(
      quic::kInvalidControlFrameId,
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      quic::QUIC_STREAM_CANCELLED);
  session_.OnStopSendingFrame(stop_sending_frame);

  // Verify that the Handle still behaves correctly after the stream is reset.
  quic::QuicRstStreamFrame rst(
      quic::kInvalidControlFrameId,
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      quic::QUIC_STREAM_CANCELLED, 0);

  stream_->OnStreamReset(rst);
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
      true, offset, std::string_view());
  stream_->OnStreamFrame(frame2);
}

TEST_P(QuicChromiumClientStreamTest, OnDataAvailable) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);
  size_t offset = 0;
  std::string header = ConstructDataHeader(data_len);
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, header));
  offset += header.length();
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(std::string_view(data), std::string_view(buffer->data(), data_len));
}

TEST_P(QuicChromiumClientStreamTest, OnDataAvailableAfterReadBody) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);

  // Start to read the body.
  TestCompletionCallback callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(2 * data_len);
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));

  size_t offset = 0;
  std::string header = ConstructDataHeader(data_len);
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, header));
  offset += header.length();

  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  EXPECT_EQ(data_len, callback.WaitForResult());
  EXPECT_EQ(std::string_view(data), std::string_view(buffer->data(), data_len));
  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, ProcessHeadersWithError) {
  quiche::HttpHeaderBlock bad_headers;
  bad_headers["NAME"] = "...";

  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      OnStreamReset(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_BAD_APPLICATION_PAYLOAD));

  auto headers = quic::test::AsHeaderList(bad_headers);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);

  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, OnDataAvailableWithError) {
  InitializeHeaders();
  auto headers = quic::test::AsHeaderList(headers_);
  ProcessHeadersFull(headers_);

  const char data[] = "hello world!";
  int data_len = strlen(data);

  // Start to read the body.
  TestCompletionCallback callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(2 * data_len);
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle_->ReadBody(
          buffer.get(), 2 * data_len,
          base::BindOnce(&QuicChromiumClientStreamTest::ResetStreamCallback,
                         base::Unretained(this), stream_)));

  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      OnStreamReset(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_STREAM_CANCELLED));

  // Receive the data and close the stream during the callback.
  size_t offset = 0;
  std::string header = ConstructDataHeader(data_len);
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, header));
  offset += header.length();
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
  std::string header = ConstructDataHeader(data_len);
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, header));
  offset += header.length();
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(std::string_view(data), std::string_view(buffer->data(), data_len));

  quiche::HttpHeaderBlock trailers;
  trailers["bar"] = "foo";

  auto t = ProcessTrailers(trailers);

  TestCompletionCallback trailers_callback;
  EXPECT_EQ(
      static_cast<int>(t.uncompressed_header_bytes()),
      handle_->ReadTrailingHeaders(&trailers_, trailers_callback.callback()));

  // Read the body and verify that it arrives correctly.
  EXPECT_EQ(0,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));

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
  std::string header = ConstructDataHeader(data_len);
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, header));
  offset += header.length();
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(std::string_view(data), std::string_view(buffer->data(), data_len));

  // Read again, and it will be pending.
  EXPECT_THAT(
      handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()),
      IsError(ERR_IO_PENDING));

  quiche::HttpHeaderBlock trailers;
  trailers["bar"] = "foo";
  quic::QuicHeaderList t = ProcessTrailers(trailers);
  EXPECT_FALSE(stream_->IsDoneReading());

  EXPECT_EQ(static_cast<int>(t.uncompressed_header_bytes()),
            handle_->ReadTrailingHeaders(&trailers_, callback.callback()));

  // Read the body and verify that it arrives correctly.
  EXPECT_EQ(0, callback.WaitForResult());

  // Make sure the stream is properly closed since trailers and data are all
  // consumed.
  EXPECT_TRUE(stream_->IsDoneReading());
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
  std::string header = ConstructDataHeader(data_len);
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, header));
  offset += header.length();
  stream_->OnStreamFrame(quic::QuicStreamFrame(
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          version_.transport_version, 0),
      /*fin=*/false,
      /*offset=*/offset, data));

  // Read the body and verify that it arrives correctly.
  TestCompletionCallback callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(2 * data_len);
  EXPECT_EQ(data_len,
            handle_->ReadBody(buffer.get(), 2 * data_len, callback.callback()));
  EXPECT_EQ(std::string_view(data), std::string_view(buffer->data(), data_len));

  // Deliver trailers. Delegate notification is posted asynchronously.
  quiche::HttpHeaderBlock trailers;
  trailers["bar"] = "foo";

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

  EXPECT_EQ(trailers, trailers_);

  base::RunLoop().RunUntilIdle();
}

TEST_P(QuicChromiumClientStreamTest, WriteStreamData) {
  testing::InSequence seq;
  const char kData1[] = "hello world";
  const size_t kDataLen = std::size(kData1);

  // All data written.
  std::string header = ConstructDataHeader(kDataLen);
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(kDataLen, true)));
  TestCompletionCallback callback;
  EXPECT_EQ(OK, handle_->WriteStreamData(std::string_view(kData1, kDataLen),
                                         true, callback.callback()));
}

TEST_P(QuicChromiumClientStreamTest, WriteStreamDataAsync) {
  testing::InSequence seq;
  const char kData1[] = "hello world";
  const size_t kDataLen = std::size(kData1);

  // No data written.
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(0, false)));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->WriteStreamData(std::string_view(kData1, kDataLen), true,
                                     callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // All data written.
  std::string header = ConstructDataHeader(kDataLen);
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(kDataLen, true)));
  stream_->OnCanWrite();
  // Do 2 writes in version 99.
  stream_->OnCanWrite();
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
  std::string header = ConstructDataHeader(buf1->size());
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(buf1->size(), false)));
  header = ConstructDataHeader(buf2->size());
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
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
  std::string header = ConstructDataHeader(buf1->size());
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      // First piece of data is written.
      .WillOnce(Return(quic::QuicConsumedData(buf1->size(), false)));
  // Second piece of data is queued.
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(0, false)));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->WritevStreamData({buf1.get(), buf2.get()},
                                      {buf1->size(), buf2->size()}, true,
                                      callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // The second piece of data is written.
  header = ConstructDataHeader(buf2->size());
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(header.length(), false)));
  EXPECT_CALL(session_,
              WritevData(stream_->id(), _, _, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(Return(quic::QuicConsumedData(buf2->size(), true)));
  stream_->OnCanWrite();
  stream_->OnCanWrite();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

TEST_P(QuicChromiumClientStreamTest, WriteConnectUdpPayload) {
  testing::InSequence seq;
  std::string packet = {1, 2, 3, 4, 5, 6};

  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      &session_, quic::HttpDatagramSupport::kRfc);
  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      SendMessage(1, _, false))
      .WillOnce(Return(quic::MESSAGE_STATUS_SUCCESS));
  EXPECT_EQ(OK, handle_->WriteConnectUdpPayload(packet));
  histogram_tester_.ExpectBucketCount(
      QuicChromiumClientStream::kHttp3DatagramDroppedHistogram, false, 1);

  // Packet is dropped if session does not have HTTP3 Datagram support.
  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      &session_, quic::HttpDatagramSupport::kNone);
  EXPECT_EQ(OK, handle_->WriteConnectUdpPayload(packet));
  histogram_tester_.ExpectBucketCount(
      QuicChromiumClientStream::kHttp3DatagramDroppedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(
      QuicChromiumClientStream::kHttp3DatagramDroppedHistogram, 2);
}

TEST_P(QuicChromiumClientStreamTest, HeadersBeforeHandle) {
  // We don't use stream_ because we want an incoming server push
  // stream.
  quic::QuicStreamId stream_id = GetNthServerInitiatedUnidirectionalStreamId(0);
  QuicChromiumClientStream* stream2 = new QuicChromiumClientStream(
      stream_id, &session_, quic::QuicServerId(), quic::READ_UNIDIRECTIONAL,
      NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS);
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
      stream_id, &session_, quic::QuicServerId(), quic::READ_UNIDIRECTIONAL,
      NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS);
  session_.ActivateStream(base::WrapUnique(stream2));

  InitializeHeaders();

  // Receive the headers and data before the delegate is set.
  quic::QuicHeaderList header_list = quic::test::AsHeaderList(headers_);
  stream2->OnStreamHeaderList(false, header_list.uncompressed_header_bytes(),
                              header_list);
  const char data[] = "hello world!";

  size_t offset = 0;
  std::string header = ConstructDataHeader(strlen(data));
  stream2->OnStreamFrame(quic::QuicStreamFrame(stream_id,
                                               /*fin=*/false,
                                               /*offset=*/offset, header));
  offset += header.length();
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
  int data_len = std::size(data) - 1;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(data_len + 1);
  ASSERT_EQ(data_len, stream2->Read(buffer.get(), data_len + 1));
  EXPECT_EQ(std::string_view(data), std::string_view(buffer->data(), data_len));
}

// Regression test for https://crbug.com/1043531.
TEST_P(QuicChromiumClientStreamTest, ResetOnEmptyResponseHeaders) {
  const quiche::HttpHeaderBlock empty_response_headers;
  ProcessHeaders(empty_response_headers);

  // Empty headers are allowed by QuicSpdyStream,
  // but an error is generated by QuicChromiumClientStream.
  int rv = handle_->ReadInitialHeaders(&headers_, CompletionOnceCallback());
  EXPECT_THAT(rv, IsError(net::ERR_QUIC_PROTOCOL_ERROR));
}

// Tests that the stream resets when it receives an invalid ":status"
// pseudo-header value.
TEST_P(QuicChromiumClientStreamTest, InvalidStatus) {
  quiche::HttpHeaderBlock headers = CreateResponseHeaders("xxx");

  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      OnStreamReset(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_BAD_APPLICATION_PAYLOAD));

  ProcessHeaders(headers);
  EXPECT_FALSE(handle_->IsOpen());
  EXPECT_EQ(quic::QUIC_BAD_APPLICATION_PAYLOAD, handle_->stream_error());
}

// Tests that the stream resets when it receives 101 Switching Protocols.
TEST_P(QuicChromiumClientStreamTest, SwitchingProtocolsResponse) {
  quiche::HttpHeaderBlock informational_headers = CreateResponseHeaders("101");

  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      OnStreamReset(quic::test::GetNthClientInitiatedBidirectionalStreamId(
                        version_.transport_version, 0),
                    quic::QUIC_BAD_APPLICATION_PAYLOAD));

  ProcessHeaders(informational_headers);
  EXPECT_FALSE(handle_->IsOpen());
  EXPECT_EQ(quic::QUIC_BAD_APPLICATION_PAYLOAD, handle_->stream_error());
}

// Tests that the stream ignores 100 Continue response.
TEST_P(QuicChromiumClientStreamTest, ContinueResponse) {
  quiche::HttpHeaderBlock informational_headers = CreateResponseHeaders("100");

  // This informational headers should be ignored.
  ProcessHeaders(informational_headers);

  // Pass the initial headers.
  InitializeHeaders();
  quic::QuicHeaderList header_list = ProcessHeaders(headers_);

  // Read the initial headers.
  quiche::HttpHeaderBlock response_headers;
  // Pass DoNothing because the initial headers is already available and the
  // callback won't be called.
  EXPECT_EQ(static_cast<int>(header_list.uncompressed_header_bytes()),
            handle_->ReadInitialHeaders(&response_headers, base::DoNothing()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(response_headers, headers_);
}

// Tests that the stream handles 103 Early Hints responses.
TEST_P(QuicChromiumClientStreamTest, EarlyHintsResponses) {
  // Pass Two Early Hints responses to the stream.
  quiche::HttpHeaderBlock hints1_headers = CreateResponseHeaders("103");
  hints1_headers["x-header1"] = "foo";
  quic::QuicHeaderList header_list = ProcessHeaders(hints1_headers);
  const size_t hints1_bytes = header_list.uncompressed_header_bytes();

  quiche::HttpHeaderBlock hints2_headers = CreateResponseHeaders("103");
  hints2_headers["x-header2"] = "foobarbaz";
  header_list = ProcessHeaders(hints2_headers);
  const size_t hints2_bytes = header_list.uncompressed_header_bytes();

  // Pass the initial headers to the stream.
  InitializeHeaders();
  header_list = ProcessHeaders(headers_);
  const size_t initial_headers_bytes = header_list.uncompressed_header_bytes();

  quiche::HttpHeaderBlock headers;

  // Read headers. The first two reads should return Early Hints.
  EXPECT_EQ(static_cast<int>(hints1_bytes),
            handle_->ReadInitialHeaders(&headers, base::DoNothing()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(headers, hints1_headers);
  base::TimeTicks first_early_hints_time = handle_->first_early_hints_time();
  EXPECT_FALSE(first_early_hints_time.is_null());

  EXPECT_EQ(static_cast<int>(hints2_bytes),
            handle_->ReadInitialHeaders(&headers, base::DoNothing()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(headers, hints2_headers);
  EXPECT_EQ(first_early_hints_time, handle_->first_early_hints_time());

  // The third read should return the initial headers.
  EXPECT_EQ(static_cast<int>(initial_headers_bytes),
            handle_->ReadInitialHeaders(&headers, base::DoNothing()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(headers, headers_);
}

// Tests that pending reads for Early Hints work.
TEST_P(QuicChromiumClientStreamTest, EarlyHintsAsync) {
  quiche::HttpHeaderBlock headers;
  TestCompletionCallback hints_callback;

  // Try to read headers. The read should be blocked.
  EXPECT_EQ(ERR_IO_PENDING,
            handle_->ReadInitialHeaders(&headers, hints_callback.callback()));

  // Pass an Early Hints and the initial headers.
  quiche::HttpHeaderBlock hints_headers = CreateResponseHeaders("103");
  hints_headers["x-header1"] = "foo";
  quic::QuicHeaderList header_list = ProcessHeaders(hints_headers);
  const size_t hints_bytes = header_list.uncompressed_header_bytes();
  InitializeHeaders();
  header_list = ProcessHeaders(headers_);
  const size_t initial_headers_bytes = header_list.uncompressed_header_bytes();

  // Wait for the pending headers read. The result should be the Early Hints.
  const int hints_result = hints_callback.WaitForResult();
  EXPECT_EQ(hints_result, static_cast<int>(hints_bytes));
  EXPECT_EQ(headers, hints_headers);

  // Second read should return the initial headers.
  EXPECT_EQ(static_cast<int>(initial_headers_bytes),
            handle_->ReadInitialHeaders(&headers, base::DoNothing()));
  EXPECT_EQ(headers, headers_);
}

// Tests that Early Hints after the initial headers is treated as an error.
TEST_P(QuicChromiumClientStreamTest, EarlyHintsAfterInitialHeaders) {
  InitializeHeaders();
  ProcessHeadersFull(headers_);

  // Early Hints after the initial headers are treated as trailers, and it
  // should result in an error because trailers must not contain pseudo-headers
  // like ":status".
  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      CloseConnection(
          quic::QUIC_INVALID_HEADERS_STREAM_DATA, _,
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));

  quiche::HttpHeaderBlock hints_headers;
  hints_headers[":status"] = "103";
  ProcessHeaders(hints_headers);
  base::RunLoop().RunUntilIdle();
}

// Similar to the above test but don't read the initial headers.
TEST_P(QuicChromiumClientStreamTest, EarlyHintsAfterInitialHeadersWithoutRead) {
  InitializeHeaders();
  ProcessHeaders(headers_);

  // Early Hints after the initial headers are treated as trailers, and it
  // should result in an error because trailers must not contain pseudo-headers
  // like ":status".
  EXPECT_CALL(
      *static_cast<quic::test::MockQuicConnection*>(session_.connection()),
      CloseConnection(
          quic::QUIC_INVALID_HEADERS_STREAM_DATA, _,
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));

  quiche::HttpHeaderBlock hints_headers;
  hints_headers[":status"] = "103";
  ProcessHeaders(hints_headers);
  base::RunLoop().RunUntilIdle();
}

// Regression test for https://crbug.com/1248970. Write an Early Hints headers,
// an initial response headers and trailers in succession without reading in
// the middle of writings.
TEST_P(QuicChromiumClientStreamTest, TrailersAfterEarlyHintsWithoutRead) {
  // Process an Early Hints response headers on the stream.
  quiche::HttpHeaderBlock hints_headers = CreateResponseHeaders("103");
  quic::QuicHeaderList hints_header_list = ProcessHeaders(hints_headers);

  // Process an initial response headers on the stream.
  InitializeHeaders();
  quic::QuicHeaderList header_list = ProcessHeaders(headers_);

  // Process a trailer headers on the stream. This should not hit any DCHECK.
  quiche::HttpHeaderBlock trailers;
  trailers["bar"] = "foo";
  quic::QuicHeaderList trailer_header_list = ProcessTrailers(trailers);
  base::RunLoop().RunUntilIdle();

  // Read the Early Hints response from the handle.
  {
    quiche::HttpHeaderBlock headers;
    TestCompletionCallback callback;
    EXPECT_EQ(static_cast<int>(hints_header_list.uncompressed_header_bytes()),
              handle_->ReadInitialHeaders(&headers, callback.callback()));
    EXPECT_EQ(headers, hints_headers);
  }

  // Read the initial headers from the handle.
  {
    quiche::HttpHeaderBlock headers;
    TestCompletionCallback callback;
    EXPECT_EQ(static_cast<int>(header_list.uncompressed_header_bytes()),
              handle_->ReadInitialHeaders(&headers, callback.callback()));
    EXPECT_EQ(headers, headers_);
  }

  // Read trailers from the handle.
  {
    quiche::HttpHeaderBlock headers;
    TestCompletionCallback callback;
    EXPECT_EQ(static_cast<int>(trailer_header_list.uncompressed_header_bytes()),
              handle_->ReadTrailingHeaders(&headers, callback.callback()));
    EXPECT_EQ(headers, trailers);
  }
}

}  // namespace
}  // namespace net::test
