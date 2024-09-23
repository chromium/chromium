// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/bidirectional_stream_spdy_impl.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const char kBodyData[] = "Body data";
const size_t kBodyDataSize = std::size(kBodyData);
// Size of the buffer to be allocated for each read.
const size_t kReadBufferSize = 4096;

// Tests the load timing of a stream that's connected and is not the first
// request sent on a connection.
void TestLoadTimingReused(const LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Tests the load timing of a stream that's connected and using a fresh
// connection.
void TestLoadTimingNotReused(const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(
      load_timing_info.connect_timing,
      CONNECT_TIMING_HAS_SSL_TIMES | CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

class TestDelegateBase : public BidirectionalStreamImpl::Delegate {
 public:
  TestDelegateBase(base::WeakPtr<SpdySession> session,
                   IOBuffer* read_buf,
                   int read_buf_len)
      : stream_(std::make_unique<BidirectionalStreamSpdyImpl>(session,
                                                              NetLogSource())),
        read_buf_(read_buf),
        read_buf_len_(read_buf_len) {}

  TestDelegateBase(const TestDelegateBase&) = delete;
  TestDelegateBase& operator=(const TestDelegateBase&) = delete;

  ~TestDelegateBase() override = default;

  void OnStreamReady(bool request_headers_sent) override {
    CHECK(!on_failed_called_);
  }

  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    response_headers_ = response_headers.Clone();
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataRead(int bytes_read) override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    on_data_read_count_++;
    CHECK_GE(bytes_read, OK);
    bytes_read_ += bytes_read;
    data_received_.append(read_buf_->data(), bytes_read);
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataSent() override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    on_data_sent_count_++;
  }

  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override {
    CHECK(!on_failed_called_);
    trailers_ = trailers.Clone();
    if (run_until_completion_)
      loop_->Quit();
  }

  void OnFailed(int error) override {
    CHECK(!on_failed_called_);
    CHECK(!not_expect_callback_);
    CHECK_NE(OK, error);
    error_ = error;
    on_failed_called_ = true;
    if (run_until_completion_)
      loop_->Quit();
  }

  void Start(const BidirectionalStreamRequestInfo* request,
             const NetLogWithSource& net_log) {
    stream_->Start(request, net_log,
                   /*send_request_headers_automatically=*/false, this,
                   std::make_unique<base::OneShotTimer>(),
                   TRAFFIC_ANNOTATION_FOR_TESTS);
    not_expect_callback_ = false;
  }

  void SendData(IOBuffer* data, int length, bool end_of_stream) {
    SendvData({data}, {length}, end_of_stream);
  }

  void SendvData(const std::vector<scoped_refptr<IOBuffer>>& data,
                 const std::vector<int>& length,
                 bool end_of_stream) {
    not_expect_callback_ = true;
    stream_->SendvData(data, length, end_of_stream);
    not_expect_callback_ = false;
  }

  // Sets whether the delegate should wait until the completion of the stream.
  void SetRunUntilCompletion(bool run_until_completion) {
    run_until_completion_ = run_until_completion;
    loop_ = std::make_unique<base::RunLoop>();
  }

  // Wait until the stream reaches completion.
  void WaitUntilCompletion() { loop_->Run(); }

  // Starts or continues read data from |stream_| until there is no more
  // byte can be read synchronously.
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
    int rv = stream_->ReadData(read_buf_.get(), read_buf_len_);
    if (rv > 0) {
      data_received_.append(read_buf_->data(), rv);
      bytes_read_ += rv;
    }
    return rv;
  }

  NextProto GetProtocol() const { return stream_->GetProtocol(); }

  int64_t GetTotalReceivedBytes() const {
      return stream_->GetTotalReceivedBytes();
  }

  int64_t GetTotalSentBytes() const {
      return stream_->GetTotalSentBytes();
  }

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
    return stream_->GetLoadTimingInfo(load_timing_info);
  }

  // Const getters for internal states.
  const std::string& data_received() const { return data_received_; }
  int bytes_read() const { return bytes_read_; }
  int error() const { return error_; }
  const quiche::HttpHeaderBlock& response_headers() const {
    return response_headers_;
  }
  const quiche::HttpHeaderBlock& trailers() const { return trailers_; }
  int on_data_read_count() const { return on_data_read_count_; }
  int on_data_sent_count() const { return on_data_sent_count_; }
  bool on_failed_called() const { return on_failed_called_; }

  // Sets whether the delegate should automatically start reading.
  void set_do_not_start_read(bool do_not_start_read) {
    do_not_start_read_ = do_not_start_read;
  }

 private:
  std::unique_ptr<BidirectionalStreamSpdyImpl> stream_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  std::string data_received_;
  std::unique_ptr<base::RunLoop> loop_;
  quiche::HttpHeaderBlock response_headers_;
  quiche::HttpHeaderBlock trailers_;
  int error_ = OK;
  int bytes_read_ = 0;
  int on_data_read_count_ = 0;
  int on_data_sent_count_ = 0;
  bool do_not_start_read_ = false;
  bool run_until_completion_ = false;
  bool not_expect_callback_ = false;
  bool on_failed_called_ = false;
};

}  // namespace

class BidirectionalStreamSpdyImplTest : public testing::TestWithParam<bool>,
                                        public WithTaskEnvironment {
 public:
  BidirectionalStreamSpdyImplTest()
      : default_url_(kDefaultUrl),
        host_port_pair_(HostPortPair::FromURL(default_url_)),
        key_(host_port_pair_,
             PRIVACY_MODE_DISABLED,
             ProxyChain::Direct(),
             SessionUsage::kDestination,
             SocketTag(),
             NetworkAnonymizationKey(),
             SecureDnsPolicy::kAllow,
             /*disable_cert_verification_network_fetches=*/false),
        ssl_data_(SSLSocketDataProvider(ASYNC, OK)) {
    ssl_data_.next_proto = kProtoHTTP2;
    ssl_data_.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  }

  bool IsBrokenConnectionDetectionEnabled() const {
    if (!session_)
      return false;

    return session_->IsBrokenConnectionDetectionEnabled();
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
                   base::span<const MockWrite> writes) {
    ASSERT_TRUE(ssl_data_.ssl_info.cert.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data_);
    sequenced_data_ = std::make_unique<SequencedSocketData>(reads, writes);
    session_deps_.socket_factory->AddSocketDataProvider(sequenced_data_.get());
    session_deps_.net_log = NetLog::Get();
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ =
        CreateSpdySession(http_session_.get(), key_, net_log_with_source_);
  }

  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::NONE)};
  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  const GURL default_url_;
  const HostPortPair host_port_pair_;
  const SpdySessionKey key_;
  std::unique_ptr<SequencedSocketData> sequenced_data_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;

 private:
  SSLSocketDataProvider ssl_data_;
};

TEST_F(BidirectionalStreamSpdyImplTest, SimplePostRequest) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOW, nullptr, 0));
  spdy::SpdySerializedFrame data_frame(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(kBodyData, kBodyDataSize), /*fin=*/true));
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
  InitSession(reads, writes);

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "POST";
  request_info.url = default_url_;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                       base::NumberToString(kBodyDataSize));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<TestDelegateBase>(
      session_, read_buffer.get(), kReadBufferSize);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(&request_info, net_log_with_source_);
  sequenced_data_->RunUntilPaused();

  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(
          std::string(kBodyData, kBodyDataSize));
  delegate->SendData(write_buffer.get(), write_buffer->size(), true);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  delegate->WaitUntilCompletion();
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(delegate->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info);

  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamSpdyImplTest, LoadTimingTwoRequests) {
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
  InitSession(reads, writes);

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = default_url_;
  request_info.end_stream_on_headers = true;

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto read_buffer2 = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<TestDelegateBase>(
      session_, read_buffer.get(), kReadBufferSize);
  auto delegate2 = std::make_unique<TestDelegateBase>(
      session_, read_buffer2.get(), kReadBufferSize);
  delegate->SetRunUntilCompletion(true);
  delegate2->SetRunUntilCompletion(true);
  delegate->Start(&request_info, net_log_with_source_);
  delegate2->Start(&request_info, net_log_with_source_);

  base::RunLoop().RunUntilIdle();
  delegate->WaitUntilCompletion();
  delegate2->WaitUntilCompletion();
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(delegate->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info);
  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(delegate2->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingReused(load_timing_info2);
}

TEST_F(BidirectionalStreamSpdyImplTest, SendDataAfterStreamFailed) {
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

  InitSession(reads, writes);

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "POST";
  request_info.url = default_url_;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                       base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<TestDelegateBase>(
      session_, read_buffer.get(), kReadBufferSize);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(&request_info, net_log_with_source_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate->on_failed_called());

  // Try to send data after OnFailed(), should not get called back.
  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>("dummy");
  delegate->SendData(buf.get(), buf->size(), false);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate->error(), IsError(ERR_HTTP2_PROTOCOL_ERROR));
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // BidirectionalStreamSpdyStreamJob does not count the bytes sent for |rst|
  // because it is sent after SpdyStream::Delegate::OnClose is called.
  EXPECT_EQ(CountWriteBytes(base::make_span(writes, 1u)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
}

INSTANTIATE_TEST_SUITE_P(BidirectionalStreamSpdyImplTests,
                         BidirectionalStreamSpdyImplTest,
                         ::testing::Bool());

// Tests that when received RST_STREAM with NO_ERROR, BidirectionalStream does
// not crash when processing pending writes. See crbug.com/650438.
TEST_P(BidirectionalStreamSpdyImplTest, RstWithNoErrorBeforeSendIsComplete) {
  bool is_test_sendv = GetParam();
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize * 3, LOW, nullptr, 0));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_NO_ERROR));
  MockRead reads[] = {CreateMockRead(resp, 1),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
                      CreateMockRead(rst, 3), MockRead(ASYNC, 0, 4)};

  InitSession(reads, writes);

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "POST";
  request_info.url = default_url_;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                       base::NumberToString(kBodyDataSize * 3));

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<TestDelegateBase>(
      session_, read_buffer.get(), kReadBufferSize);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(&request_info, net_log_with_source_);
  sequenced_data_->RunUntilPaused();
  // Make a write pending before receiving RST_STREAM.
  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(
          std::string(kBodyData, kBodyDataSize));
  delegate->SendData(write_buffer.get(), write_buffer->size(), false);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  // Make sure OnClose() without an error completes any pending write().
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_FALSE(delegate->on_failed_called());

  if (is_test_sendv) {
    std::vector<scoped_refptr<IOBuffer>> three_buffers = {
        write_buffer.get(), write_buffer.get(), write_buffer.get()};
    std::vector<int> three_lengths = {
        write_buffer->size(), write_buffer->size(), write_buffer->size()};
    delegate->SendvData(three_buffers, three_lengths, /*end_of_stream=*/true);
    base::RunLoop().RunUntilIdle();
  } else {
    for (size_t j = 0; j < 3; j++) {
      delegate->SendData(write_buffer.get(), write_buffer->size(),
                         /*end_of_stream=*/j == 2);
      base::RunLoop().RunUntilIdle();
    }
  }
  delegate->WaitUntilCompletion();
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(delegate->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info);

  EXPECT_THAT(delegate->error(), IsError(OK));
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(is_test_sendv ? 2 : 4, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(base::make_span(writes, 1u)),
            delegate->GetTotalSentBytes());
  // Should not count RST stream.
  EXPECT_EQ(CountReadBytes(base::make_span(reads).first(std::size(reads) - 2)),
            delegate->GetTotalReceivedBytes());

  // Now call SendData again should produce an error because end of stream
  // flag has been written.
  if (is_test_sendv) {
    std::vector<scoped_refptr<IOBuffer>> buffer = {write_buffer.get()};
    std::vector<int> buffer_size = {write_buffer->size()};
    delegate->SendvData(buffer, buffer_size, true);
  } else {
    delegate->SendData(write_buffer.get(), write_buffer->size(), true);
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate->error(), IsError(ERR_UNEXPECTED));
  EXPECT_TRUE(delegate->on_failed_called());
  EXPECT_EQ(is_test_sendv ? 2 : 4, delegate->on_data_sent_count());
}

TEST_F(BidirectionalStreamSpdyImplTest, RequestDetectBrokenConnection) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kBodyDataSize, LOW, nullptr, 0));
  spdy::SpdySerializedFrame data_frame(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(kBodyData, kBodyDataSize), /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(data_frame, 3),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame response_body_frame(
      spdy_util_.ConstructSpdyDataFrame(1, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(response_body_frame, 4),
      MockRead(ASYNC, 0, 5),
  };
  InitSession(reads, writes);
  EXPECT_FALSE(IsBrokenConnectionDetectionEnabled());

  BidirectionalStreamRequestInfo request_info;
  request_info.method = "POST";
  request_info.url = default_url_;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                       base::NumberToString(kBodyDataSize));
  request_info.detect_broken_connection = true;
  request_info.heartbeat_interval = base::Seconds(1);

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  auto delegate = std::make_unique<TestDelegateBase>(
      session_, read_buffer.get(), kReadBufferSize);
  delegate->SetRunUntilCompletion(true);
  delegate->Start(&request_info, net_log_with_source_);
  sequenced_data_->RunUntilPaused();

  // Since we set request_info.detect_broken_connection to true, this should be
  // enabled for the bidi stream lifetime.
  EXPECT_TRUE(IsBrokenConnectionDetectionEnabled());

  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(
          std::string(kBodyData, kBodyDataSize));
  delegate->SendData(write_buffer.get(), write_buffer->size(), true);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  delegate->WaitUntilCompletion();
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(delegate->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info);

  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), delegate->GetTotalReceivedBytes());

  delegate.reset();
  // Once the bidi stream has been destroyed this should go back to being
  // disabled.
  EXPECT_FALSE(IsBrokenConnectionDetectionEnabled());
}

}  // namespace net
