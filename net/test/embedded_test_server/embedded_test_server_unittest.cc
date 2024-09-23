// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net::test_server {

// Gets notified by the EmbeddedTestServer on incoming connections being
// accepted, read from, or closed.
class TestConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  TestConnectionListener()
      : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  TestConnectionListener(const TestConnectionListener&) = delete;
  TestConnectionListener& operator=(const TestConnectionListener&) = delete;

  ~TestConnectionListener() override = default;

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  std::unique_ptr<StreamSocket> AcceptedSocket(
      std::unique_ptr<StreamSocket> connection) override {
    base::AutoLock lock(lock_);
    ++socket_accepted_count_;
    accept_loop_.Quit();
    return connection;
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {
    base::AutoLock lock(lock_);
    did_read_from_socket_ = true;
  }

  void OnResponseCompletedSuccessfully(
      std::unique_ptr<StreamSocket> socket) override {
    base::AutoLock lock(lock_);
    did_get_socket_on_complete_ = socket && socket->IsConnected();
    complete_loop_.Quit();
  }

  void WaitUntilFirstConnectionAccepted() { accept_loop_.Run(); }

  void WaitUntilGotSocketFromResponseCompleted() { complete_loop_.Run(); }

  size_t SocketAcceptedCount() const {
    base::AutoLock lock(lock_);
    return socket_accepted_count_;
  }

  bool DidReadFromSocket() const {
    base::AutoLock lock(lock_);
    return did_read_from_socket_;
  }

  bool DidGetSocketOnComplete() const {
    base::AutoLock lock(lock_);
    return did_get_socket_on_complete_;
  }

 private:
  size_t socket_accepted_count_ = 0;
  bool did_read_from_socket_ = false;
  bool did_get_socket_on_complete_ = false;

  base::RunLoop accept_loop_;
  base::RunLoop complete_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mutable base::Lock lock_;
};

struct EmbeddedTestServerConfig {
  EmbeddedTestServer::Type type;
  HttpConnection::Protocol protocol;
};

std::vector<EmbeddedTestServerConfig> EmbeddedTestServerConfigs() {
  return {
      {EmbeddedTestServer::TYPE_HTTP, HttpConnection::Protocol::kHttp1},
      {EmbeddedTestServer::TYPE_HTTPS, HttpConnection::Protocol::kHttp1},
      {EmbeddedTestServer::TYPE_HTTPS, HttpConnection::Protocol::kHttp2},
  };
}

class EmbeddedTestServerTest
    : public testing::TestWithParam<EmbeddedTestServerConfig>,
      public WithTaskEnvironment {
 public:
  EmbeddedTestServerTest()
      : context_(CreateTestURLRequestContextBuilder()->Build()) {}

  void SetUp() override {
    server_ = std::make_unique<EmbeddedTestServer>(GetParam().type,
                                                   GetParam().protocol);
    server_->AddDefaultHandlers();
    server_->SetConnectionListener(&connection_listener_);
  }

  void TearDown() override {
    if (server_->Started())
      ASSERT_TRUE(server_->ShutdownAndWaitUntilComplete());
  }

  // Handles |request| sent to |path| and returns the response per |content|,
  // |content type|, and |code|. Saves the request URL for verification.
  std::unique_ptr<HttpResponse> HandleRequest(const std::string& path,
                                              const std::string& content,
                                              const std::string& content_type,
                                              HttpStatusCode code,
                                              const HttpRequest& request) {
    request_relative_url_ = request.relative_url;
    request_absolute_url_ = request.GetURL();

    if (request_absolute_url_.path() == path) {
      auto http_response = std::make_unique<BasicHttpResponse>();
      http_response->set_code(code);
      http_response->set_content(content);
      http_response->set_content_type(content_type);
      return http_response;
    }

    return nullptr;
  }

 protected:
  std::string request_relative_url_;
  GURL request_absolute_url_;
  std::unique_ptr<URLRequestContext> context_;
  TestConnectionListener connection_listener_;
  std::unique_ptr<EmbeddedTestServer> server_;
  base::OnceClosure quit_run_loop_;
};

TEST_P(EmbeddedTestServerTest, GetBaseURL) {
  ASSERT_TRUE(server_->Start());
  if (GetParam().type == EmbeddedTestServer::TYPE_HTTPS) {
    EXPECT_EQ(base::StringPrintf("https://127.0.0.1:%u/", server_->port()),
              server_->base_url().spec());
  } else {
    EXPECT_EQ(base::StringPrintf("http://127.0.0.1:%u/", server_->port()),
              server_->base_url().spec());
  }
}

TEST_P(EmbeddedTestServerTest, GetURL) {
  ASSERT_TRUE(server_->Start());
  if (GetParam().type == EmbeddedTestServer::TYPE_HTTPS) {
    EXPECT_EQ(base::StringPrintf("https://127.0.0.1:%u/path?query=foo",
                                 server_->port()),
              server_->GetURL("/path?query=foo").spec());
  } else {
    EXPECT_EQ(base::StringPrintf("http://127.0.0.1:%u/path?query=foo",
                                 server_->port()),
              server_->GetURL("/path?query=foo").spec());
  }
}

TEST_P(EmbeddedTestServerTest, GetURLWithHostname) {
  ASSERT_TRUE(server_->Start());
  if (GetParam().type == EmbeddedTestServer::TYPE_HTTPS) {
    EXPECT_EQ(base::StringPrintf("https://foo.com:%d/path?query=foo",
                                 server_->port()),
              server_->GetURL("foo.com", "/path?query=foo").spec());
  } else {
    EXPECT_EQ(
        base::StringPrintf("http://foo.com:%d/path?query=foo", server_->port()),
        server_->GetURL("foo.com", "/path?query=foo").spec());
  }
}

TEST_P(EmbeddedTestServerTest, RegisterRequestHandler) {
  server_->RegisterRequestHandler(base::BindRepeating(
      &EmbeddedTestServerTest::HandleRequest, base::Unretained(this), "/test",
      "<b>Worked!</b>", "text/html", HTTP_OK));
  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(
      context_->CreateRequest(server_->GetURL("/test?q=foo"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, delegate.request_status());
  ASSERT_TRUE(request->response_headers());
  EXPECT_EQ(HTTP_OK, request->response_headers()->response_code());
  EXPECT_EQ("<b>Worked!</b>", delegate.data_received());
  std::string content_type;
  ASSERT_TRUE(request->response_headers()->GetNormalizedHeader("Content-Type",
                                                               &content_type));
  EXPECT_EQ("text/html", content_type);

  EXPECT_EQ("/test?q=foo", request_relative_url_);
  EXPECT_EQ(server_->GetURL("/test?q=foo"), request_absolute_url_);
}

TEST_P(EmbeddedTestServerTest, ServeFilesFromDirectory) {
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  server_->ServeFilesFromDirectory(
      src_dir.AppendASCII("net").AppendASCII("data"));
  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(
      context_->CreateRequest(server_->GetURL("/test.html"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, delegate.request_status());
  ASSERT_TRUE(request->response_headers());
  EXPECT_EQ(HTTP_OK, request->response_headers()->response_code());
  EXPECT_EQ("<p>Hello World!</p>", delegate.data_received());
  std::string content_type;
  ASSERT_TRUE(request->response_headers()->GetNormalizedHeader("Content-Type",
                                                               &content_type));
  EXPECT_EQ("text/html", content_type);
}

TEST_P(EmbeddedTestServerTest, MockHeadersWithoutCRLF) {
  // Messing with raw headers isn't compatible with HTTP/2
  if (GetParam().protocol == HttpConnection::Protocol::kHttp2)
    return;

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  server_->ServeFilesFromDirectory(
      src_dir.AppendASCII("net").AppendASCII("data").AppendASCII(
          "embedded_test_server"));
  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      server_->GetURL("/mock-headers-without-crlf.html"), DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, delegate.request_status());
  ASSERT_TRUE(request->response_headers());
  EXPECT_EQ(HTTP_OK, request->response_headers()->response_code());
  EXPECT_EQ("<p>Hello World!</p>", delegate.data_received());
  std::string content_type;
  ASSERT_TRUE(request->response_headers()->GetNormalizedHeader("Content-Type",
                                                               &content_type));
  EXPECT_EQ("text/html", content_type);
}

TEST_P(EmbeddedTestServerTest, DefaultNotFoundResponse) {
  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      server_->GetURL("/non-existent"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, delegate.request_status());
  ASSERT_TRUE(request->response_headers());
  EXPECT_EQ(HTTP_NOT_FOUND, request->response_headers()->response_code());
}

TEST_P(EmbeddedTestServerTest, ConnectionListenerAccept) {
  ASSERT_TRUE(server_->Start());

  net::AddressList address_list;
  EXPECT_TRUE(server_->GetAddressList(&address_list));

  std::unique_ptr<StreamSocket> socket =
      ClientSocketFactory::GetDefaultFactory()->CreateTransportClientSocket(
          address_list, nullptr, nullptr, NetLog::Get(), NetLogSource());
  TestCompletionCallback callback;
  ASSERT_THAT(callback.GetResult(socket->Connect(callback.callback())), IsOk());

  connection_listener_.WaitUntilFirstConnectionAccepted();

  EXPECT_EQ(1u, connection_listener_.SocketAcceptedCount());
  EXPECT_FALSE(connection_listener_.DidReadFromSocket());
  EXPECT_FALSE(connection_listener_.DidGetSocketOnComplete());
}

TEST_P(EmbeddedTestServerTest, ConnectionListenerRead) {
  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      server_->GetURL("/non-existent"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(1u, connection_listener_.SocketAcceptedCount());
  EXPECT_TRUE(connection_listener_.DidReadFromSocket());
}

TEST_P(EmbeddedTestServerTest,
       UpgradeRequestHandlerEvalContinuesOnKNotHandled) {
  if (GetParam().protocol == HttpConnection::Protocol::kHttp2) {
    GTEST_SKIP() << "This test is not supported on HTTP/2";
  }

  const std::string websocket_upgrade_path = "/websocket_upgrade_path";

  base::AtomicFlag first_handler_called, second_handler_called;
  server_->RegisterUpgradeRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request, HttpConnection* connection)
          -> EmbeddedTestServer::UpgradeResultOrHttpResponse {
        first_handler_called.Set();
        if (request.relative_url == websocket_upgrade_path) {
          return UpgradeResult::kUpgraded;
        }
        return UpgradeResult::kNotHandled;
      }));
  server_->RegisterUpgradeRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request, HttpConnection* connection)
          -> EmbeddedTestServer::UpgradeResultOrHttpResponse {
        second_handler_called.Set();
        if (request.relative_url == websocket_upgrade_path) {
          return UpgradeResult::kUpgraded;
        }
        return UpgradeResult::kNotHandled;
      }));

  auto server_handle = server_->StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL a_different_url = server_->GetURL("/a_different_path");
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(
      context_->CreateRequest(a_different_url, DEFAULT_PRIORITY, &delegate,
                              TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_TRUE(first_handler_called.IsSet());
  EXPECT_TRUE(second_handler_called.IsSet());
}

TEST_P(EmbeddedTestServerTest, UpgradeRequestHandlerTransfersSocket) {
  if (GetParam().protocol == HttpConnection::Protocol::kHttp2) {
    GTEST_SKIP() << "This test is not supported on HTTP/2";
  }

  const std::string websocket_upgrade_path = "/websocket_upgrade_path";

  base::AtomicFlag handler_called;
  server_->RegisterUpgradeRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request, HttpConnection* connection)
          -> EmbeddedTestServer::UpgradeResultOrHttpResponse {
        handler_called.Set();
        if (request.relative_url == websocket_upgrade_path) {
          auto socket = connection->TakeSocket();
          EXPECT_TRUE(socket);
          return UpgradeResult::kUpgraded;
        }
        return UpgradeResult::kNotHandled;
      }));

  auto server_handle = server_->StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL websocket_upgrade_url = server_->GetURL(websocket_upgrade_path);
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(
      context_->CreateRequest(websocket_upgrade_url, DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();
  EXPECT_TRUE(handler_called.IsSet());
}

TEST_P(EmbeddedTestServerTest, UpgradeRequestHandlerEvalStopsOnErrorResponse) {
  if (GetParam().protocol == HttpConnection::Protocol::kHttp2) {
    GTEST_SKIP() << "This test is not supported on HTTP/2";
  }

  const std::string websocket_upgrade_path = "/websocket_upgrade_path";

  base::AtomicFlag first_handler_called;
  base::AtomicFlag second_handler_called;
  server_->RegisterUpgradeRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request, HttpConnection* connection)
          -> EmbeddedTestServer::UpgradeResultOrHttpResponse {
        first_handler_called.Set();
        if (request.relative_url == websocket_upgrade_path) {
          auto error_response = std::make_unique<BasicHttpResponse>();
          error_response->set_code(HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
          error_response->set_content("Internal Server Error");
          error_response->set_content_type("text/plain");
          return base::unexpected(std::move(error_response));
        }
        return UpgradeResult::kNotHandled;
      }));

  server_->RegisterUpgradeRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request, HttpConnection* connection)
          -> EmbeddedTestServer::UpgradeResultOrHttpResponse {
        second_handler_called.Set();
        return UpgradeResult::kNotHandled;
      }));

  auto server_handle = server_->StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL websocket_upgrade_url = server_->GetURL(websocket_upgrade_path);
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(
      context_->CreateRequest(websocket_upgrade_url, DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_TRUE(first_handler_called.IsSet());
  EXPECT_EQ(net::OK, delegate.request_status());
  ASSERT_TRUE(request->response_headers());
  EXPECT_EQ(HTTP_INTERNAL_SERVER_ERROR,
            request->response_headers()->response_code());
  EXPECT_FALSE(second_handler_called.IsSet());
}

// TODO(http://crbug.com/1166868): Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ConnectionListenerComplete DISABLED_ConnectionListenerComplete
#else
#define MAYBE_ConnectionListenerComplete ConnectionListenerComplete
#endif
TEST_P(EmbeddedTestServerTest, MAYBE_ConnectionListenerComplete) {
  // OnResponseCompletedSuccessfully() makes the assumption that a connection is
  // "finished" before the socket is closed, and in the case of HTTP/2 this is
  // not supported
  if (GetParam().protocol == HttpConnection::Protocol::kHttp2)
    return;

  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  // Need to send a Keep-Alive response header since the EmbeddedTestServer only
  // invokes OnResponseCompletedSuccessfully() if the socket is still open, and
  // the network stack will close the socket if not reuable, resulting in
  // potentially racilly closing the socket before
  // OnResponseCompletedSuccessfully() is invoked.
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      server_->GetURL("/set-header?Connection: Keep-Alive"), DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(1u, connection_listener_.SocketAcceptedCount());
  EXPECT_TRUE(connection_listener_.DidReadFromSocket());

  connection_listener_.WaitUntilGotSocketFromResponseCompleted();
  EXPECT_TRUE(connection_listener_.DidGetSocketOnComplete());
}

TEST_P(EmbeddedTestServerTest, ConcurrentFetches) {
  server_->RegisterRequestHandler(base::BindRepeating(
      &EmbeddedTestServerTest::HandleRequest, base::Unretained(this), "/test1",
      "Raspberry chocolate", "text/html", HTTP_OK));
  server_->RegisterRequestHandler(base::BindRepeating(
      &EmbeddedTestServerTest::HandleRequest, base::Unretained(this), "/test2",
      "Vanilla chocolate", "text/html", HTTP_OK));
  server_->RegisterRequestHandler(base::BindRepeating(
      &EmbeddedTestServerTest::HandleRequest, base::Unretained(this), "/test3",
      "No chocolates", "text/plain", HTTP_NOT_FOUND));
  ASSERT_TRUE(server_->Start());

  TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1(
      context_->CreateRequest(server_->GetURL("/test1"), DEFAULT_PRIORITY,
                              &delegate1, TRAFFIC_ANNOTATION_FOR_TESTS));
  TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2(
      context_->CreateRequest(server_->GetURL("/test2"), DEFAULT_PRIORITY,
                              &delegate2, TRAFFIC_ANNOTATION_FOR_TESTS));
  TestDelegate delegate3;
  std::unique_ptr<URLRequest> request3(
      context_->CreateRequest(server_->GetURL("/test3"), DEFAULT_PRIORITY,
                              &delegate3, TRAFFIC_ANNOTATION_FOR_TESTS));

  // Fetch the three URLs concurrently. Have to manually create RunLoops when
  // running multiple requests simultaneously, to avoid the deprecated
  // RunUntilIdle() path.
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  delegate1.set_on_complete(run_loop1.QuitClosure());
  delegate2.set_on_complete(run_loop2.QuitClosure());
  delegate3.set_on_complete(run_loop3.QuitClosure());
  request1->Start();
  request2->Start();
  request3->Start();
  run_loop1.Run();
  run_loop2.Run();
  run_loop3.Run();

  EXPECT_EQ(net::OK, delegate2.request_status());
  ASSERT_TRUE(request1->response_headers());
  EXPECT_EQ(HTTP_OK, request1->response_headers()->response_code());
  EXPECT_EQ("Raspberry chocolate", delegate1.data_received());
  std::string content_type1;
  ASSERT_TRUE(request1->response_headers()->GetNormalizedHeader(
      "Content-Type", &content_type1));
  EXPECT_EQ("text/html", content_type1);

  EXPECT_EQ(net::OK, delegate2.request_status());
  ASSERT_TRUE(request2->response_headers());
  EXPECT_EQ(HTTP_OK, request2->response_headers()->response_code());
  EXPECT_EQ("Vanilla chocolate", delegate2.data_received());
  std::string content_type2;
  ASSERT_TRUE(request2->response_headers()->GetNormalizedHeader(
      "Content-Type", &content_type2));
  EXPECT_EQ("text/html", content_type2);

  EXPECT_EQ(net::OK, delegate3.request_status());
  ASSERT_TRUE(request3->response_headers());
  EXPECT_EQ(HTTP_NOT_FOUND, request3->response_headers()->response_code());
  EXPECT_EQ("No chocolates", delegate3.data_received());
  std::string content_type3;
  ASSERT_TRUE(request3->response_headers()->GetNormalizedHeader(
      "Content-Type", &content_type3));
  EXPECT_EQ("text/plain", content_type3);
}

namespace {

class CancelRequestDelegate : public TestDelegate {
 public:
  CancelRequestDelegate() { set_on_complete(base::DoNothing()); }

  CancelRequestDelegate(const CancelRequestDelegate&) = delete;
  CancelRequestDelegate& operator=(const CancelRequestDelegate&) = delete;

  ~CancelRequestDelegate() override = default;

  void OnResponseStarted(URLRequest* request, int net_error) override {
    TestDelegate::OnResponseStarted(request, net_error);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), base::Seconds(1));
  }

  void WaitUntilDone() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class InfiniteResponse : public BasicHttpResponse {
 public:
  InfiniteResponse() = default;

  InfiniteResponse(const InfiniteResponse&) = delete;
  InfiniteResponse& operator=(const InfiniteResponse&) = delete;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override {
    delegate->SendResponseHeaders(code(), GetHttpReasonPhrase(code()),
                                  BuildHeaders());
    SendInfinite(delegate);
  }

 private:
  void SendInfinite(base::WeakPtr<HttpResponseDelegate> delegate) {
    if (!delegate) {
      return;
    }

    delegate->SendContents(
        "echo", base::BindOnce(&InfiniteResponse::OnSendDone,
                               weak_ptr_factory_.GetWeakPtr(), delegate));
  }

  void OnSendDone(base::WeakPtr<HttpResponseDelegate> delegate) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&InfiniteResponse::SendInfinite,
                                  weak_ptr_factory_.GetWeakPtr(), delegate));
  }

  base::WeakPtrFactory<InfiniteResponse> weak_ptr_factory_{this};
};

std::unique_ptr<HttpResponse> HandleInfiniteRequest(
    const HttpRequest& request) {
  return std::make_unique<InfiniteResponse>();
}

}  // anonymous namespace

// Tests the case the connection is closed while the server is sending a
// response.  May non-deterministically end up at one of three paths
// (Discover the close event synchronously, asynchronously, or server
// shutting down before it is discovered).
TEST_P(EmbeddedTestServerTest, CloseDuringWrite) {
  CancelRequestDelegate cancel_delegate;
  cancel_delegate.set_cancel_in_response_started(true);
  server_->RegisterRequestHandler(
      base::BindRepeating(&HandlePrefixedRequest, "/infinite",
                          base::BindRepeating(&HandleInfiniteRequest)));
  ASSERT_TRUE(server_->Start());

  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(server_->GetURL("/infinite"), DEFAULT_PRIORITY,
                              &cancel_delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  cancel_delegate.WaitUntilDone();
}

const struct CertificateValuesEntry {
  const EmbeddedTestServer::ServerCertificate server_cert;
  const bool is_expired;
  const char* common_name;
  const char* issuer_common_name;
  size_t certs_count;
} kCertificateValuesEntry[] = {
    {EmbeddedTestServer::CERT_OK, false, "127.0.0.1", "Test Root CA", 1},
    {EmbeddedTestServer::CERT_OK_BY_INTERMEDIATE, false, "127.0.0.1",
     "Test Intermediate CA", 2},
    {EmbeddedTestServer::CERT_MISMATCHED_NAME, false, "127.0.0.1",
     "Test Root CA", 1},
    {EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN, false, "localhost",
     "Test Root CA", 1},
    {EmbeddedTestServer::CERT_EXPIRED, true, "127.0.0.1", "Test Root CA", 1},
};

TEST_P(EmbeddedTestServerTest, GetCertificate) {
  if (GetParam().type != EmbeddedTestServer::TYPE_HTTPS)
    return;

  for (const auto& cert_entry : kCertificateValuesEntry) {
    SCOPED_TRACE(cert_entry.server_cert);
    server_->SetSSLConfig(cert_entry.server_cert);
    scoped_refptr<X509Certificate> cert = server_->GetCertificate();
    ASSERT_TRUE(cert);
    EXPECT_EQ(cert->HasExpired(), cert_entry.is_expired);
    EXPECT_EQ(cert->subject().common_name, cert_entry.common_name);
    EXPECT_EQ(cert->issuer().common_name, cert_entry.issuer_common_name);
    EXPECT_EQ(cert->intermediate_buffers().size(), cert_entry.certs_count - 1);
  }
}

TEST_P(EmbeddedTestServerTest, AcceptCHFrame) {
  // The ACCEPT_CH frame is only supported for HTTP/2 connections
  if (GetParam().protocol == HttpConnection::Protocol::kHttp1)
    return;

  server_->SetAlpsAcceptCH("", "foo");
  server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);

  ASSERT_TRUE(server_->Start());

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request_a(context_->CreateRequest(
      server_->GetURL("/non-existent"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request_a->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(1u, delegate.transports().size());
  EXPECT_EQ("foo", delegate.transports().back().accept_ch_frame);
}

TEST_P(EmbeddedTestServerTest, AcceptCHFrameDifferentOrigins) {
  // The ACCEPT_CH frame is only supported for HTTP/2 connections
  if (GetParam().protocol == HttpConnection::Protocol::kHttp1)
    return;

  server_->SetAlpsAcceptCH("a.test", "a");
  server_->SetAlpsAcceptCH("b.test", "b");
  server_->SetAlpsAcceptCH("c.b.test", "c");
  server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

  ASSERT_TRUE(server_->Start());

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request_a(context_->CreateRequest(
        server_->GetURL("a.test", "/non-existent"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    request_a->Start();
    delegate.RunUntilComplete();

    EXPECT_EQ(1u, delegate.transports().size());
    EXPECT_EQ("a", delegate.transports().back().accept_ch_frame);
  }

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request_a(context_->CreateRequest(
        server_->GetURL("b.test", "/non-existent"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    request_a->Start();
    delegate.RunUntilComplete();

    EXPECT_EQ(1u, delegate.transports().size());
    EXPECT_EQ("b", delegate.transports().back().accept_ch_frame);
  }

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request_a(context_->CreateRequest(
        server_->GetURL("c.b.test", "/non-existent"), DEFAULT_PRIORITY,
        &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    request_a->Start();
    delegate.RunUntilComplete();

    EXPECT_EQ(1u, delegate.transports().size());
    EXPECT_EQ("c", delegate.transports().back().accept_ch_frame);
  }
}

TEST_P(EmbeddedTestServerTest, LargePost) {
  // HTTP/2's default flow-control window is 65K. Send a larger request body
  // than that to verify the server correctly updates flow control.
  std::string large_post_body(100 * 1024, 'a');
  server_->RegisterRequestMonitor(
      base::BindLambdaForTesting([=](const HttpRequest& request) {
        EXPECT_EQ(request.method, METHOD_POST);
        EXPECT_TRUE(request.has_content);
        EXPECT_EQ(large_post_body, request.content);
      }));

  server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(server_->Start());

  auto reader = std::make_unique<UploadBytesElementReader>(
      base::as_byte_span(large_post_body));
  auto stream = ElementsUploadDataStream::CreateWithReader(std::move(reader));

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(
      context_->CreateRequest(server_->GetURL("/test"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->set_method("POST");
  request->set_upload(std::move(stream));
  request->Start();
  delegate.RunUntilComplete();
}

INSTANTIATE_TEST_SUITE_P(EmbeddedTestServerTestInstantiation,
                         EmbeddedTestServerTest,
                         testing::ValuesIn(EmbeddedTestServerConfigs()));
// Below test exercises EmbeddedTestServer's ability to cope with the situation
// where there is no MessageLoop available on the thread at EmbeddedTestServer
// initialization and/or destruction.

typedef std::tuple<bool, bool, EmbeddedTestServerConfig> ThreadingTestParams;

class EmbeddedTestServerThreadingTest
    : public testing::TestWithParam<ThreadingTestParams>,
      public WithTaskEnvironment {};

class EmbeddedTestServerThreadingTestDelegate
    : public base::PlatformThread::Delegate {
 public:
  EmbeddedTestServerThreadingTestDelegate(
      bool message_loop_present_on_initialize,
      bool message_loop_present_on_shutdown,
      EmbeddedTestServerConfig config)
      : message_loop_present_on_initialize_(message_loop_present_on_initialize),
        message_loop_present_on_shutdown_(message_loop_present_on_shutdown),
        type_(config.type),
        protocol_(config.protocol) {}

  EmbeddedTestServerThreadingTestDelegate(
      const EmbeddedTestServerThreadingTestDelegate&) = delete;
  EmbeddedTestServerThreadingTestDelegate& operator=(
      const EmbeddedTestServerThreadingTestDelegate&) = delete;

  // base::PlatformThread::Delegate:
  void ThreadMain() override {
    std::unique_ptr<base::SingleThreadTaskExecutor> executor;
    if (message_loop_present_on_initialize_) {
      executor = std::make_unique<base::SingleThreadTaskExecutor>(
          base::MessagePumpType::IO);
    }

    // Create the test server instance.
    EmbeddedTestServer server(type_, protocol_);
    base::FilePath src_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
    ASSERT_TRUE(server.Start());

    // Make a request and wait for the reply.
    if (!executor) {
      executor = std::make_unique<base::SingleThreadTaskExecutor>(
          base::MessagePumpType::IO);
    }

    auto context = CreateTestURLRequestContextBuilder()->Build();
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request(
        context->CreateRequest(server.GetURL("/test?q=foo"), DEFAULT_PRIORITY,
                               &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

    request->Start();
    delegate.RunUntilComplete();
    request.reset();
    // Flush the socket pool on the same thread by destroying the context.
    context.reset();

    // Shut down.
    if (message_loop_present_on_shutdown_)
      executor.reset();

    ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
  }

 private:
  const bool message_loop_present_on_initialize_;
  const bool message_loop_present_on_shutdown_;
  const EmbeddedTestServer::Type type_;
  const HttpConnection::Protocol protocol_;
};

TEST_P(EmbeddedTestServerThreadingTest, RunTest) {
  // The actual test runs on a separate thread so it can screw with the presence
  // of a MessageLoop - the test suite already sets up a MessageLoop for the
  // main test thread.
  base::PlatformThreadHandle thread_handle;
  EmbeddedTestServerThreadingTestDelegate delegate(std::get<0>(GetParam()),
                                                   std::get<1>(GetParam()),
                                                   std::get<2>(GetParam()));
  ASSERT_TRUE(base::PlatformThread::Create(0, &delegate, &thread_handle));
  base::PlatformThread::Join(thread_handle);
}

INSTANTIATE_TEST_SUITE_P(
    EmbeddedTestServerThreadingTestInstantiation,
    EmbeddedTestServerThreadingTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::ValuesIn(EmbeddedTestServerConfigs())));

}  // namespace net::test_server
