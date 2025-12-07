// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end tests for WebSocket.
//
// A python server is (re)started for each test, which is moderately
// inefficient. However, it makes these tests a good fit for scenarios which
// require special server configurations.

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/request_priority.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_host_matching_rules.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/storage_access_api/status.h"
#include "net/test/embedded_test_server/create_websocket_handler.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "net/test/embedded_test_server/register_basic_auth_handler.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net {
class HttpResponseHeaders;
class ProxyServer;
class SSLInfo;
struct WebSocketHandshakeRequestInfo;

namespace {

using test_server::BasicHttpResponse;
using test_server::HttpRequest;
using test_server::HttpResponse;

static constexpr char kEchoServer[] = "/echo-with-no-extension";

// Simplify changing URL schemes.
GURL ReplaceUrlScheme(const GURL& in_url, std::string_view scheme) {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  return in_url.ReplaceComponents(replacements);
}

// An implementation of WebSocketEventInterface that waits for and records the
// results of the connect.
class ConnectTestingEventInterface : public WebSocketEventInterface {
 public:
  ConnectTestingEventInterface();

  ConnectTestingEventInterface(const ConnectTestingEventInterface&) = delete;
  ConnectTestingEventInterface& operator=(const ConnectTestingEventInterface&) =
      delete;

  void WaitForResponse() { on_response_future_.Get(); }

  bool failed() const { return failed_; }

  const std::unique_ptr<WebSocketHandshakeResponseInfo>& response() const {
    return response_;
  }

  // Only set if the handshake failed, otherwise empty.
  std::string failure_message() const;

  std::string selected_subprotocol() const;

  std::string extensions() const;

  // Implementation of WebSocketEventInterface.
  void OnCreateURLRequest(URLRequest* request) override {}

  int OnURLRequestConnected(net::URLRequest* request,
                            const net::TransportInfo& info,
                            net::CompletionOnceCallback callback) override;

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
                     std::optional<int> response_code) override;

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
                     std::optional<AuthCredentials>* credentials) override;

  std::string GetDataFramePayload();

  void WaitForDropChannel() { drop_channel_future_.Get(); }

 private:
  void SetReceivedMessageFuture(std::string received_message);

  // failed_ is true if the handshake failed (ie. OnFailChannel was called).
  bool failed_ = false;
  std::unique_ptr<WebSocketHandshakeResponseInfo> response_;
  std::string selected_subprotocol_;
  std::string extensions_;
  std::string failure_message_;
  std::optional<base::RunLoop> run_loop_;

  base::test::TestFuture<std::string> received_message_future_;
  base::test::TestFuture<void> drop_channel_future_;
  base::test::TestFuture<void> on_response_future_;
};

ConnectTestingEventInterface::ConnectTestingEventInterface() = default;


std::string ConnectTestingEventInterface::failure_message() const {
  return failure_message_;
}

std::string ConnectTestingEventInterface::selected_subprotocol() const {
  return selected_subprotocol_;
}

std::string ConnectTestingEventInterface::extensions() const {
  return extensions_;
}

int ConnectTestingEventInterface::OnURLRequestConnected(
    net::URLRequest* request,
    const net::TransportInfo& info,
    net::CompletionOnceCallback callback) {
  return OK;
}

void ConnectTestingEventInterface::OnAddChannelResponse(
    std::unique_ptr<WebSocketHandshakeResponseInfo> response,
    const std::string& selected_subprotocol,
    const std::string& extensions) {
  response_ = std::move(response);
  selected_subprotocol_ = selected_subprotocol;
  extensions_ = extensions;
  on_response_future_.SetValue();
}

void ConnectTestingEventInterface::OnDataFrame(bool fin,
                                               WebSocketMessageType type,
                                               base::span<const char> payload) {
  DVLOG(3) << "Received WebSocket data frame with message:"
           << std::string(payload.begin(), payload.end());
  SetReceivedMessageFuture(std::string(base::as_string_view(payload)));
}

void ConnectTestingEventInterface::OnSendDataFrameDone() {}

void ConnectTestingEventInterface::OnClosingHandshake() {
  DVLOG(3) << "OnClosingHandeshake() invoked.";
}

void ConnectTestingEventInterface::OnDropChannel(bool was_clean,
                                                 uint16_t code,
                                                 const std::string& reason) {
  DVLOG(3) << "OnDropChannel() invoked, was_clean: " << was_clean
           << ", code: " << code << ", reason: " << reason;
  if (was_clean) {
    drop_channel_future_.SetValue();
  } else {
    DVLOG(1) << "OnDropChannel() did not receive a clean close.";
  }
}

void ConnectTestingEventInterface::OnFailChannel(
    const std::string& message,
    int net_error,
    std::optional<int> response_code) {
  DVLOG(3) << "OnFailChannel invoked with message: " << message;
  failed_ = true;
  failure_message_ = message;
  on_response_future_.SetValue();
}

void ConnectTestingEventInterface::OnStartOpeningHandshake(
    std::unique_ptr<WebSocketHandshakeRequestInfo> request) {}

void ConnectTestingEventInterface::OnSSLCertificateError(
    std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
    const GURL& url,
    int net_error,
    const SSLInfo& ssl_info,
    bool fatal) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SSLErrorCallbacks::CancelSSLRequest,
                                base::Owned(ssl_error_callbacks.release()),
                                ERR_SSL_PROTOCOL_ERROR, &ssl_info));
}

int ConnectTestingEventInterface::OnAuthRequired(
    const AuthChallengeInfo& auth_info,
    scoped_refptr<HttpResponseHeaders> response_headers,
    const IPEndPoint& remote_endpoint,
    base::OnceCallback<void(const AuthCredentials*)> callback,
    std::optional<AuthCredentials>* credentials) {
  *credentials = std::nullopt;
  return OK;
}

void ConnectTestingEventInterface::SetReceivedMessageFuture(
    std::string received_message) {
  received_message_future_.SetValue(received_message);
}

std::string ConnectTestingEventInterface::GetDataFramePayload() {
  return received_message_future_.Get();
}

// Addition on top of ConnectTestingEventInterface that allows for delayed
// connections based on the response from OnURLRequestConnected.
class DelayedOnURLConnectedEventInterface
    : public ConnectTestingEventInterface {
 public:
  // Returns ERR_IO_PENDING.
  int OnURLRequestConnected(net::URLRequest* request,
                            const net::TransportInfo& info,
                            net::CompletionOnceCallback callback) override;

  void WaitForConnectedEvent() { on_connected_future_.Get(); }
  void RunCallback(int net_err);

 private:
  base::test::TestFuture<void> on_connected_future_;
  net::CompletionOnceCallback callback_;
};

int DelayedOnURLConnectedEventInterface::OnURLRequestConnected(
    net::URLRequest* request,
    const net::TransportInfo& info,
    net::CompletionOnceCallback callback) {
  callback_ = std::move(callback);
  on_connected_future_.SetValue();
  return ERR_IO_PENDING;
}

void DelayedOnURLConnectedEventInterface::RunCallback(int net_err) {
  if (callback_) {
    std::move(callback_).Run(net_err);
  } else {
    DVLOG(3) << "No callback to run";
  }
}

// A subclass of TestNetworkDelegate that additionally implements the
// OnResolveProxy callback and records the information passed to it.
class TestProxyDelegateWithProxyInfo : public ProxyDelegate {
 public:
  TestProxyDelegateWithProxyInfo() = default;

  TestProxyDelegateWithProxyInfo(const TestProxyDelegateWithProxyInfo&) =
      delete;
  TestProxyDelegateWithProxyInfo& operator=(
      const TestProxyDelegateWithProxyInfo&) = delete;

  struct ResolvedProxyInfo {
    GURL url;
    ProxyInfo proxy_info;
  };

  const ResolvedProxyInfo& resolved_proxy_info() const {
    return resolved_proxy_info_;
  }

 protected:
  void OnResolveProxy(const GURL& url,
                      const NetworkAnonymizationKey& network_anonymization_key,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override {
    resolved_proxy_info_.url = url;
    resolved_proxy_info_.proxy_info = *result;
  }

  void OnSuccessfulRequestAfterFailures(
      const ProxyRetryInfoMap& proxy_retry_info) override {}

  void OnFallback(const ProxyChain& bad_chain, int net_error) override {}

  base::expected<HttpRequestHeaders, Error> OnBeforeTunnelRequest(
      const ProxyChain& proxy_chain,
      size_t proxy_index,
      OnBeforeTunnelRequestCallback callback) override {
    return HttpRequestHeaders();
  }

  Error OnTunnelHeadersReceived(const ProxyChain& proxy_chain,
                                size_t proxy_index,
                                const HttpResponseHeaders& response_headers,
                                CompletionOnceCallback callback) override {
    return OK;
  }

  void SetProxyResolutionService(
      ProxyResolutionService* proxy_resolution_service) override {}

  bool AliasRequiresProxyOverride(
      const std::string scheme,
      const std::vector<std::string>& dns_aliases,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    return false;
  }

 private:
  ResolvedProxyInfo resolved_proxy_info_;
};

class WebSocketEndToEndTest : public TestWithTaskEnvironment {
 protected:
  WebSocketEndToEndTest()
      : proxy_delegate_(std::make_unique<TestProxyDelegateWithProxyInfo>()),
        context_builder_(CreateTestURLRequestContextBuilder()) {}

  // Initialise the URLRequestContext. Normally done automatically by
  // ConnectAndWait(). This method is for the use of tests that need the
  // URLRequestContext initialised before calling ConnectAndWait().
  void InitialiseContext() {
    DCHECK(!context_);
    context_ = context_builder_->Build();
    context_->proxy_resolution_service()->SetProxyDelegate(
        proxy_delegate_.get());
  }

  void Connect(const GURL& socket_url,
               std::unique_ptr<ConnectTestingEventInterface> event_interface) {
    if (!context_) {
      InitialiseContext();
    }
    url::Origin origin = url::Origin::Create(GURL("http://localhost"));
    net::SiteForCookies site_for_cookies =
        net::SiteForCookies::FromOrigin(origin);
    IsolationInfo isolation_info =
        IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin,
                              origin, SiteForCookies::FromOrigin(origin));
    event_interface_ = event_interface.get();
    channel_ = std::make_unique<WebSocketChannel>(std::move(event_interface),
                                                  context_.get());
    channel_->SendAddChannelRequest(
        GURL(socket_url), sub_protocols_, origin, site_for_cookies,
        StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Send the connect request to |socket_url| and wait for a response. Returns
  // true if the handshake succeeded.
  bool ConnectAndWait(const GURL& socket_url) {
    Connect(socket_url, std::make_unique<ConnectTestingEventInterface>());
    event_interface_->WaitForResponse();
    return !event_interface_->failed();
  }

  [[nodiscard]] WebSocketChannel::ChannelState SendMessage(
      const std::string& message) {
    scoped_refptr<IOBufferWithSize> buffer =
        base::MakeRefCounted<IOBufferWithSize>(message.size());

    buffer->span().copy_from(base::as_byte_span(message));
    return channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText, buffer,
                               message.size());
  }

  std::string ReceiveMessage() {
    auto channel_state = channel_->ReadFrames();
    if (channel_state != WebSocketChannel::ChannelState::CHANNEL_ALIVE) {
      ADD_FAILURE()
          << "WebSocket channel is no longer alive after reading frames. State:"
          << channel_state;
      return {};
    }
    return event_interface_->GetDataFramePayload();
  }

  void CloseWebSocket() {
    const uint16_t close_code = 1000;
    const std::string close_reason = "Closing connection";

    DVLOG(3) << "Sending close handshake with code: " << close_code
             << " and reason: " << close_reason;

    auto channel_state =
        channel_->StartClosingHandshake(close_code, close_reason);

    EXPECT_EQ(channel_state, WebSocketChannel::ChannelState::CHANNEL_ALIVE)
        << "WebSocket channel is no longer alive after sending the "
           "Close frame. State: "
        << channel_state;
  }

  void CloseWebSocketSuccessfully() {
    CloseWebSocket();
    event_interface_->WaitForDropChannel();
  }

  void RunBasicSmokeTest(net::EmbeddedTestServer::Type server_type) {
    test_server::EmbeddedTestServer embedded_test_server(server_type);

    test_server::InstallDefaultWebSocketHandlers(&embedded_test_server);

    ASSERT_TRUE(embedded_test_server.Start());

    GURL echo_url = test_server::ToWebSocketUrl(
        embedded_test_server.GetURL("/echo-with-no-extension"));
    EXPECT_TRUE(ConnectAndWait(echo_url));
  }

  raw_ptr<ConnectTestingEventInterface, DanglingUntriaged>
      event_interface_;  // owned by channel_
  std::unique_ptr<TestProxyDelegateWithProxyInfo> proxy_delegate_;
  std::unique_ptr<URLRequestContextBuilder> context_builder_;
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<WebSocketChannel> channel_;
  std::vector<std::string> sub_protocols_;
};

// Basic test of connectivity. If this test fails, nothing else can be expected
// to work.
TEST_F(WebSocketEndToEndTest, BasicSmokeTest) {
  RunBasicSmokeTest(net::EmbeddedTestServer::TYPE_HTTP);
}

TEST_F(WebSocketEndToEndTest, BasicSmokeTestSSL) {
  RunBasicSmokeTest(net::EmbeddedTestServer::TYPE_HTTPS);
}

TEST_F(WebSocketEndToEndTest, WebSocketEchoHandlerTest) {
  test_server::EmbeddedTestServer embedded_test_server(
      test_server::EmbeddedTestServer::TYPE_HTTP);

  test_server::InstallDefaultWebSocketHandlers(&embedded_test_server);

  ASSERT_TRUE(embedded_test_server.Start());

  GURL echo_url = test_server::ToWebSocketUrl(
      embedded_test_server.GetURL("/echo-with-no-extension"));
  ASSERT_TRUE(ConnectAndWait(echo_url));

  const std::string test_message = "hello echo";

  auto channel_state = SendMessage(test_message);

  ASSERT_EQ(channel_state, WebSocketChannel::ChannelState::CHANNEL_ALIVE);

  std::string received_message = ReceiveMessage();

  EXPECT_EQ(test_message, received_message);
  CloseWebSocketSuccessfully();
}

// Test for issue crbug.com/433695 "Unencrypted WebSocket connection via
// authenticated proxy times out".
TEST_F(WebSocketEndToEndTest, HttpsProxyUnauthedFails) {
  // Set up WebSocket server. Should not actually be used, beyond providing a
  // URL that is blocked by the proxy requesting authentication.
  EmbeddedTestServer ws_server(EmbeddedTestServer::Type::TYPE_HTTP);
  test_server::InstallDefaultWebSocketHandlers(&ws_server);
  ASSERT_TRUE(ws_server.Start());

  EmbeddedTestServer proxy_server(EmbeddedTestServer::Type::TYPE_HTTP);
  proxy_server.EnableConnectProxy({ws_server.host_port_pair()});
  RegisterProxyBasicAuthHandler(proxy_server, "user", "pass");
  ASSERT_TRUE(proxy_server.Start());

  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(
      "https=" + proxy_server.host_port_pair().ToString());
  // TODO(crbug.com/40600992): Don't rely on proxying localhost.
  proxy_config.proxy_rules().bypass_rules.AddRulesToSubtractImplicit();

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS)));
  ASSERT_TRUE(proxy_resolution_service);
  context_builder_->set_proxy_resolution_service(
      std::move(proxy_resolution_service));

  EXPECT_FALSE(
      ConnectAndWait(test_server::GetWebSocketURL(ws_server, kEchoServer)));
  EXPECT_EQ("Proxy authentication failed", event_interface_->failure_message());
}

TEST_F(WebSocketEndToEndTest, HttpsWssProxyUnauthedFails) {
  // Set up WebSocket server. Should not actually be used, beyond providing a
  // URL that is blocked by the proxy requesting authentication.
  EmbeddedTestServer wss_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  test_server::InstallDefaultWebSocketHandlers(&wss_server);
  ASSERT_TRUE(wss_server.Start());

  EmbeddedTestServer proxy_server(net::EmbeddedTestServer::Type::TYPE_HTTP);
  proxy_server.EnableConnectProxy({wss_server.host_port_pair()});
  RegisterProxyBasicAuthHandler(proxy_server, "user", "pass");
  ASSERT_TRUE(proxy_server.Start());

  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(
      "https=" + proxy_server.host_port_pair().ToString());
  // TODO(crbug.com/40600992): Don't rely on proxying localhost.
  proxy_config.proxy_rules().bypass_rules.AddRulesToSubtractImplicit();

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS)));
  ASSERT_TRUE(proxy_resolution_service);
  context_builder_->set_proxy_resolution_service(
      std::move(proxy_resolution_service));
  EXPECT_FALSE(
      ConnectAndWait(test_server::GetWebSocketURL(wss_server, kEchoServer)));
  EXPECT_EQ("Proxy authentication failed", event_interface_->failure_message());
}

// Regression test for crbug.com/426736 "WebSocket connections not using
// configured system HTTPS Proxy".
TEST_F(WebSocketEndToEndTest, HttpsProxyUsed) {
  EmbeddedTestServer ws_server(EmbeddedTestServer::Type::TYPE_HTTP);
  test_server::InstallDefaultWebSocketHandlers(&ws_server);
  ASSERT_TRUE(ws_server.Start());

  EmbeddedTestServer proxy_server(net::EmbeddedTestServer::Type::TYPE_HTTP);
  proxy_server.EnableConnectProxy({ws_server.host_port_pair()});

  ASSERT_TRUE(proxy_server.Start());

  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(
      "https=" + proxy_server.host_port_pair().ToString());
  // TODO(crbug.com/40600992): Don't rely on proxying localhost.
  proxy_config.proxy_rules().bypass_rules.AddRulesToSubtractImplicit();

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS)));
  context_builder_->set_proxy_resolution_service(
      std::move(proxy_resolution_service));
  InitialiseContext();

  GURL ws_url = test_server::GetWebSocketURL(ws_server, kEchoServer);
  EXPECT_TRUE(ConnectAndWait(ws_url));
  const TestProxyDelegateWithProxyInfo::ResolvedProxyInfo& info =
      proxy_delegate_->resolved_proxy_info();
  EXPECT_EQ(ws_url, info.url);
  EXPECT_EQ(info.proxy_info.ToDebugString(),
            base::StrCat({"PROXY ", proxy_server.host_port_pair().ToString()}));
}

std::unique_ptr<HttpResponse> ProxyPacHandler(const HttpRequest& request) {
  GURL url = request.GetURL();
  EXPECT_EQ(url.path(), "/proxy.pac");
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
TEST_F(WebSocketEndToEndTest, ProxyPacUsed) {
  if constexpr (!BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE)) {
    GTEST_SKIP() << "Test not supported on this platform";
  }

  EmbeddedTestServer ws_server(EmbeddedTestServer::Type::TYPE_HTTP);
  test_server::InstallDefaultWebSocketHandlers(&ws_server);
  ASSERT_TRUE(ws_server.Start());

  EmbeddedTestServer proxy_pac_server(EmbeddedTestServer::Type::TYPE_HTTP);
  proxy_pac_server.RegisterRequestHandler(base::BindRepeating(ProxyPacHandler));
  ASSERT_TRUE(proxy_pac_server.Start());

  // Use a name other than localhost, since localhost implicitly bypasses the
  // use of proxy.pac.
  GURL ws_url =
      test_server::GetWebSocketURL(ws_server, "stealth-localhost", kEchoServer);

  EmbeddedTestServer proxy_server(EmbeddedTestServer::Type::TYPE_HTTP);
  proxy_server.EnableConnectProxy({HostPortPair::FromURL(ws_url)});
  ASSERT_TRUE(proxy_server.Start());

  ProxyConfig proxy_config =
      ProxyConfig::CreateFromCustomPacURL(proxy_pac_server.GetURL(base::StrCat(
          {"/proxy.pac?proxy=", proxy_server.host_port_pair().ToString()})));
  proxy_config.set_pac_mandatory(true);
  auto proxy_config_service = std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation(proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service),
          /*host_resolver_for_override_rules=*/nullptr, NetLog::Get(),
          /*quick_check_enabled=*/true));
  ASSERT_EQ(ws_server.host_port_pair().host(), "127.0.0.1");
  context_builder_->set_proxy_resolution_service(
      std::move(proxy_resolution_service));
  InitialiseContext();

  EXPECT_TRUE(ConnectAndWait(ws_url));
  const auto& info = proxy_delegate_->resolved_proxy_info();
  EXPECT_EQ(ws_url, info.url);
  EXPECT_EQ(info.proxy_info.ToDebugString(),
            base::StrCat({"PROXY ", proxy_server.host_port_pair().ToString()}));
}

// This is a regression test for crbug.com/408061 Crash in
// net::WebSocketBasicHandshakeStream::Upgrade.
TEST_F(WebSocketEndToEndTest, TruncatedResponse) {
  EmbeddedTestServer ws_server(EmbeddedTestServer::Type::TYPE_HTTP);
  test_server::InstallDefaultWebSocketHandlers(&ws_server);
  ASSERT_TRUE(ws_server.Start());
  InitialiseContext();

  GURL ws_url = test_server::GetWebSocketURL(ws_server, "/truncated-headers");
  EXPECT_FALSE(ConnectAndWait(ws_url));
}

// Regression test for crbug.com/455215 "HSTS not applied to WebSocket"
TEST_F(WebSocketEndToEndTest, HstsHttpsToWebSocket) {
  base::test::ScopedFeatureList features;
  // Websocket upgrades can't happen when only top-level navigations are
  // upgraded, so disable the feature for this test.
  features.InitAndDisableFeature(features::kHstsTopLevelNavigationsOnly);

  EmbeddedTestServer https_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  std::string test_server_hostname = "a.test";
  https_server.SetCertHostnames({test_server_hostname});
  https_server.ServeFilesFromSourceDirectory("net/data/url_request_unittest");
  ASSERT_TRUE(https_server.Start());

  EmbeddedTestServer wss_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  wss_server.SetCertHostnames({test_server_hostname});
  test_server::InstallDefaultWebSocketHandlers(&wss_server);
  ASSERT_TRUE(wss_server.Start());

  InitialiseContext();

  // Set HSTS via https:
  TestDelegate delegate;
  GURL https_page =
      https_server.GetURL(test_server_hostname, "/hsts-headers.html");
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      https_page, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ(OK, delegate.request_status());

  // Check HSTS with ws:
  // Change the scheme from wss: to ws: to verify that it is switched back.
  GURL ws_url =
      ReplaceUrlScheme(test_server::GetWebSocketURL(
                           wss_server, test_server_hostname, kEchoServer),
                       "ws");
  EXPECT_TRUE(ConnectAndWait(ws_url));
}

// Tests that when kHstsTopLevelNavigationsOnly is enabled websocket isn't
// upgraded.
TEST_F(WebSocketEndToEndTest, HstsHttpsToWebSocketNotApplied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kHstsTopLevelNavigationsOnly);

  EmbeddedTestServer https_server(net::EmbeddedTestServer::Type::TYPE_HTTPS);
  https_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_server.ServeFilesFromSourceDirectory("net/data/url_request_unittest");

  EmbeddedTestServer ws_server(net::EmbeddedTestServer::TYPE_HTTP);
  net::test_server::InstallDefaultWebSocketHandlers(&ws_server);

  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(ws_server.Start());
  InitialiseContext();
  // Set HSTS via https:
  TestDelegate delegate;
  GURL https_page = https_server.GetURL("/hsts-headers.html");
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      https_page, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ(OK, delegate.request_status());

  // Check that the ws connection was not upgraded.
  GURL ws_url = net::test_server::GetWebSocketURL(ws_server, kEchoServer);
  EXPECT_TRUE(ConnectAndWait(ws_url));
}

TEST_F(WebSocketEndToEndTest, HstsWebSocketToHttps) {
  EmbeddedTestServer https_server(net::EmbeddedTestServer::Type::TYPE_HTTPS);
  std::string test_server_hostname = "a.test";
  https_server.SetCertHostnames({test_server_hostname});
  https_server.ServeFilesFromSourceDirectory("net/data/url_request_unittest");
  ASSERT_TRUE(https_server.Start());

  EmbeddedTestServer wss_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  wss_server.SetCertHostnames({test_server_hostname});
  test_server::InstallDefaultWebSocketHandlers(&wss_server);
  ASSERT_TRUE(wss_server.Start());

  InitialiseContext();
  // Set HSTS via wss:
  GURL wss_url = test_server::GetWebSocketURL(wss_server, test_server_hostname,
                                              "/set-hsts");
  EXPECT_TRUE(ConnectAndWait(wss_url));

  // Verify via http:
  TestDelegate delegate;
  GURL http_page = ReplaceUrlScheme(
      https_server.GetURL(test_server_hostname, "/simple.html"), "http");
  url::Origin http_origin = url::Origin::Create(http_page);
  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      http_page, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->set_isolation_info(IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, http_origin, http_origin,
      SiteForCookies::FromOrigin(http_origin)));
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ(OK, delegate.request_status());
  EXPECT_TRUE(request->url().SchemeIs("https"));
}

TEST_F(WebSocketEndToEndTest, HstsWebSocketToWebSocket) {
  base::test::ScopedFeatureList features;
  // Websocket upgrades can't happen when only top-level navigations are
  // upgraded, so disable the feature for this test.
  features.InitAndDisableFeature(features::kHstsTopLevelNavigationsOnly);

  std::string test_server_hostname = "a.test";
  EmbeddedTestServer wss_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  wss_server.SetCertHostnames({test_server_hostname});
  test_server::InstallDefaultWebSocketHandlers(&wss_server);
  ASSERT_TRUE(wss_server.Start());

  InitialiseContext();
  // Set HSTS via wss:
  GURL wss_url = test_server::GetWebSocketURL(wss_server, test_server_hostname,
                                              "/set-hsts");
  EXPECT_TRUE(ConnectAndWait(wss_url));

  // Verify via ws:
  GURL ws_url = ReplaceUrlScheme(
      wss_server.GetURL(test_server_hostname, kEchoServer), "ws");
  EXPECT_TRUE(ConnectAndWait(ws_url));
}

// WebSocketHandler that sends HTTP response headers with trailing whitespace.
class WebSocketTrailingWhitespaceHandler
    : public test_server::WebSocketHandler {
 public:
  explicit WebSocketTrailingWhitespaceHandler(
      scoped_refptr<test_server::WebSocketConnection> connection)
      : test_server::WebSocketHandler(std::move(connection)) {}

  void OnHandshake(const test_server::HttpRequest& request) override {
    CHECK(connection());
    connection()->SetResponseHeader("Sec-WebSocket-Protocol", "sip    ");
  }
};

// Regression test for crbug.com/180504 "WebSocket handshake fails when HTTP
// headers have trailing LWS".
TEST_F(WebSocketEndToEndTest, TrailingWhitespace) {
  const std::string kPath = "/trailing-whitespace";
  EmbeddedTestServer ws_server(EmbeddedTestServer::Type::TYPE_HTTP);
  test_server::RegisterWebSocketHandler<WebSocketTrailingWhitespaceHandler>(
      &ws_server, kPath);
  ASSERT_TRUE(ws_server.Start());

  GURL ws_url = test_server::GetWebSocketURL(ws_server, kPath);
  sub_protocols_.push_back("sip");
  EXPECT_TRUE(ConnectAndWait(ws_url));
  EXPECT_EQ("sip", event_interface_->selected_subprotocol());
}

// WebSocketHandler that sends HTTP response headers with a continuation.
class WebSocketHeaderContinuationHandler
    : public test_server::WebSocketHandler {
 public:
  explicit WebSocketHeaderContinuationHandler(
      scoped_refptr<test_server::WebSocketConnection> connection)
      : test_server::WebSocketHandler(std::move(connection)) {}

  void OnHandshake(const test_server::HttpRequest& request) override {
    CHECK(connection());
    // Response headers are added blindly, so this results in a continuation.
    connection()->SetResponseHeader("Sec-WebSocket-Extensions",
                                    "permessage-deflate;\r\n"
                                    "  server_max_window_bits=10");
  }
};

// This is a regression test for crbug.com/169448 "WebSockets should support
// header continuations"
// TODO(ricea): HTTP continuation headers have been deprecated by RFC7230.  If
// support for continuation headers is removed from Chrome, then this test will
// break and should be removed.
TEST_F(WebSocketEndToEndTest, HeaderContinuations) {
  const std::string kPath = "/header-continuation";
  EmbeddedTestServer ws_server(EmbeddedTestServer::Type::TYPE_HTTP);
  test_server::RegisterWebSocketHandler<WebSocketHeaderContinuationHandler>(
      &ws_server, kPath);
  ASSERT_TRUE(ws_server.Start());

  GURL ws_url = test_server::GetWebSocketURL(ws_server, kPath);

  EXPECT_TRUE(ConnectAndWait(ws_url));
  EXPECT_EQ("permessage-deflate; server_max_window_bits=10",
            event_interface_->extensions());
}

// Test that ws->wss scheme upgrade is supported on receiving a DNS HTTPS
// record.
TEST_F(WebSocketEndToEndTest, DnsSchemeUpgradeSupported) {
  const std::string kTestServerHostname = "a.test";

  EmbeddedTestServer wss_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  wss_server.SetCertHostnames({kTestServerHostname});
  test_server::InstallDefaultWebSocketHandlers(&wss_server);
  ASSERT_TRUE(wss_server.Start());

  GURL wss_url = test_server::GetWebSocketURL(wss_server, kTestServerHostname,
                                              kEchoServer);
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kWsScheme);
  GURL ws_url = wss_url.ReplaceComponents(replacements);

  // Note that due to socket pool behavior, HostResolver will see the ws/wss
  // requests as http/https.
  auto host_resolver = std::make_unique<MockHostResolver>();
  MockHostResolverBase::RuleResolver::RuleKey unencrypted_resolve_key;
  unencrypted_resolve_key.scheme = url::kHttpScheme;
  host_resolver->rules()->AddRule(std::move(unencrypted_resolve_key),
                                  ERR_DNS_NAME_HTTPS_ONLY);
  MockHostResolverBase::RuleResolver::RuleKey encrypted_resolve_key;
  encrypted_resolve_key.scheme = url::kHttpsScheme;
  host_resolver->rules()->AddRule(std::move(encrypted_resolve_key),
                                  "127.0.0.1");
  context_builder_->set_host_resolver(std::move(host_resolver));

  EXPECT_TRUE(ConnectAndWait(ws_url));

  // Expect request to have reached the server using the upgraded URL.
  EXPECT_EQ(event_interface_->response()->url, wss_url);
}

// Test that wss connections can use HostResolverEndpointResults from DNS.
TEST_F(WebSocketEndToEndTest, HostResolverEndpointResult) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kUseDnsHttpsSvcb);

  const std::string kTestServerHostname = "a.test";

  EmbeddedTestServer wss_server(EmbeddedTestServer::Type::TYPE_HTTPS);
  wss_server.SetCertHostnames({kTestServerHostname});
  test_server::InstallDefaultWebSocketHandlers(&wss_server);
  ASSERT_TRUE(wss_server.Start());

  uint16_t port = wss_server.port();
  GURL wss_url = test_server::GetWebSocketURL(wss_server, kTestServerHostname,
                                              kEchoServer);

  auto host_resolver = std::make_unique<MockHostResolver>();
  MockHostResolverBase::RuleResolver::RuleKey resolve_key;
  // The DNS query itself is made with the https scheme rather than wss.
  resolve_key.scheme = url::kHttpsScheme;
  resolve_key.hostname_pattern = kTestServerHostname;
  resolve_key.port = port;
  HostResolverEndpointResult result;
  result.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), port)};
  result.metadata.supported_protocol_alpns = {"http/1.1"};
  host_resolver->rules()->AddRule(
      std::move(resolve_key),
      MockHostResolverBase::RuleResolver::RuleResult(std::vector{result}));
  context_builder_->set_host_resolver(std::move(host_resolver));

  EXPECT_TRUE(ConnectAndWait(wss_url));

  // Expect request to have reached the server using the upgraded URL.
  EXPECT_EQ(event_interface_->response()->url, wss_url);
}

// Test that wss connections can use EncryptedClientHello.
TEST_F(WebSocketEndToEndTest, EncryptedClientHello) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kUseDnsHttpsSvcb);

  // Configure a test server that speaks ECH.
  static constexpr char kRealName[] = "secret.example";
  static constexpr char kPublicName[] = "public.example";
  EmbeddedTestServer::ServerCertificateConfig server_cert_config;
  server_cert_config.dns_names = {kRealName};
  SSLServerConfig ssl_server_config;
  std::vector<uint8_t> ech_config_list;
  ssl_server_config.ech_keys =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/128, &ech_config_list);
  ASSERT_TRUE(ssl_server_config.ech_keys);

  // Only complete the handshake if ECH was actually used.
  ssl_server_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting(
          [&](const SSL_CLIENT_HELLO* client_hello) -> bool {
            return SSL_ech_accepted(client_hello->ssl);
          });

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(server_cert_config, ssl_server_config);
  test_server::InstallDefaultWebSocketHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  GURL wss_url =
      test_server::GetWebSocketURL(test_server, kRealName, kEchoServer);

  auto host_resolver = std::make_unique<MockHostResolver>();
  MockHostResolverBase::RuleResolver::RuleKey resolve_key;
  // The DNS query itself is made with the https scheme rather than wss.
  resolve_key.scheme = url::kHttpsScheme;
  resolve_key.hostname_pattern = wss_url.GetHost();
  resolve_key.port = wss_url.IntPort();
  HostResolverEndpointResult result;
  result.ip_endpoints = {
      IPEndPoint(IPAddress::IPv4Localhost(), wss_url.IntPort())};
  result.metadata.supported_protocol_alpns = {"http/1.1"};
  result.metadata.ech_config_list = ech_config_list;
  host_resolver->rules()->AddRule(
      std::move(resolve_key),
      MockHostResolverBase::RuleResolver::RuleResult(std::vector{result}));
  context_builder_->set_host_resolver(std::move(host_resolver));

  EXPECT_TRUE(ConnectAndWait(wss_url));

  // Expect request to have reached the server using the upgraded URL.
  EXPECT_EQ(event_interface_->response()->url, wss_url);
}

TEST_F(WebSocketEndToEndTest, WebSocketDelayedConnectionTest) {
  test_server::EmbeddedTestServer embedded_test_server(
      test_server::EmbeddedTestServer::TYPE_HTTP);

  test_server::InstallDefaultWebSocketHandlers(&embedded_test_server);

  ASSERT_TRUE(embedded_test_server.Start());

  GURL echo_url = test_server::ToWebSocketUrl(
      embedded_test_server.GetURL("/echo-with-no-extension"));
  std::unique_ptr<DelayedOnURLConnectedEventInterface> event_interface =
      std::make_unique<DelayedOnURLConnectedEventInterface>();

  DelayedOnURLConnectedEventInterface* event_interface_ptr =
      event_interface.get();
  Connect(echo_url, std::move(event_interface));
  event_interface_ptr->WaitForConnectedEvent();
  event_interface_ptr->RunCallback(OK);
  event_interface_->WaitForResponse();
  ASSERT_TRUE(!event_interface_->failed());
}

TEST_F(WebSocketEndToEndTest, WebSocketDelayedConnectionFailedTest) {
  test_server::EmbeddedTestServer embedded_test_server(
      test_server::EmbeddedTestServer::TYPE_HTTP);

  test_server::InstallDefaultWebSocketHandlers(&embedded_test_server);

  ASSERT_TRUE(embedded_test_server.Start());

  GURL echo_url = test_server::ToWebSocketUrl(
      embedded_test_server.GetURL("/echo-with-no-extension"));
  std::unique_ptr<DelayedOnURLConnectedEventInterface> event_interface =
      std::make_unique<DelayedOnURLConnectedEventInterface>();

  DelayedOnURLConnectedEventInterface* event_interface_ptr =
      event_interface.get();
  Connect(echo_url, std::move(event_interface));
  event_interface_ptr->WaitForConnectedEvent();
  // RunUntilIdle() to prove that the connection won't continue without
  // an OK from the callback.
  RunUntilIdle();
  event_interface_ptr->RunCallback(ERR_FAILED);
  event_interface_->WaitForResponse();
  ASSERT_TRUE(event_interface_->failed());
}

// Reset channel_ after OnURLConnected is called and returns ERR_IO_PENDING, for
// ASAN/MSAN coverage.
TEST_F(WebSocketEndToEndTest, WebSocketDelayedConnectionResetChannelTest) {
  test_server::EmbeddedTestServer embedded_test_server(
      test_server::EmbeddedTestServer::TYPE_HTTP);

  test_server::InstallDefaultWebSocketHandlers(&embedded_test_server);

  ASSERT_TRUE(embedded_test_server.Start());

  GURL echo_url = test_server::ToWebSocketUrl(
      embedded_test_server.GetURL("/echo-with-no-extension"));
  std::unique_ptr<DelayedOnURLConnectedEventInterface> event_interface =
      std::make_unique<DelayedOnURLConnectedEventInterface>();

  DelayedOnURLConnectedEventInterface* event_interface_ptr =
      event_interface.get();
  Connect(echo_url, std::move(event_interface));
  event_interface_ptr->WaitForConnectedEvent();
  channel_.reset();
}

}  // namespace
}  // namespace net
