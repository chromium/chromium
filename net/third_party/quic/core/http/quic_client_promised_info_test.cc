// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_client_promised_info.h"

#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/http/quic_spdy_client_session.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/quic_client_promised_info_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

using spdy::SpdyHeaderBlock;
using testing::_;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class MockQuicSpdyClientSession : public QuicSpdyClientSession {
 public:
  explicit MockQuicSpdyClientSession(
      QuicConnection* connection,
      QuicClientPushPromiseIndex* push_promise_index)
      : QuicSpdyClientSession(DefaultQuicConfig(),
                              connection,
                              QuicServerId("example.com", 443, false),
                              &crypto_config_,
                              push_promise_index),
        crypto_config_(crypto_test_utils::ProofVerifierForTesting(),
                       TlsClientHandshaker::CreateSslCtx()),
        authorized_(true) {}
  MockQuicSpdyClientSession(const MockQuicSpdyClientSession&) = delete;
  MockQuicSpdyClientSession& operator=(const MockQuicSpdyClientSession&) =
      delete;
  ~MockQuicSpdyClientSession() override {}

  bool IsAuthorized(const QuicString& authority) override {
    return authorized_;
  }

  void set_authorized(bool authorized) { authorized_ = authorized; }

  MOCK_METHOD1(CloseStream, void(QuicStreamId stream_id));

 private:
  QuicCryptoClientConfig crypto_config_;

  bool authorized_;
};

class QuicClientPromisedInfoTest : public QuicTest {
 public:
  class StreamVisitor;

  QuicClientPromisedInfoTest()
      : connection_(new StrictMock<MockQuicConnection>(&helper_,
                                                       &alarm_factory_,
                                                       Perspective::IS_CLIENT)),
        session_(connection_, &push_promise_index_),
        body_("hello world"),
        promise_id_(
            QuicUtils::GetInvalidStreamId(connection_->transport_version())) {
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    session_.Initialize();

    headers_[":status"] = "200";
    headers_["content-length"] = "11";

    stream_ = QuicMakeUnique<QuicSpdyClientStream>(
        QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0),
        &session_, BIDIRECTIONAL);
    stream_visitor_ = QuicMakeUnique<StreamVisitor>();
    stream_->set_visitor(stream_visitor_.get());

    push_promise_[":path"] = "/bar";
    push_promise_[":authority"] = "www.google.com";
    push_promise_[":version"] = "HTTP/1.1";
    push_promise_[":method"] = "GET";
    push_promise_[":scheme"] = "https";

    promise_url_ = SpdyUtils::GetPromisedUrlFromHeaders(push_promise_);

    client_request_ = push_promise_.Clone();
    promise_id_ =
        QuicSpdySessionPeer::GetNthServerInitiatedStreamId(session_, 0);
  }

  class StreamVisitor : public QuicSpdyClientStream::Visitor {
    void OnClose(QuicSpdyStream* stream) override {
      QUIC_DVLOG(1) << "stream " << stream->id();
    }
  };

  void ReceivePromise(QuicStreamId id) {
    auto headers = AsHeaderList(push_promise_);
    stream_->OnPromiseHeaderList(id, headers.uncompressed_header_bytes(),
                                 headers);
  }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  QuicClientPushPromiseIndex push_promise_index_;

  MockQuicSpdyClientSession session_;
  std::unique_ptr<QuicSpdyClientStream> stream_;
  std::unique_ptr<StreamVisitor> stream_visitor_;
  std::unique_ptr<QuicSpdyClientStream> promised_stream_;
  SpdyHeaderBlock headers_;
  QuicString body_;
  SpdyHeaderBlock push_promise_;
  QuicStreamId promise_id_;
  QuicString promise_url_;
  SpdyHeaderBlock client_request_;
};

TEST_F(QuicClientPromisedInfoTest, PushPromise) {
  ReceivePromise(promise_id_);

  // Verify that the promise is in the unclaimed streams map.
  EXPECT_NE(session_.GetPromisedById(promise_id_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseCleanupAlarm) {
  ReceivePromise(promise_id_);

  // Verify that the promise is in the unclaimed streams map.
  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  ASSERT_NE(promised, nullptr);

  // Fire the alarm that will cancel the promised stream.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_PUSH_STREAM_TIMED_OUT));
  alarm_factory_.FireAlarm(QuicClientPromisedInfoPeer::GetAlarm(promised));

  // Verify that the promise is gone after the alarm fires.
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseInvalidMethod) {
  // Promise with an unsafe method
  push_promise_[":method"] = "PUT";

  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_INVALID_PROMISE_METHOD));
  ReceivePromise(promise_id_);

  // Verify that the promise headers were ignored
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseMissingMethod) {
  // Promise with a missing method
  push_promise_.erase(":method");

  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_INVALID_PROMISE_METHOD));
  ReceivePromise(promise_id_);

  // Verify that the promise headers were ignored
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseInvalidUrl) {
  // Remove required header field to make URL invalid
  push_promise_.erase(":authority");

  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_INVALID_PROMISE_URL));
  ReceivePromise(promise_id_);

  // Verify that the promise headers were ignored
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseUnauthorizedUrl) {
  session_.set_authorized(false);

  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_UNAUTHORIZED_PROMISE_URL));

  ReceivePromise(promise_id_);

  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  ASSERT_EQ(promised, nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseMismatch) {
  ReceivePromise(promise_id_);

  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  ASSERT_NE(promised, nullptr);

  // Need to send the promised response headers and initiate the
  // rendezvous for secondary validation to proceed.
  QuicSpdyClientStream* promise_stream = static_cast<QuicSpdyClientStream*>(
      session_.GetOrCreateStream(promise_id_));
  auto headers = AsHeaderList(headers_);
  promise_stream->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                                     headers);

  TestPushPromiseDelegate delegate(/*match=*/false);
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_PROMISE_VARY_MISMATCH));
  EXPECT_CALL(session_, CloseStream(promise_id_));

  promised->HandleClientRequest(client_request_, &delegate);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseVaryWaits) {
  ReceivePromise(promise_id_);

  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  EXPECT_FALSE(promised->is_validating());
  ASSERT_NE(promised, nullptr);

  // Now initiate rendezvous.
  TestPushPromiseDelegate delegate(/*match=*/true);
  promised->HandleClientRequest(client_request_, &delegate);
  EXPECT_TRUE(promised->is_validating());

  // Promise is still there, waiting for response.
  EXPECT_NE(session_.GetPromisedById(promise_id_), nullptr);

  // Send Response, should trigger promise validation and complete rendezvous
  QuicSpdyClientStream* promise_stream = static_cast<QuicSpdyClientStream*>(
      session_.GetOrCreateStream(promise_id_));
  ASSERT_NE(promise_stream, nullptr);
  auto headers = AsHeaderList(headers_);
  promise_stream->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                                     headers);

  // Promise is gone
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseVaryNoWait) {
  ReceivePromise(promise_id_);

  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  ASSERT_NE(promised, nullptr);

  QuicSpdyClientStream* promise_stream = static_cast<QuicSpdyClientStream*>(
      session_.GetOrCreateStream(promise_id_));
  ASSERT_NE(promise_stream, nullptr);

  // Send Response, should trigger promise validation and complete rendezvous
  auto headers = AsHeaderList(headers_);
  promise_stream->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                                     headers);

  // Now initiate rendezvous.
  TestPushPromiseDelegate delegate(/*match=*/true);
  promised->HandleClientRequest(client_request_, &delegate);

  // Promise is gone
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
  // Have a push stream
  EXPECT_TRUE(delegate.rendezvous_fired());

  EXPECT_NE(delegate.rendezvous_stream(), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseWaitCancels) {
  ReceivePromise(promise_id_);

  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  ASSERT_NE(promised, nullptr);

  // Now initiate rendezvous.
  TestPushPromiseDelegate delegate(/*match=*/true);
  promised->HandleClientRequest(client_request_, &delegate);

  // Promise is still there, waiting for response.
  EXPECT_NE(session_.GetPromisedById(promise_id_), nullptr);

  // Create response stream, but no data yet.
  session_.GetOrCreateStream(promise_id_);

  // Cancel the promised stream.
  EXPECT_CALL(session_, CloseStream(promise_id_));
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(promise_id_, QUIC_STREAM_CANCELLED));
  promised->Cancel();

  // Promise is gone
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
}

TEST_F(QuicClientPromisedInfoTest, PushPromiseDataClosed) {
  ReceivePromise(promise_id_);

  QuicClientPromisedInfo* promised = session_.GetPromisedById(promise_id_);
  ASSERT_NE(promised, nullptr);

  QuicSpdyClientStream* promise_stream = static_cast<QuicSpdyClientStream*>(
      session_.GetOrCreateStream(promise_id_));
  ASSERT_NE(promise_stream, nullptr);

  // Send response, rendezvous will be able to finish synchronously.
  auto headers = AsHeaderList(headers_);
  promise_stream->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                                     headers);

  EXPECT_CALL(session_, CloseStream(promise_id_));
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(promise_id_, QUIC_STREAM_PEER_GOING_AWAY));
  session_.SendRstStream(promise_id_, QUIC_STREAM_PEER_GOING_AWAY, 0);

  // Now initiate rendezvous.
  TestPushPromiseDelegate delegate(/*match=*/true);
  EXPECT_EQ(promised->HandleClientRequest(client_request_, &delegate),
            QUIC_FAILURE);

  // Got an indication of the stream failure, client should retry
  // request.
  EXPECT_FALSE(delegate.rendezvous_fired());
  EXPECT_EQ(delegate.rendezvous_stream(), nullptr);

  // Promise is gone
  EXPECT_EQ(session_.GetPromisedById(promise_id_), nullptr);
}

}  // namespace
}  // namespace test
}  // namespace quic
