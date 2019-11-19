// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/log/net_log.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_task_environment.h"
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
  TestHostResolverProc()
      : HostResolverProc(nullptr), insecure_queries_served_(0) {}

  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    insecure_queries_served_++;
    *addrlist = AddressList::CreateFromIPAddress(IPAddress(127, 0, 0, 1), 443);
    return OK;
  }

  uint32_t insecure_queries_served() { return insecure_queries_served_; }

 private:
  ~TestHostResolverProc() override {}
  uint32_t insecure_queries_served_;
};

class HttpWithDnsOverHttpsTest : public TestWithTaskEnvironment {
 public:
  HttpWithDnsOverHttpsTest()
      : resolver_(HostResolver::CreateStandaloneContextResolver(nullptr)),
        host_resolver_proc_(new TestHostResolverProc()),
        cert_verifier_(std::make_unique<MockCertVerifier>()),
        request_context_(true),
        doh_server_(EmbeddedTestServer::Type::TYPE_HTTPS),
        test_server_(EmbeddedTestServer::Type::TYPE_HTTPS),
        doh_queries_served_(0),
        test_https_requests_served_(0) {
    doh_server_.RegisterRequestHandler(
        base::BindRepeating(&HttpWithDnsOverHttpsTest::HandleDefaultConnect,
                            base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HttpWithDnsOverHttpsTest::HandleDefaultConnect,
                            base::Unretained(this)));
    EXPECT_TRUE(doh_server_.Start());
    EXPECT_TRUE(test_server_.Start());
    GURL url(doh_server_.GetURL("doh-server.com", "/dns_query"));
    std::unique_ptr<DnsClient> dns_client(DnsClient::CreateClient(nullptr));

    DnsConfig config;
    config.nameservers.push_back(IPEndPoint());
    EXPECT_TRUE(config.IsValid());
    dns_client->SetSystemConfig(std::move(config));

    resolver_->SetRequestContext(&request_context_);
    resolver_->SetProcParamsForTesting(
        ProcTaskParams(host_resolver_proc_.get(), 1));
    resolver_->GetManagerForTesting()->SetDnsClientForTesting(
        std::move(dns_client));

    DnsConfigOverrides overrides;
    overrides.dns_over_https_servers.emplace(
        {DnsConfig::DnsOverHttpsServerConfig(url.spec(), true /* use_post */)});
    overrides.secure_dns_mode = DnsConfig::SecureDnsMode::SECURE;
    overrides.use_local_ipv6 = true;
    resolver_->GetManagerForTesting()->SetDnsConfigOverrides(
        std::move(overrides));
    request_context_.set_host_resolver(resolver_.get());

    cert_verifier_->set_default_result(net::OK);
    request_context_.set_cert_verifier(cert_verifier_.get());

    request_context_.Init();
  }

  URLRequestContext* context() { return &request_context_; }

  std::unique_ptr<test_server::HttpResponse> HandleDefaultConnect(
      const test_server::HttpRequest& request) {
    if (request.relative_url.compare("/dns_query") == 0) {
      doh_queries_served_++;

      // Parse request content as a DnsQuery to access the question.
      auto request_buffer =
          base::MakeRefCounted<IOBufferWithSize>(request.content.size());
      memcpy(request_buffer->data(), request.content.data(),
             request.content.size());
      DnsQuery query(std::move(request_buffer));
      EXPECT_TRUE(query.Parse(request.content.size()));

      char header_data[kHeaderSize];
      base::BigEndianWriter header_writer(header_data, kHeaderSize);
      header_writer.WriteU16(query.id());  // Same ID as before
      char flags[] = {0x81, 0x80};
      header_writer.WriteBytes(flags, 2);
      header_writer.WriteU16(1);  // 1 question
      header_writer.WriteU16(1);  // 1 answer
      header_writer.WriteU16(0);  // No authority records
      header_writer.WriteU16(0);  // No additional records

      const uint8_t answer_data[]{0xC0, 0x0C,  // - NAME
                                  0x00, 0x01,  // - TYPE
                                  0x00, 0x01,  // - CLASS
                                  0x00, 0x00,  //
                                  0x18, 0x4C,  // - TTL
                                  0x00, 0x04,  // - RDLENGTH = 4 bytes
                                  0x7f, 0x00,  // - RDDATA, IP is 127.0.0.1
                                  0x00, 0x01};

      std::unique_ptr<test_server::BasicHttpResponse> http_response(
          new test_server::BasicHttpResponse);
      http_response->set_content(
          std::string(header_data, sizeof(header_data)) +
          query.question().as_string() +
          std::string((char*)answer_data, sizeof(answer_data)));
      http_response->set_content_type("application/dns-message");
      return std::move(http_response);
    } else {
      test_https_requests_served_++;
      std::unique_ptr<test_server::BasicHttpResponse> http_response(
          new test_server::BasicHttpResponse);
      http_response->set_content(kTestBody);
      http_response->set_content_type("text/html");
      return std::move(http_response);
    }
  }

 protected:
  std::unique_ptr<ContextHostResolver> resolver_;
  scoped_refptr<net::TestHostResolverProc> host_resolver_proc_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  TestURLRequestContext request_context_;
  EmbeddedTestServer doh_server_;
  EmbeddedTestServer test_server_;
  uint32_t doh_queries_served_;
  uint32_t test_https_requests_served_;
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
                      const SSLConfig& used_ssl_config,
                      const ProxyInfo& used_proxy_info) override {}

  void OnCertificateError(int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info) override {}

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}

  void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info) override {}

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

  ClientSocketPool::GroupId group_id(
      HostPortPair(request_info.url.host(), request_info.url.IntPort()),
      ClientSocketPool::SocketType::kHttp, PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkIsolationKey(), false /* disable_secure_dns */);
  EXPECT_EQ(network_session
                ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                ProxyServer::Direct())
                ->IdleSocketCountInGroup(group_id),
            1u);

  // The domain "localhost" is resolved locally, so no DNS lookups should have
  // occurred.
  EXPECT_EQ(doh_queries_served_, 0u);
  EXPECT_EQ(host_resolver_proc_->insecure_queries_served(), 0u);
  // A stream was established, but no HTTPS request has been made yet.
  EXPECT_EQ(test_https_requests_served_, 0u);

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

  // There should be two DoH lookups for "bar.example.com" (both A and AAAA
  // records are queried).
  EXPECT_EQ(doh_queries_served_, 2u);
  // The requests to the DoH server are pooled, so there should only be one
  // insecure lookup for the DoH server hostname.
  EXPECT_EQ(host_resolver_proc_->insecure_queries_served(), 1u);
  // There should be one non-DoH HTTPS request for the connection to
  // "bar.example.com".
  EXPECT_EQ(test_https_requests_served_, 1u);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

}  // namespace
}  // namespace net
