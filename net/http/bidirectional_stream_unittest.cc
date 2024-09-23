// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/bidirectional_stream.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const char kBodyData[] = "Body data";
const size_t kBodyDataSize = std::size(kBodyData);
const std::string kBodyDataString(kBodyData, kBodyDataSize);
// Size of the buffer to be allocated for each read.
const size_t kReadBufferSize = 4096;

// Expects that fields of |load_timing_info| are valid time stamps.
void ExpectLoadTimingValid(const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.request_start.is_null());
  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.receive_headers_end.is_null());
  EXPECT_FALSE(load_timing_info.send_start.is_null());
  EXPECT_FALSE(load_timing_info.send_end.is_null());
  EXPECT_TRUE(load_timing_info.request_start <=
              load_timing_info.receive_headers_end);
  EXPECT_TRUE(load_timing_info.send_start <= load_timing_info.send_end);
}

// Tests the load timing of a stream that's connected and is not the first
// request sent on a connection.
void TestLoadTimingReused(const LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingValid(load_timing_info);
}

// Tests the load timing of a stream that's connected and using a fresh
// connection.
void TestLoadTimingNotReused(const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);

  ExpectConnectTimingHasTimes(
      load_timing_info.connect_timing,
      CONNECT_TIMING_HAS_SSL_TIMES | CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingValid(load_timing_info);
}

// Delegate that reads data but does not send any data.
class TestDelegateBase : public BidirectionalStream::Delegate {
 public:
  TestDelegateBase(IOBuffer* read_buf, int read_buf_len)
      : TestDelegateBase(read_buf,
                         read_buf_len,
                         std::make_unique<base::OneShotTimer>()) {}

  TestDelegateBase(IOBuffer* read_buf,
                   int read_buf_len,
                   std::unique_ptr<base::OneShotTimer> timer)
      : read_buf_(read_buf),
        read_buf_len_(read_buf_len),
        timer_(std::move(timer)) {}

  TestDelegateBase(const TestDelegateBase&) = delete;
  TestDelegateBase& operator=(const TestDelegateBase&) = delete;

  ~TestDelegateBase() override = default;

  void OnStreamReady(bool request_headers_sent) override {
    // Request headers should always be sent in H2's case, because the
    // functionality to combine header frame with data frames is not
    // implemented.
    EXPECT_TRUE(request_headers_sent);
    if (callback_.is_null())
      return;
    std::move(callback_).Run(OK);
  }

  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override {
    CHECK(!not_expect_callback_);

    response_headers_ = response_headers.Clone();

    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataRead(int bytes_read) override {
    CHECK(!not_expect_callback_);

    ++on_data_read_count_;
    CHECK_GE(bytes_read, OK);
    data_received_.append(read_buf_->data(), bytes_read);
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataSent() override {
    CHECK(!not_expect_callback_);

    ++on_data_sent_count_;
  }

  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override {
    CHECK(!not_expect_callback_);

    trailers_ = trailers.Clone();
    if (run_until_completion_)
      loop_->Quit();
  }

  void OnFailed(int error) override {
    CHECK(!not_expect_callback_);
    CHECK_EQ(OK, error_);
    CHECK_NE(OK, error);

    error_ = error;
    if (run_until_completion_)
      loop_->Quit();
  }

  void Start(std::unique_ptr<BidirectionalStreamRequestInfo> request_info,
             HttpNetworkSession* session) {
    stream_ = std::make_unique<BidirectionalStream>(
        std::move(request_info), session, true, this, std::move(timer_));
    if (run_until_completion_)
      loop_->Run();
  }

  void Start(std::unique_ptr<BidirectionalStreamRequestInfo> request_info,
             HttpNetworkSession* session,
             CompletionOnceCallback cb) {
    callback_ = std::move(cb);
    stream_ = std::make_unique<BidirectionalStream>(
        std::move(request_info), session, true, this, std::move(timer_));
    if (run_until_completion_)
      WaitUntilCompletion();
  }

  void WaitUntilCompletion() { loop_->Run(); }

  void SendData(const scoped_refptr<IOBuffer>& data,
                int length,
                bool end_of_stream) {
    SendvData({data}, {length}, end_of_stream);
  }

  void SendvData(const std::vector<scoped_refptr<IOBuffer>>& data,
                 const std::vector<int>& length,
                 bool end_of_stream) {
    not_expect_callback_ = true;
    stream_->SendvData(data, length, end_of_stream);
    not_expect_callback_ = false;
  }

  // Starts or continues reading data from |stream_| until no more bytes
  // can be read synchronously.
  void StartOrContinueReading() {
    int rv = ReadData();
    while (rv > 0) {
      rv = ReadData();
    }
    if (run_until_completion_ && rv == 0)
      loop_->Quit();
  }

  // Calls ReadData on the |stream_| and updates internal states.
  int ReadData() {
    not_expect_callback_ = true;
    int rv = stream_->ReadData(read_buf_.get(), read_buf_len_);
    not_expect_callback_ = false;
    if (rv > 0)
      data_received_.append(read_buf_->data(), rv);
    return rv;
  }

  // Deletes |stream_|.
  void DeleteStream() {
    next_proto_ = stream_->GetProtocol();
    received_bytes_ = stream_->GetTotalReceivedBytes();
    sent_bytes_ = stream_->GetTotalSentBytes();
    stream_->GetLoadTimingInfo(&load_timing_info_);
    stream_.reset();
  }

  NextProto GetProtocol() const {
    if (stream_)
      return stream_->GetProtocol();
    return next_proto_;
  }

  int64_t GetTotalReceivedBytes() const {
    if (stream_)
      return stream_->GetTotalReceivedBytes();
    return received_bytes_;
  }

  int64_t GetTotalSentBytes() const {
    if (stream_)
      return stream_->GetTotalSentBytes();
    return sent_bytes_;
  }

  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
    if (stream_) {
      stream_->GetLoadTimingInfo(load_timing_info);
      return;
    }
    *load_timing_info = load_timing_info_;
  }

  // Const getters for internal states.
  const std::string& data_received() const { return data_received_; }
  int error() const { return error_; }
  const quiche::HttpHeaderBlock& response_headers() const {
    return response_headers_;
  }
  const quiche::HttpHeaderBlock& trailers() const { return trailers_; }
  int on_data_read_count() const { return on_data_read_count_; }
  int on_data_sent_count() const { return on_data_sent_count_; }

  // Sets whether the delegate should automatically start reading.
  void set_do_not_start_read(bool do_not_start_read) {
    do_not_start_read_ = do_not_start_read;
  }
  // Sets whether the delegate should wait until the completion of the stream.
  void SetRunUntilCompletion(bool run_until_completion) {
    run_until_completion_ = run_until_completion;
    loop_ = std::make_unique<base::RunLoop>();
  }

 protected:
  // Quits |loop_|.
  void QuitLoop() { loop_->Quit(); }

 private:
  std::unique_ptr<BidirectionalStream> stream_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  std::unique_ptr<base::OneShotTimer> timer_;
  std::string data_received_;
  std::unique_ptr<base::RunLoop> loop_;
  quiche::HttpHeaderBlock response_headers_;
  quiche::HttpHeaderBlock trailers_;
  NextProto next_proto_;
  int64_t received_bytes_ = 0;
  int64_t sent_bytes_ = 0;
  LoadTimingInfo load_timing_info_;
  int error_ = OK;
  int on_data_read_count_ = 0;
  int on_data_sent_count_ = 0;
  bool do_not_start_read_ = false;
  bool run_until_completion_ = false;
  // This is to ensure that delegate callback is not invoked synchronously when
  // calling into |stream_|.
  bool not_expect_callback_ = false;

  CompletionOnceCallback callback_;
};

// A delegate that deletes the stream in a particular callback.
class DeleteStreamDelegate : public TestDelegateBase {
 public:
  // Specifies in which callback the stream can be deleted.
  enum Phase {
    ON_HEADERS_RECEIVED,
    ON_DATA_READ,
    ON_TRAILERS_RECEIVED,
    ON_FAILED,
  };

  DeleteStreamDelegate(IOBuffer* buf, int buf_len, Phase phase)
      : TestDelegateBase(buf, buf_len), phase_(phase) {}

  DeleteStreamDelegate(const DeleteStreamDelegate&) = delete;
  DeleteStreamDelegate& operator=(const DeleteStreamDelegate&) = delete;

  ~DeleteStreamDelegate() override = default;

  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override {
    TestDelegateBase::OnHeadersReceived(response_headers);
    if (phase_ == ON_HEADERS_RECEIVED) {
      DeleteStream();
      QuitLoop();
    }
  }

  void OnDataSent() override { NOTREACHED_IN_MIGRATION(); }

  void OnDataRead(int bytes_read) override {
    if (phase_ == ON_HEADERS_RECEIVED) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    TestDelegateBase::OnDataRead(bytes_read);
    if (phase_ == ON_DATA_READ) {
      DeleteStream();
      QuitLoop();
    }
  }

  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override {
    if (phase_ == ON_HEADERS_RECEIVED || phase_ == ON_DATA_READ) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    TestDelegateBase::OnTrailersReceived(trailers);
    if (phase_ == ON_TRAILERS_RECEIVED) {
      DeleteStream();
      QuitLoop();
    }
  }

  void OnFailed(int error) override {
    if (phase_ != ON_FAILED) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    TestDelegateBase::OnFailed(error);
    DeleteStream();
    QuitLoop();
  }

 private:
  // Indicates in which callback the delegate should cancel or delete the
  // stream.
  Phase phase_;
};

// A Timer that does not start a delayed task unless the timer is fired.
class MockTimer : public base::MockOneShotTimer {
 public:
  MockTimer() = default;

  MockTimer(const MockTimer&) = delete;
  MockTimer& operator=(const MockTimer&) = delete;

  ~MockTimer() override = default;

  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::OnceClosure user_task) override {
    // Sets a maximum delay, so the timer does not fire unless it is told to.
    base::TimeDelta infinite_delay = base::TimeDelta::Max();
    base::MockOneShotTimer::Start(posted_from, infinite_delay,
                                  std::move(user_task));
  }
};

}  // namespace

class BidirectionalStreamTest : public TestWithTaskEnvironment {
 public:
  BidirectionalStreamTest()
      : default_url_(kDefaultUrl),
        host_port_pair_(HostPortPair::FromURL(default_url_)),
        ssl_data_(SSLSocketDataProvider(ASYNC, OK)) {
    // Explicitly disable HappyEyeballsV3 because it doesn't support
    // bidirectional streams.
    // TODO(crbug.com/346835898): Support bidirectional streams in
    // HappyEyeballsV3.
    feature_list_.InitAndDisableFeature(features::kHappyEyeballsV3);
    ssl_data_.next_proto = kProtoHTTP2;
    ssl_data_.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
    net_log_observer_.SetObserverCaptureMode(NetLogCaptureMode::kEverything);
    auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
    socket_factory_ = socket_factory.get();
    session_deps_.socket_factory = std::move(socket_factory);
  }

 protected:
  void TearDown() override {
    if (sequenced_data_) {
      EXPECT_TRUE(sequenced_data_->AllReadDataConsumed());
      EXPECT_TRUE(sequenced_data_->AllWriteDataConsumed());
    }
  }

  // Initializes the session using SequencedSocketData.
  void InitSession(base::span<const MockRead> reads,
                   base::span<const MockWrite> writes,
                   const SocketTag& socket_tag) {
    ASSERT_TRUE(ssl_data_.ssl_info.cert.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data_);
    sequenced_data_ = std::make_unique<SequencedSocketData>(reads, writes);
    session_deps_.socket_factory->AddSocketDataProvider(sequenced_data_.get());
    session_deps_.net_log = NetLog::Get();
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    SpdySessionKey key(host_port_pair_, PRIVACY_MODE_DISABLED,
                       ProxyChain::Direct(), SessionUsage::kDestination,
                       socket_tag, NetworkAnonymizationKey(),
                       SecureDnsPolicy::kAllow,
                       /*disable_cert_verification_network_fetches=*/false);
    session_ =
        CreateSpdySession(http_session_.get(), key,
                          NetLogWithSource::Make(NetLogSourceType::NONE));
  }

  RecordingNetLogObserver net_log_observer_;
  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  const GURL default_url_;
  const HostPortPair host_port_pair_;
  std::unique_ptr<SequencedSocketData> sequenced_data_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  raw_ptr<MockTaggingClientSocketFactory> socket_factory_;

 private:
  SSLSocketDataProvider ssl_data_;
  base::WeakPtr<SpdySession> session_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidirectionalStreamTest, CreateInsecureStream) {
  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = GURL("http://www.example.org/");

  auto session = std::make_unique<HttpNetworkSession>(
      SpdySessionDependencies::CreateSessionParams(&session_deps_),
      SpdySessionDependencies::CreateSessionContext(&session_deps_));
  TestDelegateBase delegate(nullptr, 0);
  delegate.SetRunUntilCompletion(true);
  delegate.Start(std::move(request_info), session.get());

  EXPECT_THAT(delegate.error(), IsError(ERR_DISALLOWED_URL_SCHEME));
}

TEST_F(BidirectionalStreamTest, SimplePostRequest) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOW, nullptr, 0));
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame, 3),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame, 4), MockRead(ASYNC, 0, 5),
  };
  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->extra_headers.SetHeader(HttpRequestHeaders::kContentLength,
                                        base::NumberToString(kBodyDataSize));
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);
  delegate->Start(std::move(request_info), http_session_.get());
  sequenced_data_->RunUntilPaused();

  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(kBodyDataString);
  delegate->SendData(buf.get(), buf->size(), true);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  LoadTimingInfo load_timing_info;
  delegate->GetLoadTimingInfo(&load_timing_info);
  TestLoadTimingNotReused(load_timing_info);

  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, LoadTimingTwoRequests) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, /*stream_id=*/1, LOW));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, /*stream_id=*/3, LOW));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(req2, 2),
  };
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, /*stream_id=*/1));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, /*stream_id=*/3));
  spdy::SpdySerializedFrame resp_body(
      spdy_util_.ConstructSpdyDataFrame(/*stream_id=*/1, /*fin=*/true));
  spdy::SpdySerializedFrame resp_body2(
      spdy_util_.ConstructSpdyDataFrame(/*stream_id=*/3, /*fin=*/true));
  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(resp_body, 3),
                      CreateMockRead(resp2, 4), CreateMockRead(resp_body2, 5),
                      MockRead(ASYNC, 0, 6)};
  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->end_stream_on_headers = true;
  auto request_info2 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info2->method = "GET";
  request_info2->url = default_url_;
  request_info2->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto read_buffer2 = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);
  auto delegate2 =
      std::make_unique<TestDelegateBase>(read_buffer2.get(), kReadBufferSize);
  delegate->Start(std::move(request_info), http_session_.get());
  delegate2->Start(std::move(request_info2), http_session_.get());
  delegate->SetRunUntilCompletion(true);
  delegate2->SetRunUntilCompletion(true);
  base::RunLoop().RunUntilIdle();

  delegate->WaitUntilCompletion();
  delegate2->WaitUntilCompletion();
  LoadTimingInfo load_timing_info;
  delegate->GetLoadTimingInfo(&load_timing_info);
  TestLoadTimingNotReused(load_timing_info);
  LoadTimingInfo load_timing_info2;
  delegate2->GetLoadTimingInfo(&load_timing_info2);
  TestLoadTimingReused(load_timing_info2);
}

// Creates a BidirectionalStream with an insecure scheme. Destroy the stream
// without waiting for the OnFailed task to be executed.
TEST_F(BidirectionalStreamTest,
       CreateInsecureStreamAndDestroyStreamRightAfter) {
  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = GURL("http://www.example.org/");

  auto delegate = std::make_unique<TestDelegateBase>(nullptr, 0);
  auto session = std::make_unique<HttpNetworkSession>(
      SpdySessionDependencies::CreateSessionParams(&session_deps_),
      SpdySessionDependencies::CreateSessionContext(&session_deps_));
  delegate->Start(std::move(request_info), session.get());
  // Reset stream right before the OnFailed task is executed.
  delegate.reset();

  base::RunLoop().RunUntilIdle();
}

TEST_F(BidirectionalStreamTest, ClientAuthRequestIgnored) {
  auto cert_request = base::MakeRefCounted<SSLCertRequestInfo>();
  cert_request->host_and_port = host_port_pair_;

  // First attempt receives client auth request.
  SSLSocketDataProvider ssl_data1(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl_data1.next_proto = kProtoHTTP2;
  ssl_data1.cert_request_info = cert_request;

  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data1);
  StaticSocketDataProvider socket_data1;
  session_deps_.socket_factory->AddSocketDataProvider(&socket_data1);

  // Second attempt succeeds.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      CreateMockRead(body_frame, 2),
      MockRead(SYNCHRONOUS, OK, 3),
  };

  SSLSocketDataProvider ssl_data2(ASYNC, OK);
  ssl_data2.next_proto = kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data2);
  SequencedSocketData socket_data2(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&socket_data2);

  http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  SpdySessionKey key(host_port_pair_, PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->end_stream_on_headers = true;
  request_info->priority = LOWEST;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);

  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());

  // Ensure the certificate was added to the client auth cache.
  scoped_refptr<X509Certificate> client_cert;
  scoped_refptr<SSLPrivateKey> client_private_key;
  ASSERT_TRUE(http_session_->ssl_client_context()->GetClientCertificate(
      host_port_pair_, &client_cert, &client_private_key));
  ASSERT_FALSE(client_cert);
  ASSERT_FALSE(client_private_key);

  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
}

// Simulates user calling ReadData after END_STREAM has been received in
// BidirectionalStreamSpdyImpl.
TEST_F(BidirectionalStreamTest, TestReadDataAfterClose) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));

  spdy::SpdySerializedFrame body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));
  // Last body frame has END_STREAM flag set.
  spdy::SpdySerializedFrame last_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(body_frame, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      CreateMockRead(body_frame, 5),
      CreateMockRead(last_body_frame, 6),
      MockRead(SYNCHRONOUS, 0, 7),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->end_stream_on_headers = true;
  request_info->priority = LOWEST;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  // Create a MockTimer. Retain a raw pointer since the underlying
  // BidirectionalStreamImpl owns it.
  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->set_do_not_start_read(true);

  delegate->Start(std::move(request_info), http_session_.get());

  // Write request, and deliver response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer_ptr->IsRunning());
  // ReadData returns asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Deliver a DATA frame.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  timer_ptr->Fire();
  // Asynchronous completion callback is invoke.
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 1,
            static_cast<int>(delegate->data_received().size()));

  // Deliver the rest. Note that user has not called a second ReadData.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  // ReadData now. Read should complete synchronously.
  rv = delegate->ReadData();
  EXPECT_EQ(kUploadDataSize * 2, rv);
  rv = delegate->ReadData();
  EXPECT_THAT(rv, IsOk());  // EOF.

  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

// Tests that the NetLog contains correct entries.
TEST_F(BidirectionalStreamTest, TestNetLogContainEntries) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame, 3),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame response_body_frame2(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  quiche::HttpHeaderBlock trailers;
  trailers["foo"] = "bar";
  spdy::SpdySerializedFrame response_trailers(
      spdy_util_.ConstructSpdyResponseHeaders(1, std::move(trailers), true));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame1, 4),
      MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause.
      CreateMockRead(response_body_frame2, 6),
      CreateMockRead(response_trailers, 7),
      MockRead(ASYNC, 0, 8),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(
      HttpRequestHeaders::kContentLength,
      base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer_ptr->IsRunning());

  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(kBodyDataString);
  // Send a DATA frame.
  delegate->SendData(buf, buf->size(), true);
  // ReadData returns asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Deliver the first DATA frame.
  sequenced_data_->Resume();
  sequenced_data_->RunUntilPaused();
  // |sequenced_data_| is now stopped after delivering first DATA frame but
  // before the second DATA frame.
  // Fire the timer to allow the first ReadData to complete asynchronously.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate->on_data_read_count());

  // Now let |sequenced_data_| run until completion.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  // All data has been delivered, and OnClosed() has been invoked.
  // Read now, and it should complete synchronously.
  rv = delegate->ReadData();
  EXPECT_EQ(kUploadDataSize, rv);
  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ("bar", delegate->trailers().find("foo")->second);
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());

  // Destroy the delegate will destroy the stream, so we can get an end event
  // for BIDIRECTIONAL_STREAM_ALIVE.
  delegate.reset();
  auto entries = net_log_observer_.GetEntries();

  size_t index = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::BIDIRECTIONAL_STREAM_ALIVE,
      NetLogEventPhase::BEGIN);
  // HTTP_STREAM_REQUEST is nested inside in BIDIRECTIONAL_STREAM_ALIVE.
  index = ExpectLogContainsSomewhere(entries, index,
                                     NetLogEventType::HTTP_STREAM_REQUEST,
                                     NetLogEventPhase::BEGIN);
  index = ExpectLogContainsSomewhere(entries, index,
                                     NetLogEventType::HTTP_STREAM_REQUEST,
                                     NetLogEventPhase::END);
  // Headers received should happen after HTTP_STREAM_REQUEST.
  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_RECV_HEADERS,
      NetLogEventPhase::NONE);
  // Trailers received should happen after headers received. It might happen
  // before the reads complete.
  ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_RECV_TRAILERS,
      NetLogEventPhase::NONE);
  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_SENDV_DATA,
      NetLogEventPhase::NONE);
  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_READ_DATA,
      NetLogEventPhase::NONE);
  EXPECT_EQ(ERR_IO_PENDING, GetIntegerValueFromParams(entries[index], "rv"));

  // Sent bytes. Sending data is always asynchronous.
  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_SENT,
      NetLogEventPhase::NONE);
  EXPECT_EQ(NetLogSourceType::BIDIRECTIONAL_STREAM, entries[index].source.type);
  // Received bytes for asynchronous read.
  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_EQ(NetLogSourceType::BIDIRECTIONAL_STREAM, entries[index].source.type);
  // Received bytes for synchronous read.
  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_EQ(NetLogSourceType::BIDIRECTIONAL_STREAM, entries[index].source.type);
  ExpectLogContainsSomewhere(entries, index,
                             NetLogEventType::BIDIRECTIONAL_STREAM_ALIVE,
                             NetLogEventPhase::END);
}

TEST_F(BidirectionalStreamTest, TestInterleaveReadDataAndSendData) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame data_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/false));
  spdy::SpdySerializedFrame data_frame2(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/false));
  spdy::SpdySerializedFrame data_frame3(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame1, 3),
      CreateMockWrite(data_frame2, 6), CreateMockWrite(data_frame3, 9),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame response_body_frame2(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame1, 4),
      MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause.
      CreateMockRead(response_body_frame2, 7),
      MockRead(ASYNC, ERR_IO_PENDING, 8),  // Force a pause.
      MockRead(ASYNC, 0, 10),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(
      HttpRequestHeaders::kContentLength,
      base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer_ptr->IsRunning());

  // Send a DATA frame.
  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(kBodyDataString);

  // Send a DATA frame.
  delegate->SendData(buf, buf->size(), false);
  // ReadData and it should return asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Deliver a DATA frame, and fire the timer.
  sequenced_data_->Resume();
  sequenced_data_->RunUntilPaused();
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(1, delegate->on_data_read_count());

  // Send a DATA frame.
  delegate->SendData(buf, buf->size(), false);
  // ReadData and it should return asynchronously because no data is buffered.
  rv = delegate->ReadData();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Deliver a DATA frame, and fire the timer.
  sequenced_data_->Resume();
  sequenced_data_->RunUntilPaused();
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  // Last DATA frame is read. Server half closes.
  EXPECT_EQ(2, delegate->on_data_read_count());
  EXPECT_EQ(2, delegate->on_data_sent_count());

  // Send the last body frame. Client half closes.
  delegate->SendData(buf, buf->size(), true);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, delegate->on_data_sent_count());

  // OnClose is invoked since both sides are closed.
  rv = delegate->ReadData();
  EXPECT_THAT(rv, IsOk());

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(2, delegate->on_data_read_count());
  EXPECT_EQ(3, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestCoalesceSmallDataBuffers) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 1, LOWEST, nullptr, 0));
  std::string body_data = "some really long piece of data";
  spdy::SpdySerializedFrame data_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, body_data, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame1, 1),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3),  // Force a pause.
      CreateMockRead(response_body_frame1, 4), MockRead(ASYNC, 0, 5),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(
      HttpRequestHeaders::kContentLength,
      base::NumberToString(kBodyDataSize * 1));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->set_do_not_start_read(true);
  TestCompletionCallback callback;
  delegate->Start(std::move(request_info), http_session_.get(),
                  callback.callback());
  // Wait until the stream is ready.
  callback.WaitForResult();
  // Send a DATA frame.
  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(body_data.substr(0, 5));
  scoped_refptr<StringIOBuffer> buf2 = base::MakeRefCounted<StringIOBuffer>(
      body_data.substr(5, body_data.size() - 5));
  delegate->SendvData({buf, buf2.get()}, {buf->size(), buf2->size()}, true);
  sequenced_data_->RunUntilPaused();  // OnHeadersReceived.
  // ReadData and it should return asynchronously because no data is buffered.
  EXPECT_THAT(delegate->ReadData(), IsError(ERR_IO_PENDING));
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(1, delegate->on_data_read_count());

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());

  auto entries = net_log_observer_.GetEntries();
  size_t index = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::BIDIRECTIONAL_STREAM_SENDV_DATA,
      NetLogEventPhase::NONE);
  EXPECT_EQ(2, GetIntegerValueFromParams(entries[index], "num_buffers"));

  index = ExpectLogContainsSomewhereAfter(
      entries, index,
      NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_SENT_COALESCED,
      NetLogEventPhase::BEGIN);
  EXPECT_EQ(2,
            GetIntegerValueFromParams(entries[index], "num_buffers_coalesced"));

  index = ExpectLogContainsSomewhereAfter(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_SENT,
      NetLogEventPhase::NONE);
  EXPECT_EQ(buf->size(),
            GetIntegerValueFromParams(entries[index], "byte_count"));

  index = ExpectLogContainsSomewhereAfter(
      entries, index + 1, NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_SENT,
      NetLogEventPhase::NONE);
  EXPECT_EQ(buf2->size(),
            GetIntegerValueFromParams(entries[index], "byte_count"));

  ExpectLogContainsSomewhere(
      entries, index,
      NetLogEventType::BIDIRECTIONAL_STREAM_BYTES_SENT_COALESCED,
      NetLogEventPhase::END);
}

// Tests that BidirectionalStreamSpdyImpl::OnClose will complete any remaining
// read even if the read queue is empty.
TEST_F(BidirectionalStreamTest, TestCompleteAsyncRead) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame, 3), MockRead(SYNCHRONOUS, 0, 4),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Write request, and deliver response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer_ptr->IsRunning());

  // ReadData should return asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Deliver END_STREAM.
  // OnClose should trigger completion of the remaining read.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(0u, delegate->data_received().size());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestBuffering) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));

  spdy::SpdySerializedFrame body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));
  // Last body frame has END_STREAM flag set.
  spdy::SpdySerializedFrame last_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      CreateMockRead(body_frame, 2),
      CreateMockRead(body_frame, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      CreateMockRead(last_body_frame, 5),
      MockRead(SYNCHRONOUS, 0, 6),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->Start(std::move(request_info), http_session_.get());
  // Deliver two DATA frames together.
  sequenced_data_->RunUntilPaused();
  EXPECT_TRUE(timer_ptr->IsRunning());
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  // This should trigger |more_read_data_pending_| to execute the task at a
  // later time, and Delegate::OnReadComplete should not have been called.
  EXPECT_TRUE(timer_ptr->IsRunning());
  EXPECT_EQ(0, delegate->on_data_read_count());

  // Fire the timer now, the two DATA frame should be combined into one
  // single Delegate::OnReadComplete callback.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 2,
            static_cast<int>(delegate->data_received().size()));

  // Deliver last DATA frame and EOF. There will be an additional
  // Delegate::OnReadComplete callback.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 3,
            static_cast<int>(delegate->data_received().size()));

  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestBufferingWithTrailers) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));

  spdy::SpdySerializedFrame body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  quiche::HttpHeaderBlock trailers;
  trailers["foo"] = "bar";
  spdy::SpdySerializedFrame response_trailers(
      spdy_util_.ConstructSpdyResponseHeaders(1, std::move(trailers), true));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      CreateMockRead(body_frame, 2),
      CreateMockRead(body_frame, 3),
      CreateMockRead(body_frame, 4),
      MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause.
      CreateMockRead(response_trailers, 6),
      MockRead(SYNCHRONOUS, 0, 7),
  };

  InitSession(reads, writes, SocketTag());

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  delegate->Start(std::move(request_info), http_session_.get());
  // Deliver all three DATA frames together.
  sequenced_data_->RunUntilPaused();
  EXPECT_TRUE(timer_ptr->IsRunning());
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  // This should trigger |more_read_data_pending_| to execute the task at a
  // later time, and Delegate::OnReadComplete should not have been called.
  EXPECT_TRUE(timer_ptr->IsRunning());
  EXPECT_EQ(0, delegate->on_data_read_count());

  // Deliver trailers. Remaining read should be completed, since OnClose is
  // called right after OnTrailersReceived. The three DATA frames should be
  // delivered in a single OnReadCompleted callback.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 3,
            static_cast<int>(delegate->data_received().size()));
  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ("bar", delegate->trailers().find("foo")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, DeleteStreamAfterSendData) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/false));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame, 3),
      CreateMockWrite(rst, 5),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      MockRead(ASYNC, 0, 6),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(
      HttpRequestHeaders::kContentLength,
      base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());

  // Send a DATA frame.
  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(kBodyDataString);
  delegate->SendData(buf, buf->size(), false);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  delegate->DeleteStream();
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(0, delegate->on_data_read_count());
  // OnDataSent may or may not have been invoked.
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // Bytes sent excludes the RST frame.
  EXPECT_EQ(
      CountWriteBytes(base::make_span(writes).first(std::size(writes) - 1)),
      delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, DeleteStreamDuringReadData) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 4),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame, 3), MockRead(ASYNC, 0, 5),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(
      HttpRequestHeaders::kContentLength,
      base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  // Delete the stream after ReadData returns ERR_IO_PENDING.
  int rv = delegate->ReadData();
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  delegate->DeleteStream();
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // Bytes sent excludes the RST frame.
  EXPECT_EQ(
      CountWriteBytes(base::make_span(writes).first(std::size(writes) - 1)),
      delegate->GetTotalSentBytes());
  // Response body frame isn't read becase stream is deleted once read returns
  // ERR_IO_PENDING.
  EXPECT_EQ(CountReadBytes(base::make_span(reads).first(std::size(reads) - 2)),
            delegate->GetTotalReceivedBytes());
}

// Receiving a header with uppercase ASCII will result in a protocol error,
// which should be propagated via Delegate::OnFailed.
TEST_F(BidirectionalStreamTest, PropagateProtocolError) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOW, nullptr, 0));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 2),
  };

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(ASYNC, 0, 3),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->extra_headers.SetHeader(
      HttpRequestHeaders::kContentLength,
      base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate->error(), IsError(ERR_HTTP2_PROTOCOL_ERROR));
  EXPECT_EQ(delegate->response_headers().end(),
            delegate->response_headers().find(":status"));
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // BidirectionalStreamSpdyStreamJob does not count the bytes sent for |rst|
  // because it is sent after SpdyStream::Delegate::OnClose is called.
  EXPECT_EQ(CountWriteBytes(base::make_span(writes, 1u)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());

  auto entries = net_log_observer_.GetEntries();

  size_t index = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::BIDIRECTIONAL_STREAM_READY,
      NetLogEventPhase::NONE);
  EXPECT_TRUE(
      GetBooleanValueFromParams(entries[index], "request_headers_sent"));

  index = ExpectLogContainsSomewhere(
      entries, index, NetLogEventType::BIDIRECTIONAL_STREAM_FAILED,
      NetLogEventPhase::NONE);
  EXPECT_EQ(ERR_HTTP2_PROTOCOL_ERROR,
            GetNetErrorCodeFromParams(entries[index]));
}

TEST_F(BidirectionalStreamTest, DeleteStreamDuringOnHeadersReceived) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));

  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 2),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(ASYNC, 0, 3),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<DeleteStreamDelegate>(
      read_buffer.get(), kReadBufferSize,
      DeleteStreamDelegate::Phase::ON_HEADERS_RECEIVED);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(0u, delegate->data_received().size());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(0, delegate->on_data_read_count());

  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // Bytes sent excludes the RST frame.
  EXPECT_EQ(
      CountWriteBytes(base::make_span(writes).first(std::size(writes) - 1)),
      delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, DeleteStreamDuringOnDataRead) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));

  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 3),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));

  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(response_body_frame, 2),
      MockRead(ASYNC, 0, 4),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<DeleteStreamDelegate>(
      read_buffer.get(), kReadBufferSize,
      DeleteStreamDelegate::Phase::ON_DATA_READ);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(kUploadDataSize * 1,
            static_cast<int>(delegate->data_received().size()));
  EXPECT_EQ(0, delegate->on_data_sent_count());

  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // Bytes sent excludes the RST frame.
  EXPECT_EQ(
      CountWriteBytes(base::make_span(writes).first(std::size(writes) - 1)),
      delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, DeleteStreamDuringOnTrailersReceived) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));

  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 4),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));

  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  quiche::HttpHeaderBlock trailers;
  trailers["foo"] = "bar";
  spdy::SpdySerializedFrame response_trailers(
      spdy_util_.ConstructSpdyResponseHeaders(1, std::move(trailers), true));

  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(response_body_frame, 2),
      CreateMockRead(response_trailers, 3), MockRead(ASYNC, 0, 5),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<DeleteStreamDelegate>(
      read_buffer.get(), kReadBufferSize,
      DeleteStreamDelegate::Phase::ON_TRAILERS_RECEIVED);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ("bar", delegate->trailers().find("foo")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  // OnDataRead may or may not have been fired before the stream is
  // deleted.
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // Bytes sent excludes the RST frame.
  EXPECT_EQ(
      CountWriteBytes(base::make_span(writes).first(std::size(writes) - 1)),
      delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, DeleteStreamDuringOnFailed) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));

  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(rst, 2),
  };

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(ASYNC, 0, 3),
  };

  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<DeleteStreamDelegate>(
      read_buffer.get(), kReadBufferSize,
      DeleteStreamDelegate::Phase::ON_FAILED);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate->response_headers().end(),
            delegate->response_headers().find(":status"));
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_THAT(delegate->error(), IsError(ERR_HTTP2_PROTOCOL_ERROR));

  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // Bytes sent excludes the RST frame.
  EXPECT_EQ(
      CountWriteBytes(base::make_span(writes).first(std::size(writes) - 1)),
      delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestHonorAlternativeServiceHeader) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kDefaultUrl, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  std::string alt_svc_header_value =
      quic::AlpnForVersion(DefaultSupportedQuicVersions().front());
  alt_svc_header_value.append("=\"www.example.org:443\"");
  const char* const kExtraResponseHeaders[] = {"alt-svc",
                                               alt_svc_header_value.c_str()};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(kExtraResponseHeaders, 1, 1));
  spdy::SpdySerializedFrame body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body_frame, 2),
      MockRead(SYNCHRONOUS, 0, 3),
  };

  // Enable QUIC so that the alternative service header can be added to
  // HttpServerProperties.
  session_deps_.enable_quic = true;
  InitSession(reads, writes, SocketTag());

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = default_url_;
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto timer = std::make_unique<MockTimer>();
  auto delegate = std::make_unique<TestDelegateBase>(
      read_buffer.get(), kReadBufferSize, std::move(timer));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());

  const quiche::HttpHeaderBlock& response_headers =
      delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ(alt_svc_header_value, response_headers.find("alt-svc")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(kUploadData, delegate->data_received());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());

  AlternativeServiceInfoVector alternative_service_info_vector =
      http_session_->http_server_properties()->GetAlternativeServiceInfos(
          url::SchemeHostPort(default_url_), NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  AlternativeService alternative_service(kProtoQUIC, "www.example.org", 443);
  EXPECT_EQ(alternative_service,
            alternative_service_info_vector[0].alternative_service());
}

// Test that a BidirectionalStream created with a specific tag, tags the
// underlying socket appropriately.
TEST_F(BidirectionalStreamTest, Tagging) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOW, nullptr, 0));
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, kBodyDataString, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(data_frame, 3),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame, 4), MockRead(ASYNC, 0, 5),
  };
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag(0x12345678, 0x87654321);
#else
  SocketTag tag;
#endif
  InitSession(reads, writes, tag);

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "POST";
  request_info->url = default_url_;
  request_info->extra_headers.SetHeader(HttpRequestHeaders::kContentLength,
                                        base::NumberToString(kBodyDataSize));
  request_info->socket_tag = tag;
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate =
      std::make_unique<TestDelegateBase>(read_buffer.get(), kReadBufferSize);
  delegate->Start(std::move(request_info), http_session_.get());
  sequenced_data_->RunUntilPaused();

  EXPECT_EQ(socket_factory_->GetLastProducedTCPSocket()->tag(), tag);
  EXPECT_TRUE(
      socket_factory_->GetLastProducedTCPSocket()->tagged_before_connected());
  void* socket = socket_factory_->GetLastProducedTCPSocket();

  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(kBodyDataString);
  delegate->SendData(buf.get(), buf->size(), true);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(socket, socket_factory_->GetLastProducedTCPSocket());
}

}  // namespace net
