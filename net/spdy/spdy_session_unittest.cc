// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_session.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/hex_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_stream.h"
#include "net/cert/ct_policy_status.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_info.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/alps_decoder.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using net::test::IsError;
using net::test::IsOk;
using testing::_;

namespace net {

namespace {

const char kBodyData[] = "Body data";
const size_t kBodyDataSize = std::size(kBodyData);
const std::string_view kBodyDataStringPiece(kBodyData, kBodyDataSize);

static base::TimeDelta g_time_delta;
static base::TimeTicks g_time_now;

base::TimeTicks TheNearFuture() {
  return base::TimeTicks::Now() + g_time_delta;
}

base::TimeTicks SlowReads() {
  g_time_delta += base::Milliseconds(2 * kYieldAfterDurationMilliseconds);
  return base::TimeTicks::Now() + g_time_delta;
}

base::TimeTicks InstantaneousReads() {
  return g_time_now;
}

class MockRequireCTDelegate : public TransportSecurityState::RequireCTDelegate {
 public:
  MOCK_METHOD3(IsCTRequiredForHost,
               CTRequirementLevel(std::string_view host,
                                  const X509Certificate* chain,
                                  const HashValueVector& hashes));
};

// SpdySessionRequest::Delegate implementation that does nothing. The test it's
// used in need to create a session request to trigger the creation of a session
// alias, but doesn't care about when or if OnSpdySessionAvailable() is invoked.
class SpdySessionRequestDelegate
    : public SpdySessionPool::SpdySessionRequest::Delegate {
 public:
  SpdySessionRequestDelegate() = default;

  SpdySessionRequestDelegate(const SpdySessionRequestDelegate&) = delete;
  SpdySessionRequestDelegate& operator=(const SpdySessionRequestDelegate&) =
      delete;

  ~SpdySessionRequestDelegate() override = default;

  void OnSpdySessionAvailable(
      base::WeakPtr<SpdySession> spdy_session) override {}
};

}  // namespace

class SpdySessionTest : public PlatformTest, public WithTaskEnvironment {
 public:
  // Functions used with RunResumeAfterUnstallTest().

  void StallSessionOnly(SpdyStream* stream) { StallSessionSend(); }

  void StallStreamOnly(SpdyStream* stream) { StallStreamSend(stream); }

  void StallSessionStream(SpdyStream* stream) {
    StallSessionSend();
    StallStreamSend(stream);
  }

  void StallStreamSession(SpdyStream* stream) {
    StallStreamSend(stream);
    StallSessionSend();
  }

  void UnstallSessionOnly(SpdyStream* stream, int32_t delta_window_size) {
    UnstallSessionSend(delta_window_size);
  }

  void UnstallStreamOnly(SpdyStream* stream, int32_t delta_window_size) {
    UnstallStreamSend(stream, delta_window_size);
  }

  void UnstallSessionStream(SpdyStream* stream, int32_t delta_window_size) {
    UnstallSessionSend(delta_window_size);
    UnstallStreamSend(stream, delta_window_size);
  }

  void UnstallStreamSession(SpdyStream* stream, int32_t delta_window_size) {
    UnstallStreamSend(stream, delta_window_size);
    UnstallSessionSend(delta_window_size);
  }

 protected:
  // Used by broken connection detection tests.
  static constexpr base::TimeDelta kHeartbeatInterval = base::Seconds(10);

  explicit SpdySessionTest(base::test::TaskEnvironment::TimeSource time_source =
                               base::test::TaskEnvironment::TimeSource::DEFAULT)
      : WithTaskEnvironment(time_source),
        old_max_group_sockets_(ClientSocketPoolManager::max_sockets_per_group(
            HttpNetworkSession::NORMAL_SOCKET_POOL)),
        old_max_pool_sockets_(ClientSocketPoolManager::max_sockets_per_pool(
            HttpNetworkSession::NORMAL_SOCKET_POOL)),
        test_url_(kDefaultUrl),
        test_server_(test_url_),
        key_(HostPortPair::FromURL(test_url_),
             PRIVACY_MODE_DISABLED,
             ProxyChain::Direct(),
             SessionUsage::kDestination,
             SocketTag(),
             NetworkAnonymizationKey(),
             SecureDnsPolicy::kAllow,
             /*disable_cert_verification_network_fetches=*/false),
        ssl_(SYNCHRONOUS, OK) {}

  ~SpdySessionTest() override {
    // Important to restore the per-pool limit first, since the pool limit must
    // always be greater than group limit, and the tests reduce both limits.
    ClientSocketPoolManager::set_max_sockets_per_pool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_pool_sockets_);
    ClientSocketPoolManager::set_max_sockets_per_group(
        HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_group_sockets_);
  }

  void SetUp() override {
    g_time_delta = base::TimeDelta();
    g_time_now = base::TimeTicks::Now();
    session_deps_.net_log = NetLog::Get();
    session_deps_.enable_server_push_cancellation = true;
  }

  void CreateNetworkSession() {
    DCHECK(!http_session_);
    DCHECK(!spdy_session_pool_);
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    spdy_session_pool_ = http_session_->spdy_session_pool();
  }

  void AddSSLSocketData() {
    ssl_.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
    ASSERT_TRUE(ssl_.ssl_info.cert);
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_);
  }

  void CreateSpdySession() {
    DCHECK(!session_);
    session_ = ::net::CreateSpdySession(http_session_.get(), key_,
                                        net_log_with_source_);
  }

  void StallSessionSend() {
    // Reduce the send window size to 0 to stall.
    while (session_send_window_size() > 0) {
      DecreaseSendWindowSize(
          std::min(kMaxSpdyFrameChunkSize, session_send_window_size()));
    }
  }

  void UnstallSessionSend(int32_t delta_window_size) {
    IncreaseSendWindowSize(delta_window_size);
  }

  void StallStreamSend(SpdyStream* stream) {
    // Reduce the send window size to 0 to stall.
    while (stream->send_window_size() > 0) {
      stream->DecreaseSendWindowSize(
          std::min(kMaxSpdyFrameChunkSize, stream->send_window_size()));
    }
  }

  void UnstallStreamSend(SpdyStream* stream, int32_t delta_window_size) {
    stream->IncreaseSendWindowSize(delta_window_size);
  }

  void RunResumeAfterUnstallTest(
      base::OnceCallback<void(SpdyStream*)> stall_function,
      base::OnceCallback<void(SpdyStream*, int32_t)> unstall_function);

  // SpdySession private methods.

  void MaybeSendPrefacePing() { session_->MaybeSendPrefacePing(); }

  void WritePingFrame(spdy::SpdyPingId unique_id, bool is_ack) {
    session_->WritePingFrame(unique_id, is_ack);
  }

  void CheckPingStatus(base::TimeTicks last_check_time) {
    session_->CheckPingStatus(last_check_time);
  }

  bool OnUnknownFrame(spdy::SpdyStreamId stream_id, uint8_t frame_type) {
    return session_->OnUnknownFrame(stream_id, frame_type);
  }

  void IncreaseSendWindowSize(int delta_window_size) {
    session_->IncreaseSendWindowSize(delta_window_size);
  }

  void DecreaseSendWindowSize(int32_t delta_window_size) {
    session_->DecreaseSendWindowSize(delta_window_size);
  }

  void IncreaseRecvWindowSize(int delta_window_size) {
    session_->IncreaseRecvWindowSize(delta_window_size);
  }

  void DecreaseRecvWindowSize(int32_t delta_window_size) {
    session_->DecreaseRecvWindowSize(delta_window_size);
  }

  // Accessors for SpdySession private members.

  void set_in_io_loop(bool in_io_loop) { session_->in_io_loop_ = in_io_loop; }

  void set_stream_hi_water_mark(spdy::SpdyStreamId stream_hi_water_mark) {
    session_->stream_hi_water_mark_ = stream_hi_water_mark;
  }

  size_t max_concurrent_streams() { return session_->max_concurrent_streams_; }

  void set_max_concurrent_streams(size_t max_concurrent_streams) {
    session_->max_concurrent_streams_ = max_concurrent_streams;
  }

  bool ping_in_flight() { return session_->ping_in_flight_; }

  spdy::SpdyPingId next_ping_id() { return session_->next_ping_id_; }

  base::TimeTicks last_read_time() { return session_->last_read_time_; }

  bool check_ping_status_pending() {
    return session_->check_ping_status_pending_;
  }

  int32_t session_send_window_size() {
    return session_->session_send_window_size_;
  }

  int32_t session_recv_window_size() {
    return session_->session_recv_window_size_;
  }

  void set_session_recv_window_size(int32_t session_recv_window_size) {
    session_->session_recv_window_size_ = session_recv_window_size;
  }

  int32_t session_unacked_recv_window_bytes() {
    return session_->session_unacked_recv_window_bytes_;
  }

  int32_t stream_initial_send_window_size() {
    return session_->stream_initial_send_window_size_;
  }

  void set_connection_at_risk_of_loss_time(base::TimeDelta duration) {
    session_->connection_at_risk_of_loss_time_ = duration;
  }

  // Quantities derived from SpdySession private members.

  size_t pending_create_stream_queue_size(RequestPriority priority) {
    DCHECK_GE(priority, MINIMUM_PRIORITY);
    DCHECK_LE(priority, MAXIMUM_PRIORITY);
    return session_->pending_create_stream_queues_[priority].size();
  }

  size_t num_active_streams() { return session_->active_streams_.size(); }

  size_t num_created_streams() { return session_->created_streams_.size(); }

  uint32_t header_encoder_table_size() const {
    return session_->buffered_spdy_framer_->header_encoder_table_size();
  }

  RecordingNetLogObserver net_log_observer_;
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::NONE)};

  // Original socket limits.  Some tests set these.  Safest to always restore
  // them once each test has been run.
  int old_max_group_sockets_;
  int old_max_pool_sockets_;

  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;
  raw_ptr<SpdySessionPool> spdy_session_pool_ = nullptr;
  const GURL test_url_;
  const url::SchemeHostPort test_server_;
  SpdySessionKey key_;
  SSLSocketDataProvider ssl_;
};

class SpdySessionTestWithMockTime : public SpdySessionTest {
 protected:
  SpdySessionTestWithMockTime()
      : SpdySessionTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// Try to create a SPDY session that will fail during
// initialization. Nothing should blow up.
TEST_F(SpdySessionTest, InitialReadError) {
  MockRead reads[] = {MockRead(ASYNC, ERR_CONNECTION_CLOSED, 0)};
  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_TRUE(session_);
  // Flush the read.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

namespace {

// A helper class that vends a callback that, when fired, destroys a
// given SpdyStreamRequest.
class StreamRequestDestroyingCallback : public TestCompletionCallbackBase {
 public:
  StreamRequestDestroyingCallback() = default;

  ~StreamRequestDestroyingCallback() override = default;

  void SetRequestToDestroy(std::unique_ptr<SpdyStreamRequest> request) {
    request_ = std::move(request);
  }

  CompletionOnceCallback MakeCallback() {
    return base::BindOnce(&StreamRequestDestroyingCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    request_.reset();
    SetResult(result);
  }

  std::unique_ptr<SpdyStreamRequest> request_;
};

}  // namespace

// Request kInitialMaxConcurrentStreams streams.  Request two more
// streams, but have the callback for one destroy the second stream
// request. Close the session. Nothing should blow up. This is a
// regression test for http://crbug.com/250841 .
TEST_F(SpdySessionTest, PendingStreamCancellingAnother) {
  MockRead reads[] = {MockRead(ASYNC, 0, 0), };

  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Create the maximum number of concurrent streams.
  for (size_t i = 0; i < kInitialMaxConcurrentStreams; ++i) {
    base::WeakPtr<SpdyStream> spdy_stream =
        CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, MEDIUM, NetLogWithSource());
    ASSERT_TRUE(spdy_stream);
  }

  SpdyStreamRequest request1;
  auto request2 = std::make_unique<SpdyStreamRequest>();

  StreamRequestDestroyingCallback callback1;
  ASSERT_EQ(ERR_IO_PENDING,
            request1.StartRequest(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, false, MEDIUM, SocketTag(),
                                  NetLogWithSource(), callback1.MakeCallback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  // |callback2| is never called.
  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            request2->StartRequest(SPDY_BIDIRECTIONAL_STREAM, session_,
                                   test_url_, false, MEDIUM, SocketTag(),
                                   NetLogWithSource(), callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  callback1.SetRequestToDestroy(std::move(request2));

  session_->CloseSessionOnError(ERR_ABORTED, "Aborting session");

  EXPECT_THAT(callback1.WaitForResult(), IsError(ERR_ABORTED));
}

// A session receiving a GOAWAY frame with no active streams should close.
TEST_F(SpdySessionTest, GoAwayWithNoActiveStreams) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
      CreateMockRead(goaway, 0),
  };
  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Read and process the GOAWAY frame.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);
}

// A session receiving a GOAWAY frame immediately with no active
// streams should then close.
TEST_F(SpdySessionTest, GoAwayImmediatelyWithNoActiveStreams) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
      CreateMockRead(goaway, 0, SYNCHRONOUS), MockRead(ASYNC, 0, 1)  // EOF
  };
  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session_);
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(data.AllReadDataConsumed());
}

// A session receiving a GOAWAY frame with active streams should close
// when the last active stream is closed.
TEST_F(SpdySessionTest, GoAwayWithActiveStreams) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), CreateMockRead(goaway, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4), MockRead(ASYNC, 0, 5)  // EOF
  };
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  quiche::HttpHeaderBlock headers2(headers.Clone());

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Read and process the GOAWAY frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));

  EXPECT_FALSE(session_->IsStreamActive(3));
  EXPECT_FALSE(spdy_stream2);
  EXPECT_TRUE(session_->IsStreamActive(1));

  EXPECT_TRUE(session_->IsGoingAway());

  // Should close the session.
  spdy_stream1->Close();
  EXPECT_FALSE(spdy_stream1);

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Regression test for https://crbug.com/547130.
TEST_F(SpdySessionTest, GoAwayWithActiveAndCreatedStream) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(0));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(goaway, 2),
  };

  // No |req2|, because the second stream will never get activated.
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);
  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  EXPECT_EQ(0u, spdy_stream1->stream_id());

  // Active stream 1.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_TRUE(session_->IsStreamActive(1));

  // Create stream corresponding to the next request.
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());

  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Read and process the GOAWAY frame before the second stream could be
  // activated.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session_);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Have a session receive two GOAWAY frames, with the last one causing
// the last active stream to be closed. The session should then be
// closed after the second GOAWAY frame.
TEST_F(SpdySessionTest, GoAwayTwice) {
  spdy::SpdySerializedFrame goaway1(spdy_util_.ConstructSpdyGoAway(1));
  spdy::SpdySerializedFrame goaway2(spdy_util_.ConstructSpdyGoAway(0));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), CreateMockRead(goaway1, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4), CreateMockRead(goaway2, 5),
      MockRead(ASYNC, ERR_IO_PENDING, 6), MockRead(ASYNC, 0, 7)  // EOF
  };
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  quiche::HttpHeaderBlock headers2(headers.Clone());

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Read and process the first GOAWAY frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));

  EXPECT_FALSE(session_->IsStreamActive(3));
  EXPECT_FALSE(spdy_stream2);
  EXPECT_TRUE(session_->IsStreamActive(1));
  EXPECT_TRUE(session_->IsGoingAway());

  // Read and process the second GOAWAY frame, which should close the
  // session.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Have a session with active streams receive a GOAWAY frame and then
// close it. It should handle the close properly (i.e., not try to
// make itself unavailable in its pool twice).
TEST_F(SpdySessionTest, GoAwayWithActiveStreamsThenClose) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), CreateMockRead(goaway, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4), MockRead(ASYNC, 0, 5)  // EOF
  };
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  quiche::HttpHeaderBlock headers2(headers.Clone());

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Read and process the GOAWAY frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));

  EXPECT_FALSE(session_->IsStreamActive(3));
  EXPECT_FALSE(spdy_stream2);
  EXPECT_TRUE(session_->IsStreamActive(1));
  EXPECT_TRUE(session_->IsGoingAway());

  session_->CloseSessionOnError(ERR_ABORTED, "Aborting session");
  EXPECT_FALSE(spdy_stream1);

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Process a joint read buffer which causes the session to begin draining, and
// then processes a GOAWAY. The session should gracefully drain. Regression test
// for crbug.com/379469
TEST_F(SpdySessionTest, GoAwayWhileDraining) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  size_t joint_size = goaway.size() * 2 + body.size();

  // Compose interleaved |goaway| and |body| frames into a single read.
  auto buffer = std::make_unique<char[]>(joint_size);
  {
    size_t out = 0;
    memcpy(&buffer[out], goaway.data(), goaway.size());
    out += goaway.size();
    memcpy(&buffer[out], body.data(), body.size());
    out += body.size();
    memcpy(&buffer[out], goaway.data(), goaway.size());
    out += goaway.size();
    ASSERT_EQ(out, joint_size);
  }
  spdy::SpdySerializedFrame joint_frames(
      spdy::test::MakeSerializedFrame(buffer.get(), joint_size));

  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(joint_frames, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  // Stream and session closed gracefully.
  EXPECT_TRUE(delegate.StreamIsClosed());
  EXPECT_THAT(delegate.WaitForClose(), IsOk());
  EXPECT_EQ(kUploadData, delegate.TakeReceivedData());
  EXPECT_FALSE(session_);
}

// Regression test for https://crbug.com/1510327.
// Have a session with active streams receive a GOAWAY frame. Ensure that
// the session is drained after all streams receive DATA frames of which
// END_STREAM flag is set, even when the peer doesn't close the connection.
TEST_F(SpdySessionTest, GoAwayWithActiveStreamsThenEndStreams) {
  const int kStreamId1 = 1;
  const int kStreamId2 = 3;

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, kStreamId1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, kStreamId2, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 1),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, kStreamId1));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, kStreamId2));

  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(kStreamId1, true));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(kStreamId2, true));

  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(kStreamId2));

  MockRead reads[] = {
      CreateMockRead(resp1, 2),           CreateMockRead(resp2, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // (1)
      CreateMockRead(goaway, 5),          CreateMockRead(body1, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7),  // (2)
      CreateMockRead(body2, 8),
      // No EOF.
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  quiche::HttpHeaderBlock headers2(headers.Clone());

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  // (1) Read and process the GOAWAY frame and the response for kStreamId1.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(spdy_stream1);
  EXPECT_TRUE(spdy_stream2);

  // (2) Read and process the response for kStreamId2.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(spdy_stream1);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());

  EXPECT_FALSE(session_);
}

// Try to create a stream after receiving a GOAWAY frame. It should
// fail.
TEST_F(SpdySessionTest, CreateStreamAfterGoAway) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(goaway, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3), MockRead(ASYNC, 0, 4)  // EOF
  };
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream->stream_id());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Read and process the GOAWAY frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_TRUE(session_->IsStreamActive(1));

  SpdyStreamRequest stream_request;
  int rv = stream_request.StartRequest(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, false, MEDIUM,
      SocketTag(), NetLogWithSource(), CompletionOnceCallback(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_FAILED));

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Receiving a HEADERS frame after a GOAWAY frame should result in
// the stream being refused.
TEST_F(SpdySessionTest, HeadersAfterGoAway) {
  spdy::SpdySerializedFrame goaway_received(spdy_util_.ConstructSpdyGoAway(1));
  spdy::SpdySerializedFrame push(spdy_util_.ConstructSpdyPushPromise(1, 2, {}));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(goaway_received, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3), CreateMockRead(push, 4),
      MockRead(ASYNC, 0, 6)  // EOF
  };
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame goaway_sent(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR, "PUSH_PROMISE received"));
  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(goaway_sent, 5)};
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream->stream_id());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Read and process the GOAWAY frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_TRUE(session_->IsStreamActive(1));

  // Read and process the HEADERS frame, the subsequent RST_STREAM,
  // and EOF.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// A session observing a network change with active streams should close
// when the last active stream is closed.
TEST_F(SpdySessionTest, NetworkChangeWithActiveStreams) {
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), MockRead(ASYNC, 0, 2)  // EOF
  };
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));

  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream->stream_id());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  spdy_session_pool_->OnIPAddressChanged();

  // The SpdySessionPool behavior differs based on how the OSs reacts to
  // network changes; see comment in SpdySessionPool::OnIPAddressChanged().
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  // For OSs where the TCP connections will close upon relevant network
  // changes, SpdySessionPool doesn't need to force them to close, so in these
  // cases verify the session has become unavailable but remains open and the
  // pre-existing stream is still active.
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));

  EXPECT_TRUE(session_->IsGoingAway());

  EXPECT_TRUE(session_->IsStreamActive(1));

  // Should close the session.
  spdy_stream->Close();
#endif
  EXPECT_FALSE(spdy_stream);

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTestWithMockTime, ClientPing) {
  session_deps_.enable_ping = true;

  spdy::SpdySerializedFrame read_ping(spdy_util_.ConstructSpdyPing(1, true));
  MockRead reads[] = {
      CreateMockRead(read_ping, 1), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };
  spdy::SpdySerializedFrame write_ping(spdy_util_.ConstructSpdyPing(1, false));
  MockWrite writes[] = {
      CreateMockWrite(write_ping, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  TestNetworkQualityEstimator estimator;

  spdy_session_pool_->set_network_quality_estimator(&estimator);

  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, "");
  spdy_stream1->SetDelegate(&delegate);

  base::TimeTicks before_ping_time = base::TimeTicks::Now();

  // Negative value means a preface ping will always be sent.
  set_connection_at_risk_of_loss_time(base::Seconds(-1));

  // Send a PING frame.  This posts CheckPingStatus() with delay.
  MaybeSendPrefacePing();

  EXPECT_TRUE(ping_in_flight());
  EXPECT_EQ(2u, next_ping_id());
  EXPECT_TRUE(check_ping_status_pending());

  // MaybeSendPrefacePing() should not send another PING frame if there is
  // already one in flight.
  MaybeSendPrefacePing();

  EXPECT_TRUE(ping_in_flight());
  EXPECT_EQ(2u, next_ping_id());
  EXPECT_TRUE(check_ping_status_pending());

  // Run posted CheckPingStatus() task.
  FastForwardUntilNoTasksRemain();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ping_in_flight());
  EXPECT_EQ(2u, next_ping_id());
  EXPECT_FALSE(check_ping_status_pending());
  EXPECT_GE(last_read_time(), before_ping_time);

  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(MainThreadIsIdle());
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);
  EXPECT_FALSE(spdy_stream1);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());

  EXPECT_LE(1u, estimator.ping_rtt_received_count());
}

TEST_F(SpdySessionTest, ServerPing) {
  spdy::SpdySerializedFrame read_ping(spdy_util_.ConstructSpdyPing(2, false));
  MockRead reads[] = {
      CreateMockRead(read_ping), MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  spdy::SpdySerializedFrame write_ping(spdy_util_.ConstructSpdyPing(2, true));
  MockWrite writes[] = {
      CreateMockWrite(write_ping),
  };
  StaticSocketDataProvider data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, "");
  spdy_stream1->SetDelegate(&delegate);

  // Flush the read completion task.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));

  EXPECT_FALSE(session_);
  EXPECT_FALSE(spdy_stream1);
}

// Cause a ping to be sent out while producing a write. The write loop
// should handle this properly, i.e. another DoWriteLoop task should
// not be posted. This is a regression test for
// http://crbug.com/261043 .
TEST_F(SpdySessionTest, PingAndWriteLoop) {
  session_deps_.enable_ping = true;
  session_deps_.time_func = TheNearFuture;

  spdy::SpdySerializedFrame write_ping(spdy_util_.ConstructSpdyPing(1, false));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(write_ping, 1),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  // Shift time so that a ping will be sent out.
  g_time_delta = base::Seconds(11);

  base::RunLoop().RunUntilIdle();
  session_->CloseSessionOnError(ERR_ABORTED, "Aborting");

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTestWithMockTime, DetectBrokenConnectionPing) {
  session_deps_.enable_ping = true;

  spdy::SpdySerializedFrame read_ping1(spdy_util_.ConstructSpdyPing(1, true));
  spdy::SpdySerializedFrame read_ping2(spdy_util_.ConstructSpdyPing(2, true));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      CreateMockRead(read_ping1, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 5),
      CreateMockRead(read_ping2, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7),
      MockRead(ASYNC, 0, 8)  // EOF
  };
  spdy::SpdySerializedFrame write_ping1(spdy_util_.ConstructSpdyPing(1, false));
  spdy::SpdySerializedFrame write_ping2(spdy_util_.ConstructSpdyPing(2, false));
  MockWrite writes[] = {CreateMockWrite(write_ping1, 0),
                        CreateMockWrite(write_ping2, 4)};
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  TestNetworkQualityEstimator estimator;

  spdy_session_pool_->set_network_quality_estimator(&estimator);

  CreateSpdySession();

  constexpr base::TimeDelta kHeartbeatInterval = base::Seconds(15);
  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  ASSERT_TRUE(spdy_stream1);
  ASSERT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  test::StreamDelegateSendImmediate delegate(spdy_stream1, "");
  spdy_stream1->SetDelegate(&delegate);

  // Negative value means a preface ping will always be sent.
  set_connection_at_risk_of_loss_time(base::Seconds(-1));

  // Initially there should be no PING in flight or check pending.
  EXPECT_FALSE(ping_in_flight());
  EXPECT_FALSE(check_ping_status_pending());
  // After kHeartbeatInterval time has passed the first PING should be in flight
  // and its status check pending.
  FastForwardBy(kHeartbeatInterval);
  EXPECT_TRUE(ping_in_flight());
  EXPECT_TRUE(check_ping_status_pending());

  // Consume the PING ack.
  data.Resume();
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(ping_in_flight());
  EXPECT_TRUE(check_ping_status_pending());
  // Consume the pending check_ping_status callback, we should be back to the
  // starting state.
  FastForwardBy(NextMainThreadPendingTaskDelay());
  EXPECT_FALSE(ping_in_flight());
  EXPECT_FALSE(check_ping_status_pending());

  // Unblock data and trigger the next heartbeat.
  data.Resume();
  FastForwardBy(NextMainThreadPendingTaskDelay());
  EXPECT_TRUE(ping_in_flight());
  EXPECT_TRUE(check_ping_status_pending());

  // Consume PING ack and check_ping_status callback.
  data.Resume();
  FastForwardBy(NextMainThreadPendingTaskDelay());
  EXPECT_FALSE(ping_in_flight());
  EXPECT_FALSE(check_ping_status_pending());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(MainThreadIsIdle());
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);
  EXPECT_FALSE(spdy_stream1);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());

  EXPECT_EQ(2u, estimator.ping_rtt_received_count());
}

TEST_F(SpdySessionTest, StreamIdSpaceExhausted) {
  // Test setup: |stream_hi_water_mark_| and |max_concurrent_streams_| are
  // fixed to allow for two stream ID assignments, and three concurrent
  // streams. Four streams are started, and two are activated. Verify the
  // session goes away, and that the created (but not activated) and
  // stalled streams are aborted. Also verify the activated streams complete,
  // at which point the session closes.

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, kLastStreamId - 2, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, kLastStreamId, MEDIUM));

  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, kLastStreamId - 2));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, kLastStreamId));

  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(kLastStreamId - 2, true));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(kLastStreamId, true));

  MockRead reads[] = {
      CreateMockRead(resp1, 2),           CreateMockRead(resp2, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4), CreateMockRead(body1, 5),
      CreateMockRead(body2, 6),           MockRead(ASYNC, 0, 7)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Fix stream_hi_water_mark_ to allow for two stream activations.
  set_stream_hi_water_mark(kLastStreamId - 2);
  // Fix max_concurrent_streams to allow for three stream creations.
  set_max_concurrent_streams(3);

  // Create three streams synchronously, and begin a fourth (which is stalled).
  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(stream1);
  stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate2(stream2);
  stream2->SetDelegate(&delegate2);

  base::WeakPtr<SpdyStream> stream3 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate3(stream3);
  stream3->SetDelegate(&delegate3);

  SpdyStreamRequest request4;
  TestCompletionCallback callback4;
  EXPECT_EQ(ERR_IO_PENDING,
            request4.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                  test_url_, false, MEDIUM, SocketTag(),
                                  NetLogWithSource(), callback4.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  // Streams 1-3 were created. 4th is stalled. No streams are active yet.
  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(3u, num_created_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(MEDIUM));

  // Activate stream 1. One ID remains available.
  stream1->SendRequestHeaders(spdy_util_.ConstructGetHeaderBlock(kDefaultUrl),
                              NO_MORE_DATA_TO_SEND);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kLastStreamId - 2u, stream1->stream_id());
  EXPECT_EQ(1u, num_active_streams());
  EXPECT_EQ(2u, num_created_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(MEDIUM));

  // Activate stream 2. ID space is exhausted.
  stream2->SendRequestHeaders(spdy_util_.ConstructGetHeaderBlock(kDefaultUrl),
                              NO_MORE_DATA_TO_SEND);
  base::RunLoop().RunUntilIdle();

  // Active streams remain active.
  EXPECT_EQ(kLastStreamId, stream2->stream_id());
  EXPECT_EQ(2u, num_active_streams());

  // Session is going away. Created and stalled streams were aborted.
  EXPECT_TRUE(session_->IsGoingAway());
  EXPECT_THAT(delegate3.WaitForClose(), IsError(ERR_HTTP2_PROTOCOL_ERROR));
  EXPECT_THAT(callback4.WaitForResult(), IsError(ERR_HTTP2_PROTOCOL_ERROR));
  EXPECT_EQ(0u, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(MEDIUM));

  // Read responses on remaining active streams.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate1.WaitForClose(), IsOk());
  EXPECT_EQ(kUploadData, delegate1.TakeReceivedData());
  EXPECT_THAT(delegate2.WaitForClose(), IsOk());
  EXPECT_EQ(kUploadData, delegate2.TakeReceivedData());

  // Session was destroyed.
  EXPECT_FALSE(session_);
}

// Regression test for https://crbug.com/481009.
TEST_F(SpdySessionTest, MaxConcurrentStreamsZero) {

  // Receive SETTINGS frame that sets max_concurrent_streams to zero.
  spdy::SettingsMap settings_zero;
  settings_zero[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = 0;
  spdy::SpdySerializedFrame settings_frame_zero(
      spdy_util_.ConstructSpdySettings(settings_zero));

  // Acknowledge it.
  spdy::SpdySerializedFrame settings_ack0(
      spdy_util_.ConstructSpdySettingsAck());

  // Receive SETTINGS frame that sets max_concurrent_streams to one.
  spdy::SettingsMap settings_one;
  settings_one[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = 1;
  spdy::SpdySerializedFrame settings_frame_one(
      spdy_util_.ConstructSpdySettings(settings_one));

  // Acknowledge it.
  spdy::SpdySerializedFrame settings_ack1(
      spdy_util_.ConstructSpdySettingsAck());

  // Request and response.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {CreateMockRead(settings_frame_zero, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      CreateMockRead(settings_frame_one, 3),
                      CreateMockRead(resp, 6),
                      CreateMockRead(body, 7),
                      MockRead(ASYNC, 0, 8)};

  MockWrite writes[] = {CreateMockWrite(settings_ack0, 1),
                        CreateMockWrite(settings_ack1, 4),
                        CreateMockWrite(req, 5)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  // Create session.
  CreateNetworkSession();
  CreateSpdySession();

  // Receive SETTINGS frame that sets max_concurrent_streams to zero.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, max_concurrent_streams());

  // Start request.
  SpdyStreamRequest request;
  TestCompletionCallback callback;
  int rv =
      request.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_,
                           false, MEDIUM, SocketTag(), NetLogWithSource(),
                           callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Stream is stalled.
  EXPECT_EQ(1u, pending_create_stream_queue_size(MEDIUM));
  EXPECT_EQ(0u, num_created_streams());

  // Receive SETTINGS frame that sets max_concurrent_streams to one.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, max_concurrent_streams());

  // Stream is created.
  EXPECT_EQ(0u, pending_create_stream_queue_size(MEDIUM));
  EXPECT_EQ(1u, num_created_streams());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Send request.
  base::WeakPtr<SpdyStream> stream = request.ReleaseStream();
  test::StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);
  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  EXPECT_THAT(delegate.WaitForClose(), IsOk());
  EXPECT_EQ("hello!", delegate.TakeReceivedData());

  // Finish async network reads/writes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());

  // Session is destroyed.
  EXPECT_FALSE(session_);
}

// Verifies that an unstalled pending stream creation racing with a new stream
// creation doesn't violate the maximum stream concurrency. Regression test for
// crbug.com/373858.
TEST_F(SpdySessionTest, UnstallRacesWithStreamCreation) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Fix max_concurrent_streams to allow for one open stream.
  set_max_concurrent_streams(1);

  // Create two streams: one synchronously, and one which stalls.
  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());

  SpdyStreamRequest request2;
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            request2.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                  test_url_, false, MEDIUM, SocketTag(),
                                  NetLogWithSource(), callback2.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(MEDIUM));

  // Cancel the first stream. A callback to unstall the second stream was
  // posted. Don't run it yet.
  stream1->Cancel(ERR_ABORTED);

  EXPECT_EQ(0u, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(MEDIUM));

  // Create a third stream prior to the second stream's callback.
  base::WeakPtr<SpdyStream> stream3 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());

  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(MEDIUM));

  // Now run the message loop. The unstalled stream will re-stall itself.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(MEDIUM));

  // Cancel the third stream and run the message loop. Verify that the second
  // stream creation now completes.
  stream3->Cancel(ERR_ABORTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(MEDIUM));
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
}

TEST_F(SpdySessionTestWithMockTime, FailedPing) {
  session_deps_.enable_ping = true;
  session_deps_.time_func = TheNearFuture;

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};  // Stall forever.
  spdy::SpdySerializedFrame write_ping(spdy_util_.ConstructSpdyPing(1, false));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR, "Failed ping."));
  MockWrite writes[] = {CreateMockWrite(write_ping), CreateMockWrite(goaway)};

  StaticSocketDataProvider data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, "");
  spdy_stream1->SetDelegate(&delegate);

  // Negative value means a preface ping will always be sent.
  set_connection_at_risk_of_loss_time(base::Seconds(-1));

  // Send a PING frame.  This posts CheckPingStatus() with delay.
  MaybeSendPrefacePing();
  EXPECT_TRUE(ping_in_flight());
  EXPECT_EQ(2u, next_ping_id());
  EXPECT_TRUE(check_ping_status_pending());

  // Assert session is not closed.
  EXPECT_TRUE(session_->IsAvailable());
  EXPECT_LT(0u, num_active_streams() + num_created_streams());
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Run CheckPingStatus() and make it believe hung_interval has passed.
  g_time_delta = base::Seconds(15);
  FastForwardUntilNoTasksRemain();
  base::RunLoop().RunUntilIdle();

  // Since no response to PING has been received,
  // CheckPingStatus() closes the connection.
  EXPECT_TRUE(MainThreadIsIdle());
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);
  EXPECT_FALSE(spdy_stream1);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Regression test for https://crbug.com/784975.
TEST_F(SpdySessionTestWithMockTime, NoPingSentWhenCheckPingPending) {
  session_deps_.enable_ping = true;
  session_deps_.time_func = TheNearFuture;

  spdy::SpdySerializedFrame read_ping(spdy_util_.ConstructSpdyPing(1, true));
  MockRead reads[] = {CreateMockRead(read_ping, 1),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};

  spdy::SpdySerializedFrame write_ping0(spdy_util_.ConstructSpdyPing(1, false));
  MockWrite writes[] = {CreateMockWrite(write_ping0, 0)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Negative value means a preface ping will always be sent.
  set_connection_at_risk_of_loss_time(base::Seconds(-1));

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, "");
  spdy_stream1->SetDelegate(&delegate);

  EXPECT_FALSE(ping_in_flight());
  EXPECT_EQ(1u, next_ping_id());
  EXPECT_FALSE(check_ping_status_pending());

  // Send preface ping and post CheckPingStatus() task with delay.
  MaybeSendPrefacePing();

  EXPECT_TRUE(ping_in_flight());
  EXPECT_EQ(2u, next_ping_id());
  EXPECT_TRUE(check_ping_status_pending());

  // Read PING ACK.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ping_in_flight());
  EXPECT_TRUE(check_ping_status_pending());

  // Fast forward mock time so that normally another ping would be sent out.
  // However, since CheckPingStatus() is still pending, no new ping is sent.
  g_time_delta = base::Seconds(15);
  MaybeSendPrefacePing();

  EXPECT_FALSE(ping_in_flight());
  EXPECT_EQ(2u, next_ping_id());
  EXPECT_TRUE(check_ping_status_pending());

  // Run CheckPingStatus().
  FastForwardUntilNoTasksRemain();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(check_ping_status_pending());

  // Read EOF.
  data.Resume();
  EXPECT_THAT(delegate.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  // Finish going away.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Request kInitialMaxConcurrentStreams + 1 streams.  Receive a
// settings frame increasing the max concurrent streams by 1.  Make
// sure nothing blows up. This is a regression test for
// http://crbug.com/57331 .
TEST_F(SpdySessionTest, OnSettings) {
  const spdy::SpdySettingsId kSpdySettingsId =
      spdy::SETTINGS_MAX_CONCURRENT_STREAMS;

  spdy::SettingsMap new_settings;
  const uint32_t max_concurrent_streams = kInitialMaxConcurrentStreams + 1;
  new_settings[kSpdySettingsId] = max_concurrent_streams;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));
  MockRead reads[] = {
      CreateMockRead(settings_frame, 0), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),
  };

  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {CreateMockWrite(settings_ack, 1)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Create the maximum number of concurrent streams.
  for (size_t i = 0; i < kInitialMaxConcurrentStreams; ++i) {
    base::WeakPtr<SpdyStream> spdy_stream =
        CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, MEDIUM, NetLogWithSource());
    ASSERT_TRUE(spdy_stream);
  }

  StreamReleaserCallback stream_releaser;
  SpdyStreamRequest request;
  ASSERT_EQ(ERR_IO_PENDING,
            request.StartRequest(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                 false, MEDIUM, SocketTag(), NetLogWithSource(),
                                 stream_releaser.MakeCallback(&request),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(stream_releaser.WaitForResult(), IsOk());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Create one more stream than maximum number of concurrent streams,
// so that one of them is pending.  Cancel one stream, which should trigger the
// creation of the pending stream.  Then cancel that one immediately as well,
// and make sure this does not lead to a crash.
// This is a regression test for https://crbug.com/63532.
TEST_F(SpdySessionTest, CancelPendingCreateStream) {
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Leave room for only one more stream to be created.
  for (size_t i = 0; i < kInitialMaxConcurrentStreams - 1; ++i) {
    base::WeakPtr<SpdyStream> spdy_stream =
        CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, MEDIUM, NetLogWithSource());
    ASSERT_TRUE(spdy_stream);
  }

  // Create 2 more streams.  First will succeed.  Second will be pending.
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);

  // Use unique_ptr to let us invalidate the memory when we want to, to trigger
  // an error in memory corruption detectors if the callback is invoked when
  // it's not supposed to be.
  auto callback = std::make_unique<TestCompletionCallback>();

  SpdyStreamRequest request;
  ASSERT_THAT(
      request.StartRequest(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                           false, MEDIUM, SocketTag(), NetLogWithSource(),
                           callback->callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      IsError(ERR_IO_PENDING));

  // Release the first one, this will allow the second to be created.
  spdy_stream1->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream1);

  request.CancelRequest();
  callback.reset();

  // Should not crash when running the pending callback.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SpdySessionTest, ChangeStreamRequestPriority) {
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  set_max_concurrent_streams(1);

  TestCompletionCallback callback1;
  SpdyStreamRequest request1;
  ASSERT_EQ(OK, request1.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                      test_url_, false, LOWEST, SocketTag(),
                                      NetLogWithSource(), callback1.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  TestCompletionCallback callback2;
  SpdyStreamRequest request2;
  ASSERT_EQ(ERR_IO_PENDING,
            request2.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                  test_url_, false, LOWEST, SocketTag(),
                                  NetLogWithSource(), callback2.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  request1.SetPriority(HIGHEST);
  request2.SetPriority(MEDIUM);

  ASSERT_EQ(0u, pending_create_stream_queue_size(HIGHEST));
  // Priority of queued request is changed.
  ASSERT_EQ(1u, pending_create_stream_queue_size(MEDIUM));
  ASSERT_EQ(0u, pending_create_stream_queue_size(LOWEST));

  base::WeakPtr<SpdyStream> stream1 = request1.ReleaseStream();
  // Priority of stream is updated if request has been fulfilled.
  ASSERT_EQ(HIGHEST, stream1->priority());
}

// Attempts to extract a NetLogSource from a set of event parameters.  Returns
// true and writes the result to |source| on success.  Returns false and
// makes |source| an invalid source on failure.
bool NetLogSourceFromEventParameters(const base::Value::Dict* event_params,
                                     NetLogSource* source) {
  const base::Value::Dict* source_dict = nullptr;
  int source_id = -1;
  int source_type = static_cast<int>(NetLogSourceType::COUNT);
  if (!event_params) {
    *source = NetLogSource();
    return false;
  }
  source_dict = event_params->FindDict("source_dependency");
  if (!source_dict) {
    *source = NetLogSource();
    return false;
  }
  std::optional<int> opt_int;
  opt_int = source_dict->FindInt("id");
  if (!opt_int) {
    *source = NetLogSource();
    return false;
  }
  source_id = opt_int.value();
  opt_int = source_dict->FindInt("type");
  if (!opt_int) {
    *source = NetLogSource();
    return false;
  }
  source_type = opt_int.value();

  DCHECK_GE(source_id, 0);
  DCHECK_LT(source_type, static_cast<int>(NetLogSourceType::COUNT));
  *source = NetLogSource(static_cast<NetLogSourceType>(source_type), source_id);
  return true;
}

TEST_F(SpdySessionTest, Initialize) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Flush the read completion task.
  base::RunLoop().RunUntilIdle();

  auto entries = net_log_observer_.GetEntries();
  EXPECT_LT(0u, entries.size());

  // Check that we logged HTTP2_SESSION_INITIALIZED correctly.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP2_SESSION_INITIALIZED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  NetLogSource socket_source;
  EXPECT_TRUE(
      NetLogSourceFromEventParameters(&entries[pos].params, &socket_source));
  EXPECT_TRUE(socket_source.IsValid());
  EXPECT_NE(net_log_with_source_.source().id, socket_source.id);
}

TEST_F(SpdySessionTest, NetLogOnSessionGoaway) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      42, spdy::ERROR_CODE_ENHANCE_YOUR_CALM, "foo"));
  MockRead reads[] = {
      CreateMockRead(goaway), MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Flush the read completion task.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);

  // Check that the NetLog was filled reasonably.
  auto entries = net_log_observer_.GetEntries();
  EXPECT_LT(0u, entries.size());

  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP2_SESSION_RECV_GOAWAY,
      NetLogEventPhase::NONE);
  ASSERT_EQ(0, GetIntegerValueFromParams(entries[pos], "active_streams"));
  ASSERT_EQ("11 (ENHANCE_YOUR_CALM)",
            GetStringValueFromParams(entries[pos], "error_code"));
  ASSERT_EQ("foo", GetStringValueFromParams(entries[pos], "debug_data"));

  // Check that we logged SPDY_SESSION_CLOSE correctly.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP2_SESSION_CLOSE, NetLogEventPhase::NONE);
  EXPECT_THAT(GetNetErrorCodeFromParams(entries[pos]), IsOk());
}

TEST_F(SpdySessionTest, NetLogOnSessionEOF) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Flush the read completion task.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));
  EXPECT_FALSE(session_);

  // Check that the NetLog was filled reasonably.
  auto entries = net_log_observer_.GetEntries();
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_SESSION_CLOSE correctly.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP2_SESSION_CLOSE, NetLogEventPhase::NONE);

  if (pos < static_cast<int>(entries.size())) {
    ASSERT_THAT(GetNetErrorCodeFromParams(entries[pos]),
                IsError(ERR_CONNECTION_CLOSED));
  } else {
    ADD_FAILURE();
  }
}

TEST_F(SpdySessionTest, HeadersCompressionHistograms) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), MockRead(ASYNC, 0, 2)  // EOF
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  // Write request headers & capture resulting histogram update.
  base::HistogramTester histogram_tester;

  base::RunLoop().RunUntilIdle();
  // Regression test of compression performance under the request fixture.
  histogram_tester.ExpectBucketCount("Net.SpdyHeadersCompressionPercentage", 76,
                                     1);

  // Read and process EOF.
  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Queue up a low-priority HEADERS followed by a high-priority
// one. The high priority one should still send first and receive
// first.
TEST_F(SpdySessionTest, OutOfOrderHeaders) {
  // Construct the request.
  spdy::SpdySerializedFrame req_highest(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame req_lowest(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req_highest, 0), CreateMockWrite(req_lowest, 1),
  };

  spdy::SpdySerializedFrame resp_highest(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body_highest(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp_lowest(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body_lowest(
      spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {
      CreateMockRead(resp_highest, 2), CreateMockRead(body_highest, 3),
      CreateMockRead(resp_lowest, 4), CreateMockRead(body_lowest, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream_lowest =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream_lowest);
  EXPECT_EQ(0u, spdy_stream_lowest->stream_id());
  test::StreamDelegateDoNothing delegate_lowest(spdy_stream_lowest);
  spdy_stream_lowest->SetDelegate(&delegate_lowest);

  base::WeakPtr<SpdyStream> spdy_stream_highest =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, HIGHEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream_highest);
  EXPECT_EQ(0u, spdy_stream_highest->stream_id());
  test::StreamDelegateDoNothing delegate_highest(spdy_stream_highest);
  spdy_stream_highest->SetDelegate(&delegate_highest);

  // Queue the lower priority one first.

  quiche::HttpHeaderBlock headers_lowest(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream_lowest->SendRequestHeaders(std::move(headers_lowest),
                                         NO_MORE_DATA_TO_SEND);

  quiche::HttpHeaderBlock headers_highest(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream_highest->SendRequestHeaders(std::move(headers_highest),
                                          NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(spdy_stream_lowest);
  EXPECT_FALSE(spdy_stream_highest);
  EXPECT_EQ(3u, delegate_lowest.stream_id());
  EXPECT_EQ(1u, delegate_highest.stream_id());
}

TEST_F(SpdySessionTest, CancelStream) {
  // Request 1, at HIGHEST priority, will be cancelled before it writes data.
  // Request 2, at LOWEST priority, will be a full request and will be id 1.
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req2, 0),
  };

  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp2, 1), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(body2, 3), MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, HIGHEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);
  EXPECT_EQ(0u, spdy_stream2->stream_id());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  EXPECT_EQ(0u, spdy_stream1->stream_id());

  spdy_stream1->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream1);

  EXPECT_EQ(0u, delegate1.stream_id());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, delegate1.stream_id());
  EXPECT_EQ(1u, delegate2.stream_id());

  spdy_stream2->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream2);
}

// Create two streams that are set to re-close themselves on close,
// and then close the session. Nothing should blow up. Also a
// regression test for http://crbug.com/139518 .
TEST_F(SpdySessionTest, CloseSessionWithTwoCreatedSelfClosingStreams) {
  // No actual data will be sent.
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, 1)  // EOF
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                HIGHEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  test::ClosingDelegate delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  test::ClosingDelegate delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session_->CloseSessionOnError(ERR_ABORTED, std::string());

  EXPECT_FALSE(spdy_stream1);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Create two streams that are set to close each other on close, and
// then close the session. Nothing should blow up.
TEST_F(SpdySessionTest, CloseSessionWithTwoCreatedMutuallyClosingStreams) {
  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                HIGHEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Make |spdy_stream1| close |spdy_stream2|.
  test::ClosingDelegate delegate1(spdy_stream2);
  spdy_stream1->SetDelegate(&delegate1);

  // Make |spdy_stream2| close |spdy_stream1|.
  test::ClosingDelegate delegate2(spdy_stream1);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session_->CloseSessionOnError(ERR_ABORTED, std::string());

  EXPECT_FALSE(spdy_stream1);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Create two streams that are set to re-close themselves on close,
// activate them, and then close the session. Nothing should blow up.
TEST_F(SpdySessionTest, CloseSessionWithTwoActivatedSelfClosingStreams) {
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  test::ClosingDelegate delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  test::ClosingDelegate delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session_->CloseSessionOnError(ERR_ABORTED, std::string());

  EXPECT_FALSE(spdy_stream1);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Create two streams that are set to close each other on close,
// activate them, and then close the session. Nothing should blow up.
TEST_F(SpdySessionTest, CloseSessionWithTwoActivatedMutuallyClosingStreams) {
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Make |spdy_stream1| close |spdy_stream2|.
  test::ClosingDelegate delegate1(spdy_stream2);
  spdy_stream1->SetDelegate(&delegate1);

  // Make |spdy_stream2| close |spdy_stream1|.
  test::ClosingDelegate delegate2(spdy_stream1);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session_->CloseSessionOnError(ERR_ABORTED, std::string());

  EXPECT_FALSE(spdy_stream1);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Delegate that closes a given session when the stream is closed.
class SessionClosingDelegate : public test::StreamDelegateDoNothing {
 public:
  SessionClosingDelegate(const base::WeakPtr<SpdyStream>& stream,
                         const base::WeakPtr<SpdySession>& session_to_close)
      : StreamDelegateDoNothing(stream),
        session_to_close_(session_to_close) {}

  ~SessionClosingDelegate() override = default;

  void OnClose(int status) override {
    session_to_close_->CloseSessionOnError(ERR_HTTP2_PROTOCOL_ERROR, "Error");
  }

 private:
  base::WeakPtr<SpdySession> session_to_close_;
};

// Close an activated stream that closes its session. Nothing should
// blow up. This is a regression test for https://crbug.com/263691.
TEST_F(SpdySessionTest, CloseActivatedStreamThatClosesSession) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR, "Error"));
  // The GOAWAY has higher-priority than the RST_STREAM, and is written first
  // despite being queued second.
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(goaway, 1),
      CreateMockWrite(rst, 3),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 0, 2)  // EOF
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream);
  EXPECT_EQ(0u, spdy_stream->stream_id());

  SessionClosingDelegate delegate(spdy_stream, session_);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  EXPECT_EQ(0u, spdy_stream->stream_id());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream->stream_id());

  // Ensure we don't crash while closing the stream (which closes the
  // session).
  spdy_stream->Cancel(ERR_ABORTED);

  EXPECT_FALSE(spdy_stream);
  EXPECT_TRUE(delegate.StreamIsClosed());

  // Write the RST_STREAM & GOAWAY.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(SpdySessionTest, VerifyDomainAuthentication) {
  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_TRUE(session_->VerifyDomainAuthentication("www.example.org"));
  EXPECT_TRUE(session_->VerifyDomainAuthentication("mail.example.org"));
  EXPECT_TRUE(session_->VerifyDomainAuthentication("mail.example.com"));
  EXPECT_FALSE(session_->VerifyDomainAuthentication("mail.google.com"));
}

TEST_F(SpdySessionTest, CloseTwoStalledCreateStream) {
  // TODO(rtenneti): Define a helper class/methods and move the common code in
  // this file.
  spdy::SettingsMap new_settings;
  const spdy::SpdySettingsId kSpdySettingsId1 =
      spdy::SETTINGS_MAX_CONCURRENT_STREAMS;
  const uint32_t max_concurrent_streams = 1;
  new_settings[kSpdySettingsId1] = max_concurrent_streams;

  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy_util_.UpdateWithStreamDestruction(3);
  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 5, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(settings_ack, 1), CreateMockWrite(req1, 2),
      CreateMockWrite(req2, 5), CreateMockWrite(req3, 8),
  };

  // Set up the socket so we read a SETTINGS frame that sets max concurrent
  // streams to 1.
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));

  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));

  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 5));
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(5, true));

  MockRead reads[] = {
      CreateMockRead(settings_frame, 0),
      CreateMockRead(resp1, 3),
      CreateMockRead(body1, 4),
      CreateMockRead(resp2, 6),
      CreateMockRead(body2, 7),
      CreateMockRead(resp3, 9),
      CreateMockRead(body3, 10),
      MockRead(ASYNC, ERR_IO_PENDING, 11),
      MockRead(ASYNC, 0, 12)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Read the settings frame.
  base::RunLoop().RunUntilIdle();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  TestCompletionCallback callback2;
  SpdyStreamRequest request2;
  ASSERT_EQ(ERR_IO_PENDING,
            request2.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                  test_url_, false, LOWEST, SocketTag(),
                                  NetLogWithSource(), callback2.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback3;
  SpdyStreamRequest request3;
  ASSERT_EQ(ERR_IO_PENDING,
            request3.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                  test_url_, false, LOWEST, SocketTag(),
                                  NetLogWithSource(), callback3.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(2u, pending_create_stream_queue_size(LOWEST));

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  // Run until 1st stream is activated and then closed.
  EXPECT_EQ(0u, delegate1.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream1);
  EXPECT_EQ(1u, delegate1.stream_id());

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(LOWEST));

  // Pump loop for SpdySession::ProcessPendingStreamRequests() to
  // create the 2nd stream.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(LOWEST));

  base::WeakPtr<SpdyStream> stream2 = request2.ReleaseStream();
  test::StreamDelegateDoNothing delegate2(stream2);
  stream2->SetDelegate(&delegate2);
  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  // Run until 2nd stream is activated and then closed.
  EXPECT_EQ(0u, delegate2.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(stream2);
  EXPECT_EQ(3u, delegate2.stream_id());

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(LOWEST));

  // Pump loop for SpdySession::ProcessPendingStreamRequests() to
  // create the 3rd stream.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(1u, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(LOWEST));

  base::WeakPtr<SpdyStream> stream3 = request3.ReleaseStream();
  test::StreamDelegateDoNothing delegate3(stream3);
  stream3->SetDelegate(&delegate3);
  quiche::HttpHeaderBlock headers3(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  stream3->SendRequestHeaders(std::move(headers3), NO_MORE_DATA_TO_SEND);

  // Run until 2nd stream is activated and then closed.
  EXPECT_EQ(0u, delegate3.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(stream3);
  EXPECT_EQ(5u, delegate3.stream_id());

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(0u, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(LOWEST));

  data.Resume();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SpdySessionTest, CancelTwoStalledCreateStream) {
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Leave room for only one more stream to be created.
  for (size_t i = 0; i < kInitialMaxConcurrentStreams - 1; ++i) {
    base::WeakPtr<SpdyStream> spdy_stream =
        CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, MEDIUM, NetLogWithSource());
    ASSERT_TRUE(spdy_stream);
  }

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  TestCompletionCallback callback2;
  SpdyStreamRequest request2;
  ASSERT_EQ(ERR_IO_PENDING,
            request2.StartRequest(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, false, LOWEST, SocketTag(),
                                  NetLogWithSource(), callback2.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback3;
  SpdyStreamRequest request3;
  ASSERT_EQ(ERR_IO_PENDING,
            request3.StartRequest(SPDY_BIDIRECTIONAL_STREAM, session_,
                                  test_url_, false, LOWEST, SocketTag(),
                                  NetLogWithSource(), callback3.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(kInitialMaxConcurrentStreams, num_created_streams());
  EXPECT_EQ(2u, pending_create_stream_queue_size(LOWEST));

  // Cancel the first stream; this will allow the second stream to be created.
  EXPECT_TRUE(spdy_stream1);
  spdy_stream1->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream1);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(kInitialMaxConcurrentStreams, num_created_streams());
  EXPECT_EQ(1u, pending_create_stream_queue_size(LOWEST));

  // Cancel the second stream; this will allow the third stream to be created.
  base::WeakPtr<SpdyStream> spdy_stream2 = request2.ReleaseStream();
  spdy_stream2->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(kInitialMaxConcurrentStreams, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(LOWEST));

  // Cancel the third stream.
  base::WeakPtr<SpdyStream> spdy_stream3 = request3.ReleaseStream();
  spdy_stream3->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream3);
  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(kInitialMaxConcurrentStreams - 1, num_created_streams());
  EXPECT_EQ(0u, pending_create_stream_queue_size(LOWEST));
}

// Test that SpdySession::DoReadLoop reads data from the socket
// without yielding.  This test makes 32k - 1 bytes of data available
// on the socket for reading. It then verifies that it has read all
// the available data without yielding.
TEST_F(SpdySessionTest, ReadDataWithoutYielding) {
  session_deps_.time_func = InstantaneousReads;

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };

  // Build buffer of size kYieldAfterBytesRead / 4
  // (-spdy_data_frame_size).
  ASSERT_EQ(32 * 1024, kYieldAfterBytesRead);
  const int kPayloadSize = kYieldAfterBytesRead / 4 - spdy::kFrameHeaderSize;
  TestDataStream test_stream;
  auto payload = base::MakeRefCounted<IOBufferWithSize>(kPayloadSize);
  char* payload_data = payload->data();
  test_stream.GetBytes(payload_data, kPayloadSize);

  spdy::SpdySerializedFrame partial_data_frame(
      spdy_util_.ConstructSpdyDataFrame(
          1, std::string_view(payload_data, kPayloadSize), /*fin=*/false));
  spdy::SpdySerializedFrame finish_data_frame(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(payload_data, kPayloadSize - 1), /*fin=*/true));

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  // Write 1 byte less than kMaxReadBytes to check that DoRead reads up to 32k
  // bytes.
  MockRead reads[] = {
      CreateMockRead(resp1, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(partial_data_frame, 3),
      CreateMockRead(partial_data_frame, 4, SYNCHRONOUS),
      CreateMockRead(partial_data_frame, 5, SYNCHRONOUS),
      CreateMockRead(finish_data_frame, 6, SYNCHRONOUS),
      MockRead(ASYNC, 0, 7)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  // Set up the TaskObserver to verify SpdySession::DoReadLoop doesn't
  // post a task.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoReadLoop");

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify SpdySession::DoReadLoop has not
  // posted a task.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream1);

  // Verify task observer's executed_count is zero, which indicates DoRead read
  // all the available data.
  EXPECT_EQ(0u, observer.executed_count());
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Test that SpdySession::DoReadLoop yields if more than
// |kYieldAfterDurationMilliseconds| has passed.  This test uses a mock time
// function that makes the response frame look very slow to read.
TEST_F(SpdySessionTest, TestYieldingSlowReads) {
  session_deps_.time_func = SlowReads;

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  MockRead reads[] = {
      CreateMockRead(resp1, 1), MockRead(ASYNC, 0, 2)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  // Set up the TaskObserver to verify that SpdySession::DoReadLoop posts a
  // task.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoReadLoop");

  EXPECT_EQ(0u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify that SpdySession::DoReadLoop has posted a
  // task.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_FALSE(spdy_stream1);

  // Verify task that the observer's executed_count is 1, which indicates DoRead
  // has posted only one task and thus yielded though there is data available
  // for it to read.
  EXPECT_EQ(1u, observer.executed_count());
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Regression test for https://crbug.com/531570.
// Test the case where DoRead() takes long but returns synchronously.
TEST_F(SpdySessionTest, TestYieldingSlowSynchronousReads) {
  session_deps_.time_func = SlowReads;

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };

  spdy::SpdySerializedFrame partial_data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "foo ", /*fin=*/false));
  spdy::SpdySerializedFrame finish_data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "bar", /*fin=*/true));

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  MockRead reads[] = {
      CreateMockRead(resp1, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(partial_data_frame, 3, ASYNC),
      CreateMockRead(partial_data_frame, 4, SYNCHRONOUS),
      CreateMockRead(partial_data_frame, 5, SYNCHRONOUS),
      CreateMockRead(finish_data_frame, 6, SYNCHRONOUS),
      MockRead(ASYNC, 0, 7)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, delegate1.stream_id());

  // Read all the data and verify SpdySession::DoReadLoop has posted a task.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("foo foo foo bar", delegate1.TakeReceivedData());
  EXPECT_FALSE(spdy_stream1);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Test that SpdySession::DoReadLoop yields while reading the
// data. This test makes 32k + 1 bytes of data available on the socket
// for reading. It then verifies that DoRead has yielded even though
// there is data available for it to read (i.e, socket()->Read didn't
// return ERR_IO_PENDING during socket reads).
TEST_F(SpdySessionTest, TestYieldingDuringReadData) {
  session_deps_.time_func = InstantaneousReads;

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };

  // Build buffer of size kYieldAfterBytesRead / 4
  // (-spdy_data_frame_size).
  ASSERT_EQ(32 * 1024, kYieldAfterBytesRead);
  const int kPayloadSize = kYieldAfterBytesRead / 4 - spdy::kFrameHeaderSize;
  TestDataStream test_stream;
  auto payload = base::MakeRefCounted<IOBufferWithSize>(kPayloadSize);
  char* payload_data = payload->data();
  test_stream.GetBytes(payload_data, kPayloadSize);

  spdy::SpdySerializedFrame partial_data_frame(
      spdy_util_.ConstructSpdyDataFrame(
          1, std::string_view(payload_data, kPayloadSize), /*fin=*/false));
  spdy::SpdySerializedFrame finish_data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "h", /*fin=*/true));

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  // Write 1 byte more than kMaxReadBytes to check that DoRead yields.
  MockRead reads[] = {
      CreateMockRead(resp1, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(partial_data_frame, 3),
      CreateMockRead(partial_data_frame, 4, SYNCHRONOUS),
      CreateMockRead(partial_data_frame, 5, SYNCHRONOUS),
      CreateMockRead(partial_data_frame, 6, SYNCHRONOUS),
      CreateMockRead(finish_data_frame, 7, SYNCHRONOUS),
      MockRead(ASYNC, 0, 8)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  // Set up the TaskObserver to verify SpdySession::DoReadLoop posts a task.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoReadLoop");

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify SpdySession::DoReadLoop has posted a task.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream1);

  // Verify task observer's executed_count is 1, which indicates DoRead has
  // posted only one task and thus yielded though there is data available for it
  // to read.
  EXPECT_EQ(1u, observer.executed_count());
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Test that SpdySession::DoReadLoop() tests interactions of yielding
// + async, by doing the following MockReads.
//
// MockRead of SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 2K
// ASYNC 8K, SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 2K.
//
// The above reads 26K synchronously. Since that is less that 32K, we
// will attempt to read again. However, that DoRead() will return
// ERR_IO_PENDING (because of async read), so DoReadLoop() will
// yield. When we come back, DoRead() will read the results from the
// async read, and rest of the data synchronously.
TEST_F(SpdySessionTest, TestYieldingDuringAsyncReadData) {
  session_deps_.time_func = InstantaneousReads;

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };

  // Build buffer of size kYieldAfterBytesRead / 4
  // (-spdy_data_frame_size).
  ASSERT_EQ(32 * 1024, kYieldAfterBytesRead);
  TestDataStream test_stream;
  const int kEightKPayloadSize =
      kYieldAfterBytesRead / 4 - spdy::kFrameHeaderSize;
  auto eightk_payload =
      base::MakeRefCounted<IOBufferWithSize>(kEightKPayloadSize);
  char* eightk_payload_data = eightk_payload->data();
  test_stream.GetBytes(eightk_payload_data, kEightKPayloadSize);

  // Build buffer of 2k size.
  TestDataStream test_stream2;
  const int kTwoKPayloadSize = kEightKPayloadSize - 6 * 1024;
  auto twok_payload = base::MakeRefCounted<IOBufferWithSize>(kTwoKPayloadSize);
  char* twok_payload_data = twok_payload->data();
  test_stream2.GetBytes(twok_payload_data, kTwoKPayloadSize);

  spdy::SpdySerializedFrame eightk_data_frame(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(eightk_payload_data, kEightKPayloadSize),
      /*fin=*/false));
  spdy::SpdySerializedFrame twok_data_frame(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(twok_payload_data, kTwoKPayloadSize),
      /*fin=*/false));
  spdy::SpdySerializedFrame finish_data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "h", /*fin=*/true));

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  MockRead reads[] = {
      CreateMockRead(resp1, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(eightk_data_frame, 3),
      CreateMockRead(eightk_data_frame, 4, SYNCHRONOUS),
      CreateMockRead(eightk_data_frame, 5, SYNCHRONOUS),
      CreateMockRead(twok_data_frame, 6, SYNCHRONOUS),
      CreateMockRead(eightk_data_frame, 7, ASYNC),
      CreateMockRead(eightk_data_frame, 8, SYNCHRONOUS),
      CreateMockRead(eightk_data_frame, 9, SYNCHRONOUS),
      CreateMockRead(eightk_data_frame, 10, SYNCHRONOUS),
      CreateMockRead(twok_data_frame, 11, SYNCHRONOUS),
      CreateMockRead(finish_data_frame, 12, SYNCHRONOUS),
      MockRead(ASYNC, 0, 13)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  // Set up the TaskObserver to monitor SpdySession::DoReadLoop
  // posting of tasks.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoReadLoop");

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify SpdySession::DoReadLoop has posted a
  // task.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream1);

  // Verify task observer's executed_count is 1, which indicates DoRead has
  // posted only one task and thus yielded though there is data available for
  // it to read.
  EXPECT_EQ(1u, observer.executed_count());
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Send a GoAway frame when SpdySession is in DoReadLoop. Make sure
// nothing blows up.
TEST_F(SpdySessionTest, GoAwayWhileInDoReadLoop) {
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(0));

  MockRead reads[] = {
      CreateMockRead(resp1, 1), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(body1, 3), CreateMockRead(goaway, 4),
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);
  ASSERT_TRUE(spdy_stream1);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream1->SendRequestHeaders(std::move(headers1), NO_MORE_DATA_TO_SEND);

  // Run until 1st read.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, spdy_stream1->stream_id());

  // Run until GoAway.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream1);
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_FALSE(session_);
}

// Within this framework, a SpdySession should be initialized with
// flow control disabled for protocol version 2, with flow control
// enabled only for streams for protocol version 3, and with flow
// control enabled for streams and sessions for higher versions.
TEST_F(SpdySessionTest, ProtocolNegotiation) {
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();
  session_ = CreateFakeSpdySession(spdy_session_pool_, key_);

  EXPECT_EQ(kDefaultInitialWindowSize, session_send_window_size());
  EXPECT_EQ(kDefaultInitialWindowSize, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());
}

// Tests the case of a non-SPDY request closing an idle SPDY session when no
// pointers to the idle session are currently held.
TEST_F(SpdySessionTest, CloseOneIdleConnection) {
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();

  ClientSocketPool* pool = http_session_->GetSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());

  // Create an idle SPDY session.
  CreateSpdySession();
  EXPECT_FALSE(pool->IsStalled());

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback2;
  auto connection2 = std::make_unique<ClientSocketHandle>();
  EXPECT_EQ(
      ERR_IO_PENDING,
      connection2->Init(
          ClientSocketPool::GroupId(
              url::SchemeHostPort(url::kHttpScheme, "2.com", 80),
              PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
              SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false),
          ClientSocketPool::SocketParams::CreateForHttpForTesting(),
          std::nullopt /* proxy_annotation_tag */, DEFAULT_PRIORITY,
          SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
          callback2.callback(), ClientSocketPool::ProxyAuthCallback(), pool,
          NetLogWithSource()));
  EXPECT_TRUE(pool->IsStalled());

  // The socket pool should close the connection asynchronously and establish a
  // new connection.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(pool->IsStalled());
  EXPECT_FALSE(session_);
}

// Tests the case of a non-SPDY request closing an idle SPDY session when no
// pointers to the idle session are currently held, in the case the SPDY session
// has an alias.
TEST_F(SpdySessionTest, CloseOneIdleConnectionWithAlias) {
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  session_deps_.host_resolver->rules()->AddIPLiteralRule(
      "www.example.org", "192.168.0.2", std::string());

  CreateNetworkSession();

  ClientSocketPool* pool = http_session_->GetSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());

  // Create an idle SPDY session.
  SpdySessionKey key1(HostPortPair("www.example.org", 80),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session1 =
      ::net::CreateSpdySession(http_session_.get(), key1, NetLogWithSource());
  EXPECT_FALSE(pool->IsStalled());

  // Set up an alias for the idle SPDY session, increasing its ref count to 2.
  SpdySessionKey key2(HostPortPair("mail.example.org", 80),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> request;
  bool is_blocking_request_for_session = false;
  SpdySessionRequestDelegate request_delegate;
  EXPECT_FALSE(spdy_session_pool_->RequestSession(
      key2, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ false, NetLogWithSource(),
      /* on_blocking_request_destroyed_callback = */ base::RepeatingClosure(),
      &request_delegate, &request, &is_blocking_request_for_session));
  EXPECT_TRUE(request);

  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(IPAddress(192, 168, 0, 2), 80)};
  // Simulate DNS resolution completing, which should set up an alias.
  EXPECT_EQ(OnHostResolutionCallbackResult::kMayBeDeletedAsync,
            spdy_session_pool_->OnHostResolutionComplete(
                key2, /* is_websocket = */ false, {endpoint},
                /*aliases=*/{}));

  // Get a session for |key2|, which should return the session created earlier.
  base::WeakPtr<SpdySession> session2 =
      spdy_session_pool_->FindAvailableSession(
          key2, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, NetLogWithSource());
  EXPECT_TRUE(session2);
  ASSERT_EQ(session1.get(), session2.get());
  EXPECT_FALSE(pool->IsStalled());

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback3;
  auto connection3 = std::make_unique<ClientSocketHandle>();
  EXPECT_EQ(
      ERR_IO_PENDING,
      connection3->Init(
          ClientSocketPool::GroupId(
              url::SchemeHostPort(url::kHttpScheme, "3.com", 80),
              PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
              SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false),
          ClientSocketPool::SocketParams::CreateForHttpForTesting(),
          std::nullopt /* proxy_annotation_tag */, DEFAULT_PRIORITY,
          SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
          callback3.callback(), ClientSocketPool::ProxyAuthCallback(), pool,
          NetLogWithSource()));
  EXPECT_TRUE(pool->IsStalled());

  // The socket pool should close the connection asynchronously and establish a
  // new connection.
  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_FALSE(pool->IsStalled());
  EXPECT_FALSE(session1);
  EXPECT_FALSE(session2);
}

// Tests that when a SPDY session becomes idle, it closes itself if there is
// a lower layer pool stalled on the per-pool socket limit.
TEST_F(SpdySessionTest, CloseSessionOnIdleWhenPoolStalled) {
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame cancel1(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(req1, 1), CreateMockWrite(cancel1, 1),
  };
  StaticSocketDataProvider data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  MockRead http_reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&http_data);

  AddSSLSocketData();

  CreateNetworkSession();

  ClientSocketPool* pool = http_session_->GetSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());

  // Create a SPDY session.
  CreateSpdySession();
  EXPECT_FALSE(pool->IsStalled());

  // Create a stream using the session, and send a request.

  TestCompletionCallback callback1;
  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, DEFAULT_PRIORITY,
      NetLogWithSource());
  ASSERT_TRUE(spdy_stream1.get());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  EXPECT_EQ(ERR_IO_PENDING, spdy_stream1->SendRequestHeaders(
                                std::move(headers1), NO_MORE_DATA_TO_SEND));

  base::RunLoop().RunUntilIdle();

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback2;
  auto connection2 = std::make_unique<ClientSocketHandle>();
  EXPECT_EQ(
      ERR_IO_PENDING,
      connection2->Init(
          ClientSocketPool::GroupId(
              url::SchemeHostPort(url::kHttpScheme, "2.com", 80),
              PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
              SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false),
          ClientSocketPool::SocketParams::CreateForHttpForTesting(),
          std::nullopt /* proxy_annotation_tag */, DEFAULT_PRIORITY,
          SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
          callback2.callback(), ClientSocketPool::ProxyAuthCallback(), pool,
          NetLogWithSource()));
  EXPECT_TRUE(pool->IsStalled());

  // Running the message loop should cause the socket pool to ask the SPDY
  // session to close an idle socket, but since the socket is in use, nothing
  // happens.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pool->IsStalled());
  EXPECT_FALSE(callback2.have_result());

  // Cancelling the request should result in the session's socket being
  // closed, since the pool is stalled.
  ASSERT_TRUE(spdy_stream1.get());
  spdy_stream1->Cancel(ERR_ABORTED);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(pool->IsStalled());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
}

// Verify that SpdySessionKey and therefore SpdySession is different when
// privacy mode is enabled or disabled.
TEST_F(SpdySessionTest, SpdySessionKeyPrivacyMode) {
  CreateNetworkSession();

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key_privacy_enabled(
      host_port_pair, PRIVACY_MODE_ENABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  SpdySessionKey key_privacy_disabled(
      host_port_pair, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_privacy_enabled));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_privacy_disabled));

  // Add SpdySession with PrivacyMode Enabled to the pool.
  base::WeakPtr<SpdySession> session_privacy_enabled =
      CreateFakeSpdySession(spdy_session_pool_, key_privacy_enabled);

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_privacy_enabled));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_privacy_disabled));

  // Add SpdySession with PrivacyMode Disabled to the pool.
  base::WeakPtr<SpdySession> session_privacy_disabled =
      CreateFakeSpdySession(spdy_session_pool_, key_privacy_disabled);

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_privacy_enabled));
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_privacy_disabled));

  session_privacy_enabled->CloseSessionOnError(ERR_ABORTED, std::string());
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_privacy_enabled));
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_privacy_disabled));

  session_privacy_disabled->CloseSessionOnError(ERR_ABORTED, std::string());
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_privacy_enabled));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_privacy_disabled));
}

// Delegate that creates another stream when its stream is closed.
class StreamCreatingDelegate : public test::StreamDelegateDoNothing {
 public:
  StreamCreatingDelegate(const base::WeakPtr<SpdyStream>& stream,
                         const base::WeakPtr<SpdySession>& session)
      : StreamDelegateDoNothing(stream),
        session_(session) {}

  ~StreamCreatingDelegate() override = default;

  void OnClose(int status) override {
    GURL url(kDefaultUrl);
    std::ignore =
        CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_, url,
                                  MEDIUM, NetLogWithSource());
  }

 private:
  const base::WeakPtr<SpdySession> session_;
};

// Create another stream in response to a stream being reset. Nothing
// should blow up. This is a regression test for
// http://crbug.com/263690 .
TEST_F(SpdySessionTest, CreateStreamOnStreamReset) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_REFUSED_STREAM));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(rst, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3), MockRead(ASYNC, 0, 4)  // EOF
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream);
  EXPECT_EQ(0u, spdy_stream->stream_id());

  StreamCreatingDelegate delegate(spdy_stream, session_);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  EXPECT_EQ(0u, spdy_stream->stream_id());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, spdy_stream->stream_id());

  // Cause the stream to be reset, which should cause another stream
  // to be created.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(spdy_stream);
  EXPECT_TRUE(delegate.StreamIsClosed());
  EXPECT_EQ(0u, num_active_streams());
  EXPECT_EQ(1u, num_created_streams());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTest, UpdateStreamsSendWindowSize) {
  // Set spdy::SETTINGS_INITIAL_WINDOW_SIZE to a small number so that
  // WINDOW_UPDATE gets sent.
  spdy::SettingsMap new_settings;
  int32_t window_size = 1;
  new_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = window_size;

  // Set up the socket so we read a SETTINGS frame that sets
  // INITIAL_WINDOW_SIZE.
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));
  MockRead reads[] = {
      CreateMockRead(settings_frame, 0), MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 0, 2)  // EOF
  };

  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {
      CreateMockWrite(settings_ack, 3),
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);
  TestCompletionCallback callback1;
  EXPECT_NE(spdy_stream1->send_window_size(), window_size);

  // Process the SETTINGS frame.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_initial_send_window_size(), window_size);
  EXPECT_EQ(spdy_stream1->send_window_size(), window_size);

  // Release the first one, this will allow the second to be created.
  spdy_stream1->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);
  EXPECT_EQ(spdy_stream2->send_window_size(), window_size);
  spdy_stream2->Cancel(ERR_ABORTED);
  EXPECT_FALSE(spdy_stream2);

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// SpdySession::{Increase,Decrease}RecvWindowSize should properly
// adjust the session receive window size. In addition,
// SpdySession::IncreaseRecvWindowSize should trigger
// sending a WINDOW_UPDATE frame for a large enough delta.
TEST_F(SpdySessionTest, AdjustRecvWindowSize) {
  const int32_t initial_window_size = kDefaultInitialWindowSize;
  const int32_t delta_window_size = 100;

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), MockRead(ASYNC, 0, 2)  // EOF
  };
  spdy::SpdySerializedFrame window_update(spdy_util_.ConstructSpdyWindowUpdate(
      spdy::kSessionFlowControlStreamId,
      initial_window_size + delta_window_size));
  MockWrite writes[] = {
      CreateMockWrite(window_update, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  IncreaseRecvWindowSize(delta_window_size);
  EXPECT_EQ(initial_window_size + delta_window_size,
            session_recv_window_size());
  EXPECT_EQ(delta_window_size, session_unacked_recv_window_bytes());

  // Should trigger sending a WINDOW_UPDATE frame.
  IncreaseRecvWindowSize(initial_window_size);
  EXPECT_EQ(initial_window_size + delta_window_size + initial_window_size,
            session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  base::RunLoop().RunUntilIdle();

  // DecreaseRecvWindowSize() expects |in_io_loop_| to be true.
  set_in_io_loop(true);
  DecreaseRecvWindowSize(initial_window_size + delta_window_size +
                         initial_window_size);
  set_in_io_loop(false);
  EXPECT_EQ(0, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// SpdySession::{Increase,Decrease}RecvWindowSize should properly
// adjust the session receive window size. In addition,
// SpdySession::IncreaseRecvWindowSize should trigger
// sending a WINDOW_UPDATE frame for a small delta after
// kDefaultTimeToBufferSmallWindowUpdates time has passed.
TEST_F(SpdySessionTestWithMockTime, FlowControlSlowReads) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();
  session_ = CreateFakeSpdySession(spdy_session_pool_, key_);

  // Re-enable the time-based window update buffering. The test harness
  // disables it by default to prevent flakiness.
  session_->SetTimeToBufferSmallWindowUpdates(
      kDefaultTimeToBufferSmallWindowUpdates);

  const int32_t initial_window_size = kDefaultInitialWindowSize;
  const int32_t delta_window_size = 100;

  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  // Receive data, consuming some of the receive window.
  set_in_io_loop(true);
  DecreaseRecvWindowSize(delta_window_size);
  set_in_io_loop(false);

  EXPECT_EQ(initial_window_size - delta_window_size,
            session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  // Consume the data, returning some of the receive window (locally)
  IncreaseRecvWindowSize(delta_window_size);
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(delta_window_size, session_unacked_recv_window_bytes());

  // Receive data, consuming some of the receive window.
  set_in_io_loop(true);
  DecreaseRecvWindowSize(delta_window_size);
  set_in_io_loop(false);

  // Window updates after a configured time second should force a WINDOW_UPDATE,
  // draining the unacked window bytes.
  AdvanceClock(kDefaultTimeToBufferSmallWindowUpdates);
  IncreaseRecvWindowSize(delta_window_size);
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());
}

// SpdySession::{Increase,Decrease}SendWindowSize should properly
// adjust the session send window size when the "enable_spdy_31" flag
// is set.
TEST_F(SpdySessionTest, AdjustSendWindowSize) {
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();
  session_ = CreateFakeSpdySession(spdy_session_pool_, key_);

  const int32_t initial_window_size = kDefaultInitialWindowSize;
  const int32_t delta_window_size = 100;

  EXPECT_EQ(initial_window_size, session_send_window_size());

  IncreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(initial_window_size + delta_window_size,
            session_send_window_size());

  DecreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(initial_window_size, session_send_window_size());
}

// Incoming data for an inactive stream should not cause the session
// receive window size to decrease, but it should cause the unacked
// bytes to increase.
TEST_F(SpdySessionTest, SessionFlowControlInactiveStream) {
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyDataFrame(1, false));
  MockRead reads[] = {
      CreateMockRead(resp, 0), MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 0, 2)  // EOF
  };
  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_EQ(kDefaultInitialWindowSize, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kDefaultInitialWindowSize, session_recv_window_size());
  EXPECT_EQ(kUploadDataSize, session_unacked_recv_window_bytes());

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// The frame header is not included in flow control, but frame payload
// (including optional pad length and padding) is.
TEST_F(SpdySessionTest, SessionFlowControlPadding) {
  const int padding_length = 42;
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyDataFrame(1, kUploadData, false, padding_length));
  MockRead reads[] = {
      CreateMockRead(resp, 0), MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 0, 2)  // EOF
  };
  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_EQ(kDefaultInitialWindowSize, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kDefaultInitialWindowSize, session_recv_window_size());
  EXPECT_EQ(kUploadDataSize + padding_length,
            session_unacked_recv_window_bytes());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Peer sends more data than stream level receiving flow control window.
TEST_F(SpdySessionTest, StreamFlowControlTooMuchData) {
  const int32_t stream_max_recv_window_size = 1024;
  const int32_t data_frame_size = 2 * stream_max_recv_window_size;

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(spdy_util_.ConstructSpdyRstStream(
      1, spdy::ERROR_CODE_FLOW_CONTROL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 4),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  const std::string payload(data_frame_size, 'a');
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, payload, false));
  MockRead reads[] = {
      CreateMockRead(resp, 1),       MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(data_frame, 3), MockRead(ASYNC, ERR_IO_PENDING, 5),
      MockRead(ASYNC, 0, 6),
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  session_deps_.http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
      stream_max_recv_window_size;
  CreateNetworkSession();

  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  EXPECT_EQ(stream_max_recv_window_size, spdy_stream->recv_window_size());

  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  EXPECT_EQ(ERR_IO_PENDING, spdy_stream->SendRequestHeaders(
                                std::move(headers), NO_MORE_DATA_TO_SEND));

  // Request and response.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, spdy_stream->stream_id());

  // Too large data frame causes flow control error, should close stream.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream);

  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Regression test for a bug that was caused by including unsent WINDOW_UPDATE
// deltas in the receiving window size when checking incoming frames for flow
// control errors at session level.
TEST_F(SpdySessionTest, SessionFlowControlTooMuchDataTwoDataFrames) {
  const int32_t session_max_recv_window_size = 500;
  const int32_t first_data_frame_size = 200;
  const int32_t second_data_frame_size = 400;

  // First data frame should not trigger a WINDOW_UPDATE.
  ASSERT_GT(session_max_recv_window_size / 2, first_data_frame_size);
  // Second data frame would be fine had there been a WINDOW_UPDATE.
  ASSERT_GT(session_max_recv_window_size, second_data_frame_size);
  // But in fact, the two data frames together overflow the receiving window at
  // session level.
  ASSERT_LT(session_max_recv_window_size,
            first_data_frame_size + second_data_frame_size);

  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_FLOW_CONTROL_ERROR,
      "delta_window_size is 400 in DecreaseRecvWindowSize, which is larger "
      "than the receive window size of 300"));
  MockWrite writes[] = {
      CreateMockWrite(goaway, 4),
  };

  const std::string first_data_frame(first_data_frame_size, 'a');
  spdy::SpdySerializedFrame first(
      spdy_util_.ConstructSpdyDataFrame(1, first_data_frame, false));
  const std::string second_data_frame(second_data_frame_size, 'b');
  spdy::SpdySerializedFrame second(
      spdy_util_.ConstructSpdyDataFrame(1, second_data_frame, false));
  MockRead reads[] = {
      CreateMockRead(first, 0), MockRead(ASYNC, ERR_IO_PENDING, 1),
      CreateMockRead(second, 2), MockRead(ASYNC, 0, 3),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();
  // Setting session level receiving window size to smaller than initial is not
  // possible via SpdySessionPoolPeer.
  set_session_recv_window_size(session_max_recv_window_size);

  // First data frame is immediately consumed and does not trigger
  // WINDOW_UPDATE.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(first_data_frame_size, session_unacked_recv_window_bytes());
  EXPECT_EQ(session_max_recv_window_size, session_recv_window_size());
  EXPECT_TRUE(session_->IsAvailable());

  // Second data frame overflows receiving window, causes session to close.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(session_->IsDraining());
}

// Regression test for a bug that was caused by including unsent WINDOW_UPDATE
// deltas in the receiving window size when checking incoming data frames for
// flow control errors at stream level.
TEST_F(SpdySessionTest, StreamFlowControlTooMuchDataTwoDataFrames) {
  const int32_t stream_max_recv_window_size = 500;
  const int32_t first_data_frame_size = 200;
  const int32_t second_data_frame_size = 400;

  // First data frame should not trigger a WINDOW_UPDATE.
  ASSERT_GT(stream_max_recv_window_size / 2, first_data_frame_size);
  // Second data frame would be fine had there been a WINDOW_UPDATE.
  ASSERT_GT(stream_max_recv_window_size, second_data_frame_size);
  // But in fact, they should overflow the receiving window at stream level.
  ASSERT_LT(stream_max_recv_window_size,
            first_data_frame_size + second_data_frame_size);

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(spdy_util_.ConstructSpdyRstStream(
      1, spdy::ERROR_CODE_FLOW_CONTROL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 6),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  const std::string first_data_frame(first_data_frame_size, 'a');
  spdy::SpdySerializedFrame first(
      spdy_util_.ConstructSpdyDataFrame(1, first_data_frame, false));
  const std::string second_data_frame(second_data_frame_size, 'b');
  spdy::SpdySerializedFrame second(
      spdy_util_.ConstructSpdyDataFrame(1, second_data_frame, false));
  MockRead reads[] = {
      CreateMockRead(resp, 1),   MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(first, 3),  MockRead(ASYNC, ERR_IO_PENDING, 4),
      CreateMockRead(second, 5), MockRead(ASYNC, ERR_IO_PENDING, 7),
      MockRead(ASYNC, 0, 8),
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  session_deps_.http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
      stream_max_recv_window_size;
  CreateNetworkSession();

  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  EXPECT_EQ(ERR_IO_PENDING, spdy_stream->SendRequestHeaders(
                                std::move(headers), NO_MORE_DATA_TO_SEND));

  // Request and response.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(spdy_stream->IsLocallyClosed());
  EXPECT_EQ(stream_max_recv_window_size, spdy_stream->recv_window_size());

  // First data frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(spdy_stream->IsLocallyClosed());
  EXPECT_EQ(stream_max_recv_window_size - first_data_frame_size,
            spdy_stream->recv_window_size());

  // Consume first data frame.  This does not trigger a WINDOW_UPDATE.
  std::string received_data = delegate.TakeReceivedData();
  EXPECT_EQ(static_cast<size_t>(first_data_frame_size), received_data.size());
  EXPECT_EQ(stream_max_recv_window_size, spdy_stream->recv_window_size());

  // Second data frame overflows receiving window, causes the stream to close.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(spdy_stream.get());

  // RST_STREAM
  EXPECT_TRUE(session_);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// A delegate that drops any received data.
class DropReceivedDataDelegate : public test::StreamDelegateSendImmediate {
 public:
  DropReceivedDataDelegate(const base::WeakPtr<SpdyStream>& stream,
                           std::string_view data)
      : StreamDelegateSendImmediate(stream, data) {}

  ~DropReceivedDataDelegate() override = default;

  // Drop any received data.
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override {}
};

// Send data back and forth but use a delegate that drops its received
// data. The receive window should still increase to its original
// value, i.e. we shouldn't "leak" receive window bytes.
TEST_F(SpdySessionTest, SessionFlowControlNoReceiveLeaks) {
  const int32_t kMsgDataSize = 100;
  const std::string msg_data(kMsgDataSize, 'a');

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kMsgDataSize, MEDIUM, nullptr, 0));
  spdy::SpdySerializedFrame msg(
      spdy_util_.ConstructSpdyDataFrame(1, msg_data, false));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(msg, 2),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame echo(
      spdy_util_.ConstructSpdyDataFrame(1, msg_data, false));
  spdy::SpdySerializedFrame window_update(spdy_util_.ConstructSpdyWindowUpdate(
      spdy::kSessionFlowControlStreamId, kMsgDataSize));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(echo, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4), MockRead(ASYNC, 0, 5)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(stream);
  EXPECT_EQ(0u, stream->stream_id());

  DropReceivedDataDelegate delegate(stream, msg_data);
  stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kMsgDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));

  const int32_t initial_window_size = kDefaultInitialWindowSize;
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(kMsgDataSize, session_unacked_recv_window_bytes());

  stream->Close();
  EXPECT_FALSE(stream);

  EXPECT_THAT(delegate.WaitForClose(), IsOk());

  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(kMsgDataSize, session_unacked_recv_window_bytes());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Send data back and forth but close the stream before its data frame
// can be written to the socket. The send window should then increase
// to its original value, i.e. we shouldn't "leak" send window bytes.
TEST_F(SpdySessionTest, SessionFlowControlNoSendLeaks) {
  const int32_t kMsgDataSize = 100;
  const std::string msg_data(kMsgDataSize, 'a');

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kMsgDataSize, MEDIUM, nullptr, 0));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(resp, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(stream);
  EXPECT_EQ(0u, stream->stream_id());

  test::StreamDelegateSendImmediate delegate(stream, msg_data);
  stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kMsgDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));

  const int32_t initial_window_size = kDefaultInitialWindowSize;
  EXPECT_EQ(initial_window_size, session_send_window_size());

  // Write request.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_window_size, session_send_window_size());

  // Read response, but do not run the message loop, so that the body is not
  // written to the socket.
  data.Resume();

  EXPECT_EQ(initial_window_size - kMsgDataSize, session_send_window_size());

  // Closing the stream should increase the session's send window.
  stream->Close();
  EXPECT_FALSE(stream);

  EXPECT_EQ(initial_window_size, session_send_window_size());

  EXPECT_THAT(delegate.WaitForClose(), IsOk());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// Send data back and forth; the send and receive windows should
// change appropriately.
TEST_F(SpdySessionTest, SessionFlowControlEndToEnd) {
  const int32_t kMsgDataSize = 100;
  const std::string msg_data(kMsgDataSize, 'a');

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kMsgDataSize, MEDIUM, nullptr, 0));
  spdy::SpdySerializedFrame msg(
      spdy_util_.ConstructSpdyDataFrame(1, msg_data, false));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(msg, 2),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame echo(
      spdy_util_.ConstructSpdyDataFrame(1, msg_data, false));
  spdy::SpdySerializedFrame window_update(spdy_util_.ConstructSpdyWindowUpdate(
      spdy::kSessionFlowControlStreamId, kMsgDataSize));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 3),
      CreateMockRead(echo, 4),
      MockRead(ASYNC, ERR_IO_PENDING, 5),
      CreateMockRead(window_update, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7),
      MockRead(ASYNC, 0, 8)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session_, test_url_,
                                MEDIUM, NetLogWithSource());
  ASSERT_TRUE(stream);
  EXPECT_EQ(0u, stream->stream_id());

  test::StreamDelegateSendImmediate delegate(stream, msg_data);
  stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kMsgDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));

  const int32_t initial_window_size = kDefaultInitialWindowSize;
  EXPECT_EQ(initial_window_size, session_send_window_size());
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  // Send request and message.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_window_size - kMsgDataSize, session_send_window_size());
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  // Read echo.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_window_size - kMsgDataSize, session_send_window_size());
  EXPECT_EQ(initial_window_size - kMsgDataSize, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  // Read window update.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_window_size, session_send_window_size());
  EXPECT_EQ(initial_window_size - kMsgDataSize, session_recv_window_size());
  EXPECT_EQ(0, session_unacked_recv_window_bytes());

  EXPECT_EQ(msg_data, delegate.TakeReceivedData());

  // Draining the delegate's read queue should increase the session's
  // receive window.
  EXPECT_EQ(initial_window_size, session_send_window_size());
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(kMsgDataSize, session_unacked_recv_window_bytes());

  stream->Close();
  EXPECT_FALSE(stream);

  EXPECT_THAT(delegate.WaitForClose(), IsOk());

  EXPECT_EQ(initial_window_size, session_send_window_size());
  EXPECT_EQ(initial_window_size, session_recv_window_size());
  EXPECT_EQ(kMsgDataSize, session_unacked_recv_window_bytes());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Given a stall function and an unstall function, runs a test to make
// sure that a stream resumes after unstall.
void SpdySessionTest::RunResumeAfterUnstallTest(
    base::OnceCallback<void(SpdyStream*)> stall_function,
    base::OnceCallback<void(SpdyStream*, int32_t)> unstall_function) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataStringPiece, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame echo(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataStringPiece, false));
  MockRead reads[] = {
      CreateMockRead(resp, 2), MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream);

  test::StreamDelegateWithBody delegate(stream, kBodyDataStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream->url().spec());

  std::move(stall_function).Run(stream.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  std::move(unstall_function).Run(stream.get(), kBodyDataSize);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  EXPECT_THAT(delegate.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());

  // Run SpdySession::PumpWriteLoop which destroys |session_|.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session_);
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Run the resume-after-unstall test with all possible stall and
// unstall sequences.

TEST_F(SpdySessionTest, ResumeAfterUnstallSession) {
  RunResumeAfterUnstallTest(base::BindOnce(&SpdySessionTest::StallSessionOnly,
                                           base::Unretained(this)),
                            base::BindOnce(&SpdySessionTest::UnstallSessionOnly,
                                           base::Unretained(this)));
}

// Equivalent to
// SpdyStreamTest.ResumeAfterSendWindowSizeIncrease.
TEST_F(SpdySessionTest, ResumeAfterUnstallStream) {
  RunResumeAfterUnstallTest(
      base::BindOnce(&SpdySessionTest::StallStreamOnly, base::Unretained(this)),
      base::BindOnce(&SpdySessionTest::UnstallStreamOnly,
                     base::Unretained(this)));
}

TEST_F(SpdySessionTest, StallSessionStreamResumeAfterUnstallSessionStream) {
  RunResumeAfterUnstallTest(
      base::BindOnce(&SpdySessionTest::StallSessionStream,
                     base::Unretained(this)),
      base::BindOnce(&SpdySessionTest::UnstallSessionStream,
                     base::Unretained(this)));
}

TEST_F(SpdySessionTest, StallStreamSessionResumeAfterUnstallSessionStream) {
  RunResumeAfterUnstallTest(
      base::BindOnce(&SpdySessionTest::StallStreamSession,
                     base::Unretained(this)),
      base::BindOnce(&SpdySessionTest::UnstallSessionStream,
                     base::Unretained(this)));
}

TEST_F(SpdySessionTest, StallStreamSessionResumeAfterUnstallStreamSession) {
  RunResumeAfterUnstallTest(
      base::BindOnce(&SpdySessionTest::StallStreamSession,
                     base::Unretained(this)),
      base::BindOnce(&SpdySessionTest::UnstallStreamSession,
                     base::Unretained(this)));
}

TEST_F(SpdySessionTest, StallSessionStreamResumeAfterUnstallStreamSession) {
  RunResumeAfterUnstallTest(
      base::BindOnce(&SpdySessionTest::StallSessionStream,
                     base::Unretained(this)),
      base::BindOnce(&SpdySessionTest::UnstallStreamSession,
                     base::Unretained(this)));
}

// Cause a stall by reducing the flow control send window to 0. The
// streams should resume in priority order when that window is then
// increased.
TEST_F(SpdySessionTest, ResumeByPriorityAfterSendWindowSizeIncrease) {
  spdy::SpdySerializedFrame req1(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame req2(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 3, kBodyDataSize, MEDIUM, nullptr, 0));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataStringPiece, true));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(3, kBodyDataStringPiece, true));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
      CreateMockWrite(body2, 2), CreateMockWrite(body1, 3),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  MockRead reads[] = {
      CreateMockRead(resp1, 4), CreateMockRead(resp2, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream1);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(stream2);

  test::StreamDelegateWithBody delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  StallSessionSend();

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream1->SendRequestHeaders(std::move(headers1),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream1->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequestHeaders(std::move(headers2),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream2->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  // This should unstall only stream2.
  UnstallSessionSend(kBodyDataSize);

  EXPECT_TRUE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  // This should then unstall stream1.
  UnstallSessionSend(kBodyDataSize);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate1.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));
  EXPECT_THAT(delegate2.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ("200", delegate1.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ("200", delegate2.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_FALSE(session_);
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

// An upload stream is stalled when the session gets unstalled, then the session
// is stalled again when the stream gets unstalled.  The stream should not fail.
// Regression test for https://crbug.com/761919.
TEST_F(SpdySessionTest, ResumeSessionWithStalledStream) {
  spdy::SpdySerializedFrame req1(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame req2(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 3, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(3, kBodyDataStringPiece, true));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataStringPiece, true));
  MockWrite writes[] = {CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
                        CreateMockWrite(body1, 2), CreateMockWrite(body2, 3)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  MockRead reads[] = {CreateMockRead(resp1, 4), CreateMockRead(resp2, 5),
                      MockRead(ASYNC, 0, 6)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream1);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream2);

  test::StreamDelegateWithBody delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  StallSessionSend();

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream1->SendRequestHeaders(std::move(headers1),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream1->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequestHeaders(std::move(headers2),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream2->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  StallStreamSend(stream1.get());

  // At this point, both |session| and |stream1| are stalled
  // by their respective flow control mechanisms.  Now unstall the session.
  // This calls session->ResumeSendStalledStreams(), which calls
  // stream1->PossiblyResumeIfSendStalled().  However, |stream1| is stalled, so
  // no data are sent on that stream.  At this point, |stream1| should not be
  // removed from session_->stream_send_unstall_queue_.
  // Then stream2->PossiblyResumeIfSendStalled() is called,
  // data are sent on |stream2|, and |session_| stalls again.
  UnstallSessionSend(kBodyDataSize);

  EXPECT_TRUE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  // Make sure that the session is stalled.  Otherwise
  // stream1->PossiblyResumeIfSendStalled() would resume the stream as soon as
  // the stream is unstalled, hiding the bug.
  EXPECT_TRUE(session_->IsSendStalled());
  UnstallStreamSend(stream1.get(), kBodyDataSize);

  // Finally, unstall session.
  UnstallSessionSend(kBodyDataSize);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate1.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));
  EXPECT_THAT(delegate2.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ("200", delegate1.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ("200", delegate2.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_FALSE(session_);
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

class StreamBrokenConnectionDetectionCheckDelegate
    : public test::StreamDelegateDoNothing {
 public:
  StreamBrokenConnectionDetectionCheckDelegate(
      const base::WeakPtr<SpdyStream>& stream,
      const base::WeakPtr<SpdySession>& session,
      bool expected_is_broken_connection_detection_enabled)
      : StreamDelegateDoNothing(stream),
        session_(session),
        expected_is_broken_connection_detection_enabled_(
            expected_is_broken_connection_detection_enabled) {}

  ~StreamBrokenConnectionDetectionCheckDelegate() override = default;

  void OnClose(int status) override {
    ASSERT_EQ(expected_is_broken_connection_detection_enabled_,
              session_->IsBrokenConnectionDetectionEnabled());
  }

 private:
  const base::WeakPtr<SpdySession> session_;
  bool expected_is_broken_connection_detection_enabled_;
};

TEST_F(SpdySessionTest, BrokenConnectionDetectionEOF) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, 0),  // EOF
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  base::WeakPtr<SpdyStream> stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  ASSERT_TRUE(stream);
  ASSERT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  StreamBrokenConnectionDetectionCheckDelegate delegate(stream, session_,
                                                        false);
  stream->SetDelegate(&delegate);

  // Let the delegate run and check broken connection detection status during
  // OnClose().
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(session_);
}

TEST_F(SpdySessionTest, BrokenConnectionDetectionCloseSession) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, 0),  // EOF
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  base::WeakPtr<SpdyStream> stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  ASSERT_TRUE(stream);
  ASSERT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  StreamBrokenConnectionDetectionCheckDelegate delegate(stream, session_,
                                                        false);
  stream->SetDelegate(&delegate);

  session_->CloseSessionOnError(ERR_ABORTED, "Aborting session");
  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(session_);
}

TEST_F(SpdySessionTest, BrokenConnectionDetectionCloseStream) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, 0),  // EOF
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  base::WeakPtr<SpdyStream> stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  ASSERT_TRUE(stream);
  ASSERT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  StreamBrokenConnectionDetectionCheckDelegate delegate(stream, session_,
                                                        false);
  stream->SetDelegate(&delegate);

  stream->Close();
  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(session_);
}

TEST_F(SpdySessionTest, BrokenConnectionDetectionCancelStream) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, 0),
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  base::WeakPtr<SpdyStream> stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  ASSERT_TRUE(stream);
  ASSERT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  StreamBrokenConnectionDetectionCheckDelegate delegate(stream, session_,
                                                        false);
  stream->SetDelegate(&delegate);

  stream->Cancel(ERR_ABORTED);
  ASSERT_FALSE(session_->IsBrokenConnectionDetectionEnabled());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(session_);
}

// When multiple streams request broken connection detection, only the last one
// to complete should disable the connection status check.
TEST_F(SpdySessionTest, BrokenConnectionDetectionMultipleRequests) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), CreateMockRead(goaway, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4), MockRead(ASYNC, 0, 5)  // EOF
  };
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 1),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  EXPECT_TRUE(spdy_stream1);
  EXPECT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  StreamBrokenConnectionDetectionCheckDelegate delegate1(spdy_stream1, session_,
                                                         false);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, test_url_, MEDIUM,
      NetLogWithSource(), true, kHeartbeatInterval);
  EXPECT_TRUE(spdy_stream2);
  EXPECT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  StreamBrokenConnectionDetectionCheckDelegate delegate2(spdy_stream2, session_,
                                                         true);
  spdy_stream2->SetDelegate(&delegate2);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  quiche::HttpHeaderBlock headers2(headers.Clone());

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);
  spdy_stream2->SendRequestHeaders(std::move(headers2), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  // Read and process the GOAWAY frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(session_->IsBrokenConnectionDetectionEnabled());
  EXPECT_FALSE(session_->IsStreamActive(3));
  EXPECT_FALSE(spdy_stream2);
  EXPECT_TRUE(session_->IsStreamActive(1));
  EXPECT_TRUE(session_->IsGoingAway());

  // Should close the session.
  spdy_stream1->Close();
  EXPECT_FALSE(spdy_stream1);

  EXPECT_TRUE(session_);
  EXPECT_FALSE(session_->IsBrokenConnectionDetectionEnabled());
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);
}

// Delegate that closes a given stream after sending its body.
class StreamClosingDelegate : public test::StreamDelegateWithBody {
 public:
  StreamClosingDelegate(const base::WeakPtr<SpdyStream>& stream,
                        std::string_view data)
      : StreamDelegateWithBody(stream, data) {}

  ~StreamClosingDelegate() override = default;

  void set_stream_to_close(const base::WeakPtr<SpdyStream>& stream_to_close) {
    stream_to_close_ = stream_to_close;
  }

  void OnDataSent() override {
    test::StreamDelegateWithBody::OnDataSent();
    if (stream_to_close_.get()) {
      stream_to_close_->Close();
      EXPECT_FALSE(stream_to_close_);
    }
  }

 private:
  base::WeakPtr<SpdyStream> stream_to_close_;
};

// Cause a stall by reducing the flow control send window to
// 0. Unstalling the session should properly handle deleted streams.
TEST_F(SpdySessionTest, SendWindowSizeIncreaseWithDeletedStreams) {
  spdy::SpdySerializedFrame req1(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame req2(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 3, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame req3(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 5, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(3, kBodyDataStringPiece, true));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
      CreateMockWrite(req3, 2), CreateMockWrite(body2, 3),
  };

  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  MockRead reads[] = {
      CreateMockRead(resp2, 4), MockRead(ASYNC, ERR_IO_PENDING, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream1);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream2);

  StreamClosingDelegate delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  base::WeakPtr<SpdyStream> stream3 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream3);

  test::StreamDelegateWithBody delegate3(stream3, kBodyDataStringPiece);
  stream3->SetDelegate(&delegate3);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());
  EXPECT_FALSE(stream3->send_stalled_by_flow_control());

  StallSessionSend();

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream1->SendRequestHeaders(std::move(headers1),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream1->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequestHeaders(std::move(headers2),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream2->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  quiche::HttpHeaderBlock headers3(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream3->SendRequestHeaders(std::move(headers3),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream3->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, stream3->stream_id());
  EXPECT_TRUE(stream3->send_stalled_by_flow_control());

  spdy::SpdyStreamId stream_id1 = stream1->stream_id();
  spdy::SpdyStreamId stream_id2 = stream2->stream_id();
  spdy::SpdyStreamId stream_id3 = stream3->stream_id();

  // Close stream1 preemptively.
  session_->CloseActiveStream(stream_id1, ERR_CONNECTION_CLOSED);
  EXPECT_FALSE(stream1);

  EXPECT_FALSE(session_->IsStreamActive(stream_id1));
  EXPECT_TRUE(session_->IsStreamActive(stream_id2));
  EXPECT_TRUE(session_->IsStreamActive(stream_id3));

  // Unstall stream2, which should then close stream3.
  delegate2.set_stream_to_close(stream3);
  UnstallSessionSend(kBodyDataSize);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(stream3);

  EXPECT_FALSE(stream2->send_stalled_by_flow_control());
  EXPECT_FALSE(session_->IsStreamActive(stream_id1));
  EXPECT_TRUE(session_->IsStreamActive(stream_id2));
  EXPECT_FALSE(session_->IsStreamActive(stream_id3));

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(stream2);
  EXPECT_FALSE(session_);

  EXPECT_THAT(delegate1.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));
  EXPECT_THAT(delegate2.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));
  EXPECT_THAT(delegate3.WaitForClose(), IsOk());

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ("200", delegate2.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_TRUE(delegate3.send_headers_completed());
  EXPECT_EQ(std::string(), delegate3.TakeReceivedData());

  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Cause a stall by reducing the flow control send window to
// 0. Unstalling the session should properly handle the session itself
// being closed.
TEST_F(SpdySessionTest, SendWindowSizeIncreaseWithDeletedSession) {
  spdy::SpdySerializedFrame req1(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame req2(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 3, kBodyDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataStringPiece, false));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0), CreateMockWrite(req2, 1),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2), MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream1);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(stream2);

  test::StreamDelegateWithBody delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  StallSessionSend();

  quiche::HttpHeaderBlock headers1(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream1->SendRequestHeaders(std::move(headers1),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream1->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  quiche::HttpHeaderBlock headers2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequestHeaders(std::move(headers2),
                                                        MORE_DATA_TO_SEND));
  EXPECT_EQ(kDefaultUrl, stream2->url().spec());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key_));

  // Unstall stream1.
  UnstallSessionSend(kBodyDataSize);

  // Close the session (since we can't do it from within the delegate
  // method, since it's in the stream's loop).
  session_->CloseSessionOnError(ERR_CONNECTION_CLOSED, "Closing session");
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session_);

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, key_));

  EXPECT_THAT(delegate1.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));
  EXPECT_THAT(delegate2.WaitForClose(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(SpdySessionTest, GoAwayOnSessionFlowControlError) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_FLOW_CONTROL_ERROR,
      "delta_window_size is 6 in DecreaseRecvWindowSize, which is larger than "
      "the receive window size of 1"));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(goaway, 4),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(resp, 2),
      CreateMockRead(body, 3),
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, LOWEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream);
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  // Write request.
  base::RunLoop().RunUntilIdle();

  // Put session on the edge of overflowing it's recv window.
  set_session_recv_window_size(1);

  // Read response headers & body. Body overflows the session window, and a
  // goaway is written.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.WaitForClose(), IsError(ERR_HTTP2_FLOW_CONTROL_ERROR));
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTest, RejectInvalidUnknownFrames) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  set_stream_hi_water_mark(5);
  // Low client (odd) ids are fine.
  EXPECT_TRUE(OnUnknownFrame(3, 0));
  // Client id exceeding watermark.
  EXPECT_FALSE(OnUnknownFrame(9, 0));

  // Frames on push streams are rejected.
  EXPECT_FALSE(OnUnknownFrame(2, 0));
}

TEST_F(SpdySessionTest, EnableWebSocket) {
  spdy::SettingsMap settings_map;
  settings_map[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings(
      spdy_util_.ConstructSpdySettings(settings_map));
  MockRead reads[] = {CreateMockRead(settings, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};

  spdy::SpdySerializedFrame ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {CreateMockWrite(ack, 1)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_FALSE(session_->support_websocket());

  // Read SETTINGS frame.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(session_->support_websocket());

  // Read EOF.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTest, DisableWebSocketDoesNothing) {
  spdy::SettingsMap settings_map;
  settings_map[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 0;
  spdy::SpdySerializedFrame settings(
      spdy_util_.ConstructSpdySettings(settings_map));
  MockRead reads[] = {CreateMockRead(settings, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};

  spdy::SpdySerializedFrame ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {CreateMockWrite(ack, 1)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_FALSE(session_->support_websocket());

  // Read SETTINGS frame.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session_->support_websocket());

  // Read EOF.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTest, EnableWebSocketThenDisableIsProtocolError) {
  spdy::SettingsMap settings_map1;
  settings_map1[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings1(
      spdy_util_.ConstructSpdySettings(settings_map1));
  spdy::SettingsMap settings_map2;
  settings_map2[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 0;
  spdy::SpdySerializedFrame settings2(
      spdy_util_.ConstructSpdySettings(settings_map2));
  MockRead reads[] = {CreateMockRead(settings1, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      CreateMockRead(settings2, 3)};

  spdy::SpdySerializedFrame ack1(spdy_util_.ConstructSpdySettingsAck());
  spdy::SpdySerializedFrame ack2(spdy_util_.ConstructSpdySettingsAck());
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR,
      "Invalid value for spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL."));
  MockWrite writes[] = {CreateMockWrite(ack1, 1), CreateMockWrite(ack2, 4),
                        CreateMockWrite(goaway, 5)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_FALSE(session_->support_websocket());

  // Read first SETTINGS frame.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(session_->support_websocket());

  // Read second SETTINGS frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_FALSE(session_);
}

TEST_F(SpdySessionTest, GreaseFrameTypeAfterSettings) {
  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps_.greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});

  // Connection preface.
  spdy::SpdySerializedFrame preface(spdy::test::MakeSerializedFrame(
      const_cast<char*>(spdy::kHttp2ConnectionHeaderPrefix),
      spdy::kHttp2ConnectionHeaderPrefixSize));

  // Initial SETTINGS frame.
  spdy::SettingsMap expected_settings;
  expected_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;
  expected_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
      kSpdyMaxHeaderListSize;
  expected_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(expected_settings));

  spdy::SpdySerializedFrame combined_frame =
      CombineFrames({&preface, &settings_frame});

  // Greased frame sent on stream 0 after initial SETTINGS frame.
  uint8_t kRawFrameData[] = {
      0x00, 0x00, 0x03,        // length
      0x0b,                    // type
      0xcc,                    // flags
      0x00, 0x00, 0x00, 0x00,  // stream ID
      'f',  'o',  'o'          // payload
  };
  spdy::SpdySerializedFrame grease(spdy::test::MakeSerializedFrame(
      reinterpret_cast<char*>(kRawFrameData), std::size(kRawFrameData)));

  MockWrite writes[] = {CreateMockWrite(combined_frame, 0),
                        CreateMockWrite(grease, 1)};

  MockRead reads[] = {MockRead(ASYNC, 0, 2)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();
  CreateNetworkSession();

  SpdySessionPoolPeer pool_peer(spdy_session_pool_);
  pool_peer.SetEnableSendingInitialData(true);

  CreateSpdySession();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

enum ReadIfReadySupport {
  // ReadIfReady() is implemented by the underlying transport.
  READ_IF_READY_SUPPORTED,
  // ReadIfReady() is unimplemented by the underlying transport.
  READ_IF_READY_NOT_SUPPORTED,
};

class SpdySessionReadIfReadyTest
    : public SpdySessionTest,
      public testing::WithParamInterface<ReadIfReadySupport> {
 public:
  void SetUp() override {
    if (GetParam() == READ_IF_READY_SUPPORTED) {
      session_deps_.socket_factory->set_enable_read_if_ready(true);
    }
    SpdySessionTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SpdySessionReadIfReadyTest,
                         testing::Values(READ_IF_READY_SUPPORTED,
                                         READ_IF_READY_NOT_SUPPORTED));

// Tests basic functionality of ReadIfReady() when it is enabled or disabled.
TEST_P(SpdySessionReadIfReadyTest, ReadIfReady) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM, session_,
                                test_url_, HIGHEST, NetLogWithSource());
  ASSERT_TRUE(spdy_stream);
  EXPECT_EQ(0u, spdy_stream->stream_id());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(spdy_stream);
  EXPECT_EQ(1u, delegate.stream_id());
}

class SendInitialSettingsOnNewSpdySessionTest : public SpdySessionTest {
 protected:
  void RunInitialSettingsTest(const spdy::SettingsMap expected_settings) {
    MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};

    spdy::SpdySerializedFrame preface(spdy::test::MakeSerializedFrame(
        const_cast<char*>(spdy::kHttp2ConnectionHeaderPrefix),
        spdy::kHttp2ConnectionHeaderPrefixSize));
    spdy::SpdySerializedFrame settings_frame(
        spdy_util_.ConstructSpdySettings(expected_settings));

    spdy::SpdySerializedFrame combined_frame =
        CombineFrames({&preface, &settings_frame});
    MockWrite writes[] = {CreateMockWrite(combined_frame, 0)};

    StaticSocketDataProvider data(reads, writes);
    session_deps_.socket_factory->AddSocketDataProvider(&data);
    AddSSLSocketData();

    CreateNetworkSession();

    SpdySessionPoolPeer pool_peer(spdy_session_pool_);
    pool_peer.SetEnableSendingInitialData(true);

    CreateSpdySession();

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(data.AllWriteDataConsumed());
  }
};

// Setting values when Params::http2_settings is empty.  Note that
// spdy::SETTINGS_INITIAL_WINDOW_SIZE is sent in production, because it is set
// to a non-default value, but it is not sent in tests, because the protocol
// default value is used in tests.
TEST_F(SendInitialSettingsOnNewSpdySessionTest, Empty) {
  spdy::SettingsMap expected_settings;
  expected_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;
  expected_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
      kSpdyMaxHeaderListSize;
  expected_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  RunInitialSettingsTest(expected_settings);
}

// When a setting is set to the protocol default value,
// no corresponding value is sent on the wire.
TEST_F(SendInitialSettingsOnNewSpdySessionTest, ProtocolDefault) {
  // SETTINGS_ENABLE_PUSH is always overridden with value 0.
  session_deps_.http2_settings[spdy::SETTINGS_ENABLE_PUSH] = 1;

  // Explicitly set protocol default values for the following settings.
  session_deps_.http2_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = 4096;
  session_deps_.http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
      64 * 1024 - 1;

  spdy::SettingsMap expected_settings;
  expected_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
      kSpdyMaxHeaderListSize;
  expected_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  RunInitialSettingsTest(expected_settings);
}

// Values set in Params::http2_settings overwrite Chromium's default values.
TEST_F(SendInitialSettingsOnNewSpdySessionTest, OverwriteValues) {
  session_deps_.http2_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = 16 * 1024;
  session_deps_.http2_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  session_deps_.http2_settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = 42;
  session_deps_.http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = 32 * 1024;
  session_deps_.http2_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
      101 * 1024;

  spdy::SettingsMap expected_settings;
  expected_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = 16 * 1024;
  expected_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  expected_settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = 42;
  expected_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = 32 * 1024;
  expected_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] = 101 * 1024;
  RunInitialSettingsTest(expected_settings);
}

// Unknown parameters should still be sent to the server.
TEST_F(SendInitialSettingsOnNewSpdySessionTest, UnknownSettings) {
  // The following parameters are not defined in the HTTP/2 specification.
  session_deps_.http2_settings[7] = 1234;
  session_deps_.http2_settings[25] = 5678;

  spdy::SettingsMap expected_settings;
  expected_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;
  expected_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
      kSpdyMaxHeaderListSize;
  expected_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  expected_settings[7] = 1234;
  expected_settings[25] = 5678;
  RunInitialSettingsTest(expected_settings);
}

class AltSvcFrameTest : public SpdySessionTest {
 public:
  AltSvcFrameTest()
      : alternative_service_(
            quic::AlpnForVersion(DefaultSupportedQuicVersions().front()),
            "alternative.example.org",
            443,
            86400,
            spdy::SpdyAltSvcWireFormat::VersionVector()) {
    // Since the default |alternative_service_| is QUIC, need to enable QUIC for
    // the not added tests to be meaningful.
    session_deps_.enable_quic = true;
  }

  void AddSocketData(const spdy::SpdyAltSvcIR& altsvc_ir) {
    altsvc_frame_ = spdy_util_.SerializeFrame(altsvc_ir);
    reads_.push_back(CreateMockRead(altsvc_frame_, 0));
    reads_.emplace_back(ASYNC, 0, 1);

    data_ =
        std::make_unique<SequencedSocketData>(reads_, base::span<MockWrite>());
    session_deps_.socket_factory->AddSocketDataProvider(data_.get());
  }

  void CreateSpdySession() {
    session_ =
        ::net::CreateSpdySession(http_session_.get(), key_, NetLogWithSource());
  }

  spdy::SpdyAltSvcWireFormat::AlternativeService alternative_service_;

 private:
  spdy::SpdySerializedFrame altsvc_frame_;
  std::vector<MockRead> reads_;
  std::unique_ptr<SequencedSocketData> data_;
};

TEST_F(AltSvcFrameTest, ProcessAltSvcFrame) {
  const char origin[] = "https://mail.example.org";
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 0);
  altsvc_ir.add_altsvc(alternative_service_);
  altsvc_ir.set_origin(origin);
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  AlternativeServiceInfoVector altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          session_origin, NetworkAnonymizationKey());
  ASSERT_TRUE(altsvc_info_vector.empty());

  altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          url::SchemeHostPort(GURL(origin)), NetworkAnonymizationKey());
  ASSERT_EQ(1u, altsvc_info_vector.size());
  AlternativeService alternative_service(kProtoQUIC, "alternative.example.org",
                                         443u);
  EXPECT_EQ(alternative_service, altsvc_info_vector[0].alternative_service());
}

// Regression test for https://crbug.com/736063.
TEST_F(AltSvcFrameTest, IgnoreQuicAltSvcWithUnsupportedVersion) {
  session_deps_.enable_quic = true;

  // Note that this test only uses the legacy Google-specific Alt-Svc format.
  const char origin[] = "https://mail.example.org";
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 0);
  spdy::SpdyAltSvcWireFormat::AlternativeService quic_alternative_service(
      "quic", "alternative.example.org", 443, 86400,
      spdy::SpdyAltSvcWireFormat::VersionVector());
  quic_alternative_service.version.push_back(/* invalid QUIC version */ 1);
  altsvc_ir.add_altsvc(quic_alternative_service);
  altsvc_ir.set_origin(origin);
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  AlternativeServiceInfoVector altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          session_origin, NetworkAnonymizationKey());
  ASSERT_TRUE(altsvc_info_vector.empty());

  altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          url::SchemeHostPort(GURL(origin)), NetworkAnonymizationKey());
  ASSERT_EQ(0u, altsvc_info_vector.size());
}

TEST_F(AltSvcFrameTest, DoNotProcessAltSvcFrameForOriginNotCoveredByCert) {
  session_deps_.enable_quic = true;

  const char origin[] = "https://invalid.example.org";
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 0);
  altsvc_ir.add_altsvc(alternative_service_);
  altsvc_ir.set_origin(origin);
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());

  ASSERT_TRUE(
      spdy_session_pool_->http_server_properties()
          ->GetAlternativeServiceInfos(url::SchemeHostPort(GURL(origin)),
                                       NetworkAnonymizationKey())
          .empty());
}

// An ALTSVC frame on stream 0 with empty origin MUST be ignored.
// (RFC 7838 Section 4)
TEST_F(AltSvcFrameTest, DoNotProcessAltSvcFrameWithEmptyOriginOnStreamZero) {
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 0);
  altsvc_ir.add_altsvc(alternative_service_);
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());
}

// An ALTSVC frame on a stream other than stream 0 with non-empty origin MUST be
// ignored.  (RFC 7838 Section 4)
TEST_F(AltSvcFrameTest,
       DoNotProcessAltSvcFrameWithNonEmptyOriginOnNonZeroStream) {
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 1);
  altsvc_ir.add_altsvc(alternative_service_);
  altsvc_ir.set_origin("https://mail.example.org");
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());
}

TEST_F(AltSvcFrameTest, ProcessAltSvcFrameOnActiveStream) {
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 1);
  altsvc_ir.add_altsvc(alternative_service_);

  spdy::SpdySerializedFrame altsvc_frame(spdy_util_.SerializeFrame(altsvc_ir));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_REFUSED_STREAM));
  MockRead reads[] = {
      CreateMockRead(altsvc_frame, 1), CreateMockRead(rst, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  const char request_origin[] = "https://mail.example.org";
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(request_origin, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, GURL(request_origin), MEDIUM,
      NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(request_origin));

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());

  AlternativeServiceInfoVector altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          url::SchemeHostPort(GURL(request_origin)), NetworkAnonymizationKey());
  ASSERT_EQ(1u, altsvc_info_vector.size());
  EXPECT_EQ(kProtoQUIC, altsvc_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("alternative.example.org",
            altsvc_info_vector[0].alternative_service().host);
  EXPECT_EQ(443u, altsvc_info_vector[0].alternative_service().port);
}

TEST_F(AltSvcFrameTest,
       ProcessAltSvcFrameOnActiveStreamWithNetworkAnonymizationKey) {
  base::test::ScopedFeatureList feature_list;
  // Need to partition connections by NetworkAnonymizationKey for
  // SpdySessionKeys to include NetworkAnonymizationKeys.
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  session_deps_.http_server_properties =
      std::make_unique<HttpServerProperties>();

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  key_ = SpdySessionKey(HostPortPair::FromURL(test_url_), PRIVACY_MODE_DISABLED,
                        ProxyChain::Direct(), SessionUsage::kDestination,
                        SocketTag(), kNetworkAnonymizationKey1,
                        SecureDnsPolicy::kAllow,
                        /*disable_cert_verification_network_fetches=*/false);

  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 1);
  altsvc_ir.add_altsvc(alternative_service_);

  spdy::SpdySerializedFrame altsvc_frame(spdy_util_.SerializeFrame(altsvc_ir));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_REFUSED_STREAM));
  MockRead reads[] = {
      CreateMockRead(altsvc_frame, 1), CreateMockRead(rst, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  const char request_origin[] = "https://mail.example.org";
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(request_origin, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, GURL(request_origin), MEDIUM,
      NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(request_origin));

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());

  AlternativeServiceInfoVector altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          url::SchemeHostPort(GURL(request_origin)), kNetworkAnonymizationKey1);
  ASSERT_EQ(1u, altsvc_info_vector.size());
  EXPECT_EQ(kProtoQUIC, altsvc_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("alternative.example.org",
            altsvc_info_vector[0].alternative_service().host);
  EXPECT_EQ(443u, altsvc_info_vector[0].alternative_service().port);

  // Make sure the alternative service information is only associated with
  // kNetworkAnonymizationKey1.
  EXPECT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(
                      url::SchemeHostPort(GURL(request_origin)),
                      kNetworkAnonymizationKey2)
                  .empty());
  EXPECT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(
                      url::SchemeHostPort(GURL(request_origin)),
                      NetworkAnonymizationKey())
                  .empty());
}

TEST_F(AltSvcFrameTest, DoNotProcessAltSvcFrameOnStreamWithInsecureOrigin) {
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 1);
  altsvc_ir.add_altsvc(alternative_service_);

  spdy::SpdySerializedFrame altsvc_frame(spdy_util_.SerializeFrame(altsvc_ir));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_REFUSED_STREAM));
  MockRead reads[] = {
      CreateMockRead(altsvc_frame, 1), CreateMockRead(rst, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  const char request_origin[] = "http://mail.example.org";
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(request_origin, 1, MEDIUM));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session_, GURL(request_origin), MEDIUM,
      NetLogWithSource());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(request_origin));

  spdy_stream1->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());

  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(
                      url::SchemeHostPort(GURL(request_origin)),
                      NetworkAnonymizationKey())
                  .empty());
}

TEST_F(AltSvcFrameTest, DoNotProcessAltSvcFrameOnNonExistentStream) {
  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 1);
  altsvc_ir.add_altsvc(alternative_service_);
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  ASSERT_TRUE(spdy_session_pool_->http_server_properties()
                  ->GetAlternativeServiceInfos(session_origin,
                                               NetworkAnonymizationKey())
                  .empty());
}

// Regression test for https://crbug.com/810404.
TEST_F(AltSvcFrameTest, InvalidOrigin) {
  // This origin parses to an invalid GURL with https scheme.
  const std::string origin("https:?");
  const GURL origin_gurl(origin);
  EXPECT_FALSE(origin_gurl.is_valid());
  EXPECT_TRUE(origin_gurl.host().empty());
  EXPECT_TRUE(origin_gurl.SchemeIs(url::kHttpsScheme));

  spdy::SpdyAltSvcIR altsvc_ir(/* stream_id = */ 0);
  altsvc_ir.add_altsvc(alternative_service_);
  altsvc_ir.set_origin(origin);
  AddSocketData(altsvc_ir);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();

  const url::SchemeHostPort session_origin("https", test_url_.host(),
                                           test_url_.EffectiveIntPort());
  AlternativeServiceInfoVector altsvc_info_vector =
      spdy_session_pool_->http_server_properties()->GetAlternativeServiceInfos(
          session_origin, NetworkAnonymizationKey());
  EXPECT_TRUE(altsvc_info_vector.empty());
}

TEST(MapFramerErrorToProtocolError, MapsValues) {
  CHECK_EQ(SPDY_ERROR_INVALID_CONTROL_FRAME,
           MapFramerErrorToProtocolError(
               http2::Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME));
  CHECK_EQ(SPDY_ERROR_INVALID_DATA_FRAME_FLAGS,
           MapFramerErrorToProtocolError(
               http2::Http2DecoderAdapter::SPDY_INVALID_DATA_FRAME_FLAGS));
  CHECK_EQ(SPDY_ERROR_HPACK_NAME_HUFFMAN_ERROR,
           MapFramerErrorToProtocolError(
               http2::Http2DecoderAdapter::SPDY_HPACK_NAME_HUFFMAN_ERROR));
  CHECK_EQ(SPDY_ERROR_UNEXPECTED_FRAME,
           MapFramerErrorToProtocolError(
               http2::Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME));
}

TEST(MapFramerErrorToNetError, MapsValue) {
  CHECK_EQ(ERR_HTTP2_PROTOCOL_ERROR,
           MapFramerErrorToNetError(
               http2::Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME));
  CHECK_EQ(ERR_HTTP2_COMPRESSION_ERROR,
           MapFramerErrorToNetError(
               http2::Http2DecoderAdapter::SPDY_DECOMPRESS_FAILURE));
  CHECK_EQ(ERR_HTTP2_FRAME_SIZE_ERROR,
           MapFramerErrorToNetError(
               http2::Http2DecoderAdapter::SPDY_CONTROL_PAYLOAD_TOO_LARGE));
  CHECK_EQ(ERR_HTTP2_FRAME_SIZE_ERROR,
           MapFramerErrorToNetError(
               http2::Http2DecoderAdapter::SPDY_OVERSIZED_PAYLOAD));
}

TEST(MapRstStreamStatusToProtocolError, MapsValues) {
  CHECK_EQ(STATUS_CODE_PROTOCOL_ERROR,
           MapRstStreamStatusToProtocolError(spdy::ERROR_CODE_PROTOCOL_ERROR));
  CHECK_EQ(
      STATUS_CODE_FRAME_SIZE_ERROR,
      MapRstStreamStatusToProtocolError(spdy::ERROR_CODE_FRAME_SIZE_ERROR));
  CHECK_EQ(
      STATUS_CODE_ENHANCE_YOUR_CALM,
      MapRstStreamStatusToProtocolError(spdy::ERROR_CODE_ENHANCE_YOUR_CALM));
  CHECK_EQ(
      STATUS_CODE_INADEQUATE_SECURITY,
      MapRstStreamStatusToProtocolError(spdy::ERROR_CODE_INADEQUATE_SECURITY));
  CHECK_EQ(
      STATUS_CODE_HTTP_1_1_REQUIRED,
      MapRstStreamStatusToProtocolError(spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
}

TEST(MapNetErrorToGoAwayStatus, MapsValue) {
  CHECK_EQ(spdy::ERROR_CODE_INADEQUATE_SECURITY,
           MapNetErrorToGoAwayStatus(ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY));
  CHECK_EQ(spdy::ERROR_CODE_FLOW_CONTROL_ERROR,
           MapNetErrorToGoAwayStatus(ERR_HTTP2_FLOW_CONTROL_ERROR));
  CHECK_EQ(spdy::ERROR_CODE_PROTOCOL_ERROR,
           MapNetErrorToGoAwayStatus(ERR_HTTP2_PROTOCOL_ERROR));
  CHECK_EQ(spdy::ERROR_CODE_COMPRESSION_ERROR,
           MapNetErrorToGoAwayStatus(ERR_HTTP2_COMPRESSION_ERROR));
  CHECK_EQ(spdy::ERROR_CODE_FRAME_SIZE_ERROR,
           MapNetErrorToGoAwayStatus(ERR_HTTP2_FRAME_SIZE_ERROR));
  CHECK_EQ(spdy::ERROR_CODE_PROTOCOL_ERROR,
           MapNetErrorToGoAwayStatus(ERR_UNEXPECTED));
}

namespace {

class TestSSLConfigService : public SSLConfigService {
 public:
  TestSSLConfigService() = default;
  ~TestSSLConfigService() override = default;

  SSLContextConfig GetSSLContextConfig() override { return config_; }

  // Returns true if |hostname| is in domains_for_pooling_. This is a simpler
  // implementation than the production implementation in SSLConfigServiceMojo.
  bool CanShareConnectionWithClientCerts(
      std::string_view hostname) const override {
    return base::Contains(domains_for_pooling_, hostname);
  }

  void SetDomainsForPooling(const std::vector<std::string>& domains) {
    domains_for_pooling_ = domains;
  }

 private:
  SSLContextConfig config_;
  std::vector<std::string> domains_for_pooling_;
};

}  // namespace

TEST(CanPoolTest, CanPool) {
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   mail.example.com

  TransportSecurityState tss;
  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert = ImportCertFromFile(GetTestCertsDirectory(),
                                     "spdy_pooling.pem");

  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "www.example.org"));
  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "mail.example.org"));
  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "mail.example.com"));
  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "www.example.org", "mail.google.com"));
}

TEST(CanPoolTest, CanNotPoolWithCertErrors) {
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   mail.example.com

  TransportSecurityState tss;
  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert = ImportCertFromFile(GetTestCertsDirectory(),
                                     "spdy_pooling.pem");
  ssl_info.cert_status = CERT_STATUS_REVOKED;

  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "www.example.org", "mail.example.org"));
}

TEST(CanPoolTest, CanNotPoolWithClientCerts) {
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   mail.example.com

  TransportSecurityState tss;
  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert = ImportCertFromFile(GetTestCertsDirectory(),
                                     "spdy_pooling.pem");
  ssl_info.client_cert_sent = true;

  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "www.example.org", "mail.example.org"));
}

TEST(CanPoolTest, CanNotPoolWithBadPins) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  TransportSecurityState tss;
  tss.EnableStaticPinsForTesting();
  tss.SetPinningListAlwaysTimelyForTesting(true);
  ScopedTransportSecurityStateSource scoped_security_state_source;

  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert = ImportCertFromFile(GetTestCertsDirectory(),
                                     "spdy_pooling.pem");
  ssl_info.is_issued_by_known_root = true;
  uint8_t bad_pin = 3;
  ssl_info.public_key_hashes.push_back(test::GetTestHashValue(bad_pin));

  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "www.example.org", "example.test"));
}

TEST(CanPoolTest, CanNotPoolWithBadCTWhenCTRequired) {
  using testing::Return;
  using CTRequirementLevel =
      TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ssl_info.is_issued_by_known_root = true;
  ssl_info.public_key_hashes.push_back(test::GetTestHashValue(1));
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  MockRequireCTDelegate require_ct_delegate;
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost("www.example.org", _, _))
      .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate,
              IsCTRequiredForHost("mail.example.org", _, _))
      .WillRepeatedly(Return(CTRequirementLevel::REQUIRED));

  TransportSecurityState tss;
  tss.SetRequireCTDelegate(&require_ct_delegate);

  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "www.example.org", "mail.example.org"));
}

TEST(CanPoolTest, CanPoolWithBadCTWhenCTNotRequired) {
  using testing::Return;
  using CTRequirementLevel =
      TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ssl_info.is_issued_by_known_root = true;
  ssl_info.public_key_hashes.push_back(test::GetTestHashValue(1));
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  MockRequireCTDelegate require_ct_delegate;
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost("www.example.org", _, _))
      .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate,
              IsCTRequiredForHost("mail.example.org", _, _))
      .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));

  TransportSecurityState tss;
  tss.SetRequireCTDelegate(&require_ct_delegate);

  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "mail.example.org"));
}

TEST(CanPoolTest, CanPoolWithGoodCTWhenCTRequired) {
  using testing::Return;
  using CTRequirementLevel =
      TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ssl_info.is_issued_by_known_root = true;
  ssl_info.public_key_hashes.push_back(test::GetTestHashValue(1));
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  MockRequireCTDelegate require_ct_delegate;
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost("www.example.org", _, _))
      .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate,
              IsCTRequiredForHost("mail.example.org", _, _))
      .WillRepeatedly(Return(CTRequirementLevel::REQUIRED));

  TransportSecurityState tss;
  tss.SetRequireCTDelegate(&require_ct_delegate);

  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "mail.example.org"));
}

TEST(CanPoolTest, CanPoolWithAcceptablePins) {
  TransportSecurityState tss;
  tss.EnableStaticPinsForTesting();
  tss.SetPinningListAlwaysTimelyForTesting(true);
  ScopedTransportSecurityStateSource scoped_security_state_source;

  TestSSLConfigService ssl_config_service;
  SSLInfo ssl_info;
  ssl_info.cert = ImportCertFromFile(GetTestCertsDirectory(),
                                     "spdy_pooling.pem");
  ssl_info.is_issued_by_known_root = true;
  HashValue hash;
  // The expected value of GoodPin1 used by |scoped_security_state_source|.
  ASSERT_TRUE(
      hash.FromString("sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  ssl_info.public_key_hashes.push_back(hash);

  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "mail.example.org"));
}

TEST(CanPoolTest, CanPoolWithClientCertsAndPolicy) {
  TransportSecurityState tss;
  SSLInfo ssl_info;
  ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ssl_info.client_cert_sent = true;

  // Configure ssl_config_service so that CanShareConnectionWithClientCerts
  // returns true for www.example.org and mail.example.org.
  TestSSLConfigService ssl_config_service;
  ssl_config_service.SetDomainsForPooling(
      {"www.example.org", "mail.example.org"});

  // Test that CanPool returns true when client certs are enabled and
  // CanShareConnectionWithClientCerts returns true for both hostnames, but not
  // just one hostname.
  EXPECT_TRUE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                   "www.example.org", "mail.example.org"));
  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "www.example.org", "mail.example.com"));
  EXPECT_FALSE(SpdySession::CanPool(&tss, ssl_info, ssl_config_service,
                                    "mail.example.com", "www.example.org"));
}

// Regression test for https://crbug.com/1115492.
TEST_F(SpdySessionTest, UpdateHeaderTableSize) {
  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = 12345;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  MockRead reads[] = {CreateMockRead(settings_frame, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};

  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {CreateMockWrite(settings_ack, 1)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_EQ(spdy::kDefaultHeaderTableSizeSetting, header_encoder_table_size());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(12345u, header_encoder_table_size());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(SpdySessionTest, PriorityUpdateDisabled) {
  session_deps_.enable_priority_update = false;

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES] = 1;
  auto settings_frame = spdy_util_.ConstructSpdySettings(settings);
  auto settings_ack = spdy_util_.ConstructSpdySettingsAck();

  MockRead reads[] = {CreateMockRead(settings_frame, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};
  MockWrite writes[] = {CreateMockWrite(settings_ack, 1)};
  SequencedSocketData data(reads, writes);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // HTTP/2 priorities enabled by default.
  // PRIORITY_UPDATE is disabled by |enable_priority_update| = false.
  EXPECT_TRUE(session_->ShouldSendHttp2Priority());
  EXPECT_FALSE(session_->ShouldSendPriorityUpdate());

  // Receive SETTINGS frame.
  base::RunLoop().RunUntilIdle();

  // Since |enable_priority_update| = false,
  // SETTINGS_DEPRECATE_HTTP2_PRIORITIES has no effect.
  EXPECT_TRUE(session_->ShouldSendHttp2Priority());
  EXPECT_FALSE(session_->ShouldSendPriorityUpdate());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(SpdySessionTest, PriorityUpdateEnabledHttp2PrioritiesDeprecated) {
  session_deps_.enable_priority_update = true;

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES] = 1;
  auto settings_frame = spdy_util_.ConstructSpdySettings(settings);
  auto settings_ack = spdy_util_.ConstructSpdySettingsAck();

  MockRead reads[] = {CreateMockRead(settings_frame, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};
  MockWrite writes[] = {CreateMockWrite(settings_ack, 1)};
  SequencedSocketData data(reads, writes);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Both priority schemes are enabled until SETTINGS frame is received.
  EXPECT_TRUE(session_->ShouldSendHttp2Priority());
  EXPECT_TRUE(session_->ShouldSendPriorityUpdate());

  // Receive SETTINGS frame.
  base::RunLoop().RunUntilIdle();

  // SETTINGS_DEPRECATE_HTTP2_PRIORITIES = 1 disables HTTP/2 priorities.
  EXPECT_FALSE(session_->ShouldSendHttp2Priority());
  EXPECT_TRUE(session_->ShouldSendPriorityUpdate());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(SpdySessionTest, PriorityUpdateEnabledHttp2PrioritiesNotDeprecated) {
  session_deps_.enable_priority_update = true;

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES] = 0;
  auto settings_frame = spdy_util_.ConstructSpdySettings(settings);
  auto settings_ack = spdy_util_.ConstructSpdySettingsAck();

  MockRead reads[] = {CreateMockRead(settings_frame, 0),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};
  MockWrite writes[] = {CreateMockWrite(settings_ack, 1)};
  SequencedSocketData data(reads, writes);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Both priority schemes are enabled until SETTINGS frame is received.
  EXPECT_TRUE(session_->ShouldSendHttp2Priority());
  EXPECT_TRUE(session_->ShouldSendPriorityUpdate());

  // Receive SETTINGS frame.
  base::RunLoop().RunUntilIdle();

  // SETTINGS_DEPRECATE_HTTP2_PRIORITIES = 0 disables PRIORITY_UPDATE.
  EXPECT_TRUE(session_->ShouldSendHttp2Priority());
  EXPECT_FALSE(session_->ShouldSendPriorityUpdate());

  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(SpdySessionTest, SettingsDeprecateHttp2PrioritiesValueMustNotChange) {
  spdy::SettingsMap settings0;
  settings0[spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES] = 0;
  auto settings_frame0 = spdy_util_.ConstructSpdySettings(settings0);
  spdy::SettingsMap settings1;
  settings1[spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES] = 1;
  auto settings_frame1 = spdy_util_.ConstructSpdySettings(settings1);
  MockRead reads[] = {
      CreateMockRead(settings_frame1, 0), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(settings_frame1, 3), MockRead(ASYNC, ERR_IO_PENDING, 5),
      CreateMockRead(settings_frame0, 6)};

  auto settings_ack = spdy_util_.ConstructSpdySettingsAck();
  auto goaway = spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR,
      "spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES value changed after first "
      "SETTINGS frame.");
  MockWrite writes[] = {
      CreateMockWrite(settings_ack, 1), CreateMockWrite(settings_ack, 4),
      CreateMockWrite(settings_ack, 7), CreateMockWrite(goaway, 8)};

  SequencedSocketData data(reads, writes);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  base::RunLoop().RunUntilIdle();
  data.Resume();
  base::RunLoop().RunUntilIdle();
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(SpdySessionTest, AlpsEmpty) {
  base::HistogramTester histogram_tester;

  ssl_.peer_application_settings = "";

  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsDecoderStatus",
      static_cast<int>(AlpsDecoder::Error::kNoError), 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsSettingParameterCount", 0, 1);
  const int kNoEntries = 0;
  histogram_tester.ExpectUniqueSample("Net.SpdySession.AlpsAcceptChEntries",
                                      kNoEntries, 1);

  histogram_tester.ExpectTotalCount("Net.SpdySession.AcceptChForOrigin", 0);
  EXPECT_EQ("", session_->GetAcceptChViaAlps(
                    url::SchemeHostPort(GURL("https://www.example.org"))));
  histogram_tester.ExpectUniqueSample("Net.SpdySession.AcceptChForOrigin",
                                      false, 1);
}

TEST_F(SpdySessionTest, AlpsSettings) {
  base::HistogramTester histogram_tester;

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = 12345;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  ssl_.peer_application_settings =
      std::string(settings_frame.data(), settings_frame.size());

  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  EXPECT_EQ(12345u, header_encoder_table_size());

  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsDecoderStatus",
      static_cast<int>(AlpsDecoder::Error::kNoError), 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsSettingParameterCount", 1, 1);
}

TEST_F(SpdySessionTest, AlpsAcceptCh) {
  base::HistogramTester histogram_tester;

  ssl_.peer_application_settings = HexDecode(
      "00001e"                    // length
      "89"                        // type ACCEPT_CH
      "00"                        // flags
      "00000000"                  // stream ID
      "0017"                      // origin length
      "68747470733a2f2f7777772e"  //
      "6578616d706c652e636f6d"    // origin "https://www.example.com"
      "0003"                      // value length
      "666f6f");                  // value "foo"

  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsDecoderStatus",
      static_cast<int>(AlpsDecoder::Error::kNoError), 1);
  const int kOnlyValidEntries = 1;
  histogram_tester.ExpectUniqueSample("Net.SpdySession.AlpsAcceptChEntries",
                                      kOnlyValidEntries, 1);

  histogram_tester.ExpectTotalCount("Net.SpdySession.AcceptChForOrigin", 0);

  EXPECT_EQ("foo", session_->GetAcceptChViaAlps(
                       url::SchemeHostPort(GURL("https://www.example.com"))));
  histogram_tester.ExpectUniqueSample("Net.SpdySession.AcceptChForOrigin", true,
                                      1);

  EXPECT_EQ("", session_->GetAcceptChViaAlps(
                    url::SchemeHostPort(GURL("https://www.example.org"))));
  histogram_tester.ExpectTotalCount("Net.SpdySession.AcceptChForOrigin", 2);
  histogram_tester.ExpectBucketCount("Net.SpdySession.AcceptChForOrigin", true,
                                     1);
  histogram_tester.ExpectBucketCount("Net.SpdySession.AcceptChForOrigin", false,
                                     1);
}

TEST_F(SpdySessionTest, AlpsAcceptChInvalidOrigin) {
  base::HistogramTester histogram_tester;

  // "www.example.com" is not a valid origin, because it does not have a scheme.
  ssl_.peer_application_settings = HexDecode(
      "000017"                            // length
      "89"                                // type ACCEPT_CH
      "00"                                // flags
      "00000000"                          // stream ID
      "0010"                              // origin length
      "2f7777772e6578616d706c652e636f6d"  // origin "www.example.com"
      "0003"                              // value length
      "666f6f");                          // value "foo"

  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  // Invalid origin error is not considered fatal for the connection.
  EXPECT_TRUE(session_->IsAvailable());

  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsDecoderStatus",
      static_cast<int>(AlpsDecoder::Error::kNoError), 1);
  const int kOnlyInvalidEntries = 2;
  histogram_tester.ExpectUniqueSample("Net.SpdySession.AlpsAcceptChEntries",
                                      kOnlyInvalidEntries, 1);
}

// Test that ConfirmHandshake() correctly handles the client aborting the
// connection. See https://crbug.com/1211639.
TEST_F(SpdySessionTest, ConfirmHandshakeAfterClose) {
  base::HistogramTester histogram_tester;

  session_deps_.enable_early_data = true;
  // Arrange for StreamSocket::ConfirmHandshake() to hang.
  ssl_.confirm = MockConfirm(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();
  CreateSpdySession();

  TestCompletionCallback callback1;
  int rv1 = session_->ConfirmHandshake(callback1.callback());
  EXPECT_THAT(rv1, IsError(ERR_IO_PENDING));

  // Abort the session. Although the underlying StreamSocket::ConfirmHandshake()
  // operation never completes, SpdySession::ConfirmHandshake() is signaled when
  // the session is discarded.
  session_->CloseSessionOnError(ERR_ABORTED, "Aborting session");
  EXPECT_THAT(callback1.GetResult(rv1), IsError(ERR_ABORTED));

  // Subsequent calls to SpdySession::ConfirmHandshake() fail gracefully. This
  // tests that SpdySession honors StreamSocket::ConfirmHandshake() invariants.
  // (MockSSLClientSocket::ConfirmHandshake() checks it internally.)
  TestCompletionCallback callback2;
  int rv2 = session_->ConfirmHandshake(callback2.callback());
  EXPECT_THAT(rv2, IsError(ERR_CONNECTION_CLOSED));
}

}  // namespace net
