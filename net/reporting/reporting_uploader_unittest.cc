// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_uploader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/http/http_status_code.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class ReportingUploaderTest : public TestWithTaskEnvironment {
 protected:
  ReportingUploaderTest()
      : server_(test_server::EmbeddedTestServer::TYPE_HTTPS),
        uploader_(ReportingUploader::Create(&context_)) {}

  TestURLRequestContext context_;
  test_server::EmbeddedTestServer server_;
  std::unique_ptr<ReportingUploader> uploader_;

  const url::Origin kOrigin = url::Origin::Create(GURL("https://origin/"));
};

const char kUploadBody[] = "{}";

void CheckUpload(const test_server::HttpRequest& request) {
  if (request.method_string != "POST") {
    return;
  }
  auto it = request.headers.find("Content-Type");
  EXPECT_TRUE(it != request.headers.end());
  EXPECT_EQ("application/reports+json", it->second);
  EXPECT_TRUE(request.has_content);
  EXPECT_EQ(kUploadBody, request.content);
}

std::unique_ptr<test_server::HttpResponse> AllowPreflight(
    const test_server::HttpRequest& request) {
  if (request.method_string != "OPTIONS") {
    return std::unique_ptr<test_server::HttpResponse>();
  }
  auto it = request.headers.find("Origin");
  EXPECT_TRUE(it != request.headers.end());
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->AddCustomHeader("Access-Control-Allow-Origin", it->second);
  response->AddCustomHeader("Access-Control-Allow-Methods", "POST");
  response->AddCustomHeader("Access-Control-Allow-Headers", "Content-Type");
  response->set_code(HTTP_OK);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

std::unique_ptr<test_server::HttpResponse> ReturnResponse(
    HttpStatusCode code,
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(code);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

std::unique_ptr<test_server::HttpResponse> ReturnInvalidResponse(
    const test_server::HttpRequest& request) {
  return std::make_unique<test_server::RawHttpResponse>(
      "", "Not a valid HTTP response.");
}

class TestUploadCallback {
 public:
  TestUploadCallback() : called_(false), waiting_(false) {}

  ReportingUploader::UploadCallback callback() {
    return base::BindOnce(&TestUploadCallback::OnUploadComplete,
                          base::Unretained(this));
  }

  void WaitForCall() {
    if (called_)
      return;

    base::RunLoop run_loop;

    waiting_ = true;
    closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  ReportingUploader::Outcome outcome() const { return outcome_; }

 private:
  void OnUploadComplete(ReportingUploader::Outcome outcome) {
    EXPECT_FALSE(called_);

    called_ = true;
    outcome_ = outcome;

    if (waiting_) {
      waiting_ = false;
      closure_.Run();
    }
  }

  bool called_;
  ReportingUploader::Outcome outcome_;

  bool waiting_;
  base::Closure closure_;
};

TEST_F(ReportingUploaderTest, Upload) {
  server_.RegisterRequestMonitor(base::BindRepeating(&CheckUpload));
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();
}

TEST_F(ReportingUploaderTest, Success) {
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback.outcome());
}

TEST_F(ReportingUploaderTest, NetworkError1) {
  ASSERT_TRUE(server_.Start());
  GURL url = server_.GetURL("/");
  ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, url, NetworkIsolationKey(), kUploadBody, 0,
                         callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

TEST_F(ReportingUploaderTest, NetworkError2) {
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnInvalidResponse));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

TEST_F(ReportingUploaderTest, ServerError) {
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_INTERNAL_SERVER_ERROR));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

std::unique_ptr<test_server::HttpResponse> VerifyPreflight(
    bool* preflight_received_out,
    const test_server::HttpRequest& request) {
  if (request.method_string != "OPTIONS") {
    return std::unique_ptr<test_server::HttpResponse>();
  }
  *preflight_received_out = true;
  return AllowPreflight(request);
}

TEST_F(ReportingUploaderTest, VerifyPreflight) {
  bool preflight_received = false;
  server_.RegisterRequestHandler(
      base::BindRepeating(&VerifyPreflight, &preflight_received));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(preflight_received);
  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback.outcome());
}

TEST_F(ReportingUploaderTest, SkipPreflightForSameOrigin) {
  bool preflight_received = false;
  server_.RegisterRequestHandler(
      base::BindRepeating(&VerifyPreflight, &preflight_received));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  auto server_origin = url::Origin::Create(server_.base_url());
  uploader_->StartUpload(server_origin, server_.GetURL("/"),
                         NetworkIsolationKey(), kUploadBody, 0,
                         callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(preflight_received);
  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback.outcome());
}

std::unique_ptr<test_server::HttpResponse> ReturnPreflightError(
    const test_server::HttpRequest& request) {
  if (request.method_string != "OPTIONS") {
    return std::unique_ptr<test_server::HttpResponse>();
  }
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_FORBIDDEN);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

TEST_F(ReportingUploaderTest, FailedCorsPreflight) {
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnPreflightError));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

std::unique_ptr<test_server::HttpResponse> ReturnPreflightWithoutOrigin(
    const test_server::HttpRequest& request) {
  if (request.method_string != "OPTIONS") {
    return std::unique_ptr<test_server::HttpResponse>();
  }
  auto it = request.headers.find("Origin");
  EXPECT_TRUE(it != request.headers.end());
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->AddCustomHeader("Access-Control-Allow-Methods", "POST");
  response->AddCustomHeader("Access-Control-Allow-Headers", "Content-Type");
  response->set_code(HTTP_OK);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

TEST_F(ReportingUploaderTest, CorsPreflightWithoutOrigin) {
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnPreflightWithoutOrigin));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

std::unique_ptr<test_server::HttpResponse> ReturnPreflightWithoutMethods(
    const test_server::HttpRequest& request) {
  if (request.method_string != "OPTIONS") {
    return std::unique_ptr<test_server::HttpResponse>();
  }
  auto it = request.headers.find("Origin");
  EXPECT_TRUE(it != request.headers.end());
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->AddCustomHeader("Access-Control-Allow-Origin", it->second);
  response->AddCustomHeader("Access-Control-Allow-Headers", "Content-Type");
  response->set_code(HTTP_OK);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

TEST_F(ReportingUploaderTest, CorsPreflightWithoutMethods) {
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnPreflightWithoutMethods));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

std::unique_ptr<test_server::HttpResponse> ReturnPreflightWithoutHeaders(
    const test_server::HttpRequest& request) {
  if (request.method_string != "OPTIONS") {
    return std::unique_ptr<test_server::HttpResponse>();
  }
  auto it = request.headers.find("Origin");
  EXPECT_TRUE(it != request.headers.end());
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->AddCustomHeader("Access-Control-Allow-Origin", it->second);
  response->AddCustomHeader("Access-Control-Allow-Methods", "POST");
  response->set_code(HTTP_OK);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

TEST_F(ReportingUploaderTest, CorsPreflightWithoutHeaders) {
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnPreflightWithoutHeaders));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

TEST_F(ReportingUploaderTest, RemoveEndpoint) {
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_GONE));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(ReportingUploader::Outcome::REMOVE_ENDPOINT, callback.outcome());
}

const char kRedirectPath[] = "/redirect";

std::unique_ptr<test_server::HttpResponse> ReturnRedirect(
    const std::string& location,
    const test_server::HttpRequest& request) {
  if (request.relative_url != "/")
    return std::unique_ptr<test_server::HttpResponse>();

  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_FOUND);
  response->AddCustomHeader("Location", location);
  response->set_content(
      "Thank you, Mario! But our Princess is in another castle.");
  response->set_content_type("text/plain");
  return std::move(response);
}

std::unique_ptr<test_server::HttpResponse> CheckRedirect(
    bool* redirect_followed_out,
    const test_server::HttpRequest& request) {
  if (request.relative_url != kRedirectPath)
    return std::unique_ptr<test_server::HttpResponse>();

  *redirect_followed_out = true;
  return ReturnResponse(HTTP_OK, request);
}

TEST_F(ReportingUploaderTest, FollowHttpsRedirect) {
  bool followed = false;
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, kRedirectPath));
  server_.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(followed);
  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback.outcome());
}

TEST_F(ReportingUploaderTest, DontFollowHttpRedirect) {
  bool followed = false;

  test_server::EmbeddedTestServer http_server_;
  http_server_.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  ASSERT_TRUE(http_server_.Start());

  const GURL target = http_server_.GetURL(kRedirectPath);
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, target.spec()));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(followed);
  EXPECT_EQ(ReportingUploader::Outcome::FAILURE, callback.outcome());
}

void CheckNoCookie(const test_server::HttpRequest& request) {
  auto it = request.headers.find("Cookie");
  EXPECT_TRUE(it == request.headers.end());
}

TEST_F(ReportingUploaderTest, DontSendCookies) {
  server_.RegisterRequestMonitor(base::BindRepeating(&CheckNoCookie));
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  ResultSavingCookieCallback<CanonicalCookie::CookieInclusionStatus>
      cookie_callback;
  GURL url = server_.GetURL("/");
  auto cookie = CanonicalCookie::Create(url, "foo=bar", base::Time::Now(),
                                        base::nullopt /* server_time */);
  context_.cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), url.scheme(), CookieOptions::MakeAllInclusive(),
      cookie_callback.MakeCallback());
  cookie_callback.WaitUntilDone();
  ASSERT_TRUE(cookie_callback.result().IsInclude());

  TestUploadCallback upload_callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, upload_callback.callback());
  upload_callback.WaitForCall();
}

std::unique_ptr<test_server::HttpResponse> SendCookie(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_OK);
  response->AddCustomHeader("Set-Cookie", "foo=bar");
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

TEST_F(ReportingUploaderTest, DontSaveCookies) {
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(base::BindRepeating(&SendCookie));
  ASSERT_TRUE(server_.Start());

  TestUploadCallback upload_callback;
  uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                         kUploadBody, 0, upload_callback.callback());
  upload_callback.WaitForCall();

  GetCookieListCallback cookie_callback;
  context_.cookie_store()->GetCookieListWithOptionsAsync(
      server_.GetURL("/"), CookieOptions::MakeAllInclusive(),
      base::BindOnce(&GetCookieListCallback::Run,
                     base::Unretained(&cookie_callback)));
  cookie_callback.WaitUntilDone();

  EXPECT_TRUE(cookie_callback.cookies().empty());
}

std::unique_ptr<test_server::HttpResponse> ReturnCacheableResponse(
    int* request_count_out,
    const test_server::HttpRequest& request) {
  ++*request_count_out;
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_OK);
  response->AddCustomHeader("Cache-Control", "max-age=86400");
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

// TODO(juliatuttle): This passes even if the uploader doesn't set
// LOAD_DISABLE_CACHE. Maybe that's okay -- Chromium might not cache POST
// responses ever -- but this test should either not exist or be sure that it is
// testing actual functionality, not a default.
TEST_F(ReportingUploaderTest, DontCacheResponse) {
  int request_count = 0;
  server_.RegisterRequestHandler(base::BindRepeating(&AllowPreflight));
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnCacheableResponse, &request_count));
  ASSERT_TRUE(server_.Start());

  {
    TestUploadCallback callback;
    uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                           kUploadBody, 0, callback.callback());
    callback.WaitForCall();
  }
  EXPECT_EQ(1, request_count);

  {
    TestUploadCallback callback;
    uploader_->StartUpload(kOrigin, server_.GetURL("/"), NetworkIsolationKey(),
                           kUploadBody, 0, callback.callback());
    callback.WaitForCall();
  }
  EXPECT_EQ(2, request_count);
}

// Create two requests with the same NetworkIsolationKey, and one request with a
// different one, and make sure only the requests with the same
// NetworkIsolationKey share a socket.
TEST_F(ReportingUploaderTest, RespectsNetworkIsolationKey) {
  // While features::kPartitionConnectionsByNetworkIsolationKey is not needed
  // for reporting code to respect NetworkIsolationKeys, this test works by
  // ensuring that Reporting's NetworkIsolationKey makes it to the socket pool
  // layer and is respected there, so this test needs to enable
  // kPartitionConnectionsByNetworkIsolationKey.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://origin2/"));
  ASSERT_NE(kOrigin, kOrigin2);
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin, kOrigin);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  MockClientSocketFactory socket_factory;
  TestURLRequestContext context(true /* delay_initialization */);
  context.set_client_socket_factory(&socket_factory);
  context.Init();

  // First socket handles first and third requests.
  MockWrite writes1[] = {
      MockWrite(SYNCHRONOUS, 0,
                "POST /1 HTTP/1.1\r\n"
                "Host: origin\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length: 2\r\n"
                "Content-Type: application/reports+json\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n"),
      MockWrite(SYNCHRONOUS, 1, kUploadBody),
      MockWrite(SYNCHRONOUS, 3,
                "POST /3 HTTP/1.1\r\n"
                "Host: origin\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length: 2\r\n"
                "Content-Type: application/reports+json\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n"),
      MockWrite(SYNCHRONOUS, 4, kUploadBody),
  };
  MockRead reads1[] = {
      MockRead(SYNCHRONOUS, 2,
               "HTTP/1.1 200 OK\r\n"
               "Connection: Keep-Alive\r\n"
               "Content-Length: 0\r\n\r\n"),
      MockRead(SYNCHRONOUS, 5,
               "HTTP/1.1 200 OK\r\n"
               "Connection: Keep-Alive\r\n"
               "Content-Length: 0\r\n\r\n"),
  };
  SequencedSocketData data1(reads1, writes1);
  socket_factory.AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl_data1(ASYNC, OK);
  socket_factory.AddSSLSocketDataProvider(&ssl_data1);

  // Second socket handles second request.
  MockWrite writes2[] = {
      MockWrite(SYNCHRONOUS, 0,
                "POST /2 HTTP/1.1\r\n"
                "Host: origin\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length: 2\r\n"
                "Content-Type: application/reports+json\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n"),
      MockWrite(SYNCHRONOUS, 1, kUploadBody),
  };
  MockRead reads2[] = {
      MockRead(SYNCHRONOUS, 2,
               "HTTP/1.1 200 OK\r\n"
               "Connection: Keep-Alive\r\n"
               "Content-Length: 0\r\n\r\n"),
  };
  SequencedSocketData data2(reads2, writes2);
  socket_factory.AddSocketDataProvider(&data2);
  SSLSocketDataProvider ssl_data2(ASYNC, OK);
  socket_factory.AddSSLSocketDataProvider(&ssl_data2);

  TestUploadCallback callback1;
  std::unique_ptr<ReportingUploader> uploader1 =
      ReportingUploader::Create(&context);
  uploader1->StartUpload(kOrigin, GURL("https://origin/1"),
                         kNetworkIsolationKey1, kUploadBody, 0,
                         callback1.callback());
  callback1.WaitForCall();
  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback1.outcome());

  // Start two more requests in parallel. The first started uses a different
  // NetworkIsolationKey, so should create a new socket, while the second one
  // gets the other socket. Start in parallel to make sure that a new socket
  // isn't created just because the first is returned to the socket pool
  // asynchronously.
  TestUploadCallback callback2;
  std::unique_ptr<ReportingUploader> uploader2 =
      ReportingUploader::Create(&context);
  uploader2->StartUpload(kOrigin, GURL("https://origin/2"),
                         kNetworkIsolationKey2, kUploadBody, 0,
                         callback2.callback());
  TestUploadCallback callback3;
  std::unique_ptr<ReportingUploader> uploader3 =
      ReportingUploader::Create(&context);
  uploader3->StartUpload(kOrigin, GURL("https://origin/3"),
                         kNetworkIsolationKey1, kUploadBody, 0,
                         callback3.callback());

  callback2.WaitForCall();
  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback2.outcome());

  callback3.WaitForCall();
  EXPECT_EQ(ReportingUploader::Outcome::SUCCESS, callback3.outcome());
}

}  // namespace
}  // namespace net
