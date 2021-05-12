// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end tests for WebSocket.
//
// A python server is (re)started for each test, which is moderately
// inefficient. However, it makes these tests a good fit for scenarios which
// require special server configurations.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/base/url_util.h"
#include "net/cert/ct_policy_status.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class URLRequest;

namespace {

using test_server::BasicHttpResponse;
using test_server::HttpRequest;
using test_server::HttpResponse;

static const char kEchoServer[] = "echo-with-no-extension";

// An implementation of WebSocketEventInterface that waits for and records the
// results of the connect.
class ConnectTestingEventInterface : public WebSocketEventInterface {
 public:
  ConnectTestingEventInterface();

  void WaitForResponse();

  bool failed() const { return failed_; }

  // Only set if the handshake failed, otherwise empty.
  std::string failure_message() const;

  std::string selected_subprotocol() const;

  std::string extensions() const;

  // Implementation of WebSocketEventInterface.
  void OnCreateURLRequest(URLRequest* request) override {}

  void OnAddChannelResponse(
      std::unique_ptr<WebSocketHandshakeResponseInfo> response,
      const std::string& selected_subprotocol,
      const std::string& extensions) override;

  void OnDataFrame(bool fin,
                   WebSocketMessageType type,
                   base::span<const char> payload) override;

  bool HasPendingDataFrames() override { return false; }

  void OnSendDataFrameDone() override;

  void OnClosingHandshake() override;

  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;

  void OnFailChannel(const std::string& message,
                     int net_error,
                     base::Optional<int> response_code) override;

  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override;

  void OnSSLCertificateError(
      std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
      const GURL& url,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override;

  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& remote_endpoint,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     base::Optional<AuthCredentials>* credentials) override;

 private:
  void QuitNestedEventLoop();

  // failed_ is true if the handshake failed (ie. OnFailChannel was called).
  bool failed_;
  std::string selected_subprotocol_;
  std::string extensions_;
  std::string failure_message_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestingEventInterface);
};

ConnectTestingEventInterface::ConnectTestingEventInterface() : failed_(false) {
}

void ConnectTestingEventInterface::WaitForResponse() {
  run_loop_.Run();
}

std::string ConnectTestingEventInterface::failure_message() const {
  return failure_message_;
}

std::string ConnectTestingEventInterface::selected_subprotocol() const {
  return selected_subprotocol_;
}

std::string ConnectTestingEventInterface::extensions() const {
  return extensions_;
}

void ConnectTestingEventInterface::OnAddChannelResponse(
    std::unique_ptr<WebSocketHandshakeResponseInfo> response,
    const std::string& selected_subprotocol,
    const std::string& extensions) {
  selected_subprotocol_ = selected_subprotocol;
  extensions_ = extensions;
  QuitNestedEventLoop();
}

void ConnectTestingEventInterface::OnDataFrame(bool fin,
                                               WebSocketMessageType type,
                                               base::span<const char> payload) {
}

void ConnectTestingEventInterface::OnSendDataFrameDone() {}

void ConnectTestingEventInterface::OnClosingHandshake() {}

void ConnectTestingEventInterface::OnDropChannel(bool was_clean,
                                                 uint16_t code,
                                                 const std::string& reason) {}

void ConnectTestingEventInterface::OnFailChannel(
    const std::string& message,
    int net_error,
    base::Optional<int> response_code) {
  failed_ = true;
  failure_message_ = message;
  QuitNestedEventLoop();
}

void ConnectTestingEventInterface::OnStartOpeningHandshake(
    std::unique_ptr<WebSocketHandshakeRequestInfo> request) {}

void ConnectTestingEventInterface::OnSSLCertificateError(
    std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
    const GURL& url,
    int net_error,
    const SSLInfo& ssl_info,
    bool fatal) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SSLErrorCallbacks::CancelSSLRequest,
                                base::Owned(ssl_error_callbacks.release()),
                                ERR_SSL_PROTOCOL_ERROR, &ssl_info));
}

int ConnectTestingEventInterface::OnAuthRequired(
    const AuthChallengeInfo& auth_info,
    scoped_refptr<HttpResponseHeaders> response_headers,
    const IPEndPoint& remote_endpoint,
    base::OnceCallback<void(const AuthCredentials*)> callback,
    base::Optional<AuthCredentials>* credentials) {
  *credentials = base::nullopt;
  return OK;
}

void ConnectTestingEventInterface::QuitNestedEventLoop() {
  run_loop_.Quit();
}

// A subclass of TestNetworkDelegate that additionally implements the
// OnResolveProxy callback and records the information passed to it.
class TestProxyDelegateWithProxyInfo : public ProxyDelegate {
 public:
  TestProxyDelegateWithProxyInfo() = default;

  struct ResolvedProxyInfo {
    GURL url;
    ProxyInfo proxy_info;
  };

  const ResolvedProxyInfo& resolved_proxy_info() const {
    return resolved_proxy_info_;
  }

 protected:
  void OnResolveProxy(const GURL& url,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override {
    resolved_proxy_info_.url = url;
    resolved_proxy_info_.proxy_info = *result;
  }

  void OnFallback(const ProxyServer& bad_proxy, int net_error) override {}

  void OnBeforeTunnelRequest(const ProxyServer& proxy_server,
                             HttpRequestHeaders* extra_headers) override {}

  Error OnTunnelHeadersReceived(
      const ProxyServer& proxy_server,
      const HttpResponseHeaders& response_headers) override {
    return OK;
  }

 private:
  ResolvedProxyInfo resolved_proxy_info_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyDelegateWithProxyInfo);
};

class WebSocketEndToEndTest : public TestWithTaskEnvironment {
 protected:
  WebSocketEndToEndTest()
      : event_interface_(),
        proxy_delegate_(std::make_unique<TestProxyDelegateWithProxyInfo>()),
        context_(true),
        channel_(),
        initialised_context_(false) {}

  // Initialise the URLRequestContext. Normally done automatically by
  // ConnectAndWait(). This method is for the use of tests that need the
  // URLRequestContext initialised before calling ConnectAndWait().
  void InitialiseContext() {
    context_.Init();
    context_.proxy_resolution_service()->SetProxyDelegate(
        proxy_delegate_.get());
    initialised_context_ = true;
  }

  // Send the connect request to |socket_url| and wait for a response. Returns
  // true if the handshake succeeded.
  bool ConnectAndWait(const GURL& socket_url) {
    if (!initialised_context_) {
      InitialiseContext();
    }
    url::Origin origin = url::Origin::Create(GURL("http://localhost"));
    net::SiteForCookies site_for_cookies =
        net::SiteForCookies::FromOrigin(origin);
    IsolationInfo isolation_info =
        IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin,
                              origin, SiteForCookies::FromOrigin(origin));
    event_interface_ = new ConnectTestingEventInterface();
    channel_ = std::make_unique<WebSocketChannel>(
        base::WrapUnique(event_interface_), &context_);
    channel_->SendAddChannelRequest(
        GURL(socket_url), sub_protocols_, origin, site_for_cookies,
        isolation_info, HttpRequestHeaders(), TRAFFIC_ANNOTATION_FOR_TESTS);
    event_interface_->WaitForResponse();
    return !event_interface_->failed();
  }

  ConnectTestingEventInterface* event_interface_;  // owned by channel_
  std::unique_ptr<TestProxyDelegateWithProxyInfo> proxy_delegate_;
  TestURLRequestContext context_;
  std::unique_ptr<WebSocketChannel> channel_;
  std::vector<std::string> sub_protocols_;
  bool initialised_context_;
};

// Basic test of connectivity. If this test fails, nothing else can be expected
// to work.
TEST_F(WebSocketEndToEndTest, BasicSmokeTest) {
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());
  EXPECT_TRUE(ConnectAndWait(ws_server.GetURL(kEchoServer)));
}

// Test for issue crbug.com/433695 "Unencrypted WebSocket connection via
// authenticated proxy times out"
// TODO(ricea): Enable this when the issue is fixed.
TEST_F(WebSocketEndToEndTest, DISABLED_HttpsProxyUnauthedFails) {
  SpawnedTestServer proxy_server(SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                                 base::FilePath());
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  ASSERT_TRUE(proxy_server.StartInBackground());
  ASSERT_TRUE(ws_server.StartInBackground());
  ASSERT_TRUE(proxy_server.BlockUntilStarted());
  ASSERT_TRUE(ws_server.BlockUntilStarted());
  std::string proxy_config =
      "https=" + proxy_server.host_port_pair().ToString();
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateFixed(
          proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(proxy_resolution_service);
  context_.set_proxy_resolution_service(proxy_resolution_service.get());
  EXPECT_FALSE(ConnectAndWait(ws_server.GetURL(kEchoServer)));
  EXPECT_EQ("Proxy authentication failed", event_interface_->failure_message());
}

// These test are not compatible with RemoteTestServer because RemoteTestServer
// doesn't support TYPE_BASIC_AUTH_PROXY.
// TODO(ricea): Make these tests work. See crbug.com/441711.
#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
#define MAYBE_HttpsWssProxyUnauthedFails DISABLED_HttpsWssProxyUnauthedFails
#define MAYBE_HttpsProxyUsed DISABLED_HttpsProxyUsed
#else
#define MAYBE_HttpsWssProxyUnauthedFails HttpsWssProxyUnauthedFails
#define MAYBE_HttpsProxyUsed HttpsProxyUsed
#endif

TEST_F(WebSocketEndToEndTest, MAYBE_HttpsWssProxyUnauthedFails) {
  SpawnedTestServer proxy_server(SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                                 base::FilePath());
  SpawnedTestServer wss_server(SpawnedTestServer::TYPE_WSS,
                               GetWebSocketTestDataDirectory());
  ASSERT_TRUE(proxy_server.StartInBackground());
  ASSERT_TRUE(wss_server.StartInBackground());
  ASSERT_TRUE(proxy_server.BlockUntilStarted());
  ASSERT_TRUE(wss_server.BlockUntilStarted());
  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(
      "https=" + proxy_server.host_port_pair().ToString());
  // TODO(https://crbug.com/901896): Don't rely on proxying localhost.
  proxy_config.proxy_rules().bypass_rules.AddRulesToSubtractImplicit();

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateFixed(ProxyConfigWithAnnotation(
          proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS)));
  ASSERT_TRUE(proxy_resolution_service);
  context_.set_proxy_resolution_service(proxy_resolution_service.get());
  EXPECT_FALSE(ConnectAndWait(wss_server.GetURL(kEchoServer)));
  EXPECT_EQ("Proxy authentication failed", event_interface_->failure_message());
}

// Regression test for crbug/426736 "WebSocket connections not using configured
// system HTTPS Proxy".
TEST_F(WebSocketEndToEndTest, MAYBE_HttpsProxyUsed) {
  SpawnedTestServer proxy_server(SpawnedTestServer::TYPE_PROXY,
                                 base::FilePath());
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  ASSERT_TRUE(proxy_server.StartInBackground());
  ASSERT_TRUE(ws_server.StartInBackground());
  ASSERT_TRUE(proxy_server.BlockUntilStarted());
  ASSERT_TRUE(ws_server.BlockUntilStarted());
  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(
      "https=" + proxy_server.host_port_pair().ToString() + ";" +
      "http=" + proxy_server.host_port_pair().ToString());
  // TODO(https://crbug.com/901896): Don't rely on proxying localhost.
  proxy_config.proxy_rules().bypass_rules.AddRulesToSubtractImplicit();

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateFixed(ProxyConfigWithAnnotation(
          proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS)));
  context_.set_proxy_resolution_service(proxy_resolution_service.get());
  InitialiseContext();

  GURL ws_url = ws_server.GetURL(kEchoServer);
  EXPECT_TRUE(ConnectAndWait(ws_url));
  const TestProxyDelegateWithProxyInfo::ResolvedProxyInfo& info =
      proxy_delegate_->resolved_proxy_info();
  EXPECT_EQ(ws_url, info.url);
  EXPECT_TRUE(info.proxy_info.is_http());
}

std::unique_ptr<HttpResponse> ProxyPacHandler(const HttpRequest& request) {
  GURL url = request.GetURL();
  EXPECT_EQ(url.path_piece(), "/proxy.pac");
  EXPECT_TRUE(url.has_query());
  std::string proxy;
  EXPECT_TRUE(GetValueForKeyInQuery(url, "proxy", &proxy));
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_content_type("application/x-ns-proxy-autoconfig");
  response->set_content(
      base::StringPrintf("function FindProxyForURL(url, host) {\n"
                         "  return 'PROXY %s';\n"
                         "}\n",
                         proxy.c_str()));
  return response;
}

// This tests the proxy.pac resolver that is built into the system. This is not
// the one that Chrome normally uses. Chrome's normal implementation is defined
// as a mojo service. It is outside //net and we can't use it from here. This
// tests the alternative implementations that are selected when the
// --winhttp-proxy-resolver flag is provided to Chrome. These only exist on OS X
// and Windows.
// TODO(ricea): Remove this test if --winhttp-proxy-resolver flag is removed.
// See crbug.com/644030.

#if defined(OS_WIN) || defined(OS_APPLE)
#define MAYBE_ProxyPacUsed ProxyPacUsed
#else
#define MAYBE_ProxyPacUsed DISABLED_ProxyPacUsed
#endif

TEST_F(WebSocketEndToEndTest, MAYBE_ProxyPacUsed) {
  EmbeddedTestServer proxy_pac_server(net::EmbeddedTestServer::Type::TYPE_HTTP);
  SpawnedTestServer proxy_server(SpawnedTestServer::TYPE_PROXY,
                                 base::FilePath());
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  proxy_pac_server.RegisterRequestHandler(base::BindRepeating(ProxyPacHandler));
  proxy_server.set_redirect_connect_to_localhost(true);

  ASSERT_TRUE(proxy_pac_server.Start());
  ASSERT_TRUE(proxy_server.StartInBackground());
  ASSERT_TRUE(ws_server.StartInBackground());
  ASSERT_TRUE(proxy_server.BlockUntilStarted());
  ASSERT_TRUE(ws_server.BlockUntilStarted());

  ProxyConfig proxy_config =
      ProxyConfig::CreateFromCustomPacURL(proxy_pac_server.GetURL(base::StrCat(
          {"/proxy.pac?proxy=", proxy_server.host_port_pair().ToString()})));
  proxy_config.set_pac_mandatory(true);
  auto proxy_config_service = std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation(proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service), NetLog::Get(),
          /*quick_check_enabled=*/true));
  ASSERT_EQ(ws_server.host_port_pair().host(), "127.0.0.1");
  context_.set_proxy_resolution_service(proxy_resolution_service.get());
  InitialiseContext();

  // Use a name other than localhost, since localhost implicitly bypasses the
  // use of proxy.pac.
  HostPortPair fake_ws_host_port_pair("stealth-localhost",
                                      ws_server.host_port_pair().port());

  GURL ws_url(base::StrCat(
      {"ws://", fake_ws_host_port_pair.ToString(), "/", kEchoServer}));
  EXPECT_TRUE(ConnectAndWait(ws_url));
  const auto& info = proxy_delegate_->resolved_proxy_info();
  EXPECT_EQ(ws_url, info.url);
  EXPECT_TRUE(info.proxy_info.is_http());
  EXPECT_EQ(info.proxy_info.ToPacString(),
            base::StrCat({"PROXY ", proxy_server.host_port_pair().ToString()}));
}

// This is a regression test for crbug.com/408061 Crash in
// net::WebSocketBasicHandshakeStream::Upgrade.
TEST_F(WebSocketEndToEndTest, TruncatedResponse) {
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());
  InitialiseContext();

  GURL ws_url = ws_server.GetURL("truncated-headers");
  EXPECT_FALSE(ConnectAndWait(ws_url));
}

// Regression test for crbug.com/180504 "WebSocket handshake fails when HTTP
// headers have trailing LWS".
TEST_F(WebSocketEndToEndTest, TrailingWhitespace) {
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  GURL ws_url = ws_server.GetURL("trailing-whitespace");
  sub_protocols_.push_back("sip");
  EXPECT_TRUE(ConnectAndWait(ws_url));
  EXPECT_EQ("sip", event_interface_->selected_subprotocol());
}

// This is a regression test for crbug.com/169448 "WebSockets should support
// header continuations"
// TODO(ricea): HTTP continuation headers have been deprecated by RFC7230.  If
// support for continuation headers is removed from Chrome, then this test will
// break and should be removed.
TEST_F(WebSocketEndToEndTest, HeaderContinuations) {
  SpawnedTestServer ws_server(SpawnedTestServer::TYPE_WS,
                              GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  GURL ws_url = ws_server.GetURL("header-continuation");

  EXPECT_TRUE(ConnectAndWait(ws_url));
  EXPECT_EQ("permessage-deflate; server_max_window_bits=10",
            event_interface_->extensions());
}

// These are not true end-to-end tests as the SpawnedTestServer doesn't
// support TLS 1.2.
// TODO(ricea): Make these be true end-to-end tests again when
// SpawnedTestServer supports TLS 1.2 or EmbeddedTestServer supports
// WebSockets.
class WebSocketHstsTest : public TestWithTaskEnvironment {
 protected:
  WebSocketHstsTest() : context_(true) {
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
  }

  void MakeHttpConnection(const GURL& url) {
    // Set up SSL details, because otherwise HSTS headers aren't processed.
    SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
    ssl_socket_data.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
    ssl_socket_data.ssl_info.is_issued_by_known_root = true;
    ssl_socket_data.ssl_info.ct_policy_compliance =
        ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    ssl_socket_data.ssl_info.cert_status = 0;
    socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

    req_ = context_.CreateRequest(url, DEFAULT_PRIORITY, &delegate_,
                                  TRAFFIC_ANNOTATION_FOR_TESTS);

    MockWrite writes[] = {
        MockWrite("GET / HTTP/1.1\r\n"
                  "Host: www.example.org\r\n"
                  "Connection: keep-alive\r\n"
                  "User-Agent: \r\n"
                  "Accept-Encoding: gzip, deflate\r\n"
                  "Accept-Language: en-us,fr\r\n\r\n")};
    MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                 "Strict-Transport-Security: max-age=123; "
                                 "includeSubdomains\r\n\r\n"),
                        MockRead(ASYNC, 0)};

    StaticSocketDataProvider data(reads, writes);
    socket_factory_.AddSocketDataProvider(&data);

    req_->Start();
    base::RunLoop().RunUntilIdle();
  }

  void MakeWebsocketConnection(const GURL& url) {
    // Set up SSL details, because otherwise HSTS headers aren't processed.
    SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
    ssl_socket_data.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
    ssl_socket_data.ssl_info.is_issued_by_known_root = true;
    ssl_socket_data.ssl_info.ct_policy_compliance =
        ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    ssl_socket_data.ssl_info.cert_status = 0;
    socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

    req_ = context_.CreateRequest(url, DEFAULT_PRIORITY, &delegate_,
                                  TRAFFIC_ANNOTATION_FOR_TESTS);

    HttpRequestHeaders headers;
    headers.SetHeader("Connection", "Upgrade");
    headers.SetHeader("Upgrade", "websocket");
    headers.SetHeader("Origin", "null");
    headers.SetHeader("Sec-WebSocket-Version", "13");
    req_->SetExtraRequestHeaders(headers);

    MockWrite writes[] = {
        MockWrite("GET / HTTP/1.1\r\n"
                  "Host: www.example.org\r\n"
                  "Connection: Upgrade\r\n"
                  "Upgrade: websocket\r\n"
                  "Origin: null\r\n"
                  "Sec-WebSocket-Version: 13\r\n"
                  "User-Agent: \r\n"
                  "Accept-Encoding: gzip, deflate\r\n"
                  "Accept-Language: en-us,fr\r\n"
                  "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                  "Sec-WebSocket-Extensions: permessage-deflate; "
                  "client_max_window_bits\r\n\r\n")};
    MockRead reads[] = {
        MockRead("HTTP/1.1 101 Switching Protocols\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
                 "Strict-Transport-Security: max-age=123; "
                 "includeSubdomains\r\n\r\n"),
        MockRead(ASYNC, 0)};

    StaticSocketDataProvider data(reads, writes);
    socket_factory_.AddSocketDataProvider(&data);

    req_->SetUserData(
        kWebSocketHandshakeUserDataKey,
        std::make_unique<TestWebSocketHandshakeStreamCreateHelper>());
    req_->SetLoadFlags(LOAD_DISABLE_CACHE);
    req_->Start();
    base::RunLoop().RunUntilIdle();
  }

  TestURLRequestContext context_;
  MockClientSocketFactory socket_factory_;
  TestDelegate delegate_;
  std::unique_ptr<URLRequest> req_;
};

// Regression test for crbug.com/455215 "HSTS not applied to WebSocket"
TEST_F(WebSocketHstsTest, HTTPSToWebSocket) {
  // Set HSTS via https:
  MakeHttpConnection(GURL("https://www.example.org"));
  EXPECT_EQ(OK, delegate_.request_status());

  ASSERT_TRUE(context_.transport_security_state()->ShouldUpgradeToSSL(
      "www.example.org"));

  // Check HSTS by starting a request over ws: and verifying that it gets
  // ugpraded to wss:.
  MakeWebsocketConnection(GURL("ws://www.example.org"));
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_TRUE(delegate_.response_completed());
  EXPECT_TRUE(req_->url().SchemeIs("wss"));
}

TEST_F(WebSocketHstsTest, WebSocketToHTTP) {
  // Set HSTS via wss:
  MakeWebsocketConnection(GURL("wss://www.example.org"));
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_TRUE(delegate_.response_completed());

  ASSERT_TRUE(context_.transport_security_state()->ShouldUpgradeToSSL(
      "www.example.org"));

  // Check HSTS by starting a request over http: and verifying that it gets
  // ugpraded to https:.
  MakeHttpConnection(GURL("http://www.example.org"));
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_TRUE(req_->url().SchemeIs("https"));
}

TEST_F(WebSocketHstsTest, WebSocketToWebSocket) {
  // Set HSTS via wss:
  MakeWebsocketConnection(GURL("wss://www.example.org"));
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_TRUE(delegate_.response_completed());

  ASSERT_TRUE(context_.transport_security_state()->ShouldUpgradeToSSL(
      "www.example.org"));

  // Check HSTS by starting a request over ws: and verifying that it gets
  // ugpraded to wss:.
  MakeWebsocketConnection(GURL("ws://www.example.org"));
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_TRUE(delegate_.response_completed());
  EXPECT_TRUE(req_->url().SchemeIs("wss"));
}

}  // namespace

}  // namespace net
