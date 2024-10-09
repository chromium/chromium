// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/features.h"
#include "net/base/hex_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/request_priority.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_proxy_delegate.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/test_upload_data_stream_not_allow_http1.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/spdy/alps_decoder.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using net::test::IsError;
using net::test::IsOk;

//-----------------------------------------------------------------------------

namespace net {

namespace {

using testing::Each;
using testing::Eq;

const int32_t kBufferSize = SpdyHttpStream::kRequestBodyBufferSize;

struct TestParams {
  TestParams(bool priority_header_enabled, bool happy_eyeballs_v3_enabled)
      : priority_header_enabled(priority_header_enabled),
        happy_eyeballs_v3_enabled(happy_eyeballs_v3_enabled) {}

  bool priority_header_enabled;
  bool happy_eyeballs_v3_enabled;
};

std::vector<TestParams> GetTestParams() {
  return {TestParams(/*priority_header_enabled=*/true,
                     /*happy_eyeballs_v3_enabled=*/false),
          TestParams(/*priority_header_enabled=*/false,
                     /*happy_eyeballs_v3_enabled=*/false),
          TestParams(/*priority_header_enabled=*/true,
                     /*happy_eyeballs_v3_enabled=*/true)};
}

}  // namespace

const char kPushedUrl[] = "https://www.example.org/foo.dat";

class SpdyNetworkTransactionTest
    : public TestWithTaskEnvironment,
      public ::testing::WithParamInterface<TestParams> {
 protected:
  SpdyNetworkTransactionTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        default_url_(kDefaultUrl),
        host_port_pair_(HostPortPair::FromURL(default_url_)),
        spdy_util_(/*use_priority_header=*/true) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (PriorityHeaderEnabled()) {
      enabled_features.emplace_back(features::kPriorityHeader);
    } else {
      disabled_features.emplace_back(features::kPriorityHeader);
    }

    if (HappyEyeballsV3Enabled()) {
      enabled_features.emplace_back(features::kHappyEyeballsV3);
    } else {
      disabled_features.emplace_back(features::kHappyEyeballsV3);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~SpdyNetworkTransactionTest() override {
    // Clear raw_ptr to upload pointer prior to deleting it, to avoid triggering
    // danling raw_ptr warning.
    request_.upload_data_stream = nullptr;

    // UploadDataStream may post a deletion task back to the message loop on
    // destruction.
    upload_data_stream_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    request_.method = "GET";
    request_.url = GURL(kDefaultUrl);
    request_.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  struct TransactionHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
    HttpResponseInfo response_info;
  };

  // A helper class that handles all the initial npn/ssl setup.
  class NormalSpdyTransactionHelper {
   public:
    NormalSpdyTransactionHelper(
        const HttpRequestInfo& request,
        RequestPriority priority,
        const NetLogWithSource& log,
        std::unique_ptr<SpdySessionDependencies> session_deps)
        : request_(request),
          priority_(priority),
          session_deps_(session_deps.get() == nullptr
                            ? std::make_unique<SpdySessionDependencies>()
                            : std::move(session_deps)),
          log_(log) {
      session_deps_->net_log = log.net_log();
      session_ =
          SpdySessionDependencies::SpdyCreateSession(session_deps_.get());
    }

    ~NormalSpdyTransactionHelper() {
      // Any test which doesn't close the socket by sending it an EOF will
      // have a valid session left open, which leaks the entire session pool.
      // This is just fine - in fact, some of our tests intentionally do this
      // so that we can check consistency of the SpdySessionPool as the test
      // finishes.  If we had put an EOF on the socket, the SpdySession would
      // have closed and we wouldn't be able to check the consistency.

      // Forcefully close existing sessions here.
      session()->spdy_session_pool()->CloseAllSessions();
    }

    void RunPreTestSetup() {
      // We're now ready to use SSL-npn SPDY.
      trans_ =
          std::make_unique<HttpNetworkTransaction>(priority_, session_.get());
    }

    // Start the transaction, read some data, finish.
    void RunDefaultTest() {
      if (!StartDefaultTest()) {
        return;
      }
      FinishDefaultTest();
    }

    bool StartDefaultTest() {
      output_.rv = trans_->Start(&request_, callback_.callback(), log_);

      // We expect an IO Pending or some sort of error.
      EXPECT_LT(output_.rv, 0);
      return output_.rv == ERR_IO_PENDING;
    }

    void FinishDefaultTest() {
      output_.rv = callback_.WaitForResult();
      // Finish async network reads/writes.
      base::RunLoop().RunUntilIdle();
      if (output_.rv != OK) {
        session_->spdy_session_pool()->CloseCurrentSessions(ERR_ABORTED);
        return;
      }

      // Verify responses.
      const HttpResponseInfo* response = trans_->GetResponseInfo();
      ASSERT_TRUE(response);
      ASSERT_TRUE(response->headers);
      EXPECT_EQ(HttpConnectionInfo::kHTTP2, response->connection_info);
      EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
      EXPECT_TRUE(response->was_fetched_via_spdy);
      EXPECT_TRUE(response->was_alpn_negotiated);
      EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
      EXPECT_EQ(443, response->remote_endpoint.port());
      output_.status_line = response->headers->GetStatusLine();
      output_.response_info = *response;  // Make a copy so we can verify.
      output_.rv = ReadTransaction(trans_.get(), &output_.response_data);
    }

    void FinishDefaultTestWithoutVerification() {
      output_.rv = callback_.WaitForResult();
      // Finish async network reads/writes.
      base::RunLoop().RunUntilIdle();
      if (output_.rv != OK) {
        session_->spdy_session_pool()->CloseCurrentSessions(ERR_ABORTED);
      }
    }

    void WaitForCallbackToComplete() { output_.rv = callback_.WaitForResult(); }

    // Most tests will want to call this function. In particular, the MockReads
    // should end with an empty read, and that read needs to be processed to
    // ensure proper deletion of the spdy_session_pool.
    void VerifyDataConsumed() {
      for (const SocketDataProvider* provider : data_vector_) {
        EXPECT_TRUE(provider->AllReadDataConsumed());
        EXPECT_TRUE(provider->AllWriteDataConsumed());
      }
    }

    // Occasionally a test will expect to error out before certain reads are
    // processed. In that case we want to explicitly ensure that the reads were
    // not processed.
    void VerifyDataNotConsumed() {
      for (const SocketDataProvider* provider : data_vector_) {
        EXPECT_FALSE(provider->AllReadDataConsumed());
        EXPECT_FALSE(provider->AllWriteDataConsumed());
      }
    }

    void RunToCompletion(SocketDataProvider* data) {
      RunPreTestSetup();
      AddData(data);
      RunDefaultTest();
      VerifyDataConsumed();
    }

    void RunToCompletionWithSSLData(
        SocketDataProvider* data,
        std::unique_ptr<SSLSocketDataProvider> ssl_provider) {
      RunPreTestSetup();
      AddDataWithSSLSocketDataProvider(data, std::move(ssl_provider));
      RunDefaultTest();
      VerifyDataConsumed();
    }

    void AddData(SocketDataProvider* data) {
      auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      ssl_provider->ssl_info.cert =
          ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
      AddDataWithSSLSocketDataProvider(data, std::move(ssl_provider));
    }

    void AddDataWithSSLSocketDataProvider(
        SocketDataProvider* data,
        std::unique_ptr<SSLSocketDataProvider> ssl_provider) {
      data_vector_.push_back(data);
      if (ssl_provider->next_proto == kProtoUnknown) {
        ssl_provider->next_proto = kProtoHTTP2;
      }
      // Even when next_protos only includes HTTP1, `application_settions`
      // always includes the full list from the HttpNetworkSession. The
      // SSLClientSocket layer, which is mocked out in these tests, is the layer
      // responsible for only sending the relevant settings.
      ssl_provider->expected_application_settings = {{{kProtoHTTP2, {}}}};

      session_deps_->socket_factory->AddSSLSocketDataProvider(
          ssl_provider.get());
      ssl_vector_.push_back(std::move(ssl_provider));

      session_deps_->socket_factory->AddSocketDataProvider(data);
    }

    size_t GetSpdySessionCount() {
      std::unique_ptr<base::Value> value(
          session_->spdy_session_pool()->SpdySessionPoolInfoToValue());
      CHECK(value && value->is_list());
      return value->GetList().size();
    }

    HttpNetworkTransaction* trans() { return trans_.get(); }
    void ResetTrans() { trans_.reset(); }
    const TransactionHelperResult& output() { return output_; }
    HttpNetworkSession* session() const { return session_.get(); }
    SpdySessionDependencies* session_deps() { return session_deps_.get(); }

   private:
    typedef std::vector<raw_ptr<SocketDataProvider>> DataVector;
    typedef std::vector<std::unique_ptr<SSLSocketDataProvider>> SSLVector;
    typedef std::vector<std::unique_ptr<SocketDataProvider>> AlternateVector;
    const HttpRequestInfo request_;
    const RequestPriority priority_;
    std::unique_ptr<SpdySessionDependencies> session_deps_;
    std::unique_ptr<HttpNetworkSession> session_;
    TransactionHelperResult output_;
    SSLVector ssl_vector_;
    TestCompletionCallback callback_;
    std::unique_ptr<HttpNetworkTransaction> trans_;
    DataVector data_vector_;
    const NetLogWithSource log_;
  };

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);

  [[nodiscard]] HttpRequestInfo CreateGetPushRequest() const {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL(kPushedUrl);
    request.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    return request;
  }

  void UsePostRequest() {
    ASSERT_FALSE(upload_data_stream_);
    std::vector<std::unique_ptr<UploadElementReader>> element_readers;
    element_readers.push_back(std::make_unique<UploadBytesElementReader>(
        base::byte_span_from_cstring(kUploadData)));
    upload_data_stream_ = std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0);

    request_.method = "POST";
    request_.upload_data_stream = upload_data_stream_.get();
  }

  void UseFilePostRequest() {
    ASSERT_FALSE(upload_data_stream_);
    base::FilePath file_path;
    CHECK(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    CHECK(base::WriteFile(file_path, kUploadData));

    std::vector<std::unique_ptr<UploadElementReader>> element_readers;
    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), file_path, 0,
        kUploadDataSize, base::Time()));
    upload_data_stream_ = std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0);

    request_.method = "POST";
    request_.upload_data_stream = upload_data_stream_.get();
    request_.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void UseUnreadableFilePostRequest() {
    ASSERT_FALSE(upload_data_stream_);
    base::FilePath file_path;
    CHECK(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    CHECK(base::WriteFile(file_path, kUploadData));
    CHECK(base::MakeFileUnreadable(file_path));

    std::vector<std::unique_ptr<UploadElementReader>> element_readers;
    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), file_path, 0,
        kUploadDataSize, base::Time()));
    upload_data_stream_ = std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0);

    request_.method = "POST";
    request_.upload_data_stream = upload_data_stream_.get();
  }

  void UseComplexPostRequest() {
    ASSERT_FALSE(upload_data_stream_);
    const int kFileRangeOffset = 1;
    const int kFileRangeLength = 3;
    CHECK_LT(kFileRangeOffset + kFileRangeLength, kUploadDataSize);

    base::FilePath file_path;
    CHECK(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    CHECK(base::WriteFile(file_path, kUploadData));

    std::vector<std::unique_ptr<UploadElementReader>> element_readers;
    element_readers.push_back(std::make_unique<UploadBytesElementReader>(
        base::byte_span_from_cstring(kUploadData)
            .first(base::checked_cast<size_t>(kFileRangeOffset))));
    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), file_path,
        kFileRangeOffset, kFileRangeLength, base::Time()));
    element_readers.push_back(std::make_unique<UploadBytesElementReader>(
        base::byte_span_from_cstring(kUploadData)
            .subspan(kFileRangeOffset + kFileRangeLength)));
    upload_data_stream_ = std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0);

    request_.method = "POST";
    request_.upload_data_stream = upload_data_stream_.get();
  }

  void UseChunkedPostRequest() {
    ASSERT_FALSE(upload_chunked_data_stream_);
    upload_chunked_data_stream_ = std::make_unique<ChunkedUploadDataStream>(0);
    request_.method = "POST";
    request_.upload_data_stream = upload_chunked_data_stream_.get();
  }

  // Read the result of a particular transaction, knowing that we've got
  // multiple transactions in the read pipeline; so as we read, we may have
  // to skip over data destined for other transactions while we consume
  // the data for |trans|.
  int ReadResult(HttpNetworkTransaction* trans, std::string* result) {
    const int kSize = 3000;

    int bytes_read = 0;
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(kSize);
    TestCompletionCallback callback;
    while (true) {
      int rv = trans->Read(buf.get(), kSize, callback.callback());
      if (rv == ERR_IO_PENDING) {
        rv = callback.WaitForResult();
      } else if (rv <= 0) {
        break;
      }
      result->append(buf->data(), rv);
      bytes_read += rv;
    }
    return bytes_read;
  }

  void VerifyStreamsClosed(const NormalSpdyTransactionHelper& helper) {
    // This lengthy block is reaching into the pool to dig out the active
    // session.  Once we have the session, we verify that the streams are
    // all closed and not leaked at this point.
    SpdySessionKey key(
        HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        request_.network_anonymization_key, SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
    HttpNetworkSession* session = helper.session();
    base::WeakPtr<SpdySession> spdy_session =
        session->spdy_session_pool()->FindAvailableSession(
            key, /* enable_ip_based_pooling = */ true,
            /* is_websocket = */ false, log_);
    ASSERT_TRUE(spdy_session);
    EXPECT_EQ(0u, num_active_streams(spdy_session));
  }

  static void DeleteSessionCallback(NormalSpdyTransactionHelper* helper,
                                    int result) {
    helper->ResetTrans();
  }

  static void StartTransactionCallback(HttpNetworkSession* session,
                                       GURL url,
                                       NetLogWithSource log,
                                       int result) {
    HttpRequestInfo request;
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session);
    TestCompletionCallback callback;
    request.method = "GET";
    request.url = url;
    request.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    int rv = trans.Start(&request, callback.callback(), log);
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    callback.WaitForResult();
  }

  ChunkedUploadDataStream* upload_chunked_data_stream() {
    return upload_chunked_data_stream_.get();
  }

  size_t num_active_streams(base::WeakPtr<SpdySession> session) {
    return session->active_streams_.size();
  }

  static spdy::SpdyStreamId spdy_stream_hi_water_mark(
      base::WeakPtr<SpdySession> session) {
    return session->stream_hi_water_mark_;
  }

  base::RepeatingClosure FastForwardByCallback(base::TimeDelta delta) {
    return base::BindRepeating(&SpdyNetworkTransactionTest::FastForwardBy,
                               base::Unretained(this), delta);
  }

  bool PriorityHeaderEnabled() const {
    return GetParam().priority_header_enabled;
  }

  bool HappyEyeballsV3Enabled() const {
    return GetParam().happy_eyeballs_v3_enabled;
  }

  const GURL default_url_;
  const HostPortPair host_port_pair_;

  const NetLogWithSource log_;
  std::unique_ptr<ChunkedUploadDataStream> upload_chunked_data_stream_;
  std::unique_ptr<UploadDataStream> upload_data_stream_;
  HttpRequestInfo request_;
  SpdyTestUtil spdy_util_;

  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SpdyNetworkTransactionTest,
                         testing::ValuesIn(GetTestParams()));

// Verify HttpNetworkTransaction constructor.
TEST_P(SpdyNetworkTransactionTest, Constructor) {
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(session_deps.get()));
  auto trans =
      std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY, session.get());
}

TEST_P(SpdyNetworkTransactionTest, Get) {
  // Construct the request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, SetPriority) {
  for (bool set_priority_before_starting_transaction : {true, false}) {
    SpdyTestUtil spdy_test_util(/*use_priority_header=*/true);
    spdy::SpdySerializedFrame req(
        spdy_test_util.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
    MockWrite writes[] = {CreateMockWrite(req, 0)};

    spdy::SpdySerializedFrame resp(
        spdy_test_util.ConstructSpdyGetReply(nullptr, 0, 1));
    spdy::SpdySerializedFrame body(
        spdy_test_util.ConstructSpdyDataFrame(1, true));
    MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(body, 2),
                        MockRead(ASYNC, 0, 3)};

    SequencedSocketData data(reads, writes);
    NormalSpdyTransactionHelper helper(request_, HIGHEST, log_, nullptr);
    helper.RunPreTestSetup();
    helper.AddData(&data);

    if (set_priority_before_starting_transaction) {
      helper.trans()->SetPriority(LOWEST);
      EXPECT_TRUE(helper.StartDefaultTest());
    } else {
      EXPECT_TRUE(helper.StartDefaultTest());
      helper.trans()->SetPriority(LOWEST);
    }

    helper.FinishDefaultTest();
    helper.VerifyDataConsumed();

    TransactionHelperResult out = helper.output();
    EXPECT_THAT(out.rv, IsOk());
    EXPECT_EQ("HTTP/1.1 200", out.status_line);
    EXPECT_EQ("hello!", out.response_data);
  }
}

// Test that changing the request priority of an existing stream triggers
// sending PRIORITY frames in case there are multiple open streams and their
// relative priorities change.
TEST_P(SpdyNetworkTransactionTest, SetPriorityOnExistingStream) {
  const char* kUrl2 = "https://www.example.org/bar";

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame req2(spdy_util_.ConstructSpdyGet(kUrl2, 3, MEDIUM));
  spdy::SpdySerializedFrame priority1(
      spdy_util_.ConstructSpdyPriority(3, 0, MEDIUM, true));
  spdy::SpdySerializedFrame priority2(
      spdy_util_.ConstructSpdyPriority(1, 3, LOWEST, true));
  MockWrite writes[] = {CreateMockWrite(req1, 0), CreateMockWrite(req2, 2),
                        CreateMockWrite(priority1, 4),
                        CreateMockWrite(priority2, 5)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {CreateMockRead(resp1, 1), CreateMockRead(resp2, 3),
                      CreateMockRead(body1, 6), CreateMockRead(body2, 7),
                      MockRead(ASYNC, 0, 8)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  EXPECT_TRUE(helper.StartDefaultTest());

  // Open HTTP/2 connection and create first stream.
  base::RunLoop().RunUntilIdle();

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  HttpRequestInfo request2;
  request2.url = GURL(kUrl2);
  request2.method = "GET";
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  int rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create second stream.
  base::RunLoop().RunUntilIdle();

  // First request has HIGHEST priority, second request has MEDIUM priority.
  // Changing the priority of the first request to LOWEST changes their order,
  // and therefore triggers sending PRIORITY frames.
  helper.trans()->SetPriority(LOWEST);

  helper.FinishDefaultTest();
  helper.VerifyDataConsumed();

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  ASSERT_TRUE(response2->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response2->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response2->headers->GetStatusLine());
}

// Create two requests: a lower priority one first, then a higher priority one.
// Test that the second request gets sent out first.
TEST_P(SpdyNetworkTransactionTest, RequestsOrderedByPriority) {
  const char* kUrl2 = "https://www.example.org/foo";

  // First send second request on stream 1, then first request on stream 3.
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(kUrl2, 1, HIGHEST));
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOW));
  MockWrite writes[] = {CreateMockWrite(req2, 0), CreateMockWrite(req1, 1)};

  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(1, "stream 1", true));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(3, "stream 3", true));
  MockRead reads[] = {CreateMockRead(resp2, 2), CreateMockRead(body2, 3),
                      CreateMockRead(resp1, 4), CreateMockRead(body1, 5),
                      MockRead(ASYNC, 0, 6)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, LOW, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  // Create HTTP/2 connection.  This is necessary because starting the first
  // transaction does not create the connection yet, so the second request
  // could not use the same connection, whereas running the message loop after
  // starting the first transaction would call Socket::Write() with the first
  // HEADERS frame, so the second transaction could not get ahead of it.
  SpdySessionKey key(HostPortPair("www.example.org", 443),
                     PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  auto spdy_session = CreateSpdySession(helper.session(), key, log_);
  EXPECT_TRUE(spdy_session);

  // Start first transaction.
  EXPECT_TRUE(helper.StartDefaultTest());

  // Start second transaction.
  HttpNetworkTransaction trans2(HIGHEST, helper.session());
  HttpRequestInfo request2;
  request2.url = GURL(kUrl2);
  request2.method = "GET";
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  int rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Complete first transaction and verify results.
  helper.FinishDefaultTest();
  helper.VerifyDataConsumed();

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("stream 3", out.response_data);

  // Complete second transaction and verify results.
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  ASSERT_TRUE(response2->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response2->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response2->headers->GetStatusLine());
  std::string response_data;
  ReadTransaction(&trans2, &response_data);
  EXPECT_EQ("stream 1", response_data);
}

// Test that already enqueued HEADERS frames are reordered if their relative
// priority changes.
TEST_P(SpdyNetworkTransactionTest, QueuedFramesReorderedOnPriorityChange) {
  const char* kUrl2 = "https://www.example.org/foo";
  const char* kUrl3 = "https://www.example.org/bar";

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, DEFAULT_PRIORITY));
  spdy::SpdySerializedFrame req3(spdy_util_.ConstructSpdyGet(kUrl3, 3, MEDIUM));
  // The headers for request 2 are set before the request is sent to SPDY and
  // are populated with the initial value (HIGHEST). The priority when it is
  // actually sent (later) is "LOWEST" which is sent on the actual priority
  // frame.
  spdy::SpdySerializedFrame req2(spdy_util_.ConstructSpdyGet(
      kUrl2, 5, LOWEST, kDefaultPriorityIncremental, HIGHEST));
  MockWrite writes[] = {MockWrite(ASYNC, ERR_IO_PENDING, 0),
                        CreateMockWrite(req1, 1), CreateMockWrite(req3, 2),
                        CreateMockWrite(req2, 3)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 5));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, "stream 1", true));
  spdy::SpdySerializedFrame body3(
      spdy_util_.ConstructSpdyDataFrame(3, "stream 3", true));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(5, "stream 5", true));
  MockRead reads[] = {CreateMockRead(resp1, 4), CreateMockRead(body1, 5),
                      CreateMockRead(resp3, 6), CreateMockRead(body3, 7),
                      CreateMockRead(resp2, 8), CreateMockRead(body2, 9),
                      MockRead(ASYNC, 0, 10)};

  SequencedSocketData data(reads, writes);
  // Priority of first request does not matter, because Socket::Write() will be
  // called with its HEADERS frame before the other requests start.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  EXPECT_TRUE(helper.StartDefaultTest());

  // Open HTTP/2 connection, create HEADERS frame for first request, and call
  // Socket::Write() with that frame.  After this, no other request can get
  // ahead of the first one.
  base::RunLoop().RunUntilIdle();

  HttpNetworkTransaction trans2(HIGHEST, helper.session());
  HttpRequestInfo request2;
  request2.url = GURL(kUrl2);
  request2.method = "GET";
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  int rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  HttpNetworkTransaction trans3(MEDIUM, helper.session());
  HttpRequestInfo request3;
  request3.url = GURL(kUrl3);
  request3.method = "GET";
  request3.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback3;
  rv = trans3.Start(&request3, callback3.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create HEADERS frames for second and third request and enqueue them in
  // SpdyWriteQueue with their original priorities.  Writing of the first
  // HEADERS frame to the socked still has not completed.
  base::RunLoop().RunUntilIdle();

  // Second request is of HIGHEST, third of MEDIUM priority.  Changing second
  // request to LOWEST changes their relative order.  This should result in
  // already enqueued frames being reordered within SpdyWriteQueue.
  trans2.SetPriority(LOWEST);

  // Complete async write of the first HEADERS frame.
  data.Resume();

  helper.FinishDefaultTest();
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("stream 1", out.response_data);

  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  ASSERT_TRUE(response2->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response2->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response2->headers->GetStatusLine());
  std::string response_data;
  ReadTransaction(&trans2, &response_data);
  EXPECT_EQ("stream 5", response_data);

  rv = callback3.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  const HttpResponseInfo* response3 = trans3.GetResponseInfo();
  ASSERT_TRUE(response3);
  ASSERT_TRUE(response3->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response3->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response3->headers->GetStatusLine());
  ReadTransaction(&trans3, &response_data);
  EXPECT_EQ("stream 3", response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, GetAtEachPriority) {
  for (RequestPriority p = MINIMUM_PRIORITY; p <= MAXIMUM_PRIORITY;
       p = RequestPriority(p + 1)) {
    SpdyTestUtil spdy_test_util(/*use_priority_header=*/true);

    // Construct the request.
    spdy::SpdySerializedFrame req(
        spdy_test_util.ConstructSpdyGet(nullptr, 0, 1, p));
    MockWrite writes[] = {CreateMockWrite(req, 0)};

    spdy::SpdyPriority spdy_prio = 0;
    EXPECT_TRUE(GetSpdyPriority(req, &spdy_prio));
    // this repeats the RequestPriority-->spdy::SpdyPriority mapping from
    // spdy::SpdyFramer::ConvertRequestPriorityToSpdyPriority to make
    // sure it's being done right.
    switch (p) {
      case HIGHEST:
        EXPECT_EQ(0, spdy_prio);
        break;
      case MEDIUM:
        EXPECT_EQ(1, spdy_prio);
        break;
      case LOW:
        EXPECT_EQ(2, spdy_prio);
        break;
      case LOWEST:
        EXPECT_EQ(3, spdy_prio);
        break;
      case IDLE:
        EXPECT_EQ(4, spdy_prio);
        break;
      case THROTTLED:
        EXPECT_EQ(5, spdy_prio);
        break;
      default:
        FAIL();
    }

    spdy::SpdySerializedFrame resp(
        spdy_test_util.ConstructSpdyGetReply(nullptr, 0, 1));
    spdy::SpdySerializedFrame body(
        spdy_test_util.ConstructSpdyDataFrame(1, true));
    MockRead reads[] = {
        CreateMockRead(resp, 1), CreateMockRead(body, 2),
        MockRead(ASYNC, 0, 3)  // EOF
    };

    SequencedSocketData data(reads, writes);

    NormalSpdyTransactionHelper helper(request_, p, log_, nullptr);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();
    EXPECT_THAT(out.rv, IsOk());
    EXPECT_EQ("HTTP/1.1 200", out.status_line);
    EXPECT_EQ("hello!", out.response_data);
  }
}

// Start three gets simultaniously; making sure that multiplexed
// streams work properly.

// This can't use the TransactionHelper method, since it only
// handles a single transaction, and finishes them as soon
// as it launches them.

// TODO(gavinp): create a working generalized TransactionHelper that
// can allow multiple streams in flight.

TEST_P(SpdyNetworkTransactionTest, ThreeGets) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fbody(spdy_util_.ConstructSpdyDataFrame(1, true));

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, false));
  spdy::SpdySerializedFrame fbody2(spdy_util_.ConstructSpdyDataFrame(3, true));

  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 5, LOWEST));
  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 5));
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(5, false));
  spdy::SpdySerializedFrame fbody3(spdy_util_.ConstructSpdyDataFrame(5, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(req2, 3),
      CreateMockWrite(req3, 6),
  };
  MockRead reads[] = {
      CreateMockRead(resp, 1),    CreateMockRead(body, 2),
      CreateMockRead(resp2, 4),   CreateMockRead(body2, 5),
      CreateMockRead(resp3, 7),   CreateMockRead(body3, 8),

      CreateMockRead(fbody, 9),   CreateMockRead(fbody2, 10),
      CreateMockRead(fbody3, 11),

      MockRead(ASYNC, 0, 12),  // EOF
  };
  SequencedSocketData data(reads, writes);
  SequencedSocketData data_placeholder1;
  SequencedSocketData data_placeholder2;

  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because three get requests are sent out at
  // the same time which results in three sockets being connected. The first
  // on will negotiate SPDY and will be used for all requests.
  helper.AddData(&data_placeholder1);
  helper.AddData(&data_placeholder2);
  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  TestCompletionCallback callback3;

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans3(DEFAULT_PRIORITY, helper.session());

  out.rv = trans1.Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  out.rv = trans2.Start(&request_, callback2.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  out.rv = trans3.Start(&request_, callback3.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));

  out.rv = callback1.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());
  out.rv = callback3.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  EXPECT_TRUE(response1->headers);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;

  trans2.GetResponseInfo();

  out.rv = ReadTransaction(&trans1, &out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_THAT(out.rv, IsOk());

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, TwoGetsLateBinding) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fbody(spdy_util_.ConstructSpdyDataFrame(1, true));

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, false));
  spdy::SpdySerializedFrame fbody2(spdy_util_.ConstructSpdyDataFrame(3, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(req2, 3),
  };
  MockRead reads[] = {
      CreateMockRead(resp, 1),  CreateMockRead(body, 2),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      CreateMockRead(fbody, 6), CreateMockRead(fbody2, 7),
      MockRead(ASYNC, 0, 8),  // EOF
  };
  SequencedSocketData data(reads, writes);

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData data_placeholder;
  data_placeholder.set_connect_data(never_finishing_connect);

  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because two requests are sent out at
  // the same time which results in two sockets being connected. The first
  // on will negotiate SPDY and will be used for all requests.
  helper.AddData(&data_placeholder);
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;

  out.rv = trans1.Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  out.rv = trans2.Start(&request_, callback2.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));

  out.rv = callback1.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());
  out.rv = callback2.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  EXPECT_TRUE(response1->headers);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(&trans1, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  EXPECT_TRUE(response2->headers);
  EXPECT_TRUE(response2->was_fetched_via_spdy);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(&trans2, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, TwoGetsLateBindingFromPreconnect) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fbody(spdy_util_.ConstructSpdyDataFrame(1, true));

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, false));
  spdy::SpdySerializedFrame fbody2(spdy_util_.ConstructSpdyDataFrame(3, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(req2, 3),
  };
  MockRead reads[] = {
      CreateMockRead(resp, 1),  CreateMockRead(body, 2),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      CreateMockRead(fbody, 6), CreateMockRead(fbody2, 7),
      MockRead(ASYNC, 0, 8),  // EOF
  };
  SequencedSocketData preconnect_data(reads, writes);

  MockConnect never_finishing_connect(ASYNC, ERR_IO_PENDING);

  SequencedSocketData data_placeholder;
  data_placeholder.set_connect_data(never_finishing_connect);

  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&preconnect_data);
  // We require placeholder data because 3 connections are attempted (first is
  // the preconnect, 2nd and 3rd are the never finished connections.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;

  // Preconnect the first.
  HttpStreamFactory* http_stream_factory =
      helper.session()->http_stream_factory();

  http_stream_factory->PreconnectStreams(1, request_);

  out.rv = trans1.Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  out.rv = trans2.Start(&request_, callback2.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));

  out.rv = callback1.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());
  out.rv = callback2.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  EXPECT_TRUE(response1->headers);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(&trans1, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  EXPECT_TRUE(response2->headers);
  EXPECT_TRUE(response2->was_fetched_via_spdy);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(&trans2, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  helper.VerifyDataConsumed();
}

// Similar to ThreeGets above, however this test adds a SETTINGS
// frame.  The SETTINGS frame is read during the IO loop waiting on
// the first transaction completion, and sets a maximum concurrent
// stream limit of 1.  This means that our IO loop exists after the
// second transaction completes, so we can assert on read_index().
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrent) {
  // Construct the request.
  // Each request fully completes before the next starts.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fbody(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy_util_.UpdateWithStreamDestruction(1);

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, false));
  spdy::SpdySerializedFrame fbody2(spdy_util_.ConstructSpdyDataFrame(3, true));
  spdy_util_.UpdateWithStreamDestruction(3);

  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 5, LOWEST));
  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 5));
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(5, false));
  spdy::SpdySerializedFrame fbody3(spdy_util_.ConstructSpdyDataFrame(5, true));

  spdy::SettingsMap settings;
  const uint32_t max_concurrent_streams = 1;
  settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = max_concurrent_streams;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(settings_ack, 5),
      CreateMockWrite(req2, 6),
      CreateMockWrite(req3, 10),
  };

  MockRead reads[] = {
      CreateMockRead(settings_frame, 1),
      CreateMockRead(resp, 2),
      CreateMockRead(body, 3),
      CreateMockRead(fbody, 4),
      CreateMockRead(resp2, 7),
      CreateMockRead(body2, 8),
      CreateMockRead(fbody2, 9),
      CreateMockRead(resp3, 11),
      CreateMockRead(body3, 12),
      CreateMockRead(fbody3, 13),

      MockRead(ASYNC, 0, 14),  // EOF
  };

  SequencedSocketData data(reads, writes);

  TransactionHelperResult out;
  {
    NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                       nullptr);
    helper.RunPreTestSetup();
    helper.AddData(&data);
    HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
    HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
    HttpNetworkTransaction trans3(DEFAULT_PRIORITY, helper.session());

    TestCompletionCallback callback1;
    TestCompletionCallback callback2;
    TestCompletionCallback callback3;

    out.rv = trans1.Start(&request_, callback1.callback(), log_);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    // Run transaction 1 through quickly to force a read of our SETTINGS
    // frame.
    out.rv = callback1.WaitForResult();
    ASSERT_THAT(out.rv, IsOk());

    out.rv = trans2.Start(&request_, callback2.callback(), log_);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = trans3.Start(&request_, callback3.callback(), log_);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = callback2.WaitForResult();
    ASSERT_THAT(out.rv, IsOk());

    out.rv = callback3.WaitForResult();
    ASSERT_THAT(out.rv, IsOk());

    const HttpResponseInfo* response1 = trans1.GetResponseInfo();
    ASSERT_TRUE(response1);
    EXPECT_TRUE(response1->headers);
    EXPECT_TRUE(response1->was_fetched_via_spdy);
    out.status_line = response1->headers->GetStatusLine();
    out.response_info = *response1;
    out.rv = ReadTransaction(&trans1, &out.response_data);
    EXPECT_THAT(out.rv, IsOk());
    EXPECT_EQ("HTTP/1.1 200", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response2 = trans2.GetResponseInfo();
    out.status_line = response2->headers->GetStatusLine();
    out.response_info = *response2;
    out.rv = ReadTransaction(&trans2, &out.response_data);
    EXPECT_THAT(out.rv, IsOk());
    EXPECT_EQ("HTTP/1.1 200", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response3 = trans3.GetResponseInfo();
    out.status_line = response3->headers->GetStatusLine();
    out.response_info = *response3;
    out.rv = ReadTransaction(&trans3, &out.response_data);
    EXPECT_THAT(out.rv, IsOk());
    EXPECT_EQ("HTTP/1.1 200", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    helper.VerifyDataConsumed();
  }
  EXPECT_THAT(out.rv, IsOk());
}

// Similar to ThreeGetsWithMaxConcurrent above, however this test adds
// a fourth transaction.  The third and fourth transactions have
// different data ("hello!" vs "hello!hello!") and because of the
// user specified priority, we expect to see them inverted in
// the response from the server.
TEST_P(SpdyNetworkTransactionTest, FourGetsWithMaxConcurrentPriority) {
  // Construct the request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fbody(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy_util_.UpdateWithStreamDestruction(1);

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, false));
  spdy::SpdySerializedFrame fbody2(spdy_util_.ConstructSpdyDataFrame(3, true));
  spdy_util_.UpdateWithStreamDestruction(3);

  spdy::SpdySerializedFrame req4(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 5, HIGHEST));
  spdy::SpdySerializedFrame resp4(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 5));
  spdy::SpdySerializedFrame fbody4(spdy_util_.ConstructSpdyDataFrame(5, true));
  spdy_util_.UpdateWithStreamDestruction(5);

  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 7, LOWEST));
  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 7));
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(7, false));
  spdy::SpdySerializedFrame fbody3(spdy_util_.ConstructSpdyDataFrame(7, true));

  spdy::SettingsMap settings;
  const uint32_t max_concurrent_streams = 1;
  settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = max_concurrent_streams;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(settings_ack, 5),
      // By making these synchronous, it guarantees that they are not *started*
      // before their sequence number, which in turn verifies that only a single
      // request is in-flight at a time.
      CreateMockWrite(req2, 6, SYNCHRONOUS),
      CreateMockWrite(req4, 10, SYNCHRONOUS),
      CreateMockWrite(req3, 13, SYNCHRONOUS),
  };
  MockRead reads[] = {
      CreateMockRead(settings_frame, 1),
      CreateMockRead(resp, 2),
      CreateMockRead(body, 3),
      CreateMockRead(fbody, 4),
      CreateMockRead(resp2, 7),
      CreateMockRead(body2, 8),
      CreateMockRead(fbody2, 9),
      CreateMockRead(resp4, 11),
      CreateMockRead(fbody4, 12),
      CreateMockRead(resp3, 14),
      CreateMockRead(body3, 15),
      CreateMockRead(fbody3, 16),

      MockRead(ASYNC, 0, 17),  // EOF
  };
  SequencedSocketData data(reads, writes);
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans3(DEFAULT_PRIORITY, helper.session());
  HttpNetworkTransaction trans4(HIGHEST, helper.session());

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  TestCompletionCallback callback3;
  TestCompletionCallback callback4;

  out.rv = trans1.Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  // Run transaction 1 through quickly to force a read of our SETTINGS frame.
  out.rv = callback1.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  // Finish async network reads and writes associated with |trans1|.
  base::RunLoop().RunUntilIdle();

  out.rv = trans2.Start(&request_, callback2.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  out.rv = trans3.Start(&request_, callback3.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));
  out.rv = trans4.Start(&request_, callback4.callback(), log_);
  ASSERT_THAT(out.rv, IsError(ERR_IO_PENDING));

  out.rv = callback2.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  out.rv = callback3.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  EXPECT_TRUE(response1->headers);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(&trans1, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(&trans2, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  // notice: response3 gets two hellos, response4 gets one
  // hello, so we know dequeuing priority was respected.
  const HttpResponseInfo* response3 = trans3.GetResponseInfo();
  out.status_line = response3->headers->GetStatusLine();
  out.response_info = *response3;
  out.rv = ReadTransaction(&trans3, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  out.rv = callback4.WaitForResult();
  EXPECT_THAT(out.rv, IsOk());
  const HttpResponseInfo* response4 = trans4.GetResponseInfo();
  out.status_line = response4->headers->GetStatusLine();
  out.response_info = *response4;
  out.rv = ReadTransaction(&trans4, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_THAT(out.rv, IsOk());
}

// Similar to ThreeGetsMaxConcurrrent above, however, this test
// deletes a session in the middle of the transaction to ensure
// that we properly remove pendingcreatestream objects from
// the spdy_session
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentDelete) {
  // Construct the request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fbody(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy_util_.UpdateWithStreamDestruction(1);

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, false));
  spdy::SpdySerializedFrame fbody2(spdy_util_.ConstructSpdyDataFrame(3, true));

  spdy::SettingsMap settings;
  const uint32_t max_concurrent_streams = 1;
  settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = max_concurrent_streams;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(settings_ack, 5),
      CreateMockWrite(req2, 6),
  };
  MockRead reads[] = {
      CreateMockRead(settings_frame, 1), CreateMockRead(resp, 2),
      CreateMockRead(body, 3),           CreateMockRead(fbody, 4),
      CreateMockRead(resp2, 7),          CreateMockRead(body2, 8),
      CreateMockRead(fbody2, 9),         MockRead(ASYNC, 0, 10),  // EOF
  };

  SequencedSocketData data(reads, writes);

  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  auto trans1 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  auto trans3 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  TestCompletionCallback callback3;

  out.rv = trans1->Start(&request_, callback1.callback(), log_);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  // Run transaction 1 through quickly to force a read of our SETTINGS frame.
  out.rv = callback1.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  out.rv = trans2->Start(&request_, callback2.callback(), log_);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = trans3->Start(&request_, callback3.callback(), log_);
  trans3.reset();
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback2.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  ASSERT_TRUE(response1);
  EXPECT_TRUE(response1->headers);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  ASSERT_TRUE(response2);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_THAT(out.rv, IsOk());
}

namespace {

// A helper class that will delete |transaction| on error when the callback is
// invoked.
class KillerCallback : public TestCompletionCallbackBase {
 public:
  explicit KillerCallback(std::unique_ptr<HttpNetworkTransaction> transaction)
      : transaction_(std::move(transaction)) {}

  ~KillerCallback() override = default;

  CompletionOnceCallback callback() {
    return base::BindOnce(&KillerCallback::OnComplete, base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    if (result < 0) {
      transaction_.reset();
    }

    SetResult(result);
  }

  std::unique_ptr<HttpNetworkTransaction> transaction_;
};

}  // namespace

// Similar to ThreeGetsMaxConcurrrentDelete above, however, this test
// closes the socket while we have a pending transaction waiting for
// a pending stream creation.  http://crbug.com/52901
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentSocketClose) {
  // Construct the request. Each stream uses a different priority to provide
  // more useful failure information if the requests are made in an unexpected
  // order.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame fin_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy_util_.UpdateWithStreamDestruction(1);

  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, MEDIUM));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));

  spdy::SettingsMap settings;
  const uint32_t max_concurrent_streams = 1;
  settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = max_concurrent_streams;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(settings_ack, 6),
                        CreateMockWrite(req2, 7)};
  MockRead reads[] = {
      CreateMockRead(settings_frame, 1), CreateMockRead(resp, 2),
      CreateMockRead(body, 3),
      // Delay the request here. For this test to pass, the three HTTP streams
      // have to be created in order, but SpdySession doesn't actually guarantee
      // that (See note in SpdySession::ProcessPendingStreamRequests). As a
      // workaround, delay finishing up the first stream until the second and
      // third streams are waiting in the SPDY stream request queue.
      MockRead(ASYNC, ERR_IO_PENDING, 4), CreateMockRead(fin_body, 5),
      CreateMockRead(resp2, 8),
      // The exact error does not matter, but some errors, such as
      // ERR_CONNECTION_RESET, may trigger a retry, which this test does not
      // account for.
      MockRead(ASYNC, ERR_SSL_BAD_RECORD_MAC_ALERT, 9),  // Abort!
  };

  SequencedSocketData data(reads, writes);
  SequencedSocketData data_placeholder;

  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);
  HttpNetworkTransaction trans1(HIGHEST, helper.session());
  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  auto trans3 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  auto* trans3_ptr = trans3.get();

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  KillerCallback callback3(std::move(trans3));

  out.rv = trans1.Start(&request_, callback1.callback(), log_);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  // Run transaction 1 through quickly to force a read of our SETTINGS frame.
  out.rv = callback1.WaitForResult();
  ASSERT_THAT(out.rv, IsOk());

  out.rv = trans2.Start(&request_, callback2.callback(), log_);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = trans3_ptr->Start(&request_, callback3.callback(), log_);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);

  // Run until both transactions are in the SpdySession's queue, waiting for the
  // final request to complete.
  base::RunLoop().RunUntilIdle();
  data.Resume();

  out.rv = callback3.WaitForResult();
  EXPECT_THAT(out.rv, IsError(ERR_SSL_BAD_RECORD_MAC_ALERT));

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  ASSERT_TRUE(response1);
  EXPECT_TRUE(response1->headers);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(&trans1, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(&trans2, &out.response_data);
  EXPECT_THAT(out.rv, IsError(ERR_SSL_BAD_RECORD_MAC_ALERT));

  helper.VerifyDataConsumed();
}

// Test that a simple PUT request works.
TEST_P(SpdyNetworkTransactionTest, Put) {
  // Setup the request.
  request_.method = "PUT";

  quiche::HttpHeaderBlock put_headers(
      spdy_util_.ConstructPutHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(put_headers), LOWEST, true));
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
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
}

// Test that a simple HEAD request works.
TEST_P(SpdyNetworkTransactionTest, Head) {
  // Setup the request.
  request_.method = "HEAD";

  quiche::HttpHeaderBlock head_headers(
      spdy_util_.ConstructHeadHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyHeaders(
      1, std::move(head_headers), LOWEST, true));
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
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
}

// Test that a simple POST works.
TEST_P(SpdyNetworkTransactionTest, Post) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a POST with a file works.
TEST_P(SpdyNetworkTransactionTest, FilePost) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UseFilePostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a POST with a unreadable file fails.
TEST_P(SpdyNetworkTransactionTest, UnreadableFilePost) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, 0)  // EOF
  };
  MockRead reads[] = {
      MockRead(ASYNC, 0, 1)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UseUnreadableFilePostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.RunDefaultTest();

  base::RunLoop().RunUntilIdle();
  helper.VerifyDataNotConsumed();
  EXPECT_THAT(helper.output().rv, IsError(ERR_ACCESS_DENIED));
}

// Test that a complex POST works.
TEST_P(SpdyNetworkTransactionTest, ComplexPost) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UseComplexPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a chunked POST works.
TEST_P(SpdyNetworkTransactionTest, ChunkedPost) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(body, 1),
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  // These chunks get merged into a single frame when being sent.
  const size_t kFirstChunkSize = kUploadDataSize / 2;
  auto [first_chunk, second_chunk] =
      base::byte_span_from_cstring(kUploadData).split_at(kFirstChunkSize);
  upload_chunked_data_stream()->AppendData(first_chunk, false);
  upload_chunked_data_stream()->AppendData(second_chunk, true);

  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ(kUploadData, out.response_data);
}

// Test that a chunked POST works with chunks appended after transaction starts.
TEST_P(SpdyNetworkTransactionTest, DelayedChunkedPost) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame chunk1(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame chunk2(spdy_util_.ConstructSpdyDataFrame(1, false));
  spdy::SpdySerializedFrame chunk3(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(chunk1, 1),
      CreateMockWrite(chunk2, 2),
      CreateMockWrite(chunk3, 3),
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 4), CreateMockRead(chunk1, 5),
      CreateMockRead(chunk2, 6), CreateMockRead(chunk3, 7),
      MockRead(ASYNC, 0, 8)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  upload_chunked_data_stream()->AppendData(
      base::byte_span_from_cstring(kUploadData), false);

  helper.RunPreTestSetup();
  helper.AddData(&data);
  ASSERT_TRUE(helper.StartDefaultTest());

  base::RunLoop().RunUntilIdle();
  upload_chunked_data_stream()->AppendData(
      base::byte_span_from_cstring(kUploadData), false);
  base::RunLoop().RunUntilIdle();
  upload_chunked_data_stream()->AppendData(
      base::byte_span_from_cstring(kUploadData), true);

  helper.FinishDefaultTest();
  helper.VerifyDataConsumed();

  std::string expected_response;
  expected_response += kUploadData;
  expected_response += kUploadData;
  expected_response += kUploadData;

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ(expected_response, out.response_data);
}

// Test that a POST without any post data works.
TEST_P(SpdyNetworkTransactionTest, NullPost) {
  // Setup the request.
  request_.method = "POST";
  // Create an empty UploadData.
  request_.upload_data_stream = nullptr;

  // When request.upload_data_stream is NULL for post, content-length is
  // expected to be 0.
  quiche::HttpHeaderBlock req_block(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(req_block), LOWEST, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_P(SpdyNetworkTransactionTest, EmptyPost) {
  // Create an empty UploadDataStream.
  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  ElementsUploadDataStream stream(std::move(element_readers), 0);

  // Setup the request.
  request_.method = "POST";
  request_.upload_data_stream = &stream;

  const uint64_t kContentLength = 0;

  quiche::HttpHeaderBlock req_block(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, kContentLength));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(req_block), LOWEST, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// While we're doing a post, the server sends the reply before upload completes.
TEST_P(SpdyNetworkTransactionTest, ResponseBeforePostCompletes) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(body, 3),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  // Write the request headers, and read the complete response
  // while still waiting for chunked request data.
  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  ASSERT_TRUE(helper.StartDefaultTest());

  base::RunLoop().RunUntilIdle();

  // Process the request headers, response headers, and response body.
  // The request body is still in flight.
  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());

  // Finish sending the request body.
  upload_chunked_data_stream()->AppendData(
      base::byte_span_from_cstring(kUploadData), true);
  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsOk());

  std::string response_body;
  EXPECT_THAT(ReadTransaction(helper.trans(), &response_body), IsOk());
  EXPECT_EQ(kUploadData, response_body);

  // Finish async network reads/writes.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// The client upon cancellation tries to send a RST_STREAM frame. The mock
// socket causes the TCP write to return zero. This test checks that the client
// tries to queue up the RST_STREAM frame again.
TEST_P(SpdyNetworkTransactionTest, SocketWriteReturnsZero) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(req, 0, SYNCHRONOUS),
      MockWrite(SYNCHRONOUS, nullptr, 0, 2),
      CreateMockWrite(rst, 3, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, nullptr, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.StartDefaultTest();
  EXPECT_THAT(helper.output().rv, IsError(ERR_IO_PENDING));

  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsOk());

  helper.ResetTrans();
  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_P(SpdyNetworkTransactionTest, ResponseWithoutHeaders) {
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(body, 1), MockRead(ASYNC, 0, 3)  // EOF
  };

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(rst, 2),
  };
  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Test that the transaction doesn't crash when we get two replies on the same
// stream ID. See http://crbug.com/45639.
TEST_P(SpdyNetworkTransactionTest, ResponseWithTwoSynReplies) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(rst, 4),
  };

  spdy::SpdySerializedFrame resp0(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp0, 1), CreateMockRead(resp1, 2),
      CreateMockRead(body, 3), MockRead(ASYNC, 0, 5)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_THAT(rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, ResetReplyWithTransferEncoding) {
  // Construct the request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(rst, 2),
  };

  const char* const headers[] = {"transfer-encoding", "chunked"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(headers, 1, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));

  helper.session()->spdy_session_pool()->CloseAllSessions();
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, CancelledTransaction) {
  // Construct the request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp),
      // This following read isn't used by the test, except during the
      // RunUntilIdle() call at the end since the SpdySession survives the
      // HttpNetworkTransaction and still tries to continue Read()'ing.  Any
      // MockRead will do here.
      MockRead(ASYNC, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  helper.ResetTrans();  // Cancel the transaction.

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataNotConsumed();
}

// Verify that the client sends a Rst Frame upon cancelling the stream.
TEST_P(SpdyNetworkTransactionTest, CancelledTransactionSendRst) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(req, 0, SYNCHRONOUS),
      CreateMockWrite(rst, 2, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, nullptr, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;

  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  helper.ResetTrans();
  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

// Verify that the client can correctly deal with the user callback attempting
// to start another transaction on a session that is closing down. See
// http://crbug.com/47455
TEST_P(SpdyNetworkTransactionTest, StartTransactionOnReadCallback) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req)};
  MockWrite writes2[] = {CreateMockWrite(req, 0),
                         MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 3)};

  // The indicated length of this frame is longer than its actual length. When
  // the session receives an empty frame after this one, it shuts down the
  // session, and calls the read callback with the incomplete data.
  const uint8_t kGetBodyFrame2[] = {
      0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x07, 'h',  'e',  'l',  'l',  'o',  '!',
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause
      MockRead(ASYNC, reinterpret_cast<const char*>(kGetBodyFrame2),
               std::size(kGetBodyFrame2), 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause
      MockRead(ASYNC, nullptr, 0, 5),      // EOF
  };
  MockRead reads2[] = {
      CreateMockRead(resp, 1), MockRead(ASYNC, nullptr, 0, 2),  // EOF
  };

  SequencedSocketData data(reads, writes);
  SequencedSocketData data2(reads2, writes2);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.AddData(&data2);
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();

  const int kSize = 3000;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kSize);
  rv = trans->Read(
      buf.get(), kSize,
      base::BindOnce(&SpdyNetworkTransactionTest::StartTransactionCallback,
                     helper.session(), default_url_, log_));
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  // This forces an err_IO_pending, which sets the callback.
  data.Resume();
  data.RunUntilPaused();

  // This finishes the read.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// Verify that the client can correctly deal with the user callback deleting
// the transaction. Failures will usually be flagged by thread and/or memory
// checking tools. See http://crbug.com/46925
TEST_P(SpdyNetworkTransactionTest, DeleteSessionOnReadCallback) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),                       // Force a pause
      CreateMockRead(body, 3), MockRead(ASYNC, nullptr, 0, 4),  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();

  // Setup a user callback which will delete the session, and clear out the
  // memory holding the stream object. Note that the callback deletes trans.
  const int kSize = 3000;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kSize);
  rv = trans->Read(
      buf.get(), kSize,
      base::BindOnce(&SpdyNetworkTransactionTest::DeleteSessionCallback,
                     base::Unretained(&helper)));
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  data.Resume();

  // Finish running rest of tasks.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, RedirectGetRequest) {
  MockClientSocketFactory socket_factory;
  auto context_builder =
      CreateSpdyTestURLRequestContextBuilder(&socket_factory);
  auto spdy_url_request_context = context_builder->Build();
  SpdySessionPoolPeer pool_peer(
      spdy_url_request_context->http_transaction_factory()
          ->GetSession()
          ->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);
  // Use a different port to avoid trying to reuse the initial H2 session.
  const char kRedirectUrl[] = "https://www.foo.com:8080/index.php";

  SSLSocketDataProvider ssl_provider0(ASYNC, OK);
  ssl_provider0.next_proto = kProtoHTTP2;
  socket_factory.AddSSLSocketDataProvider(&ssl_provider0);

  quiche::HttpHeaderBlock headers0(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  headers0["user-agent"] = "";
  headers0["accept-encoding"] = "gzip, deflate";

  spdy::SpdySerializedFrame req0(
      spdy_util_.ConstructSpdyHeaders(1, std::move(headers0), LOWEST, true));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes0[] = {CreateMockWrite(req0, 0), CreateMockWrite(rst, 2)};

  const char* const kExtraHeaders[] = {"location", kRedirectUrl};
  spdy::SpdySerializedFrame resp0(spdy_util_.ConstructSpdyReplyError(
      "301", kExtraHeaders, std::size(kExtraHeaders) / 2, 1));
  MockRead reads0[] = {CreateMockRead(resp0, 1), MockRead(ASYNC, 0, 3)};

  SequencedSocketData data0(reads0, writes0);
  socket_factory.AddSocketDataProvider(&data0);

  SSLSocketDataProvider ssl_provider1(ASYNC, OK);
  ssl_provider1.next_proto = kProtoHTTP2;
  socket_factory.AddSSLSocketDataProvider(&ssl_provider1);

  SpdyTestUtil spdy_util1(/*use_priority_header=*/true);
  quiche::HttpHeaderBlock headers1(
      spdy_util1.ConstructGetHeaderBlock(kRedirectUrl));
  headers1["user-agent"] = "";
  headers1["accept-encoding"] = "gzip, deflate";
  spdy::SpdySerializedFrame req1(
      spdy_util1.ConstructSpdyHeaders(1, std::move(headers1), LOWEST, true));
  MockWrite writes1[] = {CreateMockWrite(req1, 0)};

  spdy::SpdySerializedFrame resp1(
      spdy_util1.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util1.ConstructSpdyDataFrame(1, true));
  MockRead reads1[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                       MockRead(ASYNC, 0, 3)};

  SequencedSocketData data1(reads1, writes1);
  socket_factory.AddSocketDataProvider(&data1);

  TestDelegate delegate;

  std::unique_ptr<URLRequest> request = spdy_url_request_context->CreateRequest(
      default_url_, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilRedirect();

  EXPECT_EQ(1, delegate.received_redirect_count());

  request->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                                  std::nullopt /* modified_headers */);
  delegate.RunUntilComplete();

  EXPECT_EQ(1, delegate.response_started_count());
  EXPECT_FALSE(delegate.received_data_before_response());
  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ("hello!", delegate.data_received());

  // Pump the message loop to allow read data to be consumed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data0.AllReadDataConsumed());
  EXPECT_TRUE(data0.AllWriteDataConsumed());
  EXPECT_TRUE(data1.AllReadDataConsumed());
  EXPECT_TRUE(data1.AllWriteDataConsumed());
}

TEST_P(SpdyNetworkTransactionTest, RedirectMultipleLocations) {
  const spdy::SpdyStreamId kStreamId = 1;
  // Construct the request and the RST frame.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyGet(
      /*extra_headers=*/nullptr, /*extra_header_count=*/0, kStreamId, LOWEST));
  spdy::SpdySerializedFrame rst(spdy_util_.ConstructSpdyRstStream(
      kStreamId, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(rst, 4)};

  // Construct the response.
  const char* const kExtraResponseHeaders[] = {
      "location",
      "https://example1.test",
      "location",
      "https://example2.test",
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyReplyError(
      "301", kExtraResponseHeaders, std::size(kExtraResponseHeaders) / 2,
      kStreamId));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(kStreamId, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION));
}

TEST_P(SpdyNetworkTransactionTest, NoConnectionPoolingOverTunnel) {
  // Use port 443 for two reasons:  This makes the endpoint is port 443 check in
  // NormalSpdyTransactionHelper pass, and this means that the tunnel uses the
  // same port as the servers, to further confuse things.
  const char kPacString[] = "PROXY myproxy:443";

  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          kPacString, TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  // Only one request uses the first connection.
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet("https://www.example.org", 1, LOWEST));
  MockWrite writes1[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.example.org:443 HTTP/1.1\r\n"
                "Host: www.example.org:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      CreateMockWrite(req1, 2),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads1[] = {MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n\r\n"),
                       CreateMockRead(resp1, 3), CreateMockRead(body1, 4),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5)};

  MockConnect connect1(ASYNC, OK);
  SequencedSocketData data1(connect1, reads1, writes1);

  // Run a transaction to completion to set up a SPDY session.
  helper.RunToCompletion(&data1);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // A new SPDY session should have been created.
  SpdySessionKey key1(
      HostPortPair("www.example.org", 443), PRIVACY_MODE_DISABLED,
      PacResultElementToProxyChain(kPacString), SessionUsage::kDestination,
      SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session1 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
          NetLogWithSource());
  ASSERT_TRUE(session1);

  // The second request uses a second connection.
  SpdyTestUtil spdy_util2(/*use_priority_header=*/true);
  spdy::SpdySerializedFrame req2(
      spdy_util2.ConstructSpdyGet("https://example.test", 1, LOWEST));
  MockWrite writes2[] = {
      MockWrite(ASYNC, 0,
                "CONNECT example.test:443 HTTP/1.1\r\n"
                "Host: example.test:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      CreateMockWrite(req2, 2),
  };

  spdy::SpdySerializedFrame resp2(
      spdy_util2.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body2(spdy_util2.ConstructSpdyDataFrame(1, true));
  MockRead reads2[] = {MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n\r\n"),
                       CreateMockRead(resp2, 3), CreateMockRead(body2, 4),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5)};

  MockConnect connect2(ASYNC, OK);
  SequencedSocketData data2(connect2, reads2, writes2);
  helper.AddData(&data2);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://example.test/");
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());

  TestCompletionCallback callback;
  EXPECT_THAT(trans2->Start(&request2, callback.callback(), NetLogWithSource()),
              IsError(ERR_IO_PENDING));

  // Wait for the second request to get headers.  It should create a new H2
  // session to do so.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  const HttpResponseInfo* response = trans2->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans2.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  // Inspect the new session.
  SpdySessionKey key2(HostPortPair("example.test", 443), PRIVACY_MODE_DISABLED,
                      PacResultElementToProxyChain(kPacString),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session2 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key2, true /* enable_ip_based_pooling */, false /* is_websocket */,
          NetLogWithSource());
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session1);
  EXPECT_NE(session1.get(), session2.get());
}

// Check that if a session is found after host resolution, but is closed before
// the task to try to use it executes, the request will continue to create a new
// socket and use it.
TEST_P(SpdyNetworkTransactionTest, ConnectionPoolingSessionClosedBeforeUse) {
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  // Only one request uses the first connection.
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet("https://www.example.org", 1, LOWEST));
  MockWrite writes1[] = {
      CreateMockWrite(req1, 0),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads1[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3)};

  MockConnect connect1(ASYNC, OK);
  SequencedSocketData data1(connect1, reads1, writes1);

  // Run a transaction to completion to set up a SPDY session.
  helper.RunToCompletion(&data1);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // A new SPDY session should have been created.
  SpdySessionKey key1(HostPortPair("www.example.org", 443),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));

  // The second request uses a second connection.
  SpdyTestUtil spdy_util2(/*use_priority_header=*/true);
  spdy::SpdySerializedFrame req2(
      spdy_util2.ConstructSpdyGet("https://example.test", 1, LOWEST));
  MockWrite writes2[] = {
      CreateMockWrite(req2, 0),
  };

  spdy::SpdySerializedFrame resp2(
      spdy_util2.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body2(spdy_util2.ConstructSpdyDataFrame(1, true));
  MockRead reads2[] = {CreateMockRead(resp2, 1), CreateMockRead(body2, 2),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3)};

  MockConnect connect2(ASYNC, OK);
  SequencedSocketData data2(connect2, reads2, writes2);
  helper.AddData(&data2);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://example.test/");
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());

  // Set on-demand mode and run the second request to the DNS lookup.
  helper.session_deps()->host_resolver->set_ondemand_mode(true);
  TestCompletionCallback callback;
  EXPECT_THAT(trans2->Start(&request2, callback.callback(), NetLogWithSource()),
              IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper.session_deps()->host_resolver->has_pending_requests());

  // Resolve the request now, which should create an alias for the SpdySession
  // immediately, but the task to use the session for the second request should
  // run asynchronously, so it hasn't run yet.
  helper.session_deps()->host_resolver->ResolveOnlyRequestNow();
  SpdySessionKey key2(HostPortPair("example.test", 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session1 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key2, true /* enable_ip_based_pooling */, false /* is_websocket */,
          NetLogWithSource());
  ASSERT_TRUE(session1);
  EXPECT_EQ(key1, session1->spdy_session_key());
  // Remove the session before the second request can try to use it.
  helper.session()->spdy_session_pool()->CloseAllSessions();

  // Wait for the second request to get headers.  It should create a new H2
  // session to do so.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  const HttpResponseInfo* response = trans2->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans2.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  // Inspect the new session.
  base::WeakPtr<SpdySession> session2 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key2, true /* enable_ip_based_pooling */, false /* is_websocket */,
          NetLogWithSource());
  ASSERT_TRUE(session2);
  EXPECT_EQ(key2, session2->spdy_session_key());
  helper.VerifyDataConsumed();
}

// Check that requests with differe LOAD_DISABLE_CERT_NETWORK_FETCHES values do
// not share a session.
TEST_P(SpdyNetworkTransactionTest,
       ConnectionPoolingDisableCertVerificationNetworkFetches) {
  // Set up and run a transaction with `LOAD_DISABLE_CERT_NETWORK_FETCHES`.

  request_.load_flags |= LOAD_DISABLE_CERT_NETWORK_FETCHES;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet("https://www.example.org", 1, LOWEST));
  MockWrite writes1[] = {
      CreateMockWrite(req1, 0),
  };
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads1[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3)};
  MockConnect connect1(ASYNC, OK);
  SequencedSocketData data1(connect1, reads1, writes1);
  // Run a transaction to completion to set up a SPDY session.
  helper.RunToCompletion(&data1);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // A new SPDY session should have been created.
  SpdySessionKey key1(HostPortPair("www.example.org", 443),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/true);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, /*enable_ip_based_pooling=*/true, /*is_websocket=*/false,
      NetLogWithSource()));

  // There should be no session with the same key, except with
  // `disable_cert_verification_network_fetches` set to false.
  SpdySessionKey key2(HostPortPair("www.example.org", 443),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_FALSE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key2, /*enable_ip_based_pooling=*/true, /*is_websocket=*/false,
      NetLogWithSource()));

  // Set up and run a second transaction without
  // LOAD_DISABLE_CERT_NETWORK_FETCHES.

  SpdyTestUtil spdy_util2(/*use_priority_header=*/true);
  spdy::SpdySerializedFrame req2(
      spdy_util2.ConstructSpdyGet("https://www.example.org/2", 1, LOWEST));
  MockWrite writes2[] = {
      CreateMockWrite(req2, 0),
  };
  spdy::SpdySerializedFrame resp2(
      spdy_util2.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body2(spdy_util2.ConstructSpdyDataFrame(1, true));
  MockRead reads2[] = {CreateMockRead(resp2, 1), CreateMockRead(body2, 2),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3)};
  MockConnect connect2(ASYNC, OK);
  SequencedSocketData data2(connect2, reads2, writes2);
  helper.AddData(&data2);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.example.org/2");
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());

  TestCompletionCallback callback;
  EXPECT_THAT(trans2->Start(&request2, callback.callback(), NetLogWithSource()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  const HttpResponseInfo* response = trans2->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans2.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);
  helper.VerifyDataConsumed();

  // There should now be two sessions, with different values of
  // `disable_cert_verification_network_fetches`.
  auto session1 = helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, /*enable_ip_based_pooling=*/true, /*is_websocket=*/false,
      NetLogWithSource());
  EXPECT_TRUE(session1);
  auto session2 = helper.session()->spdy_session_pool()->FindAvailableSession(
      key2, /*enable_ip_based_pooling=*/true, /*is_websocket=*/false,
      NetLogWithSource());
  EXPECT_TRUE(session2);
  // Make sure the sessions are distinct.
  EXPECT_NE(session1.get(), session2.get());
}

#if BUILDFLAG(IS_ANDROID)

// Test this if two HttpNetworkTransactions try to repurpose the same
// SpdySession with two different SocketTags, only one request gets the session,
// while the other makes a new SPDY session.
TEST_P(SpdyNetworkTransactionTest, ConnectionPoolingMultipleSocketTags) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  if (HappyEyeballsV3Enabled()) {
    return;
  }

  const SocketTag kSocketTag1(SocketTag::UNSET_UID, 1);
  const SocketTag kSocketTag2(SocketTag::UNSET_UID, 2);
  const SocketTag kSocketTag3(SocketTag::UNSET_UID, 3);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  // The first and third requests use the first connection.
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet("https://www.example.org", 1, LOWEST));
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet("https://example.test/request3", 3, LOWEST));
  MockWrite writes1[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req3, 3),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads1[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                       CreateMockRead(resp3, 4), CreateMockRead(body3, 5),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6)};

  SequencedSocketData data1(MockConnect(ASYNC, OK), reads1, writes1);
  helper.AddData(&data1);

  // Due to the vagaries of how the socket pools work, in this particular case,
  // the second ConnectJob will be cancelled, but only after it tries to start
  // connecting. This does not happen in the general case of a bunch of requests
  // using the same socket tag.
  SequencedSocketData data2(MockConnect(SYNCHRONOUS, ERR_IO_PENDING),
                            base::span<const MockRead>(),
                            base::span<const MockWrite>());
  helper.AddData(&data2);

  // The second request uses a second connection.
  SpdyTestUtil spdy_util2(/*use_priority_header=*/true);
  spdy::SpdySerializedFrame req2(
      spdy_util2.ConstructSpdyGet("https://example.test/request2", 1, LOWEST));
  MockWrite writes2[] = {
      CreateMockWrite(req2, 0),
  };

  spdy::SpdySerializedFrame resp2(
      spdy_util2.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body2(spdy_util2.ConstructSpdyDataFrame(1, true));
  MockRead reads2[] = {CreateMockRead(resp2, 1), CreateMockRead(body2, 2),
                       MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3)};

  SequencedSocketData data3(MockConnect(ASYNC, OK), reads2, writes2);
  helper.AddData(&data3);

  // Run a transaction to completion to set up a SPDY session. This can't use
  // RunToCompletion(), since it can't call VerifyDataConsumed() yet.
  helper.RunPreTestSetup();
  helper.RunDefaultTest();
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // A new SPDY session should have been created.
  SpdySessionKey key1(HostPortPair("www.example.org", 443),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));

  // Set on-demand mode for the next two requests.
  helper.session_deps()->host_resolver->set_ondemand_mode(true);

  HttpRequestInfo request2;
  request2.socket_tag = kSocketTag2;
  request2.method = "GET";
  request2.url = GURL("https://example.test/request2");
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  TestCompletionCallback callback2;
  EXPECT_THAT(
      trans2->Start(&request2, callback2.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  HttpRequestInfo request3;
  request3.socket_tag = kSocketTag3;
  request3.method = "GET";
  request3.url = GURL("https://example.test/request3");
  request3.load_flags = 0;
  request3.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  auto trans3 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  TestCompletionCallback callback3;
  EXPECT_THAT(
      trans3->Start(&request3, callback3.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  // Run the message loop until both requests are waiting on the host resolver.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper.session_deps()->host_resolver->has_pending_requests());

  // Complete the second requests's DNS lookup now, which should create an alias
  // for the SpdySession immediately, but the task to use the session for the
  // second request should run asynchronously, so it hasn't run yet.
  helper.session_deps()->host_resolver->ResolveNow(2);
  SpdySessionKey key2(HostPortPair("example.test", 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      kSocketTag2, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);

  // Complete the third requests's DNS lookup now, which should hijack the
  // SpdySession from the second request.
  helper.session_deps()->host_resolver->ResolveNow(3);
  SpdySessionKey key3(HostPortPair("example.test", 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      kSocketTag3, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);

  // Wait for the second request to get headers.  It should create a new H2
  // session to do so.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  const HttpResponseInfo* response = trans2->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans2.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  // Wait for the third request to get headers.  It should have reused the first
  // session.
  EXPECT_THAT(callback3.WaitForResult(), IsOk());

  response = trans3->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  ASSERT_THAT(ReadTransaction(trans3.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, SocketTagChangeSessionTagWithDnsAliases) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  if (HappyEyeballsV3Enabled()) {
    return;
  }

  SocketTag socket_tag_1(SocketTag::UNSET_UID, 1);
  SocketTag socket_tag_2(SocketTag::UNSET_UID, 2);
  request_.socket_tag = socket_tag_1;

  std::unique_ptr<SpdySessionDependencies> session_deps =
      std::make_unique<SpdySessionDependencies>();
  std::unique_ptr<MockCachingHostResolver> host_resolver =
      std::make_unique<MockCachingHostResolver>(2 /* cache_invalidation_num */);
  session_deps->host_resolver = std::move(host_resolver);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  GURL url = request_.url;
  std::set<std::string> dns_aliases({"alias1", "alias2", "alias3"});
  helper.session_deps()->host_resolver->rules()->AddIPLiteralRuleWithDnsAliases(
      url.host(), "127.0.0.1", dns_aliases);

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(url.spec().c_str(), 1, DEFAULT_PRIORITY));
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(url.spec().c_str(), 3, DEFAULT_PRIORITY));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 3),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6)};

  SequencedSocketData data(MockConnect(ASYNC, OK), reads, writes);
  helper.AddData(&data);

  // Run a transaction to completion to set up a SPDY session. This can't use
  // RunToCompletion(), since it can't call VerifyDataConsumed() yet because
  // there are still further requests expected.
  helper.RunPreTestSetup();
  helper.RunDefaultTest();
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // A new SPDY session should have been created.
  EXPECT_EQ(1u, helper.GetSpdySessionCount());
  SpdySessionKey key1(HostPortPair(url.host(), 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      socket_tag_1, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_EQ(
      dns_aliases,
      helper.session()->spdy_session_pool()->GetDnsAliasesForSessionKey(key1));

  // Clear host resolver rules to ensure that cached values for DNS aliases
  // are used.
  helper.session_deps()->host_resolver->rules()->ClearRules();

  HttpRequestInfo request2;
  request2.socket_tag = socket_tag_2;
  request2.method = "GET";
  request2.url = url;
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  SpdySessionKey key2(HostPortPair(url.host(), 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      socket_tag_2, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  TestCompletionCallback callback2;
  EXPECT_THAT(
      trans2->Start(&request2, callback2.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  // Wait for the second request to get headers.  It should have reused the
  // first session but changed the tag.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  EXPECT_EQ(1u, helper.GetSpdySessionCount());
  EXPECT_FALSE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_TRUE(helper.session()
                  ->spdy_session_pool()
                  ->GetDnsAliasesForSessionKey(key1)
                  .empty());
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key2, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_EQ(
      dns_aliases,
      helper.session()->spdy_session_pool()->GetDnsAliasesForSessionKey(key2));

  const HttpResponseInfo* response = trans2->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans2.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest,
       SocketTagChangeFromIPAliasedSessionWithDnsAliases) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  if (HappyEyeballsV3Enabled()) {
    return;
  }

  SocketTag socket_tag_1(SocketTag::UNSET_UID, 1);
  SocketTag socket_tag_2(SocketTag::UNSET_UID, 2);
  request_.socket_tag = socket_tag_1;

  std::unique_ptr<SpdySessionDependencies> session_deps =
      std::make_unique<SpdySessionDependencies>();
  std::unique_ptr<MockCachingHostResolver> host_resolver =
      std::make_unique<MockCachingHostResolver>(2 /* cache_invalidation_num */);
  session_deps->host_resolver = std::move(host_resolver);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  GURL url1 = request_.url;
  std::set<std::string> dns_aliases1({"alias1", "alias2", "alias3"});
  GURL url2("https://example.test/");
  std::set<std::string> dns_aliases2({"example.net", "example.com"});

  helper.session_deps()->host_resolver->rules()->AddIPLiteralRuleWithDnsAliases(
      url1.host(), "127.0.0.1", dns_aliases1);
  helper.session_deps()->host_resolver->rules()->AddIPLiteralRuleWithDnsAliases(
      url2.host(), "127.0.0.1", dns_aliases2);

  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(url1.spec().c_str(), 1, DEFAULT_PRIORITY));
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(url2.spec().c_str(), 3, DEFAULT_PRIORITY));
  spdy_util_.UpdateWithStreamDestruction(3);
  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(url2.spec().c_str(), 5, DEFAULT_PRIORITY));
  spdy_util_.UpdateWithStreamDestruction(5);
  spdy::SpdySerializedFrame req4(
      spdy_util_.ConstructSpdyGet(url1.spec().c_str(), 7, DEFAULT_PRIORITY));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 3),
      CreateMockWrite(req3, 6),
      CreateMockWrite(req4, 9),
  };

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  spdy::SpdySerializedFrame resp3(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 5));
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(5, true));
  spdy::SpdySerializedFrame resp4(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 7));
  spdy::SpdySerializedFrame body4(spdy_util_.ConstructSpdyDataFrame(7, true));
  MockRead reads[] = {CreateMockRead(resp1, 1),
                      CreateMockRead(body1, 2),
                      CreateMockRead(resp2, 4),
                      CreateMockRead(body2, 5),
                      CreateMockRead(resp3, 7),
                      CreateMockRead(body3, 8),
                      CreateMockRead(resp4, 10),
                      CreateMockRead(body4, 11),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 12)};

  SequencedSocketData data(MockConnect(ASYNC, OK), reads, writes);
  helper.AddData(&data);

  // Run a transaction to completion to set up a SPDY session. This can't use
  // RunToCompletion(), since it can't call VerifyDataConsumed() yet.
  helper.RunPreTestSetup();
  helper.RunDefaultTest();
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // A new SPDY session should have been created.
  EXPECT_EQ(1u, helper.GetSpdySessionCount());
  SpdySessionKey key1(HostPortPair(url1.host(), 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      socket_tag_1, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_EQ(
      dns_aliases1,
      helper.session()->spdy_session_pool()->GetDnsAliasesForSessionKey(key1));

  HttpRequestInfo request2;
  request2.socket_tag = socket_tag_1;
  request2.method = "GET";
  request2.url = url2;
  request2.load_flags = 0;
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  SpdySessionKey key2(HostPortPair(url2.host(), 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      socket_tag_1, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  TestCompletionCallback callback2;
  EXPECT_THAT(
      trans2->Start(&request2, callback2.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  // Wait for the second request to get headers.  It should have reused the
  // first session.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  EXPECT_EQ(1u, helper.GetSpdySessionCount());
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key2, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_EQ(
      dns_aliases2,
      helper.session()->spdy_session_pool()->GetDnsAliasesForSessionKey(key2));

  const HttpResponseInfo* response = trans2->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans2.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  // Clear host resolver rules to ensure that cached values for DNS aliases
  // are used.
  helper.session_deps()->host_resolver->rules()->ClearRules();
  trans2.reset();

  HttpRequestInfo request3;
  request3.socket_tag = socket_tag_2;
  request3.method = "GET";
  request3.url = url2;
  request3.load_flags = 0;
  request3.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  SpdySessionKey key3(HostPortPair(url2.host(), 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      socket_tag_2, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  auto trans3 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  TestCompletionCallback callback3;
  EXPECT_THAT(
      trans3->Start(&request3, callback3.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  // Wait for the third request to get headers.  It should have reused the
  // first session but changed the socket tag.
  EXPECT_THAT(callback3.WaitForResult(), IsOk());

  EXPECT_EQ(1u, helper.GetSpdySessionCount());
  EXPECT_FALSE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key2, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_TRUE(helper.session()
                  ->spdy_session_pool()
                  ->GetDnsAliasesForSessionKey(key2)
                  .empty());
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key3, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_EQ(
      dns_aliases2,
      helper.session()->spdy_session_pool()->GetDnsAliasesForSessionKey(key3));

  response = trans3->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  ASSERT_THAT(ReadTransaction(trans3.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  trans3.reset();

  HttpRequestInfo request4;
  request4.socket_tag = socket_tag_2;
  request4.method = "GET";
  request4.url = url1;
  request4.load_flags = 0;
  request4.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  SpdySessionKey key4(HostPortPair(url1.host(), 443), PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      socket_tag_2, NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  auto trans4 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         helper.session());
  TestCompletionCallback callback4;
  EXPECT_THAT(
      trans4->Start(&request4, callback4.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  // Wait for the third request to get headers.  It should have reused the
  // first session but changed the socket tag.
  EXPECT_THAT(callback4.WaitForResult(), IsOk());

  EXPECT_EQ(1u, helper.GetSpdySessionCount());
  EXPECT_FALSE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key1, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_TRUE(helper.session()
                  ->spdy_session_pool()
                  ->GetDnsAliasesForSessionKey(key1)
                  .empty());
  EXPECT_TRUE(helper.session()->spdy_session_pool()->FindAvailableSession(
      key4, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));
  EXPECT_EQ(
      dns_aliases1,
      helper.session()->spdy_session_pool()->GetDnsAliasesForSessionKey(key4));

  response = trans4->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  ASSERT_THAT(ReadTransaction(trans4.get(), &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  helper.VerifyDataConsumed();
}

#endif  // BUILDFLAG(IS_ANDROID)

// Verify that various response headers parse correctly through the HTTP layer.
TEST_P(SpdyNetworkTransactionTest, ResponseHeaders) {
  struct ResponseHeadersTests {
    int extra_header_count;
    const char* extra_headers[4];
    size_t expected_header_count;
    std::string_view expected_headers[8];
  } test_cases[] = {
      // No extra headers.
      {0, {}, 1, {"hello", "bye"}},
      // Comma-separated header value.
      {1,
       {"cookie", "val1, val2"},
       2,
       {"hello", "bye", "cookie", "val1, val2"}},
      // Multiple headers are preserved: they are joined with \0 separator in
      // quiche::HttpHeaderBlock.AppendValueOrAddHeader(), then split up in
      // HpackEncoder, then joined with \0 separator when
      // spdy::HpackDecoderAdapter::ListenerAdapter::OnHeader() calls
      // quiche::HttpHeaderBlock.AppendValueOrAddHeader(), then split up again
      // in
      // HttpResponseHeaders.
      {2,
       {"content-encoding", "val1", "content-encoding", "val2"},
       3,
       {"hello", "bye", "content-encoding", "val1", "content-encoding",
        "val2"}},
      // Cookie header is not split up by HttpResponseHeaders.
      {2,
       {"cookie", "val1", "cookie", "val2"},
       2,
       {"hello", "bye", "cookie", "val1; val2"}}};

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    SpdyTestUtil spdy_test_util(/*use_priority_header=*/true);
    spdy::SpdySerializedFrame req(
        spdy_test_util.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
    MockWrite writes[] = {CreateMockWrite(req, 0)};

    spdy::SpdySerializedFrame resp(spdy_test_util.ConstructSpdyGetReply(
        test_cases[i].extra_headers, test_cases[i].extra_header_count, 1));
    spdy::SpdySerializedFrame body(
        spdy_test_util.ConstructSpdyDataFrame(1, true));
    MockRead reads[] = {
        CreateMockRead(resp, 1), CreateMockRead(body, 2),
        MockRead(ASYNC, 0, 3)  // EOF
    };

    SequencedSocketData data(reads, writes);
    NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                       nullptr);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();

    EXPECT_THAT(out.rv, IsOk());
    EXPECT_EQ("HTTP/1.1 200", out.status_line);
    EXPECT_EQ("hello!", out.response_data);

    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    ASSERT_TRUE(headers);
    EXPECT_EQ("HTTP/1.1 200", headers->GetStatusLine());
    size_t iter = 0;
    std::string name, value;
    size_t expected_header_index = 0;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      ASSERT_LT(expected_header_index, test_cases[i].expected_header_count);
      EXPECT_EQ(name,
                test_cases[i].expected_headers[2 * expected_header_index]);
      EXPECT_EQ(value,
                test_cases[i].expected_headers[2 * expected_header_index + 1]);
      ++expected_header_index;
    }
    EXPECT_EQ(expected_header_index, test_cases[i].expected_header_count);
  }
}

// Verify that we don't crash on invalid response headers.
TEST_P(SpdyNetworkTransactionTest, InvalidResponseHeaders) {
  struct InvalidResponseHeadersTests {
    int num_headers;
    const char* headers[10];
  } test_cases[] = {// Response headers missing status header
                    {2, {"cookie", "val1", "cookie", "val2", nullptr}},
                    // Response headers with no headers
                    {0, {nullptr}}};

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    SpdyTestUtil spdy_test_util(/*use_priority_header=*/true);

    spdy::SpdySerializedFrame req(
        spdy_test_util.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
    spdy::SpdySerializedFrame rst(spdy_test_util.ConstructSpdyRstStream(
        1, spdy::ERROR_CODE_PROTOCOL_ERROR));
    MockWrite writes[] = {
        CreateMockWrite(req, 0),
        CreateMockWrite(rst, 2),
    };

    // Construct the reply.
    quiche::HttpHeaderBlock reply_headers;
    AppendToHeaderBlock(test_cases[i].headers, test_cases[i].num_headers,
                        &reply_headers);
    spdy::SpdySerializedFrame resp(
        spdy_test_util.ConstructSpdyReply(1, std::move(reply_headers)));
    MockRead reads[] = {
        CreateMockRead(resp, 1), MockRead(ASYNC, 0, 3)  // EOF
    };

    SequencedSocketData data(reads, writes);
    NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                       nullptr);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();
    EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
  }
}

TEST_P(SpdyNetworkTransactionTest, CorruptFrameSessionError) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_COMPRESSION_ERROR,
      "Framer error: 24 (HPACK_TRUNCATED_BLOCK)."));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(goaway, 2)};

  // This is the length field that's too short.
  spdy::SpdySerializedFrame reply_wrong_length(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  size_t right_size = reply_wrong_length.size() - spdy::kFrameHeaderSize;
  size_t wrong_size = right_size - 4;
  spdy::test::SetFrameLength(&reply_wrong_length, wrong_size);

  MockRead reads[] = {
      MockRead(ASYNC, reply_wrong_length.data(), reply_wrong_length.size() - 4,
               1),
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_COMPRESSION_ERROR));
}

TEST_P(SpdyNetworkTransactionTest, GoAwayOnDecompressionFailure) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_COMPRESSION_ERROR,
      "Framer error: 24 (HPACK_TRUNCATED_BLOCK)."));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(goaway, 2)};

  // Read HEADERS with corrupted payload.
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  memset(resp.data() + 12, 0xcf, resp.size() - 12);
  MockRead reads[] = {CreateMockRead(resp, 1)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_COMPRESSION_ERROR));
}

TEST_P(SpdyNetworkTransactionTest, GoAwayOnFrameSizeError) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_FRAME_SIZE_ERROR,
      "Framer error: 9 (INVALID_CONTROL_FRAME_SIZE)."));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(goaway, 2)};

  // Read WINDOW_UPDATE with incorrectly-sized payload.
  spdy::SpdySerializedFrame bad_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, 1));
  spdy::test::SetFrameLength(&bad_window_update, bad_window_update.size() - 1);
  MockRead reads[] = {CreateMockRead(bad_window_update, 1)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_FRAME_SIZE_ERROR));
}

// Test that we shutdown correctly on write errors.
TEST_P(SpdyNetworkTransactionTest, WriteError) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {
      // We'll write 10 bytes successfully
      MockWrite(ASYNC, req.data(), 10, 1),
      // Followed by ERROR!
      MockWrite(ASYNC, ERR_FAILED, 2),
      // Session drains and attempts to write a GOAWAY: Another ERROR!
      MockWrite(ASYNC, ERR_FAILED, 3),
  };

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  EXPECT_TRUE(helper.StartDefaultTest());
  helper.FinishDefaultTest();
  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_FAILED));
}

// Test that partial writes work.
TEST_P(SpdyNetworkTransactionTest, PartialWrite) {
  // Chop the HEADERS frame into 5 chunks.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  const size_t kChunks = 5u;
  std::unique_ptr<MockWrite[]> writes = ChopWriteFrame(req, kChunks);
  for (size_t i = 0; i < kChunks; ++i) {
    writes[i].sequence_number = i;
  }

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, kChunks), CreateMockRead(body, kChunks + 1),
      MockRead(ASYNC, 0, kChunks + 2)  // EOF
  };

  SequencedSocketData data(reads, base::make_span(writes.get(), kChunks));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that the NetLog contains good data for a simple GET request.
TEST_P(SpdyNetworkTransactionTest, NetLog) {
  static const char* const kExtraHeaders[] = {
      "user-agent",
      "Chrome",
  };
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(kExtraHeaders, 1, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  RecordingNetLogObserver net_log_observer;

  SequencedSocketData data(reads, writes);
  request_.extra_headers.SetHeader("User-Agent", "Chrome");
  NormalSpdyTransactionHelper helper(
      request_, DEFAULT_PRIORITY,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the NetLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of the
  // log; instead we just check to make sure that certain events exist, and that
  // they are in the right order.
  auto entries = net_log_observer.GetEntries();

  EXPECT_LT(0u, entries.size());
  int pos = 0;
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST,
      NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(
      entries, pos + 1, NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST,
      NetLogEventPhase::END);
  pos = ExpectLogContainsSomewhere(
      entries, pos + 1, NetLogEventType::HTTP_TRANSACTION_READ_HEADERS,
      NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(
      entries, pos + 1, NetLogEventType::HTTP_TRANSACTION_READ_HEADERS,
      NetLogEventPhase::END);
  pos = ExpectLogContainsSomewhere(entries, pos + 1,
                                   NetLogEventType::HTTP_TRANSACTION_READ_BODY,
                                   NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(entries, pos + 1,
                                   NetLogEventType::HTTP_TRANSACTION_READ_BODY,
                                   NetLogEventPhase::END);

  // Check that we logged all the headers correctly
  pos = ExpectLogContainsSomewhere(entries, 0,
                                   NetLogEventType::HTTP2_SESSION_SEND_HEADERS,
                                   NetLogEventPhase::NONE);

  ASSERT_TRUE(entries[pos].HasParams());
  auto* header_list = entries[pos].params.FindList("headers");
  ASSERT_TRUE(header_list);
  if (base::FeatureList::IsEnabled(net::features::kPriorityHeader)) {
    ASSERT_EQ(6u, header_list->size());
  } else {
    ASSERT_EQ(5u, header_list->size());
  }

  ASSERT_TRUE((*header_list)[0].is_string());
  EXPECT_EQ(":method: GET", (*header_list)[0].GetString());

  ASSERT_TRUE((*header_list)[1].is_string());
  EXPECT_EQ(":authority: www.example.org", (*header_list)[1].GetString());

  ASSERT_TRUE((*header_list)[2].is_string());
  EXPECT_EQ(":scheme: https", (*header_list)[2].GetString());

  ASSERT_TRUE((*header_list)[3].is_string());
  EXPECT_EQ(":path: /", (*header_list)[3].GetString());

  ASSERT_TRUE((*header_list)[4].is_string());
  EXPECT_EQ("user-agent: Chrome", (*header_list)[4].GetString());

  // Incoming HEADERS frame is logged as HTTP2_SESSION_RECV_HEADERS.
  pos = ExpectLogContainsSomewhere(entries, 0,
                                   NetLogEventType::HTTP2_SESSION_RECV_HEADERS,
                                   NetLogEventPhase::NONE);
  ASSERT_TRUE(entries[pos].HasParams());
  // END_STREAM is not set on the HEADERS frame, so `fin` is false.
  std::optional<bool> fin = entries[pos].params.FindBool("fin");
  ASSERT_TRUE(fin.has_value());
  EXPECT_FALSE(*fin);

  // Incoming DATA frame is logged as HTTP2_SESSION_RECV_DATA.
  pos = ExpectLogContainsSomewhere(entries, 0,
                                   NetLogEventType::HTTP2_SESSION_RECV_DATA,
                                   NetLogEventPhase::NONE);
  ASSERT_TRUE(entries[pos].HasParams());
  std::optional<int> size = entries[pos].params.FindInt("size");
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(static_cast<int>(strlen("hello!")), *size);
  // END_STREAM is set on the DATA frame, so `fin` is true.
  fin = entries[pos].params.FindBool("fin");
  ASSERT_TRUE(fin.has_value());
  EXPECT_TRUE(*fin);
}

TEST_P(SpdyNetworkTransactionTest, NetLogForResponseWithNoBody) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  quiche::HttpHeaderBlock response_headers;
  response_headers[spdy::kHttp2StatusHeader] = "200";
  response_headers["hello"] = "bye";
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyResponseHeaders(
      1, std::move(response_headers), /* fin = */ true));
  MockRead reads[] = {CreateMockRead(resp, 1), MockRead(ASYNC, 0, 2)};

  RecordingNetLogObserver net_log_observer;

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(
      request_, DEFAULT_PRIORITY,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("", out.response_data);

  // Incoming HEADERS frame is logged as HTTP2_SESSION_RECV_HEADERS.
  auto entries = net_log_observer.GetEntries();
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP2_SESSION_RECV_HEADERS,
      NetLogEventPhase::NONE);
  ASSERT_TRUE(entries[pos].HasParams());
  // END_STREAM is set on the HEADERS frame, so `fin` is true.
  std::optional<bool> fin = entries[pos].params.FindBool("fin");
  ASSERT_TRUE(fin.has_value());
  EXPECT_TRUE(*fin);

  // No DATA frame is received.
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, 0, NetLogEventType::HTTP2_SESSION_RECV_DATA));
}

// Since we buffer the IO from the stream to the renderer, this test verifies
// that when we read out the maximum amount of data (e.g. we received 50 bytes
// on the network, but issued a Read for only 5 of those bytes) that the data
// flow still works correctly.
TEST_P(SpdyNetworkTransactionTest, BufferFull) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  // 2 data frames in a single read.
  spdy::SpdySerializedFrame data_frame_1(
      spdy_util_.ConstructSpdyDataFrame(1, "goodby", /*fin=*/false));
  spdy::SpdySerializedFrame data_frame_2(
      spdy_util_.ConstructSpdyDataFrame(1, "e worl", /*fin=*/false));
  spdy::SpdySerializedFrame combined_data_frames =
      CombineFrames({&data_frame_1, &data_frame_2});

  spdy::SpdySerializedFrame last_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "d", /*fin=*/true));

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause
      CreateMockRead(combined_data_frames, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause
      CreateMockRead(last_frame, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data(reads, writes);

  TestCompletionCallback callback;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 3;
    auto buf = base::MakeRefCounted<IOBufferWithSize>(kSmallReadSize);
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv == ERR_IO_PENDING) {
      data.Resume();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      NOTREACHED_IN_MIGRATION();
    }
  } while (rv > 0);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("goodbye world", out.response_data);
}

// Verify that basic buffering works; when multiple data frames arrive
// at the same time, ensure that we don't notify a read completion for
// each data frame individually.
TEST_P(SpdyNetworkTransactionTest, Buffering) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  // 4 data frames in a single read.
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "message", /*fin=*/false));
  spdy::SpdySerializedFrame data_frame_fin(
      spdy_util_.ConstructSpdyDataFrame(1, "message", /*fin=*/true));
  spdy::SpdySerializedFrame combined_data_frames =
      CombineFrames({&data_frame, &data_frame, &data_frame, &data_frame_fin});

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause
      CreateMockRead(combined_data_frames, 3), MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    auto buf = base::MakeRefCounted<IOBufferWithSize>(kSmallReadSize);
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv == ERR_IO_PENDING) {
      data.Resume();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);  // Reads are: 14 bytes, 14 bytes, 0 bytes.

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data but read it after it has been buffered.
TEST_P(SpdyNetworkTransactionTest, BufferedAll) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  // 5 data frames in a single read.
  spdy::SpdySerializedFrame reply(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "message", /*fin=*/false));
  spdy::SpdySerializedFrame data_frame_fin(
      spdy_util_.ConstructSpdyDataFrame(1, "message", /*fin=*/true));
  spdy::SpdySerializedFrame combined_frames = CombineFrames(
      {&reply, &data_frame, &data_frame, &data_frame, &data_frame_fin});

  MockRead reads[] = {
      CreateMockRead(combined_frames, 1), MockRead(ASYNC, 0, 2)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    auto buf = base::MakeRefCounted<IOBufferWithSize>(kSmallReadSize);
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data and close the connection.
TEST_P(SpdyNetworkTransactionTest, BufferedClosed) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  // All data frames in a single read.
  // NOTE: We don't FIN the stream.
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "message", /*fin=*/false));
  spdy::SpdySerializedFrame combined_data_frames =
      CombineFrames({&data_frame, &data_frame, &data_frame, &data_frame});
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a wait
      CreateMockRead(combined_data_frames, 3), MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;

  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TransactionHelperResult out = helper.output();
  rv = callback.WaitForResult();
  EXPECT_EQ(rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Allocate a large buffer to allow buffering. If a single read fills the
    // buffer, no buffering happens.
    const int kLargeReadSize = 1000;
    auto buf = base::MakeRefCounted<IOBufferWithSize>(kLargeReadSize);
    rv = trans->Read(buf.get(), kLargeReadSize, read_callback.callback());
    if (rv == ERR_IO_PENDING) {
      data.Resume();
      rv = read_callback.WaitForResult();
    }

    if (rv < 0) {
      // This test intentionally closes the connection, and will get an error.
      EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));
      break;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(0, reads_completed);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Verify the case where we buffer data and cancel the transaction.
TEST_P(SpdyNetworkTransactionTest, BufferedCancelled) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(rst, 4)};

  // NOTE: We don't FIN the stream.
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "message", /*fin=*/false));

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),                   // Force a wait
      CreateMockRead(data_frame, 3), MockRead(ASYNC, 0, 5)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;

  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  const int kReadSize = 256;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kReadSize);
  rv = trans->Read(buf.get(), kReadSize, read_callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv) << "Unexpected read: " << rv;

  // Complete the read now, which causes buffering to start.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  // Destroy the transaction, causing the stream to get cancelled
  // and orphaning the buffered IO task.
  helper.ResetTrans();

  // Flush the MessageLoop; this will cause the buffered IO task
  // to run for the final time.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Request should fail upon receiving a GOAWAY frame
// with Last-Stream-ID lower than the stream id corresponding to the request
// and with error code other than NO_ERROR.
TEST_P(SpdyNetworkTransactionTest, FailOnGoAway) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame go_away(
      spdy_util_.ConstructSpdyGoAway(0, spdy::ERROR_CODE_INTERNAL_ERROR, ""));
  MockRead reads[] = {
      CreateMockRead(go_away, 1),
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Request should be retried on a new connection upon receiving a GOAWAY frame
// with Last-Stream-ID lower than the stream id corresponding to the request
// and with error code NO_ERROR.
TEST_P(SpdyNetworkTransactionTest, RetryOnGoAway) {
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  // First connection.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes1[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame go_away(
      spdy_util_.ConstructSpdyGoAway(0, spdy::ERROR_CODE_NO_ERROR, ""));
  MockRead reads1[] = {CreateMockRead(go_away, 1)};
  SequencedSocketData data1(reads1, writes1);
  helper.AddData(&data1);

  // Second connection.
  MockWrite writes2[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads2[] = {CreateMockRead(resp, 1), CreateMockRead(body, 2),
                       MockRead(ASYNC, 0, 3)};
  SequencedSocketData data2(reads2, writes2);
  helper.AddData(&data2);

  helper.RunPreTestSetup();
  helper.RunDefaultTest();

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());

  helper.VerifyDataConsumed();
}

// A server can gracefully shut down by sending a GOAWAY frame
// with maximum last-stream-id value.
// Transactions started before receiving such a GOAWAY frame should succeed,
// but SpdySession should be unavailable for new streams.
TEST_P(SpdyNetworkTransactionTest, GracefulGoaway) {
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet("https://www.example.org/foo", 3, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req1, 0), CreateMockWrite(req2, 3)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0x7fffffff, spdy::ERROR_CODE_NO_ERROR, "Graceful shutdown."));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                      CreateMockRead(goaway, 4), CreateMockRead(resp2, 5),
                      CreateMockRead(body2, 6)};

  // Run first transaction.
  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.RunDefaultTest();

  // Verify first response.
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // GOAWAY frame has not yet been received, SpdySession should be available.
  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  SpdySessionKey key(host_port_pair_, PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(
      spdy_session_pool->HasAvailableSession(key, /* is_websocket = */ false));
  base::WeakPtr<SpdySession> spdy_session =
      spdy_session_pool->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, log_);
  EXPECT_TRUE(spdy_session);

  // Start second transaction.
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  TestCompletionCallback callback;
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.example.org/foo");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  int rv = trans2.Start(&request2, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  // Verify second response.
  const HttpResponseInfo* response = trans2.GetResponseInfo();
  ASSERT_TRUE(response);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response->connection_info);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(443, response->remote_endpoint.port());
  std::string response_data;
  rv = ReadTransaction(&trans2, &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  // Graceful GOAWAY was received, SpdySession should be unavailable.
  EXPECT_FALSE(
      spdy_session_pool->HasAvailableSession(key, /* is_websocket = */ false));
  spdy_session = spdy_session_pool->FindAvailableSession(
      key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ false, log_);
  EXPECT_FALSE(spdy_session);

  helper.VerifyDataConsumed();
}

// Verify that an active stream with ID not exceeding the Last-Stream-ID field
// of the incoming GOAWAY frame can receive data both before and after the
// GOAWAY frame.
TEST_P(SpdyNetworkTransactionTest, ActiveStreamWhileGoingAway) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      /* last_good_stream_id = */ 1, spdy::ERROR_CODE_NO_ERROR,
      "Graceful shutdown."));
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, "foo", false));
  spdy::SpdySerializedFrame body2(
      spdy_util_.ConstructSpdyDataFrame(1, "bar", true));
  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(body1, 2),
                      CreateMockRead(goaway, 3), CreateMockRead(body2, 4)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.AddData(&data);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, helper.session());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), log_);
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();

  const HttpResponseInfo* response = trans.GetResponseInfo();
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->was_fetched_via_spdy);

  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_THAT(ReadTransaction(&trans, &response_data), IsOk());
  EXPECT_EQ("foobar", response_data);
}

TEST_P(SpdyNetworkTransactionTest, CloseWithActiveStream) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {
      CreateMockRead(resp, 1), MockRead(SYNCHRONOUS, 0, 2)  // EOF
  };

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.StartDefaultTest();
  EXPECT_THAT(helper.output().rv, IsError(ERR_IO_PENDING));

  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsError(ERR_CONNECTION_CLOSED));

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, GoAwayImmediately) {
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {CreateMockRead(goaway, 0, SYNCHRONOUS)};
  SequencedSocketData data(reads, base::span<MockWrite>());

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.StartDefaultTest();
  EXPECT_THAT(helper.output().rv, IsError(ERR_IO_PENDING));

  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsError(ERR_CONNECTION_CLOSED));

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  EXPECT_FALSE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Retry with HTTP/1.1 when receiving HTTP_1_1_REQUIRED.  Note that no actual
// protocol negotiation happens, instead this test forces protocols for both
// sockets.
TEST_P(SpdyNetworkTransactionTest, HTTP11RequiredRetry) {
  request_.method = "GET";
  // Do not force SPDY so that second socket can negotiate HTTP/1.1.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  // First socket: HTTP/2 request rejected with HTTP_1_1_REQUIRED.
  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(headers), LOWEST, true));
  MockWrite writes0[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
  MockRead reads0[] = {CreateMockRead(rst, 1)};
  SequencedSocketData data0(reads0, writes0);

  auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect HTTP/2 protocols too in SSLConfig.
  ssl_provider0->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  // Force SPDY.
  ssl_provider0->next_proto = kProtoHTTP2;
  helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider0));

  // Second socket: falling back to HTTP/1.1.
  MockWrite writes1[] = {MockWrite(ASYNC, 0,
                                   "GET / HTTP/1.1\r\n"
                                   "Host: www.example.org\r\n"
                                   "Connection: keep-alive\r\n\r\n")};
  MockRead reads1[] = {MockRead(ASYNC, 1,
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 5\r\n\r\n"
                                "hello")};
  SequencedSocketData data1(reads1, writes1);

  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect only HTTP/1.1 protocol in SSLConfig.
  ssl_provider1->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP11};
  // Force HTTP/1.1.
  ssl_provider1->next_proto = kProtoHTTP11;
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));

  HttpServerProperties* http_server_properties =
      helper.session()->spdy_session_pool()->http_server_properties();
  EXPECT_FALSE(http_server_properties->RequiresHTTP11(
      url::SchemeHostPort(request_.url), NetworkAnonymizationKey()));

  helper.RunPreTestSetup();
  helper.StartDefaultTest();
  helper.FinishDefaultTestWithoutVerification();
  helper.VerifyDataConsumed();
  EXPECT_TRUE(http_server_properties->RequiresHTTP11(
      url::SchemeHostPort(request_.url), NetworkAnonymizationKey()));

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_TRUE(request_.url.SchemeIs("https"));
  EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(443, response->remote_endpoint.port());
  std::string response_data;
  ASSERT_THAT(ReadTransaction(helper.trans(), &response_data), IsOk());
  EXPECT_EQ("hello", response_data);
}

// Same as above test, but checks that NetworkAnonymizationKeys are respected.
TEST_P(SpdyNetworkTransactionTest,
       HTTP11RequiredRetryWithNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);

  const NetworkIsolationKey kNetworkIsolationKeys[] = {
      kNetworkIsolationKey1, kNetworkIsolationKey2, NetworkIsolationKey()};

  base::test::ScopedFeatureList feature_list;
  // Need to partition connections by NetworkAnonymizationKey for
  // SpdySessionKeys to include NetworkAnonymizationKeys.
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Do not force SPDY so that sockets can negotiate HTTP/1.1.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  // For each server, set up and tear down a QUIC session cleanly, and check
  // that stats have been added to HttpServerProperties using the correct
  // NetworkAnonymizationKey.
  for (size_t i = 0; i < std::size(kNetworkIsolationKeys); ++i) {
    SCOPED_TRACE(i);

    request_.method = "GET";
    request_.network_isolation_key = kNetworkIsolationKeys[i];
    request_.network_anonymization_key =
        net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
            kNetworkIsolationKeys[i]);

    // First socket: HTTP/2 request rejected with HTTP_1_1_REQUIRED.
    SpdyTestUtil spdy_util(/*use_priority_header=*/true);
    quiche::HttpHeaderBlock headers(
        spdy_util.ConstructGetHeaderBlock(kDefaultUrl));
    spdy::SpdySerializedFrame req(
        spdy_util.ConstructSpdyHeaders(1, std::move(headers), LOWEST, true));
    MockWrite writes0[] = {CreateMockWrite(req, 0)};
    spdy::SpdySerializedFrame rst(spdy_util.ConstructSpdyRstStream(
        1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
    MockRead reads0[] = {CreateMockRead(rst, 1)};
    SequencedSocketData data0(reads0, writes0);

    auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    // Expect HTTP/2 protocols too in SSLConfig.
    ssl_provider0->next_protos_expected_in_ssl_config =
        NextProtoVector{kProtoHTTP2, kProtoHTTP11};
    // Force SPDY.
    ssl_provider0->next_proto = kProtoHTTP2;
    helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider0));

    // Second socket: falling back to HTTP/1.1.
    MockWrite writes1[] = {MockWrite(ASYNC, 0,
                                     "GET / HTTP/1.1\r\n"
                                     "Host: www.example.org\r\n"
                                     "Connection: keep-alive\r\n\r\n")};
    MockRead reads1[] = {MockRead(ASYNC, 1,
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Length: 5\r\n\r\n"
                                  "hello")};
    SequencedSocketData data1(reads1, writes1);

    auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    // Expect only HTTP/1.1 protocol in SSLConfig.
    ssl_provider1->next_protos_expected_in_ssl_config =
        NextProtoVector{kProtoHTTP11};
    // Force HTTP/1.1.
    ssl_provider1->next_proto = kProtoHTTP11;
    helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));

    HttpServerProperties* http_server_properties =
        helper.session()->spdy_session_pool()->http_server_properties();
    EXPECT_FALSE(http_server_properties->RequiresHTTP11(
        url::SchemeHostPort(request_.url),
        net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
            kNetworkIsolationKeys[i])));

    HttpNetworkTransaction trans(DEFAULT_PRIORITY, helper.session());

    TestCompletionCallback callback;
    int rv = trans.Start(&request_, callback.callback(), log_);
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    const HttpResponseInfo* response = trans.GetResponseInfo();
    ASSERT_TRUE(response);
    ASSERT_TRUE(response->headers);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
    EXPECT_TRUE(response->was_alpn_negotiated);
    EXPECT_TRUE(request_.url.SchemeIs("https"));
    EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
    EXPECT_EQ(443, response->remote_endpoint.port());
    std::string response_data;
    ASSERT_THAT(ReadTransaction(&trans, &response_data), IsOk());
    EXPECT_EQ("hello", response_data);

    for (size_t j = 0; j < std::size(kNetworkIsolationKeys); ++j) {
      // NetworkAnonymizationKeys up to kNetworkIsolationKeys[j] are known
      // to require HTTP/1.1, others are not.
      if (j <= i) {
        EXPECT_TRUE(http_server_properties->RequiresHTTP11(
            url::SchemeHostPort(request_.url),
            net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
                kNetworkIsolationKeys[j])));
      } else {
        EXPECT_FALSE(http_server_properties->RequiresHTTP11(
            url::SchemeHostPort(request_.url),
            net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
                kNetworkIsolationKeys[j])));
      }
    }
  }
}

// Retry with HTTP/1.1 to the proxy when receiving HTTP_1_1_REQUIRED from the
// proxy.  Note that no actual protocol negotiation happens, instead this test
// forces protocols for both sockets.
TEST_P(SpdyNetworkTransactionTest, HTTP11RequiredProxyRetry) {
  request_.method = "GET";
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  // Do not force SPDY so that second socket can negotiate HTTP/1.1.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  // First socket: HTTP/2 CONNECT rejected with HTTP_1_1_REQUIRED.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair("www.example.org", 443)));
  MockWrite writes0[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
  MockRead reads0[] = {CreateMockRead(rst, 1)};
  SequencedSocketData data0(reads0, writes0);

  auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect HTTP/2 protocols too in SSLConfig.
  ssl_provider0->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  // Force SPDY.
  ssl_provider0->next_proto = kProtoHTTP2;
  helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider0));

  // Second socket: retry using HTTP/1.1.
  MockWrite writes1[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.example.org:443 HTTP/1.1\r\n"
                "Host: www.example.org:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 2,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: keep-alive\r\n\r\n"),
  };

  MockRead reads1[] = {
      MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(ASYNC, 3,
               "HTTP/1.1 200 OK\r\n"
               "Content-Length: 5\r\n\r\n"
               "hello"),
  };
  SequencedSocketData data1(reads1, writes1);

  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect only HTTP/1.1 protocol in SSLConfig.
  ssl_provider1->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP11};
  // Force HTTP/1.1.
  ssl_provider1->next_proto = kProtoHTTP11;
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));

  // A third socket is needed for the tunnelled connection.
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      ssl_provider2.get());

  HttpServerProperties* http_server_properties =
      helper.session()->spdy_session_pool()->http_server_properties();
  url::SchemeHostPort proxy_scheme_host_port(url::kHttpsScheme, "myproxy", 70);
  EXPECT_FALSE(http_server_properties->RequiresHTTP11(
      proxy_scheme_host_port, NetworkAnonymizationKey()));

  helper.RunPreTestSetup();
  helper.StartDefaultTest();
  helper.FinishDefaultTestWithoutVerification();
  helper.VerifyDataConsumed();
  EXPECT_TRUE(http_server_properties->RequiresHTTP11(
      proxy_scheme_host_port, NetworkAnonymizationKey()));

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  EXPECT_FALSE(response->was_alpn_negotiated);
  EXPECT_TRUE(request_.url.SchemeIs("https"));
  EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(70, response->remote_endpoint.port());
  std::string response_data;
  ASSERT_THAT(ReadTransaction(helper.trans(), &response_data), IsOk());
  EXPECT_EQ("hello", response_data);
}

// Same as above, but also test that NetworkAnonymizationKeys are respected.
TEST_P(SpdyNetworkTransactionTest,
       HTTP11RequiredProxyRetryWithNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);

  const NetworkAnonymizationKey kNetworkAnonymizationKeys[] = {
      kNetworkAnonymizationKey1, kNetworkAnonymizationKey2,
      NetworkAnonymizationKey()};
  const NetworkIsolationKey kNetworkIsolationKeys[] = {
      kNetworkIsolationKey1, kNetworkIsolationKey2, NetworkIsolationKey()};

  base::test::ScopedFeatureList feature_list;
  // Need to partition connections by NetworkAnonymizationKey for
  // SpdySessionKeys to include NetworkAnonymizationKeys.
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  request_.method = "GET";
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  // Do not force SPDY so that second socket can negotiate HTTP/1.1.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  for (size_t i = 0; i < std::size(kNetworkAnonymizationKeys); ++i) {
    // First socket: HTTP/2 CONNECT rejected with HTTP_1_1_REQUIRED.

    SpdyTestUtil spdy_util(/*use_priority_header=*/true);
    spdy::SpdySerializedFrame req(spdy_util.ConstructSpdyConnect(
        nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair("www.example.org", 443)));
    MockWrite writes0[] = {CreateMockWrite(req, 0)};
    spdy::SpdySerializedFrame rst(spdy_util.ConstructSpdyRstStream(
        1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
    MockRead reads0[] = {CreateMockRead(rst, 1)};
    SequencedSocketData data0(reads0, writes0);

    auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    // Expect HTTP/2 protocols too in SSLConfig.
    ssl_provider0->next_protos_expected_in_ssl_config =
        NextProtoVector{kProtoHTTP2, kProtoHTTP11};
    // Force SPDY.
    ssl_provider0->next_proto = kProtoHTTP2;
    helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider0));

    // Second socket: retry using HTTP/1.1.
    MockWrite writes1[] = {
        MockWrite(ASYNC, 0,
                  "CONNECT www.example.org:443 HTTP/1.1\r\n"
                  "Host: www.example.org:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
        MockWrite(ASYNC, 2,
                  "GET / HTTP/1.1\r\n"
                  "Host: www.example.org\r\n"
                  "Connection: keep-alive\r\n\r\n"),
    };

    MockRead reads1[] = {
        MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n\r\n"),
        MockRead(ASYNC, 3,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Length: 5\r\n\r\n"
                 "hello"),
    };
    SequencedSocketData data1(reads1, writes1);

    auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    // Expect only HTTP/1.1 protocol in SSLConfig.
    ssl_provider1->next_protos_expected_in_ssl_config =
        NextProtoVector{kProtoHTTP11};
    // Force HTTP/1.1.
    ssl_provider1->next_proto = kProtoHTTP11;
    helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));

    // A third socket is needed for the tunnelled connection.
    auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
        ssl_provider2.get());

    HttpServerProperties* http_server_properties =
        helper.session()->spdy_session_pool()->http_server_properties();
    url::SchemeHostPort proxy_scheme_host_port(url::kHttpsScheme, "myproxy",
                                               70);
    EXPECT_FALSE(http_server_properties->RequiresHTTP11(
        proxy_scheme_host_port, kNetworkAnonymizationKeys[i]));

    request_.network_isolation_key = kNetworkIsolationKeys[i];
    request_.network_anonymization_key = kNetworkAnonymizationKeys[i];
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, helper.session());
    TestCompletionCallback callback;
    int rv = trans.Start(&request_, callback.callback(), log_);
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    helper.VerifyDataConsumed();

    const HttpResponseInfo* response = trans.GetResponseInfo();
    ASSERT_TRUE(response);
    ASSERT_TRUE(response->headers);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
    EXPECT_FALSE(response->was_alpn_negotiated);
    EXPECT_TRUE(request_.url.SchemeIs("https"));
    EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
    EXPECT_EQ(70, response->remote_endpoint.port());
    std::string response_data;
    ASSERT_THAT(ReadTransaction(&trans, &response_data), IsOk());
    EXPECT_EQ("hello", response_data);

    for (size_t j = 0; j < std::size(kNetworkAnonymizationKeys); ++j) {
      // The proxy SchemeHostPort URL should not be marked as requiring HTTP/1.1
      // using the current NetworkAnonymizationKey, and the state of others
      // should be unchanged since the last loop iteration..
      if (j <= i) {
        EXPECT_TRUE(http_server_properties->RequiresHTTP11(
            proxy_scheme_host_port, kNetworkAnonymizationKeys[j]));
      } else {
        EXPECT_FALSE(http_server_properties->RequiresHTTP11(
            proxy_scheme_host_port, kNetworkAnonymizationKeys[j]));
      }
    }

    // The destination SchemeHostPort should not be marked as requiring
    // HTTP/1.1.
    EXPECT_FALSE(http_server_properties->RequiresHTTP11(
        url::SchemeHostPort(request_.url), kNetworkAnonymizationKeys[i]));
  }
}

// Same as HTTP11RequiredProxyRetry above except for nested proxies where
// HTTP_1_1_REQUIRED is received from the first proxy in the chain.
TEST_P(SpdyNetworkTransactionTest, HTTP11RequiredNestedProxyFirstProxyRetry) {
  request_.method = "GET";

  // Configure a nested proxy.
  const ProxyServer kProxyServer1{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy1.test", 70)};
  const ProxyServer kProxyServer2{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy2.test", 71)};
  const ProxyChain kNestedProxyChain =
      ProxyChain::ForIpProtection({{kProxyServer1, kProxyServer2}});

  ProxyList proxy_list;
  proxy_list.AddProxyChain(kNestedProxyChain);
  ProxyConfig proxy_config = ProxyConfig::CreateForTesting(proxy_list);

  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS)));
  // Do not force SPDY so that second socket can negotiate HTTP/1.1.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  // First socket: HTTP/2 CONNECT rejected with HTTP_1_1_REQUIRED.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      kProxyServer2.host_port_pair()));
  MockWrite writes0[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
  MockRead reads0[] = {CreateMockRead(rst, 1)};
  SequencedSocketData data0(reads0, writes0);

  auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect HTTP/2 protocols too in SSLConfig.
  ssl_provider0->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  // Force SPDY.
  ssl_provider0->next_proto = kProtoHTTP2;
  helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider0));

  // Second socket: retry using HTTP/1.1.
  MockWrite writes1[] = {
      MockWrite(ASYNC, 0,
                "CONNECT proxy2.test:71 HTTP/1.1\r\n"
                "Host: proxy2.test:71\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 2,
                "CONNECT www.example.org:443 HTTP/1.1\r\n"
                "Host: www.example.org:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 4,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: keep-alive\r\n\r\n"),
  };

  MockRead reads1[] = {
      MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(ASYNC, 5,
               "HTTP/1.1 200 OK\r\n"
               "Content-Length: 5\r\n\r\n"
               "hello"),
  };
  SequencedSocketData data1(reads1, writes1);

  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect only HTTP/1.1 protocol in SSLConfig.
  ssl_provider1->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP11};
  // Force HTTP/1.1.
  ssl_provider1->next_proto = kProtoHTTP11;
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));

  // A third and fourth socket are needed for the connection to the second hop
  // and for the tunnelled GET request.
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider2->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      ssl_provider2.get());
  auto ssl_provider3 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      ssl_provider3.get());

  HttpServerProperties* http_server_properties =
      helper.session()->spdy_session_pool()->http_server_properties();
  url::SchemeHostPort proxy_scheme_host_port(
      url::kHttpsScheme, kProxyServer1.host_port_pair().host(),
      kProxyServer1.host_port_pair().port());
  EXPECT_FALSE(http_server_properties->RequiresHTTP11(
      proxy_scheme_host_port, NetworkAnonymizationKey()));

  helper.RunPreTestSetup();
  helper.StartDefaultTest();
  helper.FinishDefaultTestWithoutVerification();
  helper.VerifyDataConsumed();
  EXPECT_TRUE(http_server_properties->RequiresHTTP11(
      proxy_scheme_host_port, NetworkAnonymizationKey()));

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  EXPECT_FALSE(response->was_alpn_negotiated);
  EXPECT_TRUE(request_.url.SchemeIs("https"));
  EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(70, response->remote_endpoint.port());
  std::string response_data;
  ASSERT_THAT(ReadTransaction(helper.trans(), &response_data), IsOk());
  EXPECT_EQ("hello", response_data);
}

// Same as above except for nested proxies where HTTP_1_1_REQUIRED is received
// from the second proxy in the chain.
// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
TEST_P(SpdyNetworkTransactionTest, HTTP11RequiredNestedProxySecondProxyRetry) {
  request_.method = "GET";

  const ProxyServer kProxyServer1{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy1.test", 70)};
  const ProxyServer kProxyServer2{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy2.test", 71)};
  const ProxyChain kNestedProxyChain =
      ProxyChain::ForIpProtection({{kProxyServer1, kProxyServer2}});

  ProxyList proxy_list;
  proxy_list.AddProxyChain(kNestedProxyChain);
  ProxyConfig proxy_config = ProxyConfig::CreateForTesting(proxy_list);

  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS)));
  // Do not force SPDY so that second socket can negotiate HTTP/1.1.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  // CONNECT to proxy2.test:71 via SPDY.
  spdy::SpdySerializedFrame proxy2_connect(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      kProxyServer2.host_port_pair()));

  spdy::SpdySerializedFrame proxy2_connect_resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  // Need to use a new `SpdyTestUtil()` so that the stream parent ID of this
  // request is calculated correctly.
  SpdyTestUtil new_spdy_util;
  // HTTP/2 endpoint CONNECT rejected with HTTP_1_1_REQUIRED.
  spdy::SpdySerializedFrame endpoint_connect(new_spdy_util.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair("www.example.org", 443)));
  spdy::SpdySerializedFrame server_rst(new_spdy_util.ConstructSpdyRstStream(
      1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
  spdy::SpdySerializedFrame client_rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));

  // Since this request and response are sent over the tunnel established
  // previously, from a socket-perspective these need to be wrapped as data
  // frames.
  spdy::SpdySerializedFrame wrapped_endpoint_connect(
      new_spdy_util.ConstructSpdyDataFrame(1, endpoint_connect, false));
  spdy::SpdySerializedFrame wrapped_server_rst(
      new_spdy_util.ConstructSpdyDataFrame(1, server_rst, /*fin=*/true));

  MockWrite writes0[] = {
      CreateMockWrite(proxy2_connect, 0),
      CreateMockWrite(wrapped_endpoint_connect, 2),
      CreateMockWrite(client_rst, 5),
  };

  MockRead reads0[] = {
      CreateMockRead(proxy2_connect_resp, 1),
      CreateMockRead(wrapped_server_rst, 3),
      MockRead(ASYNC, 0, 4),
  };

  SequencedSocketData data0(reads0, writes0);

  auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect HTTP/2 protocols too in SSLConfig.
  ssl_provider0->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  ssl_provider0->next_proto = kProtoHTTP2;
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      ssl_provider0.get());

  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider1->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  // Force SPDY.
  ssl_provider1->next_proto = kProtoHTTP2;
  helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider1));

  // Second socket: retry using HTTP/1.1.
  MockWrite writes1[] = {
      MockWrite(ASYNC, 0,
                "CONNECT proxy2.test:71 HTTP/1.1\r\n"
                "Host: proxy2.test:71\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 2,
                "CONNECT www.example.org:443 HTTP/1.1\r\n"
                "Host: www.example.org:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 4,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: keep-alive\r\n\r\n"),
  };

  MockRead reads1[] = {
      MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(ASYNC, 5,
               "HTTP/1.1 200 OK\r\n"
               "Content-Length: 5\r\n\r\n"
               "hello"),
  };
  SequencedSocketData data1(reads1, writes1);

  // Create a new SSLSocketDataProvider for the new connection to the first
  // proxy.
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Force HTTP/1.1 for the reconnection to the first proxy for simplicity.
  ssl_provider2->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP2, kProtoHTTP11};
  ssl_provider2->next_proto = kProtoHTTP11;
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider2));

  // Create a new SSLSocketDataProvider for the new connection to the second
  // proxy.
  auto ssl_provider3 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Expect only HTTP/1.1 protocol in the SSLConfig for the second proxy.
  ssl_provider3->next_protos_expected_in_ssl_config =
      NextProtoVector{kProtoHTTP11};
  // Force HTTP/1.1.
  ssl_provider3->next_proto = kProtoHTTP11;
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      ssl_provider3.get());

  // One final SSL provider for the connection through the proxy.
  auto ssl_provider4 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      ssl_provider4.get());

  HttpServerProperties* http_server_properties =
      helper.session()->spdy_session_pool()->http_server_properties();
  url::SchemeHostPort proxy_scheme_host_port(
      url::kHttpsScheme, kProxyServer2.host_port_pair().host(),
      kProxyServer2.host_port_pair().port());
  EXPECT_FALSE(http_server_properties->RequiresHTTP11(
      proxy_scheme_host_port, NetworkAnonymizationKey()));

  helper.RunPreTestSetup();
  helper.StartDefaultTest();
  helper.FinishDefaultTestWithoutVerification();
  helper.VerifyDataConsumed();
  EXPECT_TRUE(http_server_properties->RequiresHTTP11(
      proxy_scheme_host_port, NetworkAnonymizationKey()));

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  EXPECT_FALSE(response->was_alpn_negotiated);
  EXPECT_TRUE(request_.url.SchemeIs("https"));
  EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(70, response->remote_endpoint.port());
  std::string response_data;
  ASSERT_THAT(ReadTransaction(helper.trans(), &response_data), IsOk());
  EXPECT_EQ("hello", response_data);
}

// Test to make sure we can correctly connect through a proxy.
TEST_P(SpdyNetworkTransactionTest, ProxyConnect) {
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "PROXY myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  const char kConnect443[] = {
      "CONNECT www.example.org:443 HTTP/1.1\r\n"
      "Host: www.example.org:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n\r\n"};
  const char kHTTP200[] = {"HTTP/1.1 200 OK\r\n\r\n"};
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kConnect443, std::size(kConnect443) - 1, 0),
      CreateMockWrite(req, 2),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kHTTP200, std::size(kHTTP200) - 1, 1),
      CreateMockRead(resp, 3),
      CreateMockRead(body, 4),
      MockRead(ASYNC, nullptr, 0, 5),
  };
  SequencedSocketData data(reads, writes);

  helper.AddData(&data);
  TestCompletionCallback callback;

  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Verify the response headers.
  HttpResponseInfo response = *trans->GetResponseInfo();
  ASSERT_TRUE(response.headers);
  EXPECT_EQ("HTTP/1.1 200", response.headers->GetStatusLine());

  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans, &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);
  helper.VerifyDataConsumed();
}

// Test to make sure we can correctly connect through a proxy to
// www.example.org, if there already exists a direct spdy connection to
// www.example.org. See https://crbug.com/49874.
TEST_P(SpdyNetworkTransactionTest, DirectConnectProxyReconnect) {
  // Use a proxy service which returns a proxy fallback list from DIRECT to
  // myproxy:70. For this test there will be no fallback, so it is equivalent
  // to simply DIRECT. The reason for appending the second proxy is to verify
  // that the session pool key used does is just "DIRECT".
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "DIRECT; PROXY myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  // When setting up the first transaction, we store the SpdySessionPool so that
  // we can use the same pool in the second transaction.
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  helper.RunPreTestSetup();

  // Construct and send a simple GET request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3),  // Force a pause
  };
  SequencedSocketData data(reads, writes);
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  TransactionHelperResult out;
  out.rv = trans->Start(&request_, callback.callback(), log_);

  EXPECT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.rv = ReadTransaction(trans, &out.response_data);
  EXPECT_THAT(out.rv, IsOk());
  out.status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the SpdySession is still in the SpdySessionPool.
  SpdySessionKey session_pool_key_direct(
      host_port_pair_, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(HasSpdySession(spdy_session_pool, session_pool_key_direct));
  SpdySessionKey session_pool_key_proxy(
      host_port_pair_, PRIVACY_MODE_DISABLED,
      ProxyUriToProxyChain("www.foo.com", ProxyServer::SCHEME_HTTP),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_FALSE(HasSpdySession(spdy_session_pool, session_pool_key_proxy));

  // New SpdyTestUtil instance for the session that will be used for the
  // proxy connection.
  SpdyTestUtil spdy_util_2(/*use_priority_header=*/true);

  // Set up data for the proxy connection.
  const char kConnect443[] = {
      "CONNECT www.example.org:443 HTTP/1.1\r\n"
      "Host: www.example.org:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n\r\n"};
  const char kHTTP200[] = {"HTTP/1.1 200 OK\r\n\r\n"};
  spdy::SpdySerializedFrame req2(
      spdy_util_2.ConstructSpdyGet(kPushedUrl, 1, LOWEST));
  spdy::SpdySerializedFrame resp2(
      spdy_util_2.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body2(spdy_util_2.ConstructSpdyDataFrame(1, true));

  MockWrite writes2[] = {
      MockWrite(SYNCHRONOUS, kConnect443, std::size(kConnect443) - 1, 0),
      CreateMockWrite(req2, 2),
  };
  MockRead reads2[] = {
      MockRead(SYNCHRONOUS, kHTTP200, std::size(kHTTP200) - 1, 1),
      CreateMockRead(resp2, 3), CreateMockRead(body2, 4),
      MockRead(ASYNC, 0, 5)  // EOF
  };

  SequencedSocketData data_proxy(reads2, writes2);

  // Create another request to www.example.org, but this time through a proxy.
  request_.method = "GET";
  request_.url = GURL(kPushedUrl);
  auto session_deps_proxy = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "PROXY myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper_proxy(request_, DEFAULT_PRIORITY, log_,
                                           std::move(session_deps_proxy));

  helper_proxy.RunPreTestSetup();
  helper_proxy.AddData(&data_proxy);

  HttpNetworkTransaction* trans_proxy = helper_proxy.trans();
  TestCompletionCallback callback_proxy;
  int rv = trans_proxy->Start(&request_, callback_proxy.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback_proxy.WaitForResult();
  EXPECT_EQ(0, rv);

  HttpResponseInfo response_proxy = *trans_proxy->GetResponseInfo();
  ASSERT_TRUE(response_proxy.headers);
  EXPECT_EQ("HTTP/1.1 200", response_proxy.headers->GetStatusLine());

  std::string response_data;
  ASSERT_THAT(ReadTransaction(trans_proxy, &response_data), IsOk());
  EXPECT_EQ("hello!", response_data);

  helper_proxy.VerifyDataConsumed();
}

// When we get a TCP-level RST, we need to retry a HttpNetworkTransaction
// on a new connection, if the connection was previously known to be good.
// This can happen when a server reboots without saying goodbye, or when
// we're behind a NAT that masked the RST.
TEST_P(SpdyNetworkTransactionTest, VerifyRetryOnConnectionReset) {
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1),
      CreateMockRead(body, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3),
      MockRead(ASYNC, ERR_CONNECTION_RESET, 4),
  };

  MockRead reads2[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  // In all cases the connection will be reset before req3 can be
  // dispatched, destroying both streams.
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  MockWrite writes1[] = {CreateMockWrite(req, 0), CreateMockWrite(req3, 5)};
  MockWrite writes2[] = {CreateMockWrite(req, 0)};

  // This test has a couple of variants.
  enum : size_t {
    // Induce the RST while waiting for our transaction to send.
    VARIANT_RST_DURING_SEND_COMPLETION = 0,
    // Induce the RST while waiting for our transaction to read.
    // In this case, the send completed - everything copied into the SNDBUF.
    VARIANT_RST_DURING_READ_COMPLETION = 1
  };

  for (size_t variant = VARIANT_RST_DURING_SEND_COMPLETION;
       variant <= VARIANT_RST_DURING_READ_COMPLETION; ++variant) {
    SequencedSocketData data1(reads,
                              base::make_span(writes1).first(1u + variant));

    SequencedSocketData data2(reads2, writes2);

    NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                       nullptr);
    helper.AddData(&data1);
    helper.AddData(&data2);
    helper.RunPreTestSetup();

    for (int i = 0; i < 2; ++i) {
      HttpNetworkTransaction trans(DEFAULT_PRIORITY, helper.session());

      TestCompletionCallback callback;
      int rv = trans.Start(&request_, callback.callback(), log_);
      EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
      // On the second transaction, we trigger the RST.
      if (i == 1) {
        if (variant == VARIANT_RST_DURING_READ_COMPLETION) {
          // Writes to the socket complete asynchronously on SPDY by running
          // through the message loop.  Complete the write here.
          base::RunLoop().RunUntilIdle();
        }

        // Now schedule the ERR_CONNECTION_RESET.
        data1.Resume();
      }
      rv = callback.WaitForResult();
      EXPECT_THAT(rv, IsOk());

      const HttpResponseInfo* response = trans.GetResponseInfo();
      ASSERT_TRUE(response);
      EXPECT_TRUE(response->headers);
      EXPECT_TRUE(response->was_fetched_via_spdy);
      std::string response_data;
      rv = ReadTransaction(&trans, &response_data);
      EXPECT_THAT(rv, IsOk());
      EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
      EXPECT_EQ("hello!", response_data);
      base::RunLoop().RunUntilIdle();
    }

    helper.VerifyDataConsumed();
    base::RunLoop().RunUntilIdle();
  }
}

// Tests that Basic authentication works over SPDY
TEST_P(SpdyNetworkTransactionTest, SpdyBasicAuth) {
  // The first request will be a bare GET, the second request will be a
  // GET with an Authorization header.
  spdy::SpdySerializedFrame req_get(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  // Will be refused for lack of auth.
  spdy_util_.UpdateWithStreamDestruction(1);
  const char* const kExtraAuthorizationHeaders[] = {"authorization",
                                                    "Basic Zm9vOmJhcg=="};
  spdy::SpdySerializedFrame req_get_authorization(spdy_util_.ConstructSpdyGet(
      kExtraAuthorizationHeaders, std::size(kExtraAuthorizationHeaders) / 2, 3,
      LOWEST));
  MockWrite spdy_writes[] = {
      CreateMockWrite(req_get, 0),
      CreateMockWrite(req_get_authorization, 3),
  };

  // The first response is a 401 authentication challenge, and the second
  // response will be a 200 response since the second request includes a valid
  // Authorization header.
  const char* const kExtraAuthenticationHeaders[] = {"www-authenticate",
                                                     "Basic realm=\"MyRealm\""};
  spdy::SpdySerializedFrame resp_authentication(
      spdy_util_.ConstructSpdyReplyError(
          "401", kExtraAuthenticationHeaders,
          std::size(kExtraAuthenticationHeaders) / 2, 1));
  spdy::SpdySerializedFrame body_authentication(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp_data(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body_data(
      spdy_util_.ConstructSpdyDataFrame(3, true));

  MockRead spdy_reads[] = {
      CreateMockRead(resp_authentication, 1),
      CreateMockRead(body_authentication, 2, SYNCHRONOUS),
      CreateMockRead(resp_data, 4),
      CreateMockRead(body_data, 5),
      MockRead(ASYNC, 0, 6),
  };

  SequencedSocketData data(spdy_reads, spdy_writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.StartDefaultTest();
  EXPECT_THAT(helper.output().rv, IsError(ERR_IO_PENDING));

  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsOk());

  // Make sure the response has an auth challenge.
  HttpNetworkTransaction* trans = helper.trans();
  const HttpResponseInfo* const response_start = trans->GetResponseInfo();
  ASSERT_TRUE(response_start);
  ASSERT_TRUE(response_start->headers);
  EXPECT_EQ(401, response_start->headers->response_code());
  EXPECT_TRUE(response_start->was_fetched_via_spdy);
  const std::optional<AuthChallengeInfo>& auth_challenge =
      response_start->auth_challenge;
  ASSERT_TRUE(auth_challenge);
  EXPECT_FALSE(auth_challenge->is_proxy);
  EXPECT_EQ(kBasicAuthScheme, auth_challenge->scheme);
  EXPECT_EQ("MyRealm", auth_challenge->realm);

  // Restart with a username/password.
  AuthCredentials credentials(u"foo", u"bar");
  TestCompletionCallback callback_restart;
  const int rv_restart =
      trans->RestartWithAuth(credentials, callback_restart.callback());
  EXPECT_THAT(rv_restart, IsError(ERR_IO_PENDING));
  const int rv_restart_complete = callback_restart.WaitForResult();
  EXPECT_THAT(rv_restart_complete, IsOk());
  // TODO(cbentzel): This is actually the same response object as before, but
  // data has changed.
  const HttpResponseInfo* const response_restart = trans->GetResponseInfo();
  ASSERT_TRUE(response_restart);
  ASSERT_TRUE(response_restart->headers);
  EXPECT_EQ(200, response_restart->headers->response_code());
  EXPECT_FALSE(response_restart->auth_challenge);
}

TEST_P(SpdyNetworkTransactionTest, ResponseHeadersTwice) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(rst, 4),
  };

  spdy::SpdySerializedFrame stream1_reply(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  quiche::HttpHeaderBlock late_headers;
  late_headers["hello"] = "bye";
  spdy::SpdySerializedFrame stream1_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, std::move(late_headers),
                                              false));
  spdy::SpdySerializedFrame stream1_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(stream1_reply, 1), CreateMockRead(stream1_headers, 2),
      CreateMockRead(stream1_body, 3), MockRead(ASYNC, 0, 5)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Tests that receiving HEADERS, DATA, HEADERS, and DATA in that sequence will
// trigger a ERR_HTTP2_PROTOCOL_ERROR because trailing HEADERS must not be
// followed by any DATA frames.
TEST_P(SpdyNetworkTransactionTest, SyncReplyDataAfterTrailers) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(rst, 5),
  };

  spdy::SpdySerializedFrame stream1_reply(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame stream1_body(
      spdy_util_.ConstructSpdyDataFrame(1, false));

  quiche::HttpHeaderBlock late_headers;
  late_headers["hello"] = "bye";
  spdy::SpdySerializedFrame stream1_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, std::move(late_headers),
                                              false));
  spdy::SpdySerializedFrame stream1_body2(
      spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(stream1_reply, 1), CreateMockRead(stream1_body, 2),
      CreateMockRead(stream1_headers, 3), CreateMockRead(stream1_body2, 4),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

TEST_P(SpdyNetworkTransactionTest, RetryAfterRefused) {
  // Construct the request.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  // Will be destroyed by the RST before stream 3 starts.
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(req2, 2),
  };

  spdy::SpdySerializedFrame refused(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_REFUSED_STREAM));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {
      CreateMockRead(refused, 1), CreateMockRead(resp, 3),
      CreateMockRead(body, 4), MockRead(ASYNC, 0, 5)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  // Finish async network reads.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());

  // Verify the response headers.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers);
  EXPECT_EQ("HTTP/1.1 200", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, OutOfOrderHeaders) {
  // This first request will start to establish the SpdySession.
  // Then we will start the second (MEDIUM priority) and then third
  // (HIGHEST priority) request in such a way that the third will actually
  // start before the second, causing the second to be numbered differently
  // than the order they were created.
  //
  // Note that the requests and responses created below are expectations
  // of what the above will produce on the wire, and hence are in the
  // initial->HIGHEST->LOWEST priority.
  //
  // Frames are created by SpdySession just before the write associated
  // with the frame is attempted, so stream dependencies will be based
  // on the streams alive at the point of the request write attempt.  Thus
  // req1 is alive when req2 is attempted (during but not after the
  // |data.RunFor(2);| statement below) but not when req3 is attempted.
  // The call to spdy_util_.UpdateWithStreamDestruction() reflects this.
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, HIGHEST));
  spdy_util_.UpdateWithStreamDestruction(1);
  spdy::SpdySerializedFrame req3(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 5, MEDIUM));
  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_IO_PENDING, 0),
      CreateMockWrite(req1, 1),
      CreateMockWrite(req2, 5),
      CreateMockWrite(req3, 6),
  };

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
      CreateMockRead(resp1, 2),  MockRead(ASYNC, ERR_IO_PENDING, 3),
      CreateMockRead(body1, 4),  CreateMockRead(resp2, 7),
      CreateMockRead(body2, 8),  CreateMockRead(resp3, 9),
      CreateMockRead(body3, 10), MockRead(ASYNC, 0, 11)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, LOWEST, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  // Start the first transaction to set up the SpdySession
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Run the message loop, but do not allow the write to complete.
  // This leaves the SpdySession with a write pending, which prevents
  // SpdySession from attempting subsequent writes until this write completes.
  base::RunLoop().RunUntilIdle();

  // Now, start both new transactions
  TestCompletionCallback callback2;
  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  rv = trans2.Start(&request_, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();

  TestCompletionCallback callback3;
  HttpNetworkTransaction trans3(HIGHEST, helper.session());
  rv = trans3.Start(&request_, callback3.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();

  // We now have two HEADERS frames queued up which will be
  // dequeued only once the first write completes, which we
  // now allow to happen.
  ASSERT_TRUE(data.IsPaused());
  data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // And now we can allow everything else to run to completion.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_THAT(callback3.WaitForResult(), IsOk());

  helper.VerifyDataConsumed();

  // At this point the test is completed and we need to safely destroy
  // all allocated structures. Helper stores a transaction that has a
  // reference to a stack allocated request, which has a short lifetime,
  // and is accessed during the transaction destruction. We need to delete
  // the transaction while the request is still a valid object.
  helper.ResetTrans();
}

// Test that sent data frames and received WINDOW_UPDATE frames change
// the send_window_size_ correctly.

// WINDOW_UPDATE is different than most other frames in that it can arrive
// while the client is still sending the request body.  In order to enforce
// this scenario, we feed a couple of dummy frames and give a delay of 0 to
// socket data provider, so that initial read that is done as soon as the
// stream is created, succeeds and schedules another read.  This way reads
// and writes are interleaved; after doing a full frame write, SpdyStream
// will break out of DoLoop and will read and process a WINDOW_UPDATE.
// Once our WINDOW_UPDATE is read, we cannot send HEADERS right away
// since request has not been completely written, therefore we feed
// enough number of WINDOW_UPDATEs to finish the first read and cause a
// write, leading to a complete write of request body; after that we send
// a reply with a body, to cause a graceful shutdown.

// TODO(agayev): develop a socket data provider where both, reads and
// writes are ordered so that writing tests like these are easy and rewrite
// all these tests using it.  Right now we are working around the
// limitations as described above and it's not deterministic, tests may
// fail under specific circumstances.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateReceived) {
  static int kFrameCount = 2;
  std::string content(kMaxSpdyFrameChunkSize, 'a');
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kMaxSpdyFrameChunkSize * kFrameCount, LOWEST, nullptr,
      0));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(1, content, false));
  spdy::SpdySerializedFrame body_end(
      spdy_util_.ConstructSpdyDataFrame(1, content, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(body, 1),
      CreateMockWrite(body_end, 2),
  };

  static const int32_t kDeltaWindowSize = 0xff;
  static const int kDeltaCount = 4;
  spdy::SpdySerializedFrame window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  spdy::SpdySerializedFrame window_update_dummy(
      spdy_util_.ConstructSpdyWindowUpdate(2, kDeltaWindowSize));
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(window_update_dummy, 3),
      CreateMockRead(window_update_dummy, 4),
      CreateMockRead(window_update_dummy, 5),
      CreateMockRead(window_update, 6),  // Four updates, therefore window
      CreateMockRead(window_update, 7),  // size should increase by
      CreateMockRead(window_update, 8),  // kDeltaWindowSize * 4
      CreateMockRead(window_update, 9),
      CreateMockRead(resp, 10),
      MockRead(ASYNC, ERR_IO_PENDING, 11),
      CreateMockRead(body_end, 12),
      MockRead(ASYNC, 0, 13)  // EOF
  };

  SequencedSocketData data(reads, writes);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  for (int i = 0; i < kFrameCount; ++i) {
    element_readers.push_back(std::make_unique<UploadBytesElementReader>(
        base::as_byte_span(content)));
  }
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  // Setup the request.
  request_.method = "POST";
  request_.upload_data_stream = &upload_data_stream;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.AddData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);

  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  data.RunUntilPaused();
  base::RunLoop().RunUntilIdle();

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->stream());
  EXPECT_EQ(static_cast<int>(kDefaultInitialWindowSize) +
                kDeltaWindowSize * kDeltaCount -
                kMaxSpdyFrameChunkSize * kFrameCount,
            stream->stream()->send_window_size());

  data.Resume();
  base::RunLoop().RunUntilIdle();

  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  helper.VerifyDataConsumed();
}

// Test that received data frames and sent WINDOW_UPDATE frames change
// the recv_window_size_ correctly.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateSent) {
  // Session level maximum window size that is more than twice the default
  // initial window size so that an initial window update is sent.
  const int32_t session_max_recv_window_size = 5 * 64 * 1024;
  ASSERT_LT(2 * kDefaultInitialWindowSize, session_max_recv_window_size);
  // Stream level maximum window size that is less than the session level
  // maximum window size so that we test for confusion between the two.
  const int32_t stream_max_recv_window_size = 4 * 64 * 1024;
  ASSERT_GT(session_max_recv_window_size, stream_max_recv_window_size);
  // Size of body to be sent.  Has to be less than or equal to both window sizes
  // so that we do not run out of receiving window.  Also has to be greater than
  // half of them so that it triggers both a session level and a stream level
  // window update frame.
  const int32_t kTargetSize = 3 * 64 * 1024;
  ASSERT_GE(session_max_recv_window_size, kTargetSize);
  ASSERT_GE(stream_max_recv_window_size, kTargetSize);
  ASSERT_LT(session_max_recv_window_size / 2, kTargetSize);
  ASSERT_LT(stream_max_recv_window_size / 2, kTargetSize);
  // Size of each DATA frame.
  const int32_t kChunkSize = 4096;
  // Size of window updates.
  ASSERT_EQ(0, session_max_recv_window_size / 2 % kChunkSize);
  const int32_t session_window_update_delta =
      session_max_recv_window_size / 2 + kChunkSize;
  ASSERT_EQ(0, stream_max_recv_window_size / 2 % kChunkSize);
  const int32_t stream_window_update_delta =
      stream_max_recv_window_size / 2 + kChunkSize;

  spdy::SpdySerializedFrame preface(spdy::test::MakeSerializedFrame(
      const_cast<char*>(spdy::kHttp2ConnectionHeaderPrefix),
      spdy::kHttp2ConnectionHeaderPrefixSize));

  spdy::SettingsMap initial_settings;
  initial_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;
  initial_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
      stream_max_recv_window_size;
  initial_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
      kSpdyMaxHeaderListSize;
  initial_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
  spdy::SpdySerializedFrame initial_settings_frame(
      spdy_util_.ConstructSpdySettings(initial_settings));

  spdy::SpdySerializedFrame initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          spdy::kSessionFlowControlStreamId,
          session_max_recv_window_size - kDefaultInitialWindowSize));

  spdy::SpdySerializedFrame combined_frames = CombineFrames(
      {&preface, &initial_settings_frame, &initial_window_update});

  std::vector<MockWrite> writes;
  writes.push_back(CreateMockWrite(combined_frames));

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  writes.push_back(CreateMockWrite(req, writes.size()));

  std::vector<MockRead> reads;
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  reads.push_back(CreateMockRead(resp, writes.size() + reads.size()));

  std::vector<spdy::SpdySerializedFrame> body_frames;
  const std::string body_data(kChunkSize, 'x');
  for (size_t remaining = kTargetSize; remaining != 0;) {
    size_t frame_size = std::min(remaining, body_data.size());
    body_frames.push_back(spdy_util_.ConstructSpdyDataFrame(
        1, std::string_view(body_data.data(), frame_size), false));
    reads.push_back(
        CreateMockRead(body_frames.back(), writes.size() + reads.size()));
    remaining -= frame_size;
  }
  // Yield.
  reads.emplace_back(SYNCHRONOUS, ERR_IO_PENDING, writes.size() + reads.size());

  spdy::SpdySerializedFrame session_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(0, session_window_update_delta));
  writes.push_back(
      CreateMockWrite(session_window_update, writes.size() + reads.size()));
  spdy::SpdySerializedFrame stream_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, stream_window_update_delta));
  writes.push_back(
      CreateMockWrite(stream_window_update, writes.size() + reads.size()));

  SequencedSocketData data(reads, writes);

  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->session_max_recv_window_size = session_max_recv_window_size;
  session_deps->http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
      stream_max_recv_window_size;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.AddData(&data);
  helper.RunPreTestSetup();

  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  SpdySessionPoolPeer pool_peer(spdy_session_pool);
  pool_peer.SetEnableSendingInitialData(true);

  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);

  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  // Finish async network reads.
  base::RunLoop().RunUntilIdle();

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->stream());

  // All data has been read, but not consumed. The window reflects this.
  EXPECT_EQ(static_cast<int>(stream_max_recv_window_size - kTargetSize),
            stream->stream()->recv_window_size());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);

  // Issue a read which will cause a WINDOW_UPDATE to be sent and window
  // size increased to default.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kTargetSize);
  EXPECT_EQ(static_cast<int>(kTargetSize),
            trans->Read(buf.get(), kTargetSize, CompletionOnceCallback()));
  EXPECT_EQ(static_cast<int>(stream_max_recv_window_size),
            stream->stream()->recv_window_size());
  EXPECT_THAT(std::string_view(buf->data(), kTargetSize), Each(Eq('x')));

  // Allow scheduled WINDOW_UPDATE frames to write.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// Test that WINDOW_UPDATE frame causing overflow is handled correctly.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateOverflow) {
  // Number of full frames we hope to write (but will not, used to
  // set content-length header correctly)
  static int kFrameCount = 3;

  std::string content(kMaxSpdyFrameChunkSize, 'a');
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kMaxSpdyFrameChunkSize * kFrameCount, LOWEST, nullptr,
      0));
  spdy::SpdySerializedFrame body(
      spdy_util_.ConstructSpdyDataFrame(1, content, false));
  spdy::SpdySerializedFrame rst(spdy_util_.ConstructSpdyRstStream(
      1, spdy::ERROR_CODE_FLOW_CONTROL_ERROR));

  // We're not going to write a data frame with FIN, we'll receive a bad
  // WINDOW_UPDATE while sending a request and will send a RST_STREAM frame.
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
      CreateMockWrite(body, 2),
      CreateMockWrite(rst, 3),
  };

  static const int32_t kDeltaWindowSize = 0x7fffffff;  // cause an overflow
  spdy::SpdySerializedFrame window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  MockRead reads[] = {
      CreateMockRead(window_update, 1), MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  for (int i = 0; i < kFrameCount; ++i) {
    element_readers.push_back(std::make_unique<UploadBytesElementReader>(
        base::as_byte_span(content)));
  }
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  // Setup the request.
  request_.method = "POST";
  request_.upload_data_stream = &upload_data_stream;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_HTTP2_FLOW_CONTROL_ERROR));
  helper.VerifyDataConsumed();
}

// Regression test for https://crbug.com/732019.
// RFC7540 Section 6.9.2: A spdy::SETTINGS_INITIAL_WINDOW_SIZE change that
// causes any stream flow control window to overflow MUST be treated as a
// connection error.
TEST_P(SpdyNetworkTransactionTest, InitialWindowSizeOverflow) {
  spdy::SpdySerializedFrame window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, 0x60000000));
  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = 0x60000000;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  MockRead reads[] = {CreateMockRead(window_update, 1),
                      CreateMockRead(settings_frame, 2)};

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  spdy::SpdySerializedFrame goaway(
      spdy_util_.ConstructSpdyGoAway(0, spdy::ERROR_CODE_FLOW_CONTROL_ERROR,
                                     "New spdy::SETTINGS_INITIAL_WINDOW_SIZE "
                                     "value overflows flow control window of "
                                     "stream 1."));
  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(settings_ack, 3),
                        CreateMockWrite(goaway, 4)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_FLOW_CONTROL_ERROR));
}

// Tests that we close the connection if we try to enqueue more frames than
// the cap allows.
TEST_P(SpdyNetworkTransactionTest, SessionMaxQueuedCappedFramesExceeded) {
  const int kTestSessionMaxQueuedCappedFrames = 5;
  const int kTestNumPings = kTestSessionMaxQueuedCappedFrames + 1;
  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = 0xffff;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  std::vector<spdy::SpdySerializedFrame> ping_frames;

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  std::vector<MockWrite> writes;
  std::vector<MockRead> reads;
  // Send request, receive SETTINGS and send a SETTINGS ACK.
  writes.push_back(CreateMockWrite(req, writes.size() + reads.size()));
  reads.push_back(CreateMockRead(settings_frame, writes.size() + reads.size()));
  writes.push_back(CreateMockWrite(settings_ack, writes.size() + reads.size()));
  // Receive more pings than our limit allows.
  for (int i = 1; i <= kTestNumPings; ++i) {
    ping_frames.push_back(
        spdy_util_.ConstructSpdyPing(/*ping_id=*/i, /*is_ack=*/false));
    reads.push_back(
        CreateMockRead(ping_frames.back(), writes.size() + reads.size()));
  }
  // Only write PING ACKs after receiving all of them to ensure they are all in
  // the write queue.
  for (int i = 1; i <= kTestNumPings; ++i) {
    ping_frames.push_back(
        spdy_util_.ConstructSpdyPing(/*ping_id=*/i, /*is_ack=*/true));
    writes.push_back(
        CreateMockWrite(ping_frames.back(), writes.size() + reads.size()));
  }
  // Stop reading.
  reads.emplace_back(ASYNC, 0, writes.size() + reads.size());

  SequencedSocketData data(reads, writes);
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->session_max_queued_capped_frames =
      kTestSessionMaxQueuedCappedFrames;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_CONNECTION_CLOSED));
}

// Test that after hitting a send window size of 0, the write process
// stalls and upon receiving WINDOW_UPDATE frame write resumes.

// This test constructs a POST request followed by enough data frames
// containing 'a' that would make the window size 0, followed by another
// data frame containing default content (which is "hello!") and this frame
// also contains a FIN flag.  SequencedSocketData is used to enforce all
// writes, save the last, go through before a read could happen.  The last frame
// ("hello!") is not permitted to go through since by the time its turn
// arrives, window size is 0.  At this point MessageLoop::Run() called via
// callback would block.  Therefore we call MessageLoop::RunUntilIdle()
// which returns after performing all possible writes.  We use DCHECKS to
// ensure that last data frame is still there and stream has stalled.
// After that, next read is artifically enforced, which causes a
// WINDOW_UPDATE to be read and I/O process resumes.
TEST_P(SpdyNetworkTransactionTest, FlowControlStallResume) {
  const int32_t initial_window_size = kDefaultInitialWindowSize;
  // Number of upload data buffers we need to send to zero out the window size
  // is the minimal number of upload buffers takes to be bigger than
  // |initial_window_size|.
  size_t num_upload_buffers =
      ceil(static_cast<double>(initial_window_size) / kBufferSize);
  // Each upload data buffer consists of |num_frames_in_one_upload_buffer|
  // frames, each with |kMaxSpdyFrameChunkSize| bytes except the last frame,
  // which has kBufferSize % kMaxSpdyChunkSize bytes.
  size_t num_frames_in_one_upload_buffer =
      ceil(static_cast<double>(kBufferSize) / kMaxSpdyFrameChunkSize);

  // Construct content for a data frame of maximum size.
  std::string content(kMaxSpdyFrameChunkSize, 'a');

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1,
      /*content_length=*/kBufferSize * num_upload_buffers + kUploadDataSize,
      LOWEST, nullptr, 0));

  // Full frames.
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, content, false));

  // Last frame in each upload data buffer.
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(content.data(), kBufferSize % kMaxSpdyFrameChunkSize),
      false));

  // The very last frame before the stalled frames.
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(
      1,
      std::string_view(content.data(), initial_window_size % kBufferSize %
                                           kMaxSpdyFrameChunkSize),
      false));

  // Data frames to be sent once WINDOW_UPDATE frame is received.

  // If kBufferSize * num_upload_buffers > initial_window_size,
  // we need one additional frame to send the rest of 'a'.
  std::string last_body(kBufferSize * num_upload_buffers - initial_window_size,
                        'a');
  spdy::SpdySerializedFrame body4(
      spdy_util_.ConstructSpdyDataFrame(1, last_body, false));

  // Also send a "hello!" after WINDOW_UPDATE.
  spdy::SpdySerializedFrame body5(spdy_util_.ConstructSpdyDataFrame(1, true));

  // Fill in mock writes.
  size_t i = 0;
  std::vector<MockWrite> writes;
  writes.push_back(CreateMockWrite(req, i++));
  for (size_t j = 0; j < num_upload_buffers; j++) {
    for (size_t k = 0; k < num_frames_in_one_upload_buffer; k++) {
      if (j == num_upload_buffers - 1 &&
          (initial_window_size % kBufferSize != 0)) {
        writes.push_back(CreateMockWrite(body3, i++));
      } else if (k == num_frames_in_one_upload_buffer - 1 &&
                 kBufferSize % kMaxSpdyFrameChunkSize != 0) {
        writes.push_back(CreateMockWrite(body2, i++));
      } else {
        writes.push_back(CreateMockWrite(body1, i++));
      }
    }
  }

  // Fill in mock reads.
  std::vector<MockRead> reads;
  // Force a pause.
  reads.emplace_back(ASYNC, ERR_IO_PENDING, i++);
  // Construct read frame for window updates that gives enough space to upload
  // the rest of the data.
  spdy::SpdySerializedFrame session_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(0,
                                           kUploadDataSize + last_body.size()));
  spdy::SpdySerializedFrame window_update(spdy_util_.ConstructSpdyWindowUpdate(
      1, kUploadDataSize + last_body.size()));

  reads.push_back(CreateMockRead(session_window_update, i++));
  reads.push_back(CreateMockRead(window_update, i++));

  // Stalled frames which can be sent after receiving window updates.
  if (last_body.size() > 0) {
    writes.push_back(CreateMockWrite(body4, i++));
  }
  writes.push_back(CreateMockWrite(body5, i++));

  spdy::SpdySerializedFrame reply(
      spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  reads.push_back(CreateMockRead(reply, i++));
  reads.push_back(CreateMockRead(body2, i++));
  reads.push_back(CreateMockRead(body5, i++));
  reads.emplace_back(ASYNC, 0, i++);  // EOF

  SequencedSocketData data(reads, writes);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  std::string upload_data_string(kBufferSize * num_upload_buffers, 'a');
  upload_data_string.append(kUploadData, kUploadDataSize);
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::as_byte_span(upload_data_string)));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  request_.method = "POST";
  request_.upload_data_stream = &upload_data_stream;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.AddData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();  // Write as much as we can.

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->stream());
  EXPECT_EQ(0, stream->stream()->send_window_size());
  if (initial_window_size % kBufferSize != 0) {
    // If it does not take whole number of full upload buffer to zero out
    // initial window size, then the upload data is not at EOF, because the
    // last read must be stalled.
    EXPECT_FALSE(upload_data_stream.IsEOF());
  } else {
    // All the body data should have been read.
    // TODO(satorux): This is because of the weirdness in reading the request
    // body in OnSendBodyComplete(). See crbug.com/113107.
    EXPECT_TRUE(upload_data_stream.IsEOF());
  }
  // But the body is not yet fully sent (kUploadData is not yet sent)
  // since we're send-stalled.
  EXPECT_TRUE(stream->stream()->send_stalled_by_flow_control());

  data.Resume();  // Read in WINDOW_UPDATE frame.
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());

  // Finish async network reads.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// Test we correctly handle the case where the SETTINGS frame results in
// unstalling the send window.
TEST_P(SpdyNetworkTransactionTest, FlowControlStallResumeAfterSettings) {
  const int32_t initial_window_size = kDefaultInitialWindowSize;
  // Number of upload data buffers we need to send to zero out the window size
  // is the minimal number of upload buffers takes to be bigger than
  // |initial_window_size|.
  size_t num_upload_buffers =
      ceil(static_cast<double>(initial_window_size) / kBufferSize);
  // Each upload data buffer consists of |num_frames_in_one_upload_buffer|
  // frames, each with |kMaxSpdyFrameChunkSize| bytes except the last frame,
  // which has kBufferSize % kMaxSpdyChunkSize bytes.
  size_t num_frames_in_one_upload_buffer =
      ceil(static_cast<double>(kBufferSize) / kMaxSpdyFrameChunkSize);

  // Construct content for a data frame of maximum size.
  std::string content(kMaxSpdyFrameChunkSize, 'a');

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1,
      /*content_length=*/kBufferSize * num_upload_buffers + kUploadDataSize,
      LOWEST, nullptr, 0));

  // Full frames.
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, content, false));

  // Last frame in each upload data buffer.
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(content.data(), kBufferSize % kMaxSpdyFrameChunkSize),
      false));

  // The very last frame before the stalled frames.
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(
      1,
      std::string_view(content.data(), initial_window_size % kBufferSize %
                                           kMaxSpdyFrameChunkSize),
      false));

  // Data frames to be sent once WINDOW_UPDATE frame is received.

  // If kBufferSize * num_upload_buffers > initial_window_size,
  // we need one additional frame to send the rest of 'a'.
  std::string last_body(kBufferSize * num_upload_buffers - initial_window_size,
                        'a');
  spdy::SpdySerializedFrame body4(
      spdy_util_.ConstructSpdyDataFrame(1, last_body, false));

  // Also send a "hello!" after WINDOW_UPDATE.
  spdy::SpdySerializedFrame body5(spdy_util_.ConstructSpdyDataFrame(1, true));

  // Fill in mock writes.
  size_t i = 0;
  std::vector<MockWrite> writes;
  writes.push_back(CreateMockWrite(req, i++));
  for (size_t j = 0; j < num_upload_buffers; j++) {
    for (size_t k = 0; k < num_frames_in_one_upload_buffer; k++) {
      if (j == num_upload_buffers - 1 &&
          (initial_window_size % kBufferSize != 0)) {
        writes.push_back(CreateMockWrite(body3, i++));
      } else if (k == num_frames_in_one_upload_buffer - 1 &&
                 kBufferSize % kMaxSpdyFrameChunkSize != 0) {
        writes.push_back(CreateMockWrite(body2, i++));
      } else {
        writes.push_back(CreateMockWrite(body1, i++));
      }
    }
  }

  // Fill in mock reads.
  std::vector<MockRead> reads;
  // Force a pause.
  reads.emplace_back(ASYNC, ERR_IO_PENDING, i++);

  // Construct read frame for SETTINGS that gives enough space to upload the
  // rest of the data.
  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = initial_window_size * 2;
  spdy::SpdySerializedFrame settings_frame_large(
      spdy_util_.ConstructSpdySettings(settings));

  reads.push_back(CreateMockRead(settings_frame_large, i++));

  spdy::SpdySerializedFrame session_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(0,
                                           last_body.size() + kUploadDataSize));
  reads.push_back(CreateMockRead(session_window_update, i++));

  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  writes.push_back(CreateMockWrite(settings_ack, i++));

  // Stalled frames which can be sent after |settings_ack|.
  if (last_body.size() > 0) {
    writes.push_back(CreateMockWrite(body4, i++));
  }
  writes.push_back(CreateMockWrite(body5, i++));

  spdy::SpdySerializedFrame reply(
      spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  reads.push_back(CreateMockRead(reply, i++));
  reads.push_back(CreateMockRead(body2, i++));
  reads.push_back(CreateMockRead(body5, i++));
  reads.emplace_back(ASYNC, 0, i++);  // EOF

  // Force all writes to happen before any read, last write will not
  // actually queue a frame, due to window size being 0.
  SequencedSocketData data(reads, writes);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  std::string upload_data_string(kBufferSize * num_upload_buffers, 'a');
  upload_data_string.append(kUploadData, kUploadDataSize);
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::as_byte_span(upload_data_string)));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  request_.method = "POST";
  request_.upload_data_stream = &upload_data_stream;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  data.RunUntilPaused();  // Write as much as we can.
  base::RunLoop().RunUntilIdle();

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->stream());
  EXPECT_EQ(0, stream->stream()->send_window_size());

  if (initial_window_size % kBufferSize != 0) {
    // If it does not take whole number of full upload buffer to zero out
    // initial window size, then the upload data is not at EOF, because the
    // last read must be stalled.
    EXPECT_FALSE(upload_data_stream.IsEOF());
  } else {
    // All the body data should have been read.
    // TODO(satorux): This is because of the weirdness in reading the request
    // body in OnSendBodyComplete(). See crbug.com/113107.
    EXPECT_TRUE(upload_data_stream.IsEOF());
  }
  // But the body is not yet fully sent (kUploadData is not yet sent)
  // since we're send-stalled.
  EXPECT_TRUE(stream->stream()->send_stalled_by_flow_control());

  // Read in SETTINGS frame to unstall.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  rv = callback.WaitForResult();
  helper.VerifyDataConsumed();
  // If stream is nullptr, that means it was unstalled and closed.
  EXPECT_TRUE(stream->stream() == nullptr);
}

// Test we correctly handle the case where the SETTINGS frame results in a
// negative send window size.
TEST_P(SpdyNetworkTransactionTest, FlowControlNegativeSendWindowSize) {
  const int32_t initial_window_size = kDefaultInitialWindowSize;
  // Number of upload data buffers we need to send to zero out the window size
  // is the minimal number of upload buffers takes to be bigger than
  // |initial_window_size|.
  size_t num_upload_buffers =
      ceil(static_cast<double>(initial_window_size) / kBufferSize);
  // Each upload data buffer consists of |num_frames_in_one_upload_buffer|
  // frames, each with |kMaxSpdyFrameChunkSize| bytes except the last frame,
  // which has kBufferSize % kMaxSpdyChunkSize bytes.
  size_t num_frames_in_one_upload_buffer =
      ceil(static_cast<double>(kBufferSize) / kMaxSpdyFrameChunkSize);

  // Construct content for a data frame of maximum size.
  std::string content(kMaxSpdyFrameChunkSize, 'a');

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1,
      /*content_length=*/kBufferSize * num_upload_buffers + kUploadDataSize,
      LOWEST, nullptr, 0));

  // Full frames.
  spdy::SpdySerializedFrame body1(
      spdy_util_.ConstructSpdyDataFrame(1, content, false));

  // Last frame in each upload data buffer.
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(
      1, std::string_view(content.data(), kBufferSize % kMaxSpdyFrameChunkSize),
      false));

  // The very last frame before the stalled frames.
  spdy::SpdySerializedFrame body3(spdy_util_.ConstructSpdyDataFrame(
      1,
      std::string_view(content.data(), initial_window_size % kBufferSize %
                                           kMaxSpdyFrameChunkSize),
      false));

  // Data frames to be sent once WINDOW_UPDATE frame is received.

  // If kBufferSize * num_upload_buffers > initial_window_size,
  // we need one additional frame to send the rest of 'a'.
  std::string last_body(kBufferSize * num_upload_buffers - initial_window_size,
                        'a');
  spdy::SpdySerializedFrame body4(
      spdy_util_.ConstructSpdyDataFrame(1, last_body, false));

  // Also send a "hello!" after WINDOW_UPDATE.
  spdy::SpdySerializedFrame body5(spdy_util_.ConstructSpdyDataFrame(1, true));

  // Fill in mock writes.
  size_t i = 0;
  std::vector<MockWrite> writes;
  writes.push_back(CreateMockWrite(req, i++));
  for (size_t j = 0; j < num_upload_buffers; j++) {
    for (size_t k = 0; k < num_frames_in_one_upload_buffer; k++) {
      if (j == num_upload_buffers - 1 &&
          (initial_window_size % kBufferSize != 0)) {
        writes.push_back(CreateMockWrite(body3, i++));
      } else if (k == num_frames_in_one_upload_buffer - 1 &&
                 kBufferSize % kMaxSpdyFrameChunkSize != 0) {
        writes.push_back(CreateMockWrite(body2, i++));
      } else {
        writes.push_back(CreateMockWrite(body1, i++));
      }
    }
  }

  // Fill in mock reads.
  std::vector<MockRead> reads;
  // Force a pause.
  reads.emplace_back(ASYNC, ERR_IO_PENDING, i++);
  // Construct read frame for SETTINGS that makes the send_window_size
  // negative.
  spdy::SettingsMap new_settings;
  new_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = initial_window_size / 2;
  spdy::SpdySerializedFrame settings_frame_small(
      spdy_util_.ConstructSpdySettings(new_settings));
  // Construct read frames for WINDOW_UPDATE that makes the send_window_size
  // positive.
  spdy::SpdySerializedFrame session_window_update_init_size(
      spdy_util_.ConstructSpdyWindowUpdate(0, initial_window_size));
  spdy::SpdySerializedFrame window_update_init_size(
      spdy_util_.ConstructSpdyWindowUpdate(1, initial_window_size));

  reads.push_back(CreateMockRead(settings_frame_small, i++));
  reads.push_back(CreateMockRead(session_window_update_init_size, i++));
  reads.push_back(CreateMockRead(window_update_init_size, i++));

  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());
  writes.push_back(CreateMockWrite(settings_ack, i++));

  // Stalled frames which can be sent after |settings_ack|.
  if (last_body.size() > 0) {
    writes.push_back(CreateMockWrite(body4, i++));
  }
  writes.push_back(CreateMockWrite(body5, i++));

  spdy::SpdySerializedFrame reply(
      spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  reads.push_back(CreateMockRead(reply, i++));
  reads.push_back(CreateMockRead(body2, i++));
  reads.push_back(CreateMockRead(body5, i++));
  reads.emplace_back(ASYNC, 0, i++);  // EOF

  // Force all writes to happen before any read, last write will not
  // actually queue a frame, due to window size being 0.
  SequencedSocketData data(reads, writes);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  std::string upload_data_string(kBufferSize * num_upload_buffers, 'a');
  upload_data_string.append(kUploadData, kUploadDataSize);
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::as_byte_span(upload_data_string)));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  request_.method = "POST";
  request_.upload_data_stream = &upload_data_stream;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  data.RunUntilPaused();  // Write as much as we can.
  base::RunLoop().RunUntilIdle();

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->stream());
  EXPECT_EQ(0, stream->stream()->send_window_size());

  if (initial_window_size % kBufferSize != 0) {
    // If it does not take whole number of full upload buffer to zero out
    // initial window size, then the upload data is not at EOF, because the
    // last read must be stalled.
    EXPECT_FALSE(upload_data_stream.IsEOF());
  } else {
    // All the body data should have been read.
    // TODO(satorux): This is because of the weirdness in reading the request
    // body in OnSendBodyComplete(). See crbug.com/113107.
    EXPECT_TRUE(upload_data_stream.IsEOF());
  }

  // Read in WINDOW_UPDATE or SETTINGS frame.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  rv = callback.WaitForResult();
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, ReceivingPushIsConnectionError) {
  quiche::HttpHeaderBlock push_headers;
  spdy_util_.AddUrlToHeaderBlock("http://www.example.org/a.dat", &push_headers);
  spdy::SpdySerializedFrame push(
      spdy_util_.ConstructSpdyPushPromise(1, 2, std::move(push_headers)));
  MockRead reads[] = {CreateMockRead(push, 1)};

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR, "PUSH_PROMISE received"));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(goaway, 2)};

  SequencedSocketData data(reads, writes);

  auto session_deps = std::make_unique<SpdySessionDependencies>();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Push streams must have even stream IDs. Test that an incoming push stream
// with odd ID is reset the same way as one with even ID.
TEST_P(SpdyNetworkTransactionTest,
       ReceivingPushWithOddStreamIdIsConnectionError) {
  quiche::HttpHeaderBlock push_headers;
  spdy_util_.AddUrlToHeaderBlock("http://www.example.org/a.dat", &push_headers);
  spdy::SpdySerializedFrame push(
      spdy_util_.ConstructSpdyPushPromise(1, 3, std::move(push_headers)));
  MockRead reads[] = {CreateMockRead(push, 1)};

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR, "PUSH_PROMISE received"));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(goaway, 2)};

  SequencedSocketData data(reads, writes);

  auto session_deps = std::make_unique<SpdySessionDependencies>();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Regression test for https://crbug.com/493348: request header exceeds 16 kB
// and thus sent in multiple frames when using HTTP/2.
TEST_P(SpdyNetworkTransactionTest, LargeRequest) {
  const std::string kKey("foo");
  const std::string kValue(1 << 15, 'z');

  request_.extra_headers.SetHeader(kKey, kValue);

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  headers[kKey] = kValue;
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(headers), LOWEST, true));
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
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Regression test for https://crbug.com/535629: response header exceeds 16 kB.
TEST_P(SpdyNetworkTransactionTest, LargeResponseHeader) {
  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(headers), LOWEST, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };

  // HPACK decoder implementation limits string literal length to 16 kB.
  const char* response_headers[2];
  const std::string kKey(16 * 1024, 'a');
  response_headers[0] = kKey.data();
  const std::string kValue(16 * 1024, 'b');
  response_headers[1] = kValue.data();

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(response_headers, 1, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  SequencedSocketData data(reads, writes);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
  ASSERT_TRUE(out.response_info.headers->HasHeaderValue(kKey, kValue));
}

// End of line delimiter is forbidden according to RFC 7230 Section 3.2.
TEST_P(SpdyNetworkTransactionTest, CRLFInHeaderValue) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_PROTOCOL_ERROR));
  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(rst, 2)};

  const char* response_headers[] = {"folded", "foo\r\nbar"};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(response_headers, 1, 1));
  MockRead reads[] = {CreateMockRead(resp, 1), MockRead(ASYNC, 0, 3)};

  SequencedSocketData data(reads, writes);

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Regression test for https://crbug.com/603182.
// No response headers received before RST_STREAM: error.
TEST_P(SpdyNetworkTransactionTest, RstStreamNoError) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  MockWrite writes[] = {CreateMockWrite(req, 0, ASYNC)};

  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_NO_ERROR));
  MockRead reads[] = {CreateMockRead(rst, 1), MockRead(ASYNC, 0, 2)};

  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));
}

// Regression test for https://crbug.com/603182.
// Response headers and data, then RST_STREAM received,
// before request body is sent: success.
TEST_P(SpdyNetworkTransactionTest, RstStreamNoErrorAfterResponse) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  MockWrite writes[] = {CreateMockWrite(req, 0, ASYNC)};

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_NO_ERROR));
  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(body, 2),
                      CreateMockRead(rst, 3), MockRead(ASYNC, 0, 4)};

  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, 100Continue) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  quiche::HttpHeaderBlock informational_headers;
  informational_headers[spdy::kHttp2StatusHeader] = "100";
  spdy::SpdySerializedFrame informational_response(
      spdy_util_.ConstructSpdyReply(1, std::move(informational_headers)));
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(informational_response, 1), CreateMockRead(resp, 2),
      CreateMockRead(body, 3), MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// "A server can send a complete response prior to the client sending an entire
// request if the response does not depend on any portion of the request that
// has not been sent and received."  (RFC7540 Section 8.1)
// Regression test for https://crbug.com/606990.  Server responds before POST
// data are sent and closes connection: this must result in
// ERR_CONNECTION_CLOSED (as opposed to ERR_HTTP2_PROTOCOL_ERROR).
TEST_P(SpdyNetworkTransactionTest, ResponseBeforePostDataSent) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(body, 2),
                      MockRead(ASYNC, 0, 3)};

  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.StartDefaultTest();
  EXPECT_THAT(helper.output().rv, IsError(ERR_IO_PENDING));
  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsError(ERR_CONNECTION_CLOSED));
}

// Regression test for https://crbug.com/606990.
// Server responds before POST data are sent and resets stream with NO_ERROR.
TEST_P(SpdyNetworkTransactionTest, ResponseAndRstStreamBeforePostDataSent) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_NO_ERROR));
  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(body, 2),
                      CreateMockRead(rst, 3), MockRead(ASYNC, 0, 4)};

  SequencedSocketData data(reads, writes);
  UseChunkedPostRequest();
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);

  helper.RunToCompletion(&data);

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Unsupported frames must be ignored.  This is especially important for frame
// type 0xb, which used to be the BLOCKED frame in previous versions of SPDY,
// but is going to be used for the ORIGIN frame.
// TODO(bnc): Implement ORIGIN frame support.  https://crbug.com/697333
TEST_P(SpdyNetworkTransactionTest, IgnoreUnsupportedOriginFrame) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  const char origin_frame_on_stream_zero[] = {
      0x00, 0x00, 0x05,        // Length
      0x0b,                    // Type
      0x00,                    // Flags
      0x00, 0x00, 0x00, 0x00,  // Stream ID
      0x00, 0x03,              // Origin-Len
      'f',  'o',  'o'          // ASCII-Origin
  };

  const char origin_frame_on_stream_one[] = {
      0x00, 0x00, 0x05,        // Length
      0x0b,                    // Type
      0x00,                    // Flags
      0x00, 0x00, 0x00, 0x01,  // Stream ID
      0x00, 0x03,              // Origin-Len
      'b',  'a',  'r'          // ASCII-Origin
  };

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {MockRead(ASYNC, origin_frame_on_stream_zero,
                               std::size(origin_frame_on_stream_zero), 1),
                      CreateMockRead(resp, 2),
                      MockRead(ASYNC, origin_frame_on_stream_one,
                               std::size(origin_frame_on_stream_one), 3),
                      CreateMockRead(body, 4), MockRead(ASYNC, 0, 5)};

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

class SpdyNetworkTransactionTLSUsageCheckTest
    : public SpdyNetworkTransactionTest {
 protected:
  void RunTLSUsageCheckTest(
      std::unique_ptr<SSLSocketDataProvider> ssl_provider) {
    spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
        0, spdy::ERROR_CODE_INADEQUATE_SECURITY, ""));
    MockWrite writes[] = {CreateMockWrite(goaway)};

    StaticSocketDataProvider data(base::span<MockRead>(), writes);
    NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                       nullptr);
    helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
    TransactionHelperResult out = helper.output();
    EXPECT_THAT(out.rv, IsError(ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SpdyNetworkTransactionTLSUsageCheckTest,
                         testing::ValuesIn(GetTestParams()));

TEST_P(SpdyNetworkTransactionTLSUsageCheckTest, TLSVersionTooOld) {
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_SSL3,
                                &ssl_provider->ssl_info.connection_status);

  RunTLSUsageCheckTest(std::move(ssl_provider));
}

TEST_P(SpdyNetworkTransactionTLSUsageCheckTest, TLSCipherSuiteSucky) {
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Set to TLS_RSA_WITH_NULL_MD5
  SSLConnectionStatusSetCipherSuite(0x1,
                                    &ssl_provider->ssl_info.connection_status);

  RunTLSUsageCheckTest(std::move(ssl_provider));
}

// Regression test for https://crbug.com/737143.
// This test sets up an old TLS version just like in TLSVersionTooOld,
// and makes sure that it results in an spdy::ERROR_CODE_INADEQUATE_SECURITY
// even for a non-secure request URL.
TEST_P(SpdyNetworkTransactionTest, InsecureUrlCreatesSecureSpdySession) {
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_SSL3,
                                &ssl_provider->ssl_info.connection_status);

  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_INADEQUATE_SECURITY, ""));
  MockWrite writes[] = {CreateMockWrite(goaway)};
  StaticSocketDataProvider data(base::span<MockRead>(), writes);

  request_.url = GURL("http://www.example.org/");

  // Need secure proxy so that insecure URL can use HTTP/2.
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY));
}

TEST_P(SpdyNetworkTransactionTest, RequestHeadersCallback) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, DEFAULT_PRIORITY));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  HttpRawRequestHeaders raw_headers;

  SequencedSocketData data(reads, writes);
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.trans()->SetRequestHeadersCallback(base::BindRepeating(
      &HttpRawRequestHeaders::Assign, base::Unretained(&raw_headers)));
  helper.StartDefaultTest();
  helper.FinishDefaultTestWithoutVerification();
  EXPECT_FALSE(raw_headers.headers().empty());
  std::string value;
  EXPECT_TRUE(raw_headers.FindHeaderForTest(":path", &value));
  EXPECT_EQ("/", value);
  EXPECT_TRUE(raw_headers.FindHeaderForTest(":method", &value));
  EXPECT_EQ("GET", value);
  EXPECT_TRUE(raw_headers.request_line().empty());
}

#if BUILDFLAG(ENABLE_WEBSOCKETS)

TEST_P(SpdyNetworkTransactionTest, WebSocketOpensNewConnection) {
  base::HistogramTester histogram_tester;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();

  // First request opens up an HTTP/2 connection.
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, DEFAULT_PRIORITY));
  MockWrite writes1[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads1[] = {CreateMockRead(resp, 1), CreateMockRead(body, 2),
                       MockRead(ASYNC, ERR_IO_PENDING, 3),
                       MockRead(ASYNC, 0, 4)};

  SequencedSocketData data1(reads1, writes1);
  helper.AddData(&data1);

  // WebSocket request opens a new connection with HTTP/2 disabled.
  MockWrite writes2[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://www.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead reads2[] = {
      MockRead("HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};

  StaticSocketDataProvider data2(reads2, writes2);

  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Test that the request has HTTP/2 disabled.
  ssl_provider2->next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // Force socket to use HTTP/1.1, the default protocol without ALPN.
  ssl_provider2->next_proto = kProtoHTTP11;
  ssl_provider2->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  TestCompletionCallback callback1;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  int rv = trans1.Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(&trans1, &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  SpdySessionKey key(HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> spdy_session =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, log_);
  ASSERT_TRUE(spdy_session);
  EXPECT_FALSE(spdy_session->support_websocket());

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://www.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://www.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  // HTTP/2 connection is still open, but WebSocket request did not pool to it.
  ASSERT_TRUE(spdy_session);

  data1.Resume();
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();

  // Server did not advertise WebSocket support.
  histogram_tester.ExpectUniqueSample("Net.SpdySession.ServerSupportsWebSocket",
                                      /* support_websocket = false */ 0,
                                      /* expected_count = */ 1);
}

// Make sure that a WebSocket job doesn't pick up a newly created SpdySession
// that doesn't support WebSockets through
// HttpStreamFactory::Job::OnSpdySessionAvailable().
TEST_P(SpdyNetworkTransactionTest,
       WebSocketDoesUseNewH2SessionWithoutWebSocketSupport) {
  base::HistogramTester histogram_tester;
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));

  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3)};

  SequencedSocketData data(
      // Just as with other operations, this means to pause during connection
      // establishment.
      MockConnect(ASYNC, ERR_IO_PENDING), reads, writes);
  helper.AddData(&data);

  MockWrite writes2[] = {
      MockWrite(SYNCHRONOUS, 0,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://www.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead reads2[] = {
      MockRead(SYNCHRONOUS, 1,
               "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};
  SequencedSocketData data2(MockConnect(ASYNC, ERR_IO_PENDING), reads2,
                            writes2);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Test that the request has HTTP/2 disabled.
  ssl_provider2->next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // Force socket to use HTTP/1.1, the default protocol without ALPN.
  ssl_provider2->next_proto = kProtoHTTP11;
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create HTTP/2 connection.
  base::RunLoop().RunUntilIdle();

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://www.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://www.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Run until waiting on both connections.
  base::RunLoop().RunUntilIdle();

  // The H2 connection completes.
  data.socket()->OnConnectComplete(MockConnect(SYNCHRONOUS, OK));
  EXPECT_EQ(OK, callback1.WaitForResult());
  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  std::string response_data;
  rv = ReadTransaction(helper.trans(), &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  SpdySessionKey key(HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);

  base::WeakPtr<SpdySession> spdy_session =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, log_);
  ASSERT_TRUE(spdy_session);
  EXPECT_FALSE(spdy_session->support_websocket());

  EXPECT_FALSE(callback2.have_result());

  // Create WebSocket stream.
  data2.socket()->OnConnectComplete(MockConnect(SYNCHRONOUS, OK));

  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, WebSocketOverHTTP2) {
  base::HistogramTester histogram_tester;
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  quiche::HttpHeaderBlock websocket_request_headers;
  websocket_request_headers[spdy::kHttp2MethodHeader] = "CONNECT";
  websocket_request_headers[spdy::kHttp2AuthorityHeader] = "www.example.org";
  websocket_request_headers[spdy::kHttp2SchemeHeader] = "https";
  websocket_request_headers[spdy::kHttp2PathHeader] = "/";
  websocket_request_headers[spdy::kHttp2ProtocolHeader] = "websocket";
  websocket_request_headers["origin"] = "http://www.example.org";
  websocket_request_headers["sec-websocket-version"] = "13";
  websocket_request_headers["sec-websocket-extensions"] =
      "permessage-deflate; client_max_window_bits";
  spdy::SpdySerializedFrame websocket_request(spdy_util_.ConstructSpdyHeaders(
      3, std::move(websocket_request_headers), MEDIUM, false));

  spdy::SpdySerializedFrame priority1(
      spdy_util_.ConstructSpdyPriority(3, 0, MEDIUM, true));
  spdy::SpdySerializedFrame priority2(
      spdy_util_.ConstructSpdyPriority(1, 3, LOWEST, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(settings_ack, 2),
      CreateMockWrite(websocket_request, 4), CreateMockWrite(priority1, 5),
      CreateMockWrite(priority2, 6)};

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame websocket_response(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  MockRead reads[] = {CreateMockRead(settings_frame, 1),
                      CreateMockRead(resp1, 3), CreateMockRead(body1, 7),
                      CreateMockRead(websocket_response, 8),
                      MockRead(ASYNC, 0, 9)};

  SequencedSocketData data(reads, writes);
  helper.AddData(&data);

  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create HTTP/2 connection.
  base::RunLoop().RunUntilIdle();

  SpdySessionKey key(HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> spdy_session =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ true, log_);
  ASSERT_TRUE(spdy_session);
  EXPECT_TRUE(spdy_session->support_websocket());

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://www.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Origin", "http://www.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  // The following two headers must be removed by WebSocketHttp2HandshakeStream.
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create WebSocket stream.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(spdy_session);

  // First request has HIGHEST priority, WebSocket request has MEDIUM priority.
  // Changing the priority of the first request to LOWEST changes their order,
  // and therefore triggers sending PRIORITY frames.
  helper.trans()->SetPriority(LOWEST);

  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(helper.trans(), &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  helper.VerifyDataConsumed();

  // Server advertised WebSocket support.
  histogram_tester.ExpectUniqueSample("Net.SpdySession.ServerSupportsWebSocket",
                                      /* support_websocket = true */ 1,
                                      /* expected_count = */ 1);
}

// Make sure that a WebSocket job doesn't pick up a newly created SpdySession
// that supports WebSockets through an HTTPS proxy when an H2 server doesn't
// support websockets. See https://crbug.com/1010491.
TEST_P(SpdyNetworkTransactionTest,
       WebSocketDoesNotUseNewH2SessionWithoutWebSocketSupportOverHttpsProxy) {
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://proxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));

  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0,
                                  "CONNECT www.example.org:443 HTTP/1.1\r\n"
                                  "Host: www.example.org:443\r\n"
                                  "Proxy-Connection: keep-alive\r\n"
                                  "User-Agent: test-ua\r\n\r\n"),
                        CreateMockWrite(req, 2)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n\r\n"),
                      CreateMockRead(resp1, 3), CreateMockRead(body1, 4),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5)};

  // SSL data for the proxy.
  SSLSocketDataProvider tunnel_ssl_data(ASYNC, OK);
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      &tunnel_ssl_data);

  SequencedSocketData data(
      // Just as with other operations, this means to pause during connection
      // establishment.
      MockConnect(ASYNC, ERR_IO_PENDING), reads, writes);
  helper.AddData(&data);

  MockWrite writes2[] = {
      MockWrite(SYNCHRONOUS, 0,
                "CONNECT www.example.org:443 HTTP/1.1\r\n"
                "Host: www.example.org:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(SYNCHRONOUS, 2,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://www.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead reads2[] = {
      MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(SYNCHRONOUS, 3,
               "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};
  SequencedSocketData data2(MockConnect(ASYNC, ERR_IO_PENDING), reads2,
                            writes2);

  // SSL data for the proxy.
  SSLSocketDataProvider tunnel_ssl_data2(ASYNC, OK);
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      &tunnel_ssl_data2);

  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Test that the request has HTTP/2 disabled.
  ssl_provider2->next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // Force socket to use HTTP/1.1, the default protocol without ALPN.
  ssl_provider2->next_proto = kProtoHTTP11;
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create HTTP/2 connection.
  base::RunLoop().RunUntilIdle();

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://www.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://www.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Run until waiting on both connections.
  base::RunLoop().RunUntilIdle();

  // The H2 connection completes.
  data.socket()->OnConnectComplete(MockConnect(SYNCHRONOUS, OK));
  EXPECT_EQ(OK, callback1.WaitForResult());
  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  std::string response_data;
  rv = ReadTransaction(helper.trans(), &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  SpdySessionKey key(
      HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
      ProxyUriToProxyChain("https://proxy:70", ProxyServer::SCHEME_HTTPS),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);

  base::WeakPtr<SpdySession> spdy_session =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, log_);
  ASSERT_TRUE(spdy_session);
  EXPECT_FALSE(spdy_session->support_websocket());

  EXPECT_FALSE(callback2.have_result());

  // Create WebSocket stream.
  data2.socket()->OnConnectComplete(MockConnect(SYNCHRONOUS, OK));

  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  helper.VerifyDataConsumed();
}

// Same as above, but checks that a WebSocket connection avoids creating a new
// socket if it detects an H2 session when host resolution completes, and
// requests also use different hostnames.
TEST_P(SpdyNetworkTransactionTest,
       WebSocketOverHTTP2DetectsNewSessionWithAliasing) {
  base::HistogramTester histogram_tester;
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->host_resolver->set_ondemand_mode(true);
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  quiche::HttpHeaderBlock websocket_request_headers;
  websocket_request_headers[spdy::kHttp2MethodHeader] = "CONNECT";
  websocket_request_headers[spdy::kHttp2AuthorityHeader] = "example.test";
  websocket_request_headers[spdy::kHttp2SchemeHeader] = "https";
  websocket_request_headers[spdy::kHttp2PathHeader] = "/";
  websocket_request_headers[spdy::kHttp2ProtocolHeader] = "websocket";
  websocket_request_headers["origin"] = "http://example.test";
  websocket_request_headers["sec-websocket-version"] = "13";
  websocket_request_headers["sec-websocket-extensions"] =
      "permessage-deflate; client_max_window_bits";
  spdy::SpdySerializedFrame websocket_request(spdy_util_.ConstructSpdyHeaders(
      3, std::move(websocket_request_headers), MEDIUM, false));

  spdy::SpdySerializedFrame priority1(
      spdy_util_.ConstructSpdyPriority(3, 0, MEDIUM, true));
  spdy::SpdySerializedFrame priority2(
      spdy_util_.ConstructSpdyPriority(1, 3, LOWEST, true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(settings_ack, 2),
      CreateMockWrite(websocket_request, 4), CreateMockWrite(priority1, 5),
      CreateMockWrite(priority2, 6)};

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame websocket_response(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  MockRead reads[] = {CreateMockRead(settings_frame, 1),
                      CreateMockRead(resp1, 3), CreateMockRead(body1, 7),
                      CreateMockRead(websocket_response, 8),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 9)};

  SequencedSocketData data(reads, writes);
  helper.AddData(&data);

  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  // This fast forward makes sure that the transaction switches to the
  // HttpStreamPool when HappyEyeballsV3 is enabled.
  FastForwardBy(base::Milliseconds(1));
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://example.test/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request2.extra_headers.SetHeader("Origin", "http://example.test");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  // The following two headers must be removed by WebSocketHttp2HandshakeStream.
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Make sure both requests are blocked on host resolution.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper.session_deps()->host_resolver->has_pending_requests());
  // Complete the first DNS lookup, which should result in the first transaction
  // creating an H2 session (And completing successfully).
  helper.session_deps()->host_resolver->ResolveNow(1);
  base::RunLoop().RunUntilIdle();

  SpdySessionKey key1(HostPortPair::FromURL(request_.url),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->HasAvailableSession(
      key1, /* is_websocket = */ false));
  base::WeakPtr<SpdySession> spdy_session1 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key1, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, log_);
  ASSERT_TRUE(spdy_session1);
  EXPECT_TRUE(spdy_session1->support_websocket());

  // Second DNS lookup completes, which results in creating a WebSocket stream.
  helper.session_deps()->host_resolver->ResolveNow(2);
  ASSERT_TRUE(spdy_session1);

  SpdySessionKey key2(HostPortPair::FromURL(request2.url),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_TRUE(helper.session()->spdy_session_pool()->HasAvailableSession(
      key2, /* is_websocket = */ true));
  base::WeakPtr<SpdySession> spdy_session2 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key1, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ true, log_);
  ASSERT_TRUE(spdy_session2);
  EXPECT_EQ(spdy_session1.get(), spdy_session2.get());

  base::RunLoop().RunUntilIdle();

  // First request has HIGHEST priority, WebSocket request has MEDIUM priority.
  // Changing the priority of the first request to LOWEST changes their order,
  // and therefore triggers sending PRIORITY frames.
  helper.trans()->SetPriority(LOWEST);

  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(helper.trans(), &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  helper.VerifyDataConsumed();
}

// Same as above, but the SpdySession is closed just before use, so the
// WebSocket is sent over a new HTTP/1.x connection instead.
TEST_P(SpdyNetworkTransactionTest,
       WebSocketOverDetectsNewSessionWithAliasingButClosedBeforeUse) {
  base::HistogramTester histogram_tester;
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->host_resolver->set_ondemand_mode(true);
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(settings_ack, 2)};

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {CreateMockRead(settings_frame, 1),
                      CreateMockRead(resp1, 3), CreateMockRead(body1, 4),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5)};

  SequencedSocketData data(reads, writes);
  helper.AddData(&data);

  MockWrite writes2[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: example.test\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://example.test\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};
  MockRead reads2[] = {
      MockRead("HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};
  StaticSocketDataProvider data2(reads2, writes2);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Test that the request has HTTP/2 disabled.
  ssl_provider2->next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // Force socket to use HTTP/1.1, the default protocol without ALPN.
  ssl_provider2->next_proto = kProtoHTTP11;
  ssl_provider2->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  // This fast forward makes sure that the transaction switches to the
  // HttpStreamPool when HappyEyeballsV3 is enabled.
  FastForwardBy(base::Milliseconds(1));
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://example.test/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");
  request2.extra_headers.SetHeader("Origin", "http://example.test");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Make sure both requests are blocked on host resolution.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper.session_deps()->host_resolver->has_pending_requests());
  // Complete the first DNS lookup, which should result in the first transaction
  // creating an H2 session (And completing successfully).
  helper.session_deps()->host_resolver->ResolveNow(1);

  // Complete first request.
  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response->headers);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
  std::string response_data;
  rv = ReadTransaction(helper.trans(), &response_data);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello!", response_data);

  SpdySessionKey key1(HostPortPair::FromURL(request_.url),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> spdy_session1 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key1, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, log_);
  ASSERT_TRUE(spdy_session1);
  EXPECT_TRUE(spdy_session1->support_websocket());

  // Second DNS lookup completes, which results in creating an alias for the
  // SpdySession immediately, and a task is posted asynchronously to use the
  // alias..
  helper.session_deps()->host_resolver->ResolveNow(2);

  SpdySessionKey key2(HostPortPair::FromURL(request2.url),
                      PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                      SessionUsage::kDestination, SocketTag(),
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> spdy_session2 =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key1, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ true, log_);
  ASSERT_TRUE(spdy_session2);
  EXPECT_EQ(spdy_session1.get(), spdy_session2.get());

  // But the session is closed before it can be used.
  helper.session()->spdy_session_pool()->CloseAllSessions();

  // The second request establishes another connection (without even doing
  // another DNS lookup) instead, and uses HTTP/1.x.
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, WebSocketNegotiatesHttp2) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("wss://www.example.org/");
  request.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request.url)));
  request.extra_headers.SetHeader("Connection", "Upgrade");
  request.extra_headers.SetHeader("Upgrade", "websocket");
  request.extra_headers.SetHeader("Origin", "http://www.example.org");
  request.extra_headers.SetHeader("Sec-WebSocket-Version", "13");

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunPreTestSetup();

  StaticSocketDataProvider data;

  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Test that the request has HTTP/2 disabled.
  ssl_provider->next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // Force socket to use HTTP/2, which should never happen (TLS implementation
  // should fail TLS handshake if server chooses HTTP/2 without client
  // advertising support).
  ssl_provider->next_proto = kProtoHTTP2;
  ssl_provider->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  helper.AddDataWithSSLSocketDataProvider(&data, std::move(ssl_provider));

  HttpNetworkTransaction* trans = helper.trans();
  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;
  trans->SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback;
  int rv = trans->Start(&request, callback.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_THAT(rv, IsError(ERR_NOT_IMPLEMENTED));

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, WebSocketHttp11Required) {
  base::HistogramTester histogram_tester;
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util_.ConstructSpdySettingsAck());

  quiche::HttpHeaderBlock websocket_request_headers;
  websocket_request_headers[spdy::kHttp2MethodHeader] = "CONNECT";
  websocket_request_headers[spdy::kHttp2AuthorityHeader] = "www.example.org";
  websocket_request_headers[spdy::kHttp2SchemeHeader] = "https";
  websocket_request_headers[spdy::kHttp2PathHeader] = "/";
  websocket_request_headers[spdy::kHttp2ProtocolHeader] = "websocket";
  websocket_request_headers["origin"] = "http://www.example.org";
  websocket_request_headers["sec-websocket-version"] = "13";
  websocket_request_headers["sec-websocket-extensions"] =
      "permessage-deflate; client_max_window_bits";
  spdy::SpdySerializedFrame websocket_request(spdy_util_.ConstructSpdyHeaders(
      3, std::move(websocket_request_headers), MEDIUM, false));

  spdy::SpdySerializedFrame priority1(
      spdy_util_.ConstructSpdyPriority(3, 0, MEDIUM, true));
  spdy::SpdySerializedFrame priority2(
      spdy_util_.ConstructSpdyPriority(1, 3, LOWEST, true));

  MockWrite writes1[] = {CreateMockWrite(req, 0),
                         CreateMockWrite(settings_ack, 2),
                         CreateMockWrite(websocket_request, 4)};

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame websocket_response_http11_required(
      spdy_util_.ConstructSpdyRstStream(3, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
  MockRead reads1[] = {CreateMockRead(settings_frame, 1),
                       CreateMockRead(resp1, 3),
                       CreateMockRead(websocket_response_http11_required, 5)};

  SequencedSocketData data1(reads1, writes1);
  helper.AddData(&data1);

  MockWrite writes2[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Origin: http://www.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Upgrade: websocket\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};
  MockRead reads2[] = {
      MockRead("HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")};
  StaticSocketDataProvider data2(reads2, writes2);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Test that the request has HTTP/2 disabled.
  ssl_provider2->next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // Force socket to use HTTP/1.1, the default protocol without ALPN.
  ssl_provider2->next_proto = kProtoHTTP11;
  ssl_provider2->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  // Create HTTP/2 connection.
  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create HTTP/2 connection.
  base::RunLoop().RunUntilIdle();

  SpdySessionKey key(HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> spdy_session =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ true, log_);
  ASSERT_TRUE(spdy_session);
  EXPECT_TRUE(spdy_session->support_websocket());

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://www.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Origin", "http://www.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  // The following two headers must be removed by WebSocketHttp2HandshakeStream.
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(callback2.GetResult(rv), IsOk());

  helper.VerifyDataConsumed();

  // Server advertised WebSocket support.
  histogram_tester.ExpectUniqueSample("Net.SpdySession.ServerSupportsWebSocket",
                                      /* support_websocket = true */ 1,
                                      /* expected_count = */ 1);
}

// When using an HTTP(S) proxy, plaintext WebSockets use CONNECT tunnels. This
// should work for HTTP/2 proxies.
TEST_P(SpdyNetworkTransactionTest, PlaintextWebSocketOverHttp2Proxy) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair("www.example.org", 80)));
  const char kWebSocketRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: www.example.org\r\n"
      "Connection: Upgrade\r\n"
      "Upgrade: websocket\r\n"
      "Origin: http://www.example.org\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Extensions: permessage-deflate; "
      "client_max_window_bits\r\n\r\n";
  spdy::SpdySerializedFrame websocket_request(spdy_util_.ConstructSpdyDataFrame(
      /*stream_id=*/1, kWebSocketRequest, /*fin=*/false));
  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(websocket_request, 2)};

  spdy::SpdySerializedFrame connect_response(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  const char kWebSocketResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
  spdy::SpdySerializedFrame websocket_response(
      spdy_util_.ConstructSpdyDataFrame(/*stream_id=*/1, kWebSocketResponse,
                                        /*fin=*/false));
  MockRead reads[] = {CreateMockRead(connect_response, 1),
                      CreateMockRead(websocket_response, 3),
                      MockRead(ASYNC, 0, 4)};

  SequencedSocketData data(reads, writes);

  request_.url = GURL("ws://www.example.org/");
  request_.extra_headers.SetHeader("Connection", "Upgrade");
  request_.extra_headers.SetHeader("Upgrade", "websocket");
  request_.extra_headers.SetHeader("Origin", "http://www.example.org");
  request_.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://proxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();
  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;
  trans->SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  EXPECT_TRUE(helper.StartDefaultTest());
  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsOk());

  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, SecureWebSocketOverH2OverH2Proxy) {
  SpdyTestUtil proxy_spdy_util(/*use_priority_header=*/true);
  SpdyTestUtil origin_spdy_util(/*use_priority_header=*/true);

  // Connect request to the origin using HTTP/2.
  spdy::SpdySerializedFrame connect_request(
      proxy_spdy_util.ConstructSpdyConnect(
          nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
          HostPortPair("www.example.org", 443)));

  // Requests through the proxy are wrapped in DATA frames on the proxy's
  // stream ID 1.
  spdy::SpdySerializedFrame req(
      origin_spdy_util.ConstructSpdyGet(nullptr, 0, 1, HIGHEST));
  spdy::SpdySerializedFrame wrapped_req(
      proxy_spdy_util.ConstructSpdyDataFrame(1, req, false));
  spdy::SpdySerializedFrame settings_ack(
      origin_spdy_util.ConstructSpdySettingsAck());
  spdy::SpdySerializedFrame wrapped_settings_ack(
      proxy_spdy_util.ConstructSpdyDataFrame(1, settings_ack, false));

  // WebSocket Extended CONNECT using HTTP/2.
  quiche::HttpHeaderBlock websocket_request_headers;
  websocket_request_headers[spdy::kHttp2MethodHeader] = "CONNECT";
  websocket_request_headers[spdy::kHttp2AuthorityHeader] = "www.example.org";
  websocket_request_headers[spdy::kHttp2SchemeHeader] = "https";
  websocket_request_headers[spdy::kHttp2PathHeader] = "/";
  websocket_request_headers[spdy::kHttp2ProtocolHeader] = "websocket";
  websocket_request_headers["origin"] = "http://www.example.org";
  websocket_request_headers["sec-websocket-version"] = "13";
  websocket_request_headers["sec-websocket-extensions"] =
      "permessage-deflate; client_max_window_bits";
  spdy::SpdySerializedFrame websocket_request(
      origin_spdy_util.ConstructSpdyHeaders(
          3, std::move(websocket_request_headers), MEDIUM, false));
  spdy::SpdySerializedFrame wrapped_websocket_request(
      proxy_spdy_util.ConstructSpdyDataFrame(1, websocket_request, false));

  MockWrite writes[] = {CreateMockWrite(connect_request, 0),
                        CreateMockWrite(wrapped_req, 2),
                        CreateMockWrite(wrapped_settings_ack, 4),
                        CreateMockWrite(wrapped_websocket_request, 6)};

  spdy::SpdySerializedFrame connect_response(
      proxy_spdy_util.ConstructSpdyGetReply(nullptr, 0, 1));

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame(
      origin_spdy_util.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame wrapped_settings_frame(
      proxy_spdy_util.ConstructSpdyDataFrame(1, settings_frame, false));
  spdy::SpdySerializedFrame resp1(
      origin_spdy_util.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame wrapped_resp1(
      proxy_spdy_util.ConstructSpdyDataFrame(1, resp1, false));
  spdy::SpdySerializedFrame body1(
      origin_spdy_util.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame wrapped_body1(
      proxy_spdy_util.ConstructSpdyDataFrame(1, body1, false));
  spdy::SpdySerializedFrame websocket_response(
      origin_spdy_util.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame wrapped_websocket_response(
      proxy_spdy_util.ConstructSpdyDataFrame(1, websocket_response, false));

  MockRead reads[] = {CreateMockRead(connect_response, 1),
                      CreateMockRead(wrapped_settings_frame, 3),
                      CreateMockRead(wrapped_resp1, 5),
                      CreateMockRead(wrapped_body1, 7),
                      CreateMockRead(wrapped_websocket_response, 8),
                      MockRead(ASYNC, 0, 9)};

  SequencedSocketData data(reads, writes);

  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://proxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));

  // |request_| is used for a plain GET request to the origin because we need
  // an existing HTTP/2 connection that has exchanged SETTINGS before we can
  // use WebSockets.
  NormalSpdyTransactionHelper helper(request_, HIGHEST, log_,
                                     std::move(session_deps));

  // Add SSL data for the proxy.
  auto proxy_ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  proxy_ssl_provider->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  proxy_ssl_provider->next_protos_expected_in_ssl_config = {kProtoHTTP2,
                                                            kProtoHTTP11};
  proxy_ssl_provider->next_proto = kProtoHTTP2;
  helper.AddDataWithSSLSocketDataProvider(&data, std::move(proxy_ssl_provider));

  // Add SSL data for the tunneled connection.
  SSLSocketDataProvider origin_ssl_provider(ASYNC, OK);
  origin_ssl_provider.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  origin_ssl_provider.next_protos_expected_in_ssl_config = {kProtoHTTP2,
                                                            kProtoHTTP11};
  // This test uses WebSocket over HTTP/2.
  origin_ssl_provider.next_proto = kProtoHTTP2;
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      &origin_ssl_provider);

  helper.RunPreTestSetup();

  TestCompletionCallback callback1;
  int rv = helper.trans()->Start(&request_, callback1.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create HTTP/2 connection.
  base::RunLoop().RunUntilIdle();

  SpdySessionKey key(
      HostPortPair::FromURL(request_.url), PRIVACY_MODE_DISABLED,
      ProxyUriToProxyChain("proxy:70", ProxyServer::SCHEME_HTTPS),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> spdy_session =
      helper.session()->spdy_session_pool()->FindAvailableSession(
          key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ true, log_);
  ASSERT_TRUE(spdy_session);
  EXPECT_TRUE(spdy_session->support_websocket());

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("wss://www.example.org/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(HostPortPair::FromURL(request_.url)
                  .Equals(HostPortPair::FromURL(request2.url)));
  request2.extra_headers.SetHeader("Origin", "http://www.example.org");
  request2.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  // The following two headers must be removed by WebSocketHttp2HandshakeStream.
  request2.extra_headers.SetHeader("Connection", "Upgrade");
  request2.extra_headers.SetHeader("Upgrade", "websocket");

  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;

  HttpNetworkTransaction trans2(MEDIUM, helper.session());
  trans2.SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Create WebSocket stream.
  base::RunLoop().RunUntilIdle();

  rv = callback1.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  rv = callback2.WaitForResult();
  ASSERT_THAT(rv, IsOk());

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, SecureWebSocketOverHttp2Proxy) {
  spdy::SpdySerializedFrame connect_request(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair("www.example.org", 443)));
  const char kWebSocketRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: www.example.org\r\n"
      "Connection: Upgrade\r\n"
      "Upgrade: websocket\r\n"
      "Origin: http://www.example.org\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Extensions: permessage-deflate; "
      "client_max_window_bits\r\n\r\n";
  spdy::SpdySerializedFrame websocket_request(
      spdy_util_.ConstructSpdyDataFrame(1, kWebSocketRequest, false));
  MockWrite writes[] = {CreateMockWrite(connect_request, 0),
                        CreateMockWrite(websocket_request, 2)};

  spdy::SpdySerializedFrame connect_response(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  const char kWebSocketResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
  spdy::SpdySerializedFrame websocket_response(
      spdy_util_.ConstructSpdyDataFrame(1, kWebSocketResponse, false));
  MockRead reads[] = {CreateMockRead(connect_response, 1),
                      CreateMockRead(websocket_response, 3),
                      MockRead(ASYNC, 0, 4)};

  SequencedSocketData data(reads, writes);

  request_.url = GURL("wss://www.example.org/");
  request_.extra_headers.SetHeader("Connection", "Upgrade");
  request_.extra_headers.SetHeader("Upgrade", "websocket");
  request_.extra_headers.SetHeader("Origin", "http://www.example.org");
  request_.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://proxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();
  helper.AddData(&data);

  // Add SSL data for the tunneled connection.
  SSLSocketDataProvider ssl_provider(ASYNC, OK);
  ssl_provider.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  // A WebSocket request should not advertise HTTP/2 support.
  ssl_provider.next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // This test uses WebSocket over HTTP/1.1.
  ssl_provider.next_proto = kProtoHTTP11;
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      &ssl_provider);

  HttpNetworkTransaction* trans = helper.trans();
  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;
  trans->SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  EXPECT_TRUE(helper.StartDefaultTest());
  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsOk());
  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_EQ(70, response->remote_endpoint.port());
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 101 Switching Protocols",
            response->headers->GetStatusLine());

  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// Regression test for https://crbug.com/828865.
TEST_P(SpdyNetworkTransactionTest,
       SecureWebSocketOverHttp2ProxyNegotiatesHttp2) {
  spdy::SpdySerializedFrame connect_request(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair("www.example.org", 443)));
  MockWrite writes[] = {CreateMockWrite(connect_request, 0)};
  spdy::SpdySerializedFrame connect_response(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead reads[] = {CreateMockRead(connect_response, 1),
                      MockRead(ASYNC, 0, 2)};
  SequencedSocketData data(reads, writes);

  request_.url = GURL("wss://www.example.org/");
  request_.extra_headers.SetHeader("Connection", "Upgrade");
  request_.extra_headers.SetHeader("Upgrade", "websocket");
  request_.extra_headers.SetHeader("Origin", "http://www.example.org");
  request_.extra_headers.SetHeader("Sec-WebSocket-Version", "13");
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://proxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  helper.RunPreTestSetup();
  helper.AddData(&data);

  // Add SSL data for the tunneled connection.
  SSLSocketDataProvider ssl_provider(ASYNC, OK);
  ssl_provider.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  // A WebSocket request should not advertise HTTP/2 support.
  ssl_provider.next_protos_expected_in_ssl_config = {kProtoHTTP11};
  // The server should not negotiate HTTP/2 over the tunnelled connection,
  // but it must be handled gracefully if it does.
  ssl_provider.next_proto = kProtoHTTP2;
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      &ssl_provider);

  HttpNetworkTransaction* trans = helper.trans();
  TestWebSocketHandshakeStreamCreateHelper websocket_stream_create_helper;
  trans->SetWebSocketHandshakeStreamCreateHelper(
      &websocket_stream_create_helper);

  EXPECT_TRUE(helper.StartDefaultTest());
  helper.WaitForCallbackToComplete();
  EXPECT_THAT(helper.output().rv, IsError(ERR_NOT_IMPLEMENTED));

  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

TEST_P(SpdyNetworkTransactionTest, ZeroRTTDoesntConfirm) {
  static const base::TimeDelta kDelay = base::Milliseconds(10);
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {
      CreateMockRead(resp, 1), CreateMockRead(body, 2),
      MockRead(ASYNC, 0, 3)  // EOF
  };

  SequencedSocketData data(reads, writes);
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->connect_callback = FastForwardByCallback(kDelay);
  // Configure |ssl_provider| to fail if ConfirmHandshake is called. The request
  // should still succeed.
  ssl_provider->confirm = MockConfirm(SYNCHRONOUS, ERR_SSL_PROTOCOL_ERROR);
  ssl_provider->confirm_callback = FastForwardByCallback(kDelay);
  base::TimeTicks start_time = base::TimeTicks::Now();
  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // The handshake time should include the time it took to run Connect(), but
  // not ConfirmHandshake().
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(helper.trans()->GetLoadTimingInfo(&load_timing_info));
  EXPECT_EQ(load_timing_info.connect_timing.connect_start, start_time);
  EXPECT_EQ(load_timing_info.connect_timing.ssl_start, start_time);
  EXPECT_EQ(load_timing_info.connect_timing.ssl_end, start_time + kDelay);
  EXPECT_EQ(load_timing_info.connect_timing.connect_end, start_time + kDelay);
}

// Run multiple concurrent streams that don't require handshake confirmation.
TEST_P(SpdyNetworkTransactionTest, ZeroRTTNoConfirmMultipleStreams) {
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 3, LOWEST));
  MockWrite writes1[] = {CreateMockWrite(req1, 0), CreateMockWrite(req2, 3)};

  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads1[] = {
      CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data1(reads1, writes1);
  SequencedSocketData data2({}, {});
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider1->confirm = MockConfirm(SYNCHRONOUS, ERR_SSL_PROTOCOL_ERROR);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider2->confirm = MockConfirm(SYNCHRONOUS, ERR_SSL_PROTOCOL_ERROR);

  helper.RunPreTestSetup();
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));
  EXPECT_TRUE(helper.StartDefaultTest());

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(kDefaultUrl);
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  int rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  helper.FinishDefaultTest();
  EXPECT_THAT(callback2.GetResult(ERR_IO_PENDING), IsOk());
  helper.VerifyDataConsumed();

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Run multiple concurrent streams that require handshake confirmation.
TEST_P(SpdyNetworkTransactionTest, ZeroRTTConfirmMultipleStreams) {
  quiche::HttpHeaderBlock req_block1(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyHeaders(1, std::move(req_block1), LOWEST, true));
  quiche::HttpHeaderBlock req_block2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyHeaders(3, std::move(req_block2), LOWEST, true));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 3),
  };
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {
      CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data1(reads, writes);
  SequencedSocketData data2({}, {});
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider1->confirm = MockConfirm(ASYNC, OK);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider2->confirm = MockConfirm(ASYNC, OK);

  helper.RunPreTestSetup();
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request1;
  request1.method = "POST";
  request1.url = GURL(kDefaultUrl);
  request1.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback1;
  int rv = trans1.Start(&request1, callback1.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request2;
  request2.method = "POST";
  request2.url = GURL(kDefaultUrl);
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(callback1.GetResult(ERR_IO_PENDING), IsOk());
  EXPECT_THAT(callback2.GetResult(ERR_IO_PENDING), IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  ASSERT_TRUE(response1);
  ASSERT_TRUE(response1->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response1->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response1->headers->GetStatusLine());
  std::string response_data;
  ReadTransaction(&trans1, &response_data);
  EXPECT_EQ("hello!", response_data);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  ASSERT_TRUE(response2->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response2->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response2->headers->GetStatusLine());
  ReadTransaction(&trans2, &response_data);
  EXPECT_EQ("hello!", response_data);

  helper.VerifyDataConsumed();
}

// Run multiple concurrent streams, the first require a confirmation and the
// second not requiring confirmation.
TEST_P(SpdyNetworkTransactionTest, ZeroRTTConfirmNoConfirmStreams) {
  // This test orders the writes such that the GET (no confirmation) is written
  // before the POST (confirmation required).
  quiche::HttpHeaderBlock req_block1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyHeaders(1, std::move(req_block1), LOWEST, true));
  quiche::HttpHeaderBlock req_block2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyHeaders(3, std::move(req_block2), LOWEST, true));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 3),
  };
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {
      CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data1(reads, writes);
  SequencedSocketData data2({}, {});
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider1->confirm = MockConfirm(ASYNC, OK);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider2->confirm = MockConfirm(ASYNC, OK);

  helper.RunPreTestSetup();
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  // TODO(crbug.com/41451271): Explicitly verify the ordering of
  // ConfirmHandshake and the second stream.

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request1;
  request1.method = "POST";
  request1.url = GURL(kDefaultUrl);
  request1.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback1;
  int rv = trans1.Start(&request1, callback1.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(kDefaultUrl);
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(callback1.GetResult(ERR_IO_PENDING), IsOk());
  EXPECT_THAT(callback2.GetResult(ERR_IO_PENDING), IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  ASSERT_TRUE(response1);
  ASSERT_TRUE(response1->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response1->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response1->headers->GetStatusLine());
  std::string response_data;
  ReadTransaction(&trans1, &response_data);
  EXPECT_EQ("hello!", response_data);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  ASSERT_TRUE(response2->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response2->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response2->headers->GetStatusLine());
  ReadTransaction(&trans2, &response_data);
  EXPECT_EQ("hello!", response_data);

  helper.VerifyDataConsumed();
}

// Run multiple concurrent streams, the first not requiring confirmation and the
// second requiring confirmation.
TEST_P(SpdyNetworkTransactionTest, ZeroRTTNoConfirmConfirmStreams) {
  // This test orders the writes such that the GET (no confirmation) is written
  // before the POST (confirmation required).
  quiche::HttpHeaderBlock req_block1(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy::SpdySerializedFrame req1(
      spdy_util_.ConstructSpdyHeaders(1, std::move(req_block1), LOWEST, true));
  quiche::HttpHeaderBlock req_block2(
      spdy_util_.ConstructPostHeaderBlock(kDefaultUrl, 0));
  spdy::SpdySerializedFrame req2(
      spdy_util_.ConstructSpdyHeaders(3, std::move(req_block2), LOWEST, true));
  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 3),
  };
  spdy::SpdySerializedFrame resp1(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body1(spdy_util_.ConstructSpdyDataFrame(1, true));
  spdy::SpdySerializedFrame resp2(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 3));
  spdy::SpdySerializedFrame body2(spdy_util_.ConstructSpdyDataFrame(3, true));
  MockRead reads[] = {
      CreateMockRead(resp1, 1), CreateMockRead(body1, 2),
      CreateMockRead(resp2, 4), CreateMockRead(body2, 5),
      MockRead(ASYNC, 0, 6)  // EOF
  };

  SequencedSocketData data1(reads, writes);
  SequencedSocketData data2({}, {});
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider1->confirm = MockConfirm(ASYNC, OK);
  auto ssl_provider2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider2->confirm = MockConfirm(ASYNC, OK);

  helper.RunPreTestSetup();
  helper.AddDataWithSSLSocketDataProvider(&data1, std::move(ssl_provider1));
  helper.AddDataWithSSLSocketDataProvider(&data2, std::move(ssl_provider2));

  // TODO(crbug.com/41451271): Explicitly verify the ordering of
  // ConfirmHandshake and the second stream.

  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL(kDefaultUrl);
  request1.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback1;
  int rv = trans1.Start(&request1, callback1.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session());
  HttpRequestInfo request2;
  request2.method = "POST";
  request2.url = GURL(kDefaultUrl);
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback2;
  rv = trans2.Start(&request2, callback2.callback(), log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(callback1.GetResult(ERR_IO_PENDING), IsOk());
  EXPECT_THAT(callback2.GetResult(ERR_IO_PENDING), IsOk());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  ASSERT_TRUE(response1);
  ASSERT_TRUE(response1->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response1->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response1->headers->GetStatusLine());
  std::string response_data;
  ReadTransaction(&trans1, &response_data);
  EXPECT_EQ("hello!", response_data);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2);
  ASSERT_TRUE(response2->headers);
  EXPECT_EQ(HttpConnectionInfo::kHTTP2, response2->connection_info);
  EXPECT_EQ("HTTP/1.1 200", response2->headers->GetStatusLine());
  ReadTransaction(&trans2, &response_data);
  EXPECT_EQ("hello!", response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, ZeroRTTSyncConfirmSyncWrite) {
  static const base::TimeDelta kDelay = base::Milliseconds(10);
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0, SYNCHRONOUS),
      CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->connect_callback = FastForwardByCallback(kDelay);
  ssl_provider->confirm = MockConfirm(SYNCHRONOUS, OK);
  ssl_provider->confirm_callback = FastForwardByCallback(kDelay);
  base::TimeTicks start_time = base::TimeTicks::Now();
  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // The handshake time should include the time it took to run Connect(), but
  // not ConfirmHandshake(). If ConfirmHandshake() returns synchronously, we
  // assume the connection did not negotiate 0-RTT or the handshake was already
  // confirmed.
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(helper.trans()->GetLoadTimingInfo(&load_timing_info));
  EXPECT_EQ(load_timing_info.connect_timing.connect_start, start_time);
  EXPECT_EQ(load_timing_info.connect_timing.ssl_start, start_time);
  EXPECT_EQ(load_timing_info.connect_timing.ssl_end, start_time + kDelay);
  EXPECT_EQ(load_timing_info.connect_timing.connect_end, start_time + kDelay);
}

TEST_P(SpdyNetworkTransactionTest, ZeroRTTSyncConfirmAsyncWrite) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0, ASYNC),
      CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->confirm = MockConfirm(SYNCHRONOUS, OK);
  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, ZeroRTTAsyncConfirmSyncWrite) {
  static const base::TimeDelta kDelay = base::Milliseconds(10);
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0, SYNCHRONOUS),
      CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->connect_callback = FastForwardByCallback(kDelay);
  ssl_provider->confirm = MockConfirm(ASYNC, OK);
  ssl_provider->confirm_callback = FastForwardByCallback(kDelay);
  base::TimeTicks start_time = base::TimeTicks::Now();
  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // The handshake time should include the time it took to run Connect() and
  // ConfirmHandshake().
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(helper.trans()->GetLoadTimingInfo(&load_timing_info));
  EXPECT_EQ(load_timing_info.connect_timing.connect_start, start_time);
  EXPECT_EQ(load_timing_info.connect_timing.ssl_start, start_time);
  EXPECT_EQ(load_timing_info.connect_timing.ssl_end, start_time + 2 * kDelay);
  EXPECT_EQ(load_timing_info.connect_timing.connect_end,
            start_time + 2 * kDelay);
}

TEST_P(SpdyNetworkTransactionTest, ZeroRTTAsyncConfirmAsyncWrite) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0, ASYNC),
      CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->confirm = MockConfirm(ASYNC, OK);
  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, ZeroRTTConfirmErrorSync) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->confirm = MockConfirm(SYNCHRONOUS, ERR_SSL_PROTOCOL_ERROR);
  helper.RunPreTestSetup();
  helper.AddDataWithSSLSocketDataProvider(&data, std::move(ssl_provider));
  helper.RunDefaultTest();
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_SSL_PROTOCOL_ERROR));
}

TEST_P(SpdyNetworkTransactionTest, ZeroRTTConfirmErrorAsync) {
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),  // POST upload frame
  };

  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };

  SequencedSocketData data(reads, writes);
  UsePostRequest();
  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_early_data = true;
  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));
  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider->confirm = MockConfirm(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  helper.RunPreTestSetup();
  helper.AddDataWithSSLSocketDataProvider(&data, std::move(ssl_provider));
  helper.RunDefaultTest();
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_SSL_PROTOCOL_ERROR));
}

TEST_P(SpdyNetworkTransactionTest, GreaseSettings) {
  RecordingNetLogObserver net_log_observer;

  auto session_deps = std::make_unique<SpdySessionDependencies>();
  session_deps->enable_http2_settings_grease = true;
  NormalSpdyTransactionHelper helper(
      request_, DEFAULT_PRIORITY,
      NetLogWithSource::Make(NetLogSourceType::NONE), std::move(session_deps));

  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  SpdySessionPoolPeer pool_peer(spdy_session_pool);
  pool_peer.SetEnableSendingInitialData(true);

  // Greased setting parameter is random.  Hang writes instead of trying to
  // construct matching mock data.  Extra write and read is needed because mock
  // data cannot end on ERR_IO_PENDING.  Writes or reads will not actually be
  // resumed.
  MockWrite writes[] = {MockWrite(ASYNC, ERR_IO_PENDING, 0),
                        MockWrite(ASYNC, OK, 1)};
  MockRead reads[] = {MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, OK, 3)};
  SequencedSocketData data(reads, writes);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  int rv = helper.trans()->Start(&request_, CompletionOnceCallback{}, log_);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  helper.ResetTrans();

  EXPECT_FALSE(data.AllReadDataConsumed());
  EXPECT_FALSE(data.AllWriteDataConsumed());

  const auto entries = net_log_observer.GetEntries();

  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::HTTP2_SESSION_SEND_SETTINGS,
      NetLogEventPhase::NONE);
  ASSERT_LT(pos, entries.size());

  const base::Value::Dict& params = entries[pos].params;
  const base::Value::List* const settings = params.FindList("settings");
  ASSERT_TRUE(settings);

  ASSERT_FALSE(settings->empty());
  // Get last setting parameter.
  const base::Value& greased_setting = (*settings)[settings->size() - 1];
  ASSERT_TRUE(greased_setting.is_string());
  std::string_view greased_setting_string(greased_setting.GetString());

  const std::string kExpectedPrefix = "[id:";
  EXPECT_EQ(kExpectedPrefix,
            greased_setting_string.substr(0, kExpectedPrefix.size()));
  int setting_identifier = 0;
  base::StringToInt(greased_setting_string.substr(kExpectedPrefix.size()),
                    &setting_identifier);
  // The setting identifier must be of format 0x?a?a.
  EXPECT_EQ(0xa, setting_identifier % 16);
  EXPECT_EQ(0xa, (setting_identifier / 256) % 16);
}

// If |http2_end_stream_with_data_frame| is false, then the HEADERS frame of a
// GET request will close the stream using the END_STREAM flag.  Test that
// |greased_http2_frame| is ignored and no reserved frames are sent on a closed
// stream.
TEST_P(SpdyNetworkTransactionTest,
       DoNotGreaseFrameTypeWithGetRequestIfHeadersFrameClosesStream) {
  auto session_deps = std::make_unique<SpdySessionDependencies>();

  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps->greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});
  session_deps->http2_end_stream_with_data_frame = false;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, DEFAULT_PRIORITY));
  MockWrite writes[] = {CreateMockWrite(req, 0)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(response_body, 2),
                      MockRead(ASYNC, 0, 3)};

  SequencedSocketData data(reads, writes);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  TestCompletionCallback callback;
  int rv = helper.trans()->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

// Test that if |http2_end_stream_with_data_frame| and |greased_http2_frame| are
// both set, then the HEADERS frame does not have the END_STREAM flag set, it is
// followed by a greased frame, and then by an empty DATA frame with END_STREAM
// set.
TEST_P(SpdyNetworkTransactionTest, GreaseFrameTypeWithGetRequest) {
  auto session_deps = std::make_unique<SpdySessionDependencies>();

  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps->greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});
  session_deps->http2_end_stream_with_data_frame = true;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(headers), DEFAULT_PRIORITY,
                                      /* fin = */ false));

  uint8_t kRawFrameData[] = {
      0x00, 0x00, 0x03,        // length
      0x0b,                    // type
      0xcc,                    // flags
      0x00, 0x00, 0x00, 0x01,  // stream ID
      'f',  'o',  'o'          // payload
  };
  spdy::SpdySerializedFrame grease(spdy::test::MakeSerializedFrame(
      reinterpret_cast<char*>(kRawFrameData), std::size(kRawFrameData)));
  spdy::SpdySerializedFrame empty_body(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));

  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(grease, 1),
                        CreateMockWrite(empty_body, 2)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {CreateMockRead(resp, 3), CreateMockRead(response_body, 4),
                      MockRead(ASYNC, 0, 5)};

  SequencedSocketData data(reads, writes);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  TestCompletionCallback callback;
  int rv = helper.trans()->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

// Test sending a greased frame before DATA frame that closes the stream when
// |http2_end_stream_with_data_frame| is false.
TEST_P(SpdyNetworkTransactionTest,
       GreaseFrameTypeWithPostRequestWhenHeadersFrameClosesStream) {
  UsePostRequest();

  auto session_deps = std::make_unique<SpdySessionDependencies>();

  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps->greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});
  session_deps->http2_end_stream_with_data_frame = true;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));

  uint8_t kRawFrameData[] = {
      0x00, 0x00, 0x03,        // length
      0x0b,                    // type
      0xcc,                    // flags
      0x00, 0x00, 0x00, 0x01,  // stream ID
      'f',  'o',  'o'          // payload
  };
  spdy::SpdySerializedFrame grease(spdy::test::MakeSerializedFrame(
      reinterpret_cast<char*>(kRawFrameData), std::size(kRawFrameData)));
  spdy::SpdySerializedFrame request_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(grease, 1),
                        CreateMockWrite(request_body, 2)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {CreateMockRead(resp, 3), CreateMockRead(response_body, 4),
                      MockRead(ASYNC, 0, 5)};

  SequencedSocketData data(reads, writes);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  TestCompletionCallback callback;
  int rv = helper.trans()->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

// Test sending a greased frame before DATA frame that closes the stream.
// |http2_end_stream_with_data_frame| is true but should make no difference,
// because the stream is already closed by a DATA frame.
TEST_P(SpdyNetworkTransactionTest,
       GreaseFrameTypeWithPostRequestWhenEmptyDataFrameClosesStream) {
  UsePostRequest();

  auto session_deps = std::make_unique<SpdySessionDependencies>();

  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps->greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});
  session_deps->http2_end_stream_with_data_frame = true;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyPost(
      kDefaultUrl, 1, kUploadDataSize, LOWEST, nullptr, 0));

  uint8_t kRawFrameData[] = {
      0x00, 0x00, 0x03,        // length
      0x0b,                    // type
      0xcc,                    // flags
      0x00, 0x00, 0x00, 0x01,  // stream ID
      'f',  'o',  'o'          // payload
  };
  spdy::SpdySerializedFrame grease(spdy::test::MakeSerializedFrame(
      reinterpret_cast<char*>(kRawFrameData), std::size(kRawFrameData)));
  spdy::SpdySerializedFrame request_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockWrite writes[] = {CreateMockWrite(req, 0), CreateMockWrite(grease, 1),
                        CreateMockWrite(request_body, 2)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {CreateMockRead(resp, 3), CreateMockRead(response_body, 4),
                      MockRead(ASYNC, 0, 5)};

  SequencedSocketData data(reads, writes);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  TestCompletionCallback callback;
  int rv = helper.trans()->Start(&request_, callback.callback(), log_);
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

// According to https://httpwg.org/specs/rfc7540.html#CONNECT, "frame types
// other than DATA or stream management frames (RST_STREAM, WINDOW_UPDATE, and
// PRIORITY) MUST NOT be sent on a connected stream".
// Also test that |http2_end_stream_with_data_frame| has no effect on proxy
// streams.
TEST_P(SpdyNetworkTransactionTest, DoNotGreaseFrameTypeWithConnect) {
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS));

  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps->greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});
  session_deps->http2_end_stream_with_data_frame = true;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  // CONNECT to proxy.
  spdy::SpdySerializedFrame connect_req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair("www.example.org", 443)));
  spdy::SpdySerializedFrame connect_response(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));

  // Tunneled transaction wrapped in DATA frames.
  const char req[] =
      "GET / HTTP/1.1\r\n"
      "Host: www.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  spdy::SpdySerializedFrame tunneled_req(
      spdy_util_.ConstructSpdyDataFrame(1, req, false));

  const char resp[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 5\r\n\r\n"
      "hello";
  spdy::SpdySerializedFrame tunneled_response(
      spdy_util_.ConstructSpdyDataFrame(1, resp, false));

  MockWrite writes[] = {CreateMockWrite(connect_req, 0),
                        CreateMockWrite(tunneled_req, 2)};

  MockRead reads[] = {CreateMockRead(connect_response, 1),
                      CreateMockRead(tunneled_response, 3),
                      MockRead(ASYNC, 0, 4)};

  SequencedSocketData data0(reads, writes);

  // HTTP/2 connection to proxy.
  auto ssl_provider0 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_provider0->next_proto = kProtoHTTP2;
  helper.AddDataWithSSLSocketDataProvider(&data0, std::move(ssl_provider0));

  // HTTP/1.1 to destination.
  SSLSocketDataProvider ssl_provider1(ASYNC, OK);
  ssl_provider1.next_proto = kProtoHTTP11;
  helper.session_deps()->socket_factory->AddSSLSocketDataProvider(
      &ssl_provider1);

  helper.RunPreTestSetup();
  helper.StartDefaultTest();
  helper.FinishDefaultTestWithoutVerification();
  helper.VerifyDataConsumed();

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1, response->connection_info);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_TRUE(request_.url.SchemeIs("https"));
  EXPECT_EQ("127.0.0.1", response->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(70, response->remote_endpoint.port());
  std::string response_data;
  ASSERT_THAT(ReadTransaction(helper.trans(), &response_data), IsOk());
  EXPECT_EQ("hello", response_data);
}

// Regression test for https://crbug.com/1081955.
// Greasing frame types is enabled, the outgoing HEADERS frame is followed by a
// frame of reserved type, then an empty DATA frame to close the stream.
// Response arrives before reserved frame and DATA frame can be sent.
// SpdyHttpStream::OnDataSent() must not crash.
TEST_P(SpdyNetworkTransactionTest, OnDataSentDoesNotCrashWithGreasedFrameType) {
  auto session_deps = std::make_unique<SpdySessionDependencies>();

  const uint8_t type = 0x0b;
  const uint8_t flags = 0xcc;
  const std::string payload("foo");
  session_deps->greased_http2_frame =
      std::optional<net::SpdySessionPool::GreasedHttp2Frame>(
          {type, flags, payload});
  session_deps->http2_end_stream_with_data_frame = true;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_,
                                     std::move(session_deps));

  quiche::HttpHeaderBlock headers(
      spdy_util_.ConstructGetHeaderBlock(kDefaultUrl));
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyHeaders(1, std::move(headers), DEFAULT_PRIORITY,
                                      /* fin = */ false));

  uint8_t kRawFrameData[] = {
      0x00, 0x00, 0x03,        // length
      0x0b,                    // type
      0xcc,                    // flags
      0x00, 0x00, 0x00, 0x01,  // stream ID
      'f',  'o',  'o'          // payload
  };
  spdy::SpdySerializedFrame grease(spdy::test::MakeSerializedFrame(
      reinterpret_cast<char*>(kRawFrameData), std::size(kRawFrameData)));
  spdy::SpdySerializedFrame empty_body(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));

  MockWrite writes[] = {
      CreateMockWrite(req, 0), MockWrite(ASYNC, ERR_IO_PENDING, 2),
      CreateMockWrite(grease, 3), CreateMockWrite(empty_body, 4)};

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame response_body(
      spdy_util_.ConstructSpdyDataFrame(1, true));

  MockRead reads[] = {CreateMockRead(resp, 1), CreateMockRead(response_body, 5),
                      MockRead(ASYNC, 0, 6)};

  SequencedSocketData data(reads, writes);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  TestCompletionCallback callback;
  int rv = helper.trans()->Start(&request_, callback.callback(), log_);
  base::RunLoop().RunUntilIdle();

  // Response headers received.  Resume sending |grease| and |empty_body|.
  data.Resume();
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  base::RunLoop().RunUntilIdle();

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, NotAllowHTTP1NotBlockH2Post) {
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructChunkedSpdyPost(nullptr, 0));
  spdy::SpdySerializedFrame body(spdy_util_.ConstructSpdyDataFrame(1, true));
  MockWrite writes[] = {
      CreateMockWrite(req, 0), CreateMockWrite(body, 1),  // POST upload frame
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyPostReply(nullptr, 0));
  MockRead reads[] = {
      CreateMockRead(resp, 2), CreateMockRead(body, 3),
      MockRead(ASYNC, 0, 4)  // EOF
  };
  SequencedSocketData data(reads, writes);

  request_.method = "POST";
  UploadDataStreamNotAllowHTTP1 upload_data(kUploadData);
  request_.upload_data_stream = &upload_data;

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsOk());
  EXPECT_EQ("HTTP/1.1 200", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, AlpsFramingError) {
  base::HistogramTester histogram_tester;

  spdy::SpdySerializedFrame goaway(spdy_util_.ConstructSpdyGoAway(
      0, spdy::ERROR_CODE_PROTOCOL_ERROR, "Error parsing ALPS: 3"));
  MockWrite writes[] = {CreateMockWrite(goaway, 0)};
  SequencedSocketData data(base::span<MockRead>(), writes);

  auto ssl_provider = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  // Not a complete HTTP/2 frame.
  ssl_provider->peer_application_settings = "boo";

  NormalSpdyTransactionHelper helper(request_, DEFAULT_PRIORITY, log_, nullptr);
  helper.RunToCompletionWithSSLData(&data, std::move(ssl_provider));

  TransactionHelperResult out = helper.output();
  EXPECT_THAT(out.rv, IsError(ERR_HTTP2_PROTOCOL_ERROR));

  histogram_tester.ExpectUniqueSample(
      "Net.SpdySession.AlpsDecoderStatus",
      static_cast<int>(AlpsDecoder::Error::kNotOnFrameBoundary), 1);
  histogram_tester.ExpectTotalCount("Net.SpdySession.AlpsAcceptChEntries", 0);
  histogram_tester.ExpectTotalCount("Net.SpdySession.AlpsSettingParameterCount",
                                    0);
}

}  // namespace net
