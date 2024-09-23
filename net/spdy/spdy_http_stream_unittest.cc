// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_http_stream.h"

#include <stdint.h>

#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/signature_creator.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/features.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_http_utils.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

namespace {

// Tests the load timing of a stream that's connected and is not the first
// request sent on a connection.
void TestLoadTimingReused(const HttpStream& stream) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream.GetLoadTimingInfo(&load_timing_info));

  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Tests the load timing of a stream that's connected and using a fresh
// connection.
void TestLoadTimingNotReused(const HttpStream& stream) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream.GetLoadTimingInfo(&load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(
      load_timing_info.connect_timing,
      CONNECT_TIMING_HAS_DNS_TIMES | CONNECT_TIMING_HAS_SSL_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

class ReadErrorUploadDataStream : public UploadDataStream {
 public:
  enum class FailureMode { SYNC, ASYNC };

  explicit ReadErrorUploadDataStream(FailureMode mode)
      : UploadDataStream(true, 0), async_(mode) {}

  ReadErrorUploadDataStream(const ReadErrorUploadDataStream&) = delete;
  ReadErrorUploadDataStream& operator=(const ReadErrorUploadDataStream&) =
      delete;

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

class CancelStreamCallback : public TestCompletionCallbackBase {
 public:
  explicit CancelStreamCallback(SpdyHttpStream* stream) : stream_(stream) {}

  CompletionOnceCallback callback() {
    return base::BindOnce(&CancelStreamCallback::CancelStream,
                          base::Unretained(this));
  }

 private:
  void CancelStream(int result) {
    stream_->Cancel();
    SetResult(result);
  }

  raw_ptr<SpdyHttpStream> stream_;
};

}  // namespace

class SpdyHttpStreamTest : public testing::TestWithParam<bool>,
                           public WithTaskEnvironment {
 public:
  SpdyHttpStreamTest()
      : spdy_util_(/*use_priority_header=*/true),
        url_(kDefaultUrl),
        host_port_pair_(HostPortPair::FromURL(url_)),
        key_(host_port_pair_,
             PRIVACY_MODE_DISABLED,
             ProxyChain::Direct(),
             SessionUsage::kDestination,
             SocketTag(),
             NetworkAnonymizationKey(),
             SecureDnsPolicy::kAllow,
             /*disable_cert_verification_network_fetches=*/false),
        ssl_(SYNCHRONOUS, OK) {
    if (PriorityHeaderEnabled()) {
      feature_list_.InitAndEnableFeature(net::features::kPriorityHeader);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kPriorityHeader);
    }
    session_deps_.net_log = NetLog::Get();
  }

  ~SpdyHttpStreamTest() override = default;

 protected:
  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sequenced_data_->AllReadDataConsumed());
    EXPECT_TRUE(sequenced_data_->AllWriteDataConsumed());
  }

  // Initializes the session using SequencedSocketData.
  void InitSession(base::span<const MockRead> reads,
                   base::span<const MockWrite> writes) {
    sequenced_data_ = std::make_unique<SequencedSocketData>(reads, writes);
    session_deps_.socket_factory->AddSocketDataProvider(sequenced_data_.get());

    ssl_.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
    ssl_.next_proto = NextProto::kProtoHTTP2;
    ASSERT_TRUE(ssl_.ssl_info.cert);
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_);

    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = CreateSpdySession(http_session_.get(), key_, NetLogWithSource());
  }

  bool PriorityHeaderEnabled() const { return GetParam(); }

  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  const GURL url_;
  const HostPortPair host_port_pair_;
  const SpdySessionKey key_;
  std::unique_ptr<SequencedSocketData> sequenced_data_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;

 private:
  SSLSocketDataProvider ssl_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, SpdyHttpStreamTest, testing::Values(true, false));

TEST_P(SpdyHttpStreamTest, SendRequest) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(SYNCHRONOUS, 0, 2)  // EOF
  };

  InitSession(reads, writes);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());
  // Make sure getting load timing information the stream early does not crash.
  LoadTimingInfo load_timing_info;
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  http_stream->RegisterRequest(&request);
  ASSERT_THAT(http_stream->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  callback.WaitForResult();

  // Can get timing information once the stream connects.
  TestLoadTimingNotReused(*http_stream);

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  TestLoadTimingNotReused(*http_stream);
  http_stream->Close(true);
  // Test that there's no crash when trying to get the load timing after the
  // stream has been closed.
  TestLoadTimingNotReused(*http_stream);

  EXPECT_EQ(static_cast<int64_t>(req.size()), http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size()),
            http_stream->GetTotalReceivedBytes());
}

TEST_P(SpdyHttpStreamTest, RequestInfoDestroyedBeforeRead) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  InitSession(reads, writes);

  std::unique_ptr<HttpRequestInfo> request =
      std::make_unique<HttpRequestInfo>();
  request->method = "GET";
  request->url = url_;
  request->traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());

  http_stream->RegisterRequest(request.get());
  ASSERT_THAT(http_stream->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());
  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  EXPECT_LE(0, callback.WaitForResult());

  TestLoadTimingNotReused(*http_stream);
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(http_stream->GetLoadTimingInfo(&load_timing_info));

  // Perform all async reads.
  base::RunLoop().RunUntilIdle();

  // Destroy the request info before Read starts.
  request.reset();

  // Read stream to completion.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(1);
  ASSERT_EQ(0,
            http_stream->ReadResponseBody(buf.get(), 1, callback.callback()));

  // Stream 1 has been read to completion.
  TestLoadTimingNotReused(*http_stream);

  EXPECT_EQ(static_cast<int64_t>(req.size()), http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + body.size()),
            http_stream->GetTotalReceivedBytes());
}

TEST_P(SpdyHttpStreamTest, LoadTimingTwoRequests) {
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(3, "", true));
  MockRead reads[] = {
      CreateMockRead(resp1, 2), CreateMockRead(body1, 3),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  InitSession(reads, writes);

  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = url_;
  request1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback1;
  HttpResponseInfo response1;
  HttpRequestHeaders headers1;
  NetLogWithSource net_log;
  auto http_stream1 =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = url_;
  request2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  HttpResponseInfo response2;
  HttpRequestHeaders headers2;
  auto http_stream2 =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());

  // First write.
  http_stream1->RegisterRequest(&request1);
  ASSERT_THAT(http_stream1->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                             CompletionOnceCallback()),
              IsOk());
  EXPECT_THAT(
      http_stream1->SendRequest(headers1, &response1, callback1.callback()),
      IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  EXPECT_LE(0, callback1.WaitForResult());

  TestLoadTimingNotReused(*http_stream1);
  LoadTimingInfo load_timing_info1;
  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(http_stream1->GetLoadTimingInfo(&load_timing_info1));
  EXPECT_FALSE(http_stream2->GetLoadTimingInfo(&load_timing_info2));

  // Second write.
  http_stream2->RegisterRequest(&request2);
  ASSERT_THAT(http_stream2->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                             CompletionOnceCallback()),
              IsOk());
  EXPECT_THAT(
      http_stream2->SendRequest(headers2, &response2, callback2.callback()),
      IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  EXPECT_LE(0, callback2.WaitForResult());

  // Perform all async reads.
  base::RunLoop().RunUntilIdle();

  TestLoadTimingReused(*http_stream2);
  EXPECT_TRUE(http_stream2->GetLoadTimingInfo(&load_timing_info2));
  EXPECT_EQ(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  // Read stream 1 to completion, before making sure we can still read load
  // timing from both streams.
  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(1);
  ASSERT_EQ(
      0, http_stream1->ReadResponseBody(buf1.get(), 1, callback1.callback()));

  // Stream 1 has been read to completion.
  TestLoadTimingNotReused(*http_stream1);

  EXPECT_EQ(static_cast<int64_t>(req1.size()),
            http_stream1->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp1.size() + body1.size()),
            http_stream1->GetTotalReceivedBytes());

  // Stream 2 still has queued body data.
  TestLoadTimingReused(*http_stream2);

  EXPECT_EQ(static_cast<int64_t>(req2.size()),
            http_stream2->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp2.size() + body2.size()),
            http_stream2->GetTotalReceivedBytes());
}

TEST_P(SpdyHttpStreamTest, SendChunkedPost) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(1, kUploadData,
                                        /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),  // request
      CreateMockWrite(body, 1)  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3, SYNCHRONOUS),
      MockRead(SYNCHRONOUS, 0, 4)  // EOF
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);
  const size_t kFirstChunkSize = kUploadDataSize / 2;
  auto [first_chunk, second_chunk] =
      base::byte_span_from_cstring(kUploadData).split_at(kFirstChunkSize);
  upload_stream.AppendData(first_chunk, false);
  upload_stream.AppendData(second_chunk, true);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  SpdyHttpStream http_stream(session_, net_log.source(), {} /* dns_aliases */);
  http_stream.RegisterRequest(&request);
  ASSERT_THAT(http_stream.InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                           CompletionOnceCallback()),
              IsOk());

  EXPECT_THAT(http_stream.SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int64_t>(req.size() + body.size()),
            http_stream.GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + body.size()),
            http_stream.GetTotalReceivedBytes());

  // Because the server closed the connection, we there shouldn't be a session
  // in the pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));
}

// This unittest tests the request callback is properly called and handled.
TEST_P(SpdyHttpStreamTest, SendChunkedPostLastEmpty) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),  // request
      CreateMockWrite(chunk, 1),
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(chunk, 3, SYNCHRONOUS),
      MockRead(SYNCHRONOUS, 0, 4)  // EOF
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);
  upload_stream.AppendData(base::byte_span_from_cstring(""), true);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  SpdyHttpStream http_stream(session_, net_log.source(), {} /* dns_aliases */);
  http_stream.RegisterRequest(&request);
  ASSERT_THAT(http_stream.InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                           CompletionOnceCallback()),
              IsOk());
  EXPECT_THAT(http_stream.SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk.size()),
            http_stream.GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + chunk.size()),
            http_stream.GetTotalReceivedBytes());

  // Because the server closed the connection, there shouldn't be a session
  // in the pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));
}

TEST_P(SpdyHttpStreamTest, ConnectionClosedDuringChunkedPost) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(1, kUploadData,
                                        /*fin=*/false));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),  // Request
      CreateMockWrite(body, 1)  // First POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_CONNECTION_CLOSED, 2)  // Server hangs up early.
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);
  // Append first chunk.
  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData), false);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  SpdyHttpStream http_stream(session_, net_log.source(), {} /* dns_aliases */);
  http_stream.RegisterRequest(&request);
  ASSERT_THAT(http_stream.InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                           CompletionOnceCallback()),
              IsOk());

  EXPECT_THAT(http_stream.SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_EQ(static_cast<int64_t>(req.size() + body.size()),
            http_stream.GetTotalSentBytes());
  EXPECT_EQ(0, http_stream.GetTotalReceivedBytes());

  // Because the server closed the connection, we there shouldn't be a session
  // in the pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  // Appending a second chunk now should not result in a crash.
  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData), true);
  // Appending data is currently done synchronously, but seems best to be
  // paranoid.
  base::RunLoop().RunUntilIdle();

  // The total sent and received bytes should be unchanged.
  EXPECT_EQ(static_cast<int64_t>(req.size() + body.size()),
            http_stream.GetTotalSentBytes());
  EXPECT_EQ(0, http_stream.GetTotalReceivedBytes());
}

// Test to ensure the SpdyStream state machine does not get confused when a
// chunk becomes available while a write is pending.
TEST_P(SpdyHttpStreamTest, DelayedSendChunkedPost) {
  const char kUploadData1[] = "12345678";
  const int kUploadData1Size = std::size(kUploadData1) - 1;
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk1(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame chunk2(
      spdy_util_.ConstructSpdyDataFrame(1, kUploadData1, false));
  spdy::SpdySerializedFrame chunk3(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(chunk1, 1),  // POST upload frames
      CreateMockWrite(chunk2, 2), CreateMockWrite(chunk3, 3),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 4), CreateMockRead(chunk1, 5),
      CreateMockRead(chunk2, 6), CreateMockRead(chunk3, 7),
      MockRead(ASYNC, 0, 8)  // EOF
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());
  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData), false);

  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());
  http_stream->RegisterRequest(&request);
  ASSERT_THAT(http_stream->InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());

  TestCompletionCallback callback;
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  // Complete the initial request write and the first chunk.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.have_result());

  // Now append the final two chunks which will enqueue two more writes.
  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData1), false);
  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData), true);

  // Finish writing all the chunks and do all reads.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk1.size() + chunk2.size() +
                                 chunk3.size()),
            http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + chunk1.size() + chunk2.size() +
                                 chunk3.size()),
            http_stream->GetTotalReceivedBytes());

  // Check response headers.
  ASSERT_THAT(http_stream->ReadResponseHeaders(callback.callback()), IsOk());

  // Check |chunk1| response.
  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(kUploadDataSize);
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf1.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Check |chunk2| response.
  auto buf2 = base::MakeRefCounted<IOBufferWithSize>(kUploadData1Size);
  ASSERT_EQ(kUploadData1Size,
            http_stream->ReadResponseBody(
                buf2.get(), kUploadData1Size, callback.callback()));
  EXPECT_EQ(kUploadData1, std::string(buf2->data(), kUploadData1Size));

  // Check |chunk3| response.
  auto buf3 = base::MakeRefCounted<IOBufferWithSize>(kUploadDataSize);
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf3.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf3->data(), kUploadDataSize));

  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
}

// Test that the SpdyStream state machine can handle sending a final empty data
// frame when uploading a chunked data stream.
TEST_P(SpdyHttpStreamTest, DelayedSendChunkedPostWithEmptyFinalDataFrame) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk1(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame chunk2(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(chunk1, 1),  // POST upload frames
      CreateMockWrite(chunk2, 2),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 3), CreateMockRead(chunk1, 4),
      CreateMockRead(chunk2, 5), MockRead(ASYNC, 0, 6)  // EOF
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());
  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData), false);

  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());
  http_stream->RegisterRequest(&request);
  ASSERT_THAT(http_stream->InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());

  TestCompletionCallback callback;
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  // Complete the initial request write and the first chunk.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.have_result());

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk1.size()),
            http_stream->GetTotalSentBytes());
  EXPECT_EQ(0, http_stream->GetTotalReceivedBytes());

  // Now end the stream with an empty data frame and the FIN set.
  upload_stream.AppendData(base::byte_span_from_cstring(""), true);

  // Finish writing the final frame, and perform all reads.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check response headers.
  ASSERT_THAT(http_stream->ReadResponseHeaders(callback.callback()), IsOk());

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk1.size() + chunk2.size()),
            http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + chunk1.size() + chunk2.size()),
            http_stream->GetTotalReceivedBytes());

  // Check |chunk1| response.
  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(kUploadDataSize);
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf1.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Check |chunk2| response.
  ASSERT_EQ(0,
            http_stream->ReadResponseBody(
                buf1.get(), kUploadDataSize, callback.callback()));

  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
}

// Test that the SpdyStream state machine handles a chunked upload with no
// payload. Unclear if this is a case worth supporting.
TEST_P(SpdyHttpStreamTest, ChunkedPostWithEmptyPayload) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(chunk, 1),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(chunk, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());
  upload_stream.AppendData(base::byte_span_from_cstring(""), true);

  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());
  http_stream->RegisterRequest(&request);
  ASSERT_THAT(http_stream->InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());

  TestCompletionCallback callback;
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  // Complete writing request, followed by a FIN.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk.size()),
            http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + chunk.size()),
            http_stream->GetTotalReceivedBytes());

  // Check response headers.
  ASSERT_THAT(http_stream->ReadResponseHeaders(callback.callback()), IsOk());

  // Check |chunk| response.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(1);
  ASSERT_EQ(0,
            http_stream->ReadResponseBody(
                buf.get(), 1, callback.callback()));

  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
}

// Test case for https://crbug.com/50058.
TEST_P(SpdyHttpStreamTest, SpdyURLTest) {
  const char* const full_url = "https://www.example.org/foo?query=what#anchor";
  const char* const base_url = "https://www.example.org/foo?query=what";
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(base_url, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(SYNCHRONOUS, 0, 2)  // EOF
  };

  InitSession(reads, writes);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL(full_url);
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());
  http_stream->RegisterRequest(&request);
  ASSERT_THAT(http_stream->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());

  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));

  EXPECT_EQ(base_url, http_stream->stream()->url().spec());

  callback.WaitForResult();

  EXPECT_EQ(static_cast<int64_t>(req.size()), http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size()),
            http_stream->GetTotalReceivedBytes());

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));
}

// Test the receipt of a WINDOW_UPDATE frame while waiting for a chunk to be
// made available is handled correctly.
TEST_P(SpdyHttpStreamTest, DelayedSendChunkedPostWithWindowUpdate) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(chunk1, 1),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kUploadDataSize));
  MockRead reads[] = {
      CreateMockRead(window_update, 2), MockRead(ASYNC, ERR_IO_PENDING, 3),
      CreateMockRead(resp, 4), CreateMockRead(chunk1, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  InitSession(reads, writes);

  ChunkedUploadDataStream upload_stream(0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_stream;

  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());
  http_stream->RegisterRequest(&request);
  ASSERT_THAT(http_stream->InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                            CompletionOnceCallback()),
              IsOk());

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  TestCompletionCallback callback;
  EXPECT_THAT(http_stream->SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  // Complete the initial request write and first chunk.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.have_result());

  EXPECT_EQ(static_cast<int64_t>(req.size()), http_stream->GetTotalSentBytes());
  EXPECT_EQ(0, http_stream->GetTotalReceivedBytes());

  upload_stream.AppendData(base::byte_span_from_cstring(kUploadData), true);

  // Verify that the window size has decreased.
  ASSERT_TRUE(http_stream->stream() != nullptr);
  EXPECT_NE(static_cast<int>(kDefaultInitialWindowSize),
            http_stream->stream()->send_window_size());

  // Read window update.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk1.size()),
            http_stream->GetTotalSentBytes());
  // The window update is not counted in the total received bytes.
  EXPECT_EQ(0, http_stream->GetTotalReceivedBytes());

  // Verify the window update.
  ASSERT_TRUE(http_stream->stream() != nullptr);
  EXPECT_EQ(static_cast<int>(kDefaultInitialWindowSize),
            http_stream->stream()->send_window_size());

  // Read rest of data.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(static_cast<int64_t>(req.size() + chunk1.size()),
            http_stream->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(resp.size() + chunk1.size()),
            http_stream->GetTotalReceivedBytes());

  // Check response headers.
  ASSERT_THAT(http_stream->ReadResponseHeaders(callback.callback()), IsOk());

  // Check |chunk1| response.
  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(kUploadDataSize);
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf1.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
}

TEST_P(SpdyHttpStreamTest, DataReadErrorSynchronous) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));

  // Server receives spdy::ERROR_CODE_INTERNAL_ERROR on client's internal
  // failure. The failure is a reading error in this case caused by
  // UploadDataStream::Read().
  spdy::SpdySerializedFrame rst_frame(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_INTERNAL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(req, 0, SYNCHRONOUS),       // Request
      CreateMockWrite(rst_frame, 1, SYNCHRONOUS)  // Reset frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));

  MockRead reads[] = {
      CreateMockRead(resp, 2), MockRead(SYNCHRONOUS, 0, 3),
  };

  InitSession(reads, writes);

  ReadErrorUploadDataStream upload_data_stream(
      ReadErrorUploadDataStream::FailureMode::SYNC);
  ASSERT_THAT(upload_data_stream.Init(TestCompletionCallback().callback(),
                                      NetLogWithSource()),
              IsOk());

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_data_stream;

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  SpdyHttpStream http_stream(session_, net_log.source(), {} /* dns_aliases */);
  http_stream.RegisterRequest(&request);
  ASSERT_THAT(http_stream.InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                           CompletionOnceCallback()),
              IsOk());

  int result = http_stream.SendRequest(headers, &response, callback.callback());
  EXPECT_THAT(callback.GetResult(result), IsError(ERR_FAILED));

  // Run posted SpdyHttpStream::ResetStreamInternal() task.
  base::RunLoop().RunUntilIdle();

  // Because the server has not closed the connection yet, there shouldn't be
  // a stream but a session in the pool
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));
}

TEST_P(SpdyHttpStreamTest, DataReadErrorAsynchronous) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));

  // Server receives spdy::ERROR_CODE_INTERNAL_ERROR on client's internal
  // failure. The failure is a reading error in this case caused by
  // UploadDataStream::Read().
  spdy::SpdySerializedFrame rst_frame(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_INTERNAL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),       // Request
      CreateMockWrite(rst_frame, 1)  // Reset frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));

  MockRead reads[] = {
      MockRead(ASYNC, 0, 2),
  };

  InitSession(reads, writes);

  ReadErrorUploadDataStream upload_data_stream(
      ReadErrorUploadDataStream::FailureMode::ASYNC);
  ASSERT_THAT(upload_data_stream.Init(TestCompletionCallback().callback(),
                                      NetLogWithSource()),
              IsOk());

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request.upload_data_stream = &upload_data_stream;

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  SpdyHttpStream http_stream(session_, net_log.source(), {} /* dns_aliases */);
  http_stream.RegisterRequest(&request);
  ASSERT_THAT(http_stream.InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                           CompletionOnceCallback()),
              IsOk());

  int result = http_stream.SendRequest(headers, &response, callback.callback());
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.GetResult(result), IsError(ERR_FAILED));

  // Run posted SpdyHttpStream::ResetStreamInternal() task.
  base::RunLoop().RunUntilIdle();

  // Because the server has closed the connection, there shouldn't be a session
  // in the pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));
}

// Regression test for https://crbug.com/622447.
TEST_P(SpdyHttpStreamTest, RequestCallbackCancelsStream) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(chunk, 1),
                        CreateMockWrite(rst, 2)};
  MockRead reads[] = {MockRead(ASYNC, 0, 3)};
  InitSession(reads, writes);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  ChunkedUploadDataStream upload_stream(0);
  request.upload_data_stream = &upload_stream;

  TestCompletionCallback upload_callback;
  ASSERT_THAT(
      upload_stream.Init(upload_callback.callback(), NetLogWithSource()),
      IsOk());
  upload_stream.AppendData(base::byte_span_from_cstring(""), true);

  NetLogWithSource net_log;
  SpdyHttpStream http_stream(session_, net_log.source(), {} /* dns_aliases */);
  http_stream.RegisterRequest(&request);
  ASSERT_THAT(http_stream.InitializeStream(false, DEFAULT_PRIORITY, net_log,
                                           CompletionOnceCallback()),
              IsOk());

  CancelStreamCallback callback(&http_stream);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_THAT(http_stream.SendRequest(headers, &response, callback.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key_));

  // The callback cancels |http_stream|.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Finish async network reads/writes.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key_));
}

// Regression test for https://crbug.com/1082683.
// SendRequest() callback should be called as soon as sending is done,
// even when sending greased frame type is allowed.
TEST_P(SpdyHttpStreamTest, DownloadWithEmptyDataFrame) {
  session_deps_.http2_end_stream_with_data_frame = true;

  // HEADERS frame without END_STREAM
  quiche::HttpHeaderBlock request_headers;
  request_headers[spdy::kHttp2MethodHeader] = "GET";
  spdy_util_.AddUrlToHeaderBlock(kDefaultUrl, &request_headers);
  spdy::SpdySerializedFrame req = spdy_util_.ConstructSpdyHeaders(
      1, std::move(request_headers), LOWEST, /* fin = */ false);

  // Empty DATA frame with END_STREAM
  spdy::SpdySerializedFrame empty_body(
      spdy_util_.ConstructSpdyDataFrame(1, "", /* fin = */ true));

  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(empty_body, 1)};

  // This test only concerns the request,
  // no need to construct a meaningful response.
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Pause reads.
      MockRead(ASYNC, 0, 3)                // Close connection.
  };

  InitSession(reads, writes);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url_;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  NetLogWithSource net_log;
  auto http_stream =
      std::make_unique<SpdyHttpStream>(session_, net_log.source(),
                                       /*dns_aliases=*/std::set<std::string>());

  http_stream->RegisterRequest(&request);
  int rv = http_stream->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                         CompletionOnceCallback());
  EXPECT_THAT(rv, IsOk());

  rv = http_stream->SendRequest(headers, &response, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The request callback should be called even though response has not been
  // received yet.
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net::test
