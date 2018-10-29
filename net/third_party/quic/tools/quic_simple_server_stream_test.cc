// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_simple_server_stream.h"

#include <list>
#include <memory>
#include <utility>

#include "net/test/gtest_util.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/tools/quic_backend_response.h"
#include "net/third_party/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quic/tools/quic_simple_server_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {

size_t kFakeFrameLen = 60;

class QuicSimpleServerStreamPeer : public QuicSimpleServerStream {
 public:
  QuicSimpleServerStreamPeer(
      QuicStreamId stream_id,
      QuicSpdySession* session,
      StreamType type,
      QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerStream(stream_id,
                               session,
                               type,
                               quic_simple_server_backend) {}

  ~QuicSimpleServerStreamPeer() override = default;

  using QuicSimpleServerStream::SendErrorResponse;
  using QuicSimpleServerStream::SendResponse;

  spdy::SpdyHeaderBlock* mutable_headers() { return &request_headers_; }
  void set_body(QuicString body) { body_ = std::move(body); }

  static void SendResponse(QuicSimpleServerStream* stream) {
    stream->SendResponse();
  }

  static void SendErrorResponse(QuicSimpleServerStream* stream) {
    stream->SendErrorResponse();
  }

  static const QuicString& body(QuicSimpleServerStream* stream) {
    return stream->body_;
  }

  static int content_length(QuicSimpleServerStream* stream) {
    return stream->content_length_;
  }

  static spdy::SpdyHeaderBlock& headers(QuicSimpleServerStream* stream) {
    return stream->request_headers_;
  }
};

namespace {

class MockQuicSimpleServerSession : public QuicSimpleServerSession {
 public:
  const size_t kMaxStreamsForTest = 100;

  explicit MockQuicSimpleServerSession(
      QuicConnection* connection,
      MockQuicSessionVisitor* owner,
      MockQuicCryptoServerStreamHelper* helper,
      QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerSession(DefaultQuicConfig(),
                                CurrentSupportedVersions(),
                                connection,
                                owner,
                                helper,
                                crypto_config,
                                compressed_certs_cache,
                                quic_simple_server_backend) {
    set_max_open_incoming_streams(kMaxStreamsForTest);
    set_max_open_outgoing_streams(kMaxStreamsForTest);
    ON_CALL(*this, WritevData(_, _, _, _, _))
        .WillByDefault(testing::Return(QuicConsumedData(0, false)));
  }

  MockQuicSimpleServerSession(const MockQuicSimpleServerSession&) = delete;
  MockQuicSimpleServerSession& operator=(const MockQuicSimpleServerSession&) =
      delete;
  ~MockQuicSimpleServerSession() override = default;

  MOCK_METHOD3(OnConnectionClosed,
               void(QuicErrorCode error,
                    const QuicString& error_details,
                    ConnectionCloseSource source));
  MOCK_METHOD1(CreateIncomingStream, QuicSpdyStream*(QuicStreamId id));
  MOCK_METHOD5(WritevData,
               QuicConsumedData(QuicStream* stream,
                                QuicStreamId id,
                                size_t write_length,
                                QuicStreamOffset offset,
                                StreamSendingState state));
  MOCK_METHOD4(OnStreamHeaderList,
               void(QuicStreamId stream_id,
                    bool fin,
                    size_t frame_len,
                    const QuicHeaderList& header_list));
  MOCK_METHOD2(OnStreamHeadersPriority,
               void(QuicStreamId stream_id, spdy::SpdyPriority priority));
  // Methods taking non-copyable types like spdy::SpdyHeaderBlock by value
  // cannot be mocked directly.
  size_t WriteHeaders(QuicStreamId id,
                      spdy::SpdyHeaderBlock headers,
                      bool fin,
                      spdy::SpdyPriority priority,
                      QuicReferenceCountedPointer<QuicAckListenerInterface>
                          ack_listener) override {
    return WriteHeadersMock(id, headers, fin, priority, ack_listener);
  }
  MOCK_METHOD5(
      WriteHeadersMock,
      size_t(QuicStreamId id,
             const spdy::SpdyHeaderBlock& headers,
             bool fin,
             spdy::SpdyPriority priority,
             const QuicReferenceCountedPointer<QuicAckListenerInterface>&
                 ack_listener));
  MOCK_METHOD3(SendRstStream,
               void(QuicStreamId stream_id,
                    QuicRstStreamErrorCode error,
                    QuicStreamOffset bytes_written));
  MOCK_METHOD1(OnHeadersHeadOfLineBlocking, void(QuicTime::Delta delta));
  // Matchers cannot be used on non-copyable types like spdy::SpdyHeaderBlock.
  void PromisePushResources(
      const QuicString& request_url,
      const std::list<QuicBackendResponse::ServerPushInfo>& resources,
      QuicStreamId original_stream_id,
      const spdy::SpdyHeaderBlock& original_request_headers) override {
    original_request_headers_ = original_request_headers.Clone();
    PromisePushResourcesMock(request_url, resources, original_stream_id,
                             original_request_headers);
  }
  MOCK_METHOD4(PromisePushResourcesMock,
               void(const QuicString&,
                    const std::list<QuicBackendResponse::ServerPushInfo>&,
                    QuicStreamId,
                    const spdy::SpdyHeaderBlock&));

  using QuicSession::ActivateStream;

  spdy::SpdyHeaderBlock original_request_headers_;
};

class QuicSimpleServerStreamTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicSimpleServerStreamTest()
      : connection_(
            new StrictMock<MockQuicConnection>(&helper_,
                                               &alarm_factory_,
                                               Perspective::IS_SERVER,
                                               SupportedVersions(GetParam()))),
        crypto_config_(new QuicCryptoServerConfig(
            QuicCryptoServerConfig::TESTING,
            QuicRandom::GetInstance(),
            crypto_test_utils::ProofSourceForTesting(),
            KeyExchangeSource::Default(),
            TlsServerHandshaker::CreateSslCtx())),
        compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
        session_(connection_,
                 &session_owner_,
                 &session_helper_,
                 crypto_config_.get(),
                 &compressed_certs_cache_,
                 &memory_cache_backend_),
        quic_response_(new QuicBackendResponse),
        body_("hello world") {
    header_list_.OnHeaderBlockStart();
    header_list_.OnHeader(":authority", "www.google.com");
    header_list_.OnHeader(":path", "/");
    header_list_.OnHeader(":method", "POST");
    header_list_.OnHeader(":version", "HTTP/1.1");
    header_list_.OnHeader("content-length", "11");
    header_list_.OnHeaderBlockEnd(128, 128);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    stream_ = new QuicSimpleServerStreamPeer(
        QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0),
        &session_, BIDIRECTIONAL, &memory_cache_backend_);
    // Register stream_ in dynamic_stream_map_ and pass ownership to session_.
    session_.ActivateStream(QuicWrapUnique(stream_));
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

  const QuicString& StreamBody() {
    return QuicSimpleServerStreamPeer::body(stream_);
  }

  QuicString StreamHeadersValue(const QuicString& key) {
    return (*stream_->mutable_headers())[key].as_string();
  }

  spdy::SpdyHeaderBlock response_headers_;
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  StrictMock<MockQuicSessionVisitor> session_owner_;
  StrictMock<MockQuicCryptoServerStreamHelper> session_helper_;
  std::unique_ptr<QuicCryptoServerConfig> crypto_config_;
  QuicCompressedCertsCache compressed_certs_cache_;
  QuicMemoryCacheBackend memory_cache_backend_;
  StrictMock<MockQuicSimpleServerSession> session_;
  QuicSimpleServerStreamPeer* stream_;  // Owned by session_.
  std::unique_ptr<QuicBackendResponse> quic_response_;
  QuicString body_;
  QuicHeaderList header_list_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicSimpleServerStreamTest,
                        ::testing::ValuesIn(AllSupportedVersions()));

TEST_P(QuicSimpleServerStreamTest, TestFraming) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
  EXPECT_EQ(body_, StreamBody());
}

TEST_P(QuicSimpleServerStreamTest, TestFramingOnePacket) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));

  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
  EXPECT_EQ(body_, StreamBody());
}

TEST_P(QuicSimpleServerStreamTest, SendQuicRstStreamNoErrorInStopReading) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));

  EXPECT_FALSE(stream_->fin_received());
  EXPECT_FALSE(stream_->rst_received());

  stream_->set_fin_sent(true);
  stream_->CloseWriteSide();

  EXPECT_CALL(session_, SendRstStream(_, QUIC_STREAM_NO_ERROR, _)).Times(1);
  stream_->StopReading();
}

TEST_P(QuicSimpleServerStreamTest, TestFramingExtraData) {
  QuicString large_body = "hello world!!!!!!";

  // We'll automatically write out an error (headers + body)
  EXPECT_CALL(session_, WriteHeadersMock(_, _, _, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(session_, SendRstStream(_, QUIC_STREAM_NO_ERROR, _)).Times(0);

  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  // Content length is still 11.  This will register as an error and we won't
  // accept the bytes.
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/true, body_.size(), large_body));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithIllegalResponseStatus) {
  // Send an illegal response with response status not supported by HTTP/2.
  spdy::SpdyHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":version"] = "HTTP/1.1";
  (*request_headers)[":method"] = "GET";

  response_headers_[":version"] = "HTTP/1.1";
  // HTTP/2 only supports integer responsecode, so "200 OK" is illegal.
  response_headers_[":status"] = "200 OK";
  response_headers_["content-length"] = "5";
  QuicString body = "Yummm";
  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);

  stream_->set_fin_received(true);

  InSequence s;
  EXPECT_CALL(session_, WriteHeadersMock(stream_->id(), _, false, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(1)
      .WillOnce(Return(QuicConsumedData(
          strlen(QuicSimpleServerStream::kErrorResponseBody), true)));

  QuicSimpleServerStreamPeer::SendResponse(stream_);
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithIllegalResponseStatus2) {
  // Send an illegal response with response status not supported by HTTP/2.
  spdy::SpdyHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":version"] = "HTTP/1.1";
  (*request_headers)[":method"] = "GET";

  response_headers_[":version"] = "HTTP/1.1";
  // HTTP/2 only supports 3-digit-integer, so "+200" is illegal.
  response_headers_[":status"] = "+200";
  response_headers_["content-length"] = "5";
  QuicString body = "Yummm";
  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);

  stream_->set_fin_received(true);

  InSequence s;
  EXPECT_CALL(session_, WriteHeadersMock(stream_->id(), _, false, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(1)
      .WillOnce(Return(QuicConsumedData(
          strlen(QuicSimpleServerStream::kErrorResponseBody), true)));

  QuicSimpleServerStreamPeer::SendResponse(stream_);
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, SendPushResponseWith404Response) {
  // Create a new promised stream with even id().
  QuicSimpleServerStreamPeer* promised_stream = new QuicSimpleServerStreamPeer(
      QuicSpdySessionPeer::GetNthServerInitiatedStreamId(session_, 0),
      &session_, WRITE_UNIDIRECTIONAL, &memory_cache_backend_);
  session_.ActivateStream(QuicWrapUnique(promised_stream));

  // Send a push response with response status 404, which will be regarded as
  // invalid server push response.
  spdy::SpdyHeaderBlock* request_headers = promised_stream->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":version"] = "HTTP/1.1";
  (*request_headers)[":method"] = "GET";

  response_headers_[":version"] = "HTTP/1.1";
  response_headers_[":status"] = "404";
  response_headers_["content-length"] = "8";
  QuicString body = "NotFound";
  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);

  InSequence s;
  EXPECT_CALL(session_,
              SendRstStream(promised_stream->id(), QUIC_STREAM_CANCELLED, 0));

  QuicSimpleServerStreamPeer::SendResponse(promised_stream);
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithValidHeaders) {
  // Add a request and response with valid headers.
  spdy::SpdyHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":version"] = "HTTP/1.1";
  (*request_headers)[":method"] = "GET";

  response_headers_[":version"] = "HTTP/1.1";
  response_headers_[":status"] = "200";
  response_headers_["content-length"] = "5";
  QuicString body = "Yummm";
  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);
  stream_->set_fin_received(true);

  InSequence s;
  EXPECT_CALL(session_, WriteHeadersMock(stream_->id(), _, false, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(1)
      .WillOnce(Return(QuicConsumedData(body.length(), true)));

  QuicSimpleServerStreamPeer::SendResponse(stream_);
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, SendReponseWithPushResources) {
  // Tests that if a reponse has push resources to be send, SendResponse() will
  // call PromisePushResources() to handle these resources.

  // Add a request and response with valid headers into cache.
  QuicString host = "www.google.com";
  QuicString request_path = "/foo";
  QuicString body = "Yummm";
  QuicBackendResponse::ServerPushInfo push_info(
      QuicUrl(host, "/bar"), spdy::SpdyHeaderBlock(),
      QuicStream::kDefaultPriority, "Push body");
  std::list<QuicBackendResponse::ServerPushInfo> push_resources;
  push_resources.push_back(push_info);
  memory_cache_backend_.AddSimpleResponseWithServerPushResources(
      host, request_path, 200, body, push_resources);

  spdy::SpdyHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = request_path;
  (*request_headers)[":authority"] = host;
  (*request_headers)[":version"] = "HTTP/1.1";
  (*request_headers)[":method"] = "GET";

  stream_->set_fin_received(true);
  InSequence s;
  EXPECT_CALL(
      session_,
      PromisePushResourcesMock(
          host + request_path, _,
          QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0), _));
  EXPECT_CALL(session_, WriteHeadersMock(stream_->id(), _, false, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(1)
      .WillOnce(Return(QuicConsumedData(body.length(), true)));
  QuicSimpleServerStreamPeer::SendResponse(stream_);
  EXPECT_EQ(*request_headers, session_.original_request_headers_);
}

TEST_P(QuicSimpleServerStreamTest, PushResponseOnClientInitiatedStream) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  // Calling PushResponse() on a client initialted stream is never supposed to
  // happen.
  EXPECT_QUIC_BUG(stream_->PushResponse(spdy::SpdyHeaderBlock()),
                  "Client initiated stream"
                  " shouldn't be used as promised stream.");
}

TEST_P(QuicSimpleServerStreamTest, PushResponseOnServerInitiatedStream) {
  // Tests that PushResponse() should take the given headers as request headers
  // and fetch response from cache, and send it out.

  // Create a stream with even stream id and test against this stream.
  const QuicStreamId kServerInitiatedStreamId =
      QuicSpdySessionPeer::GetNthServerInitiatedStreamId(session_, 0);
  // Create a server initiated stream and pass it to session_.
  QuicSimpleServerStreamPeer* server_initiated_stream =
      new QuicSimpleServerStreamPeer(kServerInitiatedStreamId, &session_,
                                     WRITE_UNIDIRECTIONAL,
                                     &memory_cache_backend_);
  session_.ActivateStream(QuicWrapUnique(server_initiated_stream));

  const QuicString kHost = "www.foo.com";
  const QuicString kPath = "/bar";
  spdy::SpdyHeaderBlock headers;
  headers[":path"] = kPath;
  headers[":authority"] = kHost;
  headers[":version"] = "HTTP/1.1";
  headers[":method"] = "GET";

  response_headers_[":version"] = "HTTP/1.1";
  response_headers_[":status"] = "200";
  response_headers_["content-length"] = "5";
  const QuicString kBody = "Hello";
  memory_cache_backend_.AddResponse(kHost, kPath, std::move(response_headers_),
                                    kBody);

  // Call PushResponse() should trigger stream to fetch response from cache
  // and send it back.
  EXPECT_CALL(session_,
              WriteHeadersMock(kServerInitiatedStreamId, _, false,
                               server_initiated_stream->priority(), _));
  EXPECT_CALL(session_, WritevData(_, kServerInitiatedStreamId, _, _, _))
      .Times(1)
      .WillOnce(Return(QuicConsumedData(kBody.size(), true)));
  server_initiated_stream->PushResponse(std::move(headers));
  EXPECT_EQ(kPath, QuicSimpleServerStreamPeer::headers(
                       server_initiated_stream)[":path"]
                       .as_string());
  EXPECT_EQ("GET", QuicSimpleServerStreamPeer::headers(
                       server_initiated_stream)[":method"]
                       .as_string());
}

TEST_P(QuicSimpleServerStreamTest, TestSendErrorResponse) {
  EXPECT_CALL(session_, SendRstStream(_, QUIC_STREAM_NO_ERROR, _)).Times(0);

  stream_->set_fin_received(true);

  InSequence s;
  EXPECT_CALL(session_, WriteHeadersMock(_, _, _, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(1)
      .WillOnce(Return(QuicConsumedData(3, true)));

  QuicSimpleServerStreamPeer::SendErrorResponse(stream_);
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidMultipleContentLength) {
  EXPECT_CALL(session_, SendRstStream(_, QUIC_STREAM_NO_ERROR, _)).Times(0);

  spdy::SpdyHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  header_list_.OnHeader("content-length", QuicStringPiece("11\00012", 5));

  EXPECT_CALL(session_, WriteHeadersMock(_, _, _, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  stream_->OnStreamHeaderList(true, kFakeFrameLen, header_list_);

  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidLeadingNullContentLength) {
  EXPECT_CALL(session_, SendRstStream(_, QUIC_STREAM_NO_ERROR, _)).Times(0);

  spdy::SpdyHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  header_list_.OnHeader("content-length", QuicStringPiece("\00012", 3));

  EXPECT_CALL(session_, WriteHeadersMock(_, _, _, _, _));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  stream_->OnStreamHeaderList(true, kFakeFrameLen, header_list_);

  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, ValidMultipleContentLength) {
  spdy::SpdyHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  header_list_.OnHeader("content-length", QuicStringPiece("11\00011", 5));

  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);

  EXPECT_EQ(11, QuicSimpleServerStreamPeer::content_length(stream_));
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());
  EXPECT_FALSE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest,
       DoNotSendQuicRstStreamNoErrorWithRstReceived) {
  InSequence s;
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_CALL(session_, SendRstStream(_, QUIC_STREAM_NO_ERROR, _)).Times(0);
  EXPECT_CALL(session_, SendRstStream(_, QUIC_RST_ACKNOWLEDGEMENT, _)).Times(1);
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidHeadersWithFin) {
  char arr[] = {
      0x3a, 0x68, 0x6f, 0x73,  // :hos
      0x74, 0x00, 0x00, 0x00,  // t...
      0x00, 0x00, 0x00, 0x00,  // ....
      0x07, 0x3a, 0x6d, 0x65,  // .:me
      0x74, 0x68, 0x6f, 0x64,  // thod
      0x00, 0x00, 0x00, 0x03,  // ....
      0x47, 0x45, 0x54, 0x00,  // GET.
      0x00, 0x00, 0x05, 0x3a,  // ...:
      0x70, 0x61, 0x74, 0x68,  // path
      0x00, 0x00, 0x00, 0x04,  // ....
      0x2f, 0x66, 0x6f, 0x6f,  // /foo
      0x00, 0x00, 0x00, 0x07,  // ....
      0x3a, 0x73, 0x63, 0x68,  // :sch
      0x65, 0x6d, 0x65, 0x00,  // eme.
      0x00, 0x00, 0x00, 0x00,  // ....
      0x00, 0x00, 0x08, 0x3a,  // ...:
      0x76, 0x65, 0x72, 0x73,  // vers
      0x96, 0x6f, 0x6e, 0x00,  // <i(69)>on.
      0x00, 0x00, 0x08, 0x48,  // ...H
      0x54, 0x54, 0x50, 0x2f,  // TTP/
      0x31, 0x2e, 0x31,        // 1.1
  };
  QuicStringPiece data(arr, QUIC_ARRAYSIZE(arr));
  QuicStreamFrame frame(stream_->id(), true, 0, data);
  // Verify that we don't crash when we get a invalid headers in stream frame.
  stream_->OnStreamFrame(frame);
}

}  // namespace
}  // namespace test
}  // namespace quic
