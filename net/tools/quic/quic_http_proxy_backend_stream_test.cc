// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_http_proxy_backend_stream.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/tools/quic_backend_response.h"
#include "net/tools/quic/quic_http_proxy_backend.h"

namespace net {
namespace test {

// Test server path and response body for the default URL used by many of the
// tests.
const char kDefaultResponsePath[] = "/defaultresponse";
const char kDefaultResponseBody[] =
    "Default response given for path: /defaultresponse";
std::string kLargeResponseBody(
    "Default response given for path: /defaultresponselarge");
const char* const kHttp2StatusHeader = ":status";

// To test uploading the contents of a file
base::FilePath GetUploadFileTestPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/BullRunSpeech.txt"));
}

// /defaultresponselarge
// Returns a valid 10 MB response.
std::unique_ptr<test_server::HttpResponse> HandleDefaultResponseLarge(
    const test_server::HttpRequest& request) {
  std::unique_ptr<test_server::BasicHttpResponse> http_response(
      new test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  // return 10 MB
  for (int i = 0; i < 200000; ++i)
    kLargeResponseBody += "01234567890123456789012345678901234567890123456789";
  http_response->set_content(kLargeResponseBody);

  return std::move(http_response);
}

int ParseHeaderStatusCode(const spdy::SpdyHeaderBlock& header) {
  int status_code;
  spdy::SpdyHeaderBlock::const_iterator it = header.find(kHttp2StatusHeader);
  if (it == header.end()) {
    return -1;
  }
  const base::StringPiece status(it->second);
  if (status.size() != 3) {
    return -1;
  }
  // First character must be an integer in range [1,5].
  if (status[0] < '1' || status[0] > '5') {
    return -1;
  }
  // The remaining two characters must be integers.
  if (!isdigit(status[1]) || !isdigit(status[2])) {
    return -1;
  }
  if (base::StringToInt(status, &status_code)) {
    return status_code;
  } else {
    return -1;
  }
}

class TestQuicServerStreamDelegate
    : public quic::QuicSimpleServerBackend::RequestHandler {
 public:
  TestQuicServerStreamDelegate()
      : send_success_(false),
        did_complete_(false),
        quic_backend_stream_(nullptr) {}

  ~TestQuicServerStreamDelegate() override {}

  void CreateProxyBackendResponseStreamForTest(
      QuicHttpProxyBackend* proxy_backend) {
    quic_backend_stream_ =
        std::make_unique<QuicHttpProxyBackendStream>(proxy_backend);
    quic_backend_stream_->set_delegate(this);
    quic_backend_stream_->Initialize(connection_id(), stream_id(), peer_host());
  }

  QuicHttpProxyBackendStream* get_proxy_backend_stream() const {
    return quic_backend_stream_.get();
  }

  const net::HttpRequestHeaders& get_request_headers() const {
    return quic_backend_stream_->request_headers();
  }

  void StartHttpRequestToBackendAndWait(
      spdy::SpdyHeaderBlock* incoming_request_headers,
      const std::string& incoming_body) {
    send_success_ = quic_backend_stream_->SendRequestToBackend(
        incoming_request_headers, incoming_body);
    EXPECT_TRUE(send_success_);
    WaitForComplete();
  }

  void WaitForComplete() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    run_loop_.Run();
  }

  quic::QuicConnectionId connection_id() const override {
    return quic::test::TestConnectionId(123);
  }
  quic::QuicStreamId stream_id() const override { return 5; }
  std::string peer_host() const override { return "127.0.0.1"; }

  void OnResponseBackendComplete(
      const quic::QuicBackendResponse* response,
      std::list<quic::QuicBackendResponse::ServerPushInfo> resources) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(did_complete_);
    EXPECT_TRUE(quic_backend_stream_);
    did_complete_ = true;
    task_runner_->PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

 private:
  bool send_success_;
  bool did_complete_;
  std::unique_ptr<QuicHttpProxyBackendStream> quic_backend_stream_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::ThreadTaskRunnerHandle::Get();
  base::RunLoop run_loop_;
};

class QuicHttpProxyBackendStreamTest : public QuicTest {
 public:
  QuicHttpProxyBackendStreamTest() {}

  ~QuicHttpProxyBackendStreamTest() override {}

  // testing::Test:
  void SetUp() override {
    SetUpServer();
    ASSERT_TRUE(test_server_->Start());

    backend_url_ = base::StringPrintf("http://127.0.0.1:%d",
                                      test_server_->host_port_pair().port());
    CreateTestBackendProxy();
    CreateTestBackendProxyToTestFailure();
  }

  void CreateTestBackendProxy() {
    ASSERT_TRUE(GURL(backend_url_).is_valid());
    proxy_backend_ = std::make_unique<QuicHttpProxyBackend>();
    proxy_backend_->InitializeBackend(backend_url_);
  }

  void CreateTestBackendProxyToTestFailure() {
    // To test against a non-running backend http server
    std::string backend_fail_url =
        base::StringPrintf("http://127.0.0.1:%d", 52);
    ASSERT_TRUE(GURL(backend_fail_url).is_valid());
    proxy_backend_fail_ = std::make_unique<QuicHttpProxyBackend>();
    proxy_backend_fail_->InitializeBackend(backend_fail_url);
  }

  // Initializes |test_server_| without starting it.  Allows subclasses to use
  // their own server configuration.
  void SetUpServer() {
    test_server_.reset(new EmbeddedTestServer);
    test_server_->AddDefaultHandlers(base::FilePath());
    test_server_->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/defaultresponselarge",
        base::BindRepeating(&HandleDefaultResponseLarge)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::string backend_url_;
  std::unique_ptr<QuicHttpProxyBackend> proxy_backend_;
  std::unique_ptr<QuicHttpProxyBackend> proxy_backend_fail_;
  std::unique_ptr<EmbeddedTestServer> test_server_;
};

TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendGetDefault) {
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = kDefaultResponsePath;
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/1.1";
  request_headers[":method"] = "GET";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, "");

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();
  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  EXPECT_EQ(kDefaultResponseBody, quic_response->body());
}

TEST_F(QuicHttpProxyBackendStreamTest, DISABLED_SendRequestToBackendGetLarge) {
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/defaultresponselarge";
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/1.1";
  request_headers[":method"] = "GET";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, "");

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();
  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  // kLargeResponseBody should be populated with huge response
  // already in HandleDefaultResponseLarge()
  EXPECT_EQ(kLargeResponseBody, quic_response->body());
}

TEST_F(QuicHttpProxyBackendStreamTest, DISABLED_SendRequestToBackendPostBody) {
  const char kUploadData[] = "bobsyeruncle";
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/echo";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":version"] = "HTTP/1.1";
  request_headers[":method"] = "POST";
  request_headers["content-length"] = "12";
  request_headers["content-type"] = "application/x-www-form-urlencoded";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, kUploadData);

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  EXPECT_EQ(kUploadData, quic_response->body());
}

TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendPostEmptyString) {
  const char kUploadData[] = "";
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/echo";
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":method"] = "POST";
  request_headers["content-length"] = "0";
  request_headers["content-type"] = "application/x-www-form-urlencoded";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, kUploadData);

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  EXPECT_EQ(kUploadData, quic_response->body());
}

TEST_F(QuicHttpProxyBackendStreamTest, DISABLED_SendRequestToBackendPostFile) {
  std::string kUploadData;
  base::FilePath upload_path = GetUploadFileTestPath();
  ASSERT_TRUE(base::ReadFileToString(upload_path, &kUploadData));

  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/echo";
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":method"] = "POST";
  request_headers["content-type"] = "application/x-www-form-urlencoded";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, kUploadData);

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  EXPECT_EQ(kUploadData, quic_response->body());
}

TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendResponse500) {
  const char kUploadData[] = "bobsyeruncle";
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/echo?status=500";
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":method"] = "POST";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, kUploadData);

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(500, ParseHeaderStatusCode(quic_response->headers()));
}

TEST_F(QuicHttpProxyBackendStreamTest, DISABLED_SendRequestToBackendFail) {
  const char kUploadData[] = "bobsyeruncle";
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/echo";
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":method"] = "POST";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_fail_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, kUploadData);

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::BACKEND_ERR_RESPONSE,
            quic_response->response_type());
}

TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendOnRedirect) {
  const std::string kRedirectTarget = backend_url_.append("/echo");
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = std::string("/server-redirect?") + kRedirectTarget;
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":method"] = "GET";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, "");

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
}

// Ensure that the proxy rewrites the content-length when receiving a Gzipped
// response
TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendHandleGzip) {
  const char kGzipData[] =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!!";
  uint64_t rawBodyLength = strlen(kGzipData);
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = std::string("/gzip-body?") + kGzipData;
  request_headers[":authority"] = "www.example.org";
  request_headers[":version"] = "HTTP/2.0";
  request_headers[":method"] = "GET";

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, "");

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();

  EXPECT_EQ(quic::QuicBackendResponse::REGULAR_RESPONSE,
            quic_response->response_type());
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  EXPECT_EQ(kGzipData, quic_response->body());
  spdy::SpdyHeaderBlock quic_response_headers =
      quic_response->headers().Clone();

  // Ensure that the content length is set to the raw body size (unencoded)
  auto responseLength = quic_response_headers.find("content-length");
  uint64_t response_header_content_length = 0;
  if (responseLength != quic_response_headers.end()) {
    base::StringToUint64(responseLength->second,
                         &response_header_content_length);
  }
  EXPECT_EQ(rawBodyLength, response_header_content_length);

  // Ensure the content-encoding header is removed for the quic response
  EXPECT_EQ(quic_response_headers.end(),
            quic_response_headers.find("content-encoding"));
}

// Ensure cookies are not saved/updated at the proxy
TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendCookiesNotSaved) {
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":authority"] = "www.example.org";
  request_headers[":method"] = "GET";

  {
    TestQuicServerStreamDelegate delegate;
    request_headers[":path"] =
        "/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1";
    delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
    delegate.StartHttpRequestToBackendAndWait(&request_headers, "");

    quic::QuicBackendResponse* quic_response =
        delegate.get_proxy_backend_stream()->GetBackendResponse();

    EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
    spdy::SpdyHeaderBlock quic_response_headers =
        quic_response->headers().Clone();
    EXPECT_TRUE(quic_response_headers.end() !=
                quic_response_headers.find("set-cookie"));
    auto cookie = quic_response_headers.find("set-cookie");
    EXPECT_TRUE(cookie->second.find("CookieToNotSave=1") != std::string::npos);
    EXPECT_TRUE(cookie->second.find("CookieToNotUpdate=1") !=
                std::string::npos);
  }
  {
    TestQuicServerStreamDelegate delegate;
    request_headers[":path"] = "/echoheader?Cookie";
    delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
    delegate.StartHttpRequestToBackendAndWait(&request_headers, "");

    quic::QuicBackendResponse* quic_response =
        delegate.get_proxy_backend_stream()->GetBackendResponse();

    EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
    EXPECT_TRUE(quic_response->body().find("CookieToNotSave=1") ==
                std::string::npos);
    EXPECT_TRUE(quic_response->body().find("CookieToNotUpdate=1") ==
                std::string::npos);
  }
}

// Ensure hop-by-hop headers are removed from the request and response to the
// backend
TEST_F(QuicHttpProxyBackendStreamTest,
       DISABLED_SendRequestToBackendHopHeaders) {
  spdy::SpdyHeaderBlock request_headers;
  request_headers[":path"] = "/echoall";
  request_headers[":authority"] = "www.example.org";
  request_headers[":method"] = "GET";
  std::set<std::string>::iterator it;
  for (it = QuicHttpProxyBackendStream::kHopHeaders.begin();
       it != QuicHttpProxyBackendStream::kHopHeaders.end(); ++it) {
    request_headers[*it] = "SomeString";
  }

  TestQuicServerStreamDelegate delegate;
  delegate.CreateProxyBackendResponseStreamForTest(proxy_backend_.get());
  delegate.StartHttpRequestToBackendAndWait(&request_headers, "");
  const net::HttpRequestHeaders& actual_request_headers =
      delegate.get_request_headers();
  for (it = QuicHttpProxyBackendStream::kHopHeaders.begin();
       it != QuicHttpProxyBackendStream::kHopHeaders.end(); ++it) {
    EXPECT_FALSE(actual_request_headers.HasHeader(*it));
  }

  quic::QuicBackendResponse* quic_response =
      delegate.get_proxy_backend_stream()->GetBackendResponse();
  EXPECT_EQ(200, ParseHeaderStatusCode(quic_response->headers()));
  spdy::SpdyHeaderBlock quic_response_headers =
      quic_response->headers().Clone();
  for (it = QuicHttpProxyBackendStream::kHopHeaders.begin();
       it != QuicHttpProxyBackendStream::kHopHeaders.end(); ++it) {
    EXPECT_EQ(quic_response_headers.end(), quic_response_headers.find(*it));
  }
}

}  // namespace test
}  // namespace net
