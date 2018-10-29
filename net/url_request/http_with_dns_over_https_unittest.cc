// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/host_resolver_impl.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/log/net_log.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {
namespace {

const size_t kHeaderSize = sizeof(dns_protocol::Header);
const char kTestBody[] = "<html><body>TEST RESPONSE</body></html>";

class TestHostResolverProc : public HostResolverProc {
 public:
  TestHostResolverProc() : HostResolverProc(nullptr) {}

  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    return ERR_NAME_NOT_RESOLVED;
  }

 private:
  ~TestHostResolverProc() override {}
};

class HttpWithDnsOverHttpsTest : public TestWithScopedTaskEnvironment {
 public:
  HttpWithDnsOverHttpsTest()
      : resolver_(HostResolver::Options(), nullptr),
        request_context_(true),
        doh_server_(EmbeddedTestServer::Type::TYPE_HTTPS),
        test_server_(EmbeddedTestServer::Type::TYPE_HTTPS),
        doh_queries_served_(0),
        test_requests_served_(0) {
    doh_server_.RegisterRequestHandler(
        base::BindRepeating(&HttpWithDnsOverHttpsTest::HandleDefaultConnect,
                            base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HttpWithDnsOverHttpsTest::HandleDefaultConnect,
                            base::Unretained(this)));
    EXPECT_TRUE(doh_server_.Start());
    EXPECT_TRUE(test_server_.Start());
    GURL url(doh_server_.GetURL("/dns_query"));
    std::unique_ptr<DnsClient> dns_client(DnsClient::CreateClient(nullptr));
    DnsConfig config;
    config.nameservers.push_back(IPEndPoint());
    config.dns_over_https_servers.emplace_back(url.spec(), true /* use_post */);
    dns_client->SetConfig(config);
    resolver_.SetRequestContext(&request_context_);
    resolver_.set_proc_params_for_test(
        HostResolverImpl::ProcTaskParams(new TestHostResolverProc(), 1));
    resolver_.SetDnsClient(std::move(dns_client));
    request_context_.set_host_resolver(&resolver_);
    request_context_.Init();
  }

  URLRequestContext* context() { return &request_context_; }

  std::unique_ptr<test_server::HttpResponse> HandleDefaultConnect(
      const test_server::HttpRequest& request) {
    if (request.relative_url.compare("/dns_query") == 0) {
      doh_queries_served_++;
      uint8_t id1 = request.content[0];
      uint8_t id2 = request.content[1];
      std::unique_ptr<test_server::BasicHttpResponse> http_response(
          new test_server::BasicHttpResponse);
      const uint8_t header_data[] = {
          id1,  id2,   // - Same ID as before
          0x81, 0x80,  // - Different flags, we'll look at this below
          0x00, 0x01,  // - 1 question
          0x00, 0x01,  // - 1 answer
          0x00, 0x00,  // - No authority records
          0x00, 0x00,  // - No additional records
      };
      std::string question = request.content.substr(kHeaderSize);

      const uint8_t answer_data[]{0xC0, 0x0C,  // - NAME
                                  0x00, 0x01,  // - TYPE
                                  0x00, 0x01,  // - CLASS
                                  0x00, 0x00,  //
                                  0x18, 0x4C,  // - TTL
                                  0x00, 0x04,  // - RDLENGTH = 4 bytes
                                  0x7f, 0x00,  // - RDDATA, IP is 127.0.0.1
                                  0x00, 0x01};
      http_response->set_content(
          std::string((char*)header_data, sizeof(header_data)) + question +
          std::string((char*)answer_data, sizeof(answer_data)));
      http_response->set_content_type("application/dns-message");
      return std::move(http_response);
    } else {
      test_requests_served_++;
      std::unique_ptr<test_server::BasicHttpResponse> http_response(
          new test_server::BasicHttpResponse);
      http_response->set_content(kTestBody);
      http_response->set_content_type("text/html");
      return std::move(http_response);
    }
  }

 protected:
  HostResolverImpl resolver_;
  TestURLRequestContext request_context_;
  EmbeddedTestServer doh_server_;
  EmbeddedTestServer test_server_;
  uint32_t doh_queries_served_;
  uint32_t test_requests_served_;
};

class TestHttpDelegate : public HttpStreamRequest::Delegate {
 public:
  TestHttpDelegate(base::RunLoop* loop) : loop_(loop) {}
  ~TestHttpDelegate() override {}
  void OnStreamReady(const SSLConfig& used_ssl_config,
                     const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    stream->Close(false);
    loop_->Quit();
  }

  void OnWebSocketHandshakeStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {}

  void OnBidirectionalStreamImplReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {}

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const SSLConfig& used_ssl_config) override {}

  void OnCertificateError(int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info) override {}

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}

  void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info) override {}

  void OnHttpsProxyTunnelResponse(const HttpResponseInfo& response_info,
                                  const SSLConfig& used_ssl_config,
                                  const ProxyInfo& used_proxy_info,
                                  std::unique_ptr<HttpStream> stream) override {
  }

  void OnQuicBroken() override {}

 private:
  base::RunLoop* loop_;
};

// This test sets up a request which will reenter the connection pools by
// triggering a DNS over HTTPS request. It also sets up an idle socket
// which was a precondition for the crash we saw in  https://crbug.com/830917.
TEST_F(HttpWithDnsOverHttpsTest, EndToEnd) {
  // Create and start http server.
  EmbeddedTestServer http_server(EmbeddedTestServer::Type::TYPE_HTTP);
  http_server.RegisterRequestHandler(base::BindRepeating(
      &HttpWithDnsOverHttpsTest::HandleDefaultConnect, base::Unretained(this)));
  EXPECT_TRUE(http_server.Start());

  // Set up an idle socket.
  HttpTransactionFactory* transaction_factory =
      request_context_.http_transaction_factory();
  HttpStreamFactory::JobFactory default_job_factory;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  base::RunLoop loop;
  TestHttpDelegate request_delegate(&loop);

  HttpStreamFactory* factory = network_session->http_stream_factory();
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = http_server.GetURL("localhost", "/preconnect");

  std::unique_ptr<HttpStreamRequest> request(factory->RequestStream(
      request_info, DEFAULT_PRIORITY, SSLConfig(), SSLConfig(),
      &request_delegate, false, false, NetLogWithSource()));
  loop.Run();

  std::string group_name(request_info.url.host() + ":" +
                         request_info.url.port());
  EXPECT_EQ(network_session
                ->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)
                ->IdleSocketCountInGroup(group_name),
            1);

  // Make a request that will trigger a DoH query as well.
  TestDelegate d;
  d.set_allow_certificate_errors(true);
  GURL main_url = test_server_.GetURL("bar.example.com", "/test");
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  base::RunLoop().Run();
  EXPECT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(http_server.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(doh_server_.ShutdownAndWaitUntilComplete());
  EXPECT_EQ(doh_queries_served_, 2u);
  EXPECT_EQ(test_requests_served_, 1u);
  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

}  // namespace
}  // namespace net
