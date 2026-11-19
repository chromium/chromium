// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_request_context_builder_mojo.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test_mojo_proxy_resolver_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "services/network/mock_mojo_dhcp_wpad_url_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace network {

namespace {

const char kPacPath[] = "/super.pac";

// When kPacPath is requested, returns a PAC script that uses the test server
// itself as the proxy.
std::unique_ptr<net::test_server::HttpResponse> HandlePacRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPacPath)
    return nullptr;
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(base::StringPrintf(
      "function FindProxyForURL(url, host) { return 'PROXY %s;'; }",
      net::HostPortPair::FromURL(request.base_url).ToString().c_str()));
  response->set_content_type("text/html");
  return std::move(response);
}

class URLRequestContextBuilderMojoTest : public PlatformTest {
 protected:
  URLRequestContextBuilderMojoTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    test_server_.RegisterRequestHandler(base::BindRepeating(&HandlePacRequest));
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  }

  base::test::TaskEnvironment task_environment_;
  TestMojoProxyResolverFactory test_mojo_proxy_resolver_factory_;
  net::EmbeddedTestServer test_server_;
  URLRequestContextBuilderMojo builder_;
};

TEST_F(URLRequestContextBuilderMojoTest, MojoProxyResolver) {
  EXPECT_TRUE(test_server_.Start());

  builder_.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation(
              net::ProxyConfig::CreateFromCustomPacURL(
                  test_server_.GetURL(kPacPath)),
              TRAFFIC_ANNOTATION_FOR_TESTS)));
  builder_.SetMojoProxyResolverFactory(
      test_mojo_proxy_resolver_factory_.CreateFactoryRemote());

#if BUILDFLAG(IS_CHROMEOS)
  builder_.SetDhcpWpadUrlClient(
      MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(std::string()));
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<net::URLRequestContext> context(builder_.Build());
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request(context->CreateRequest(
      GURL("http://hats:12345/echoheader?Foo"), net::DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS,
      net::handles::kInvalidNetworkHandle));
  request->SetExtraRequestHeaderByName("Foo", "Bar", false);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ("Bar", delegate.data_received());

  // Make sure that the Mojo factory was used.
  EXPECT_TRUE(test_mojo_proxy_resolver_factory_.resolver_created());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
class TestSystemProxyResolver
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override {
    ++request_count_;
    net::ProxyList proxies;
    proxies.Set("native.test:8080");
    auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
    status->is_success = true;
    std::move(callback).Run(proxies, std::move(status));
  }

  int request_count() const { return request_count_; }

 private:
  int request_count_ = 0;
};

class URLRequestContextBuilderMojoSystemProxyTest
    : public URLRequestContextBuilderMojoTest,
      public testing::WithParamInterface<bool> {
 protected:
  int ResolveProxySync(net::ConfiguredProxyResolutionService* service,
                       net::ProxyInfo* info) {
    net::TestCompletionCallback callback;
    std::unique_ptr<net::ProxyResolutionRequest> request;
    int rv = service->ResolveProxy(
        GURL("https://example.test/"), "GET", net::NetworkAnonymizationKey(),
        net::handles::kInvalidNetworkHandle, info, callback.callback(),
        &request, net::NetLogWithSource(), net::DEFAULT_PRIORITY);
    return callback.GetResult(rv);
  }
};

TEST_P(URLRequestContextBuilderMojoSystemProxyTest, NativeSystemBaseline) {
  TestSystemProxyResolver resolver;
  mojo::Receiver<proxy_resolver::mojom::SystemProxyResolver> receiver(
      &resolver);
  builder_.SetMojoSystemProxyResolver(receiver.BindNewPipeAndPassRemote());
  if (GetParam()) {
    builder_.SetMojoProxyResolverFactory(
        test_mojo_proxy_resolver_factory_.CreateFactoryRemote());
  }
  net::ProxyConfig config;
  config.set_from_system(true);
  builder_.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation(config,
                                         TRAFFIC_ANNOTATION_FOR_TESTS)));
  std::unique_ptr<net::URLRequestContext> context = builder_.Build();
  ASSERT_TRUE(context->proxy_resolution_service());
  net::ConfiguredProxyResolutionService* configured_service = nullptr;
  ASSERT_TRUE(
      context->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&configured_service));

  net::ProxyInfo info;
  EXPECT_EQ(net::OK, ResolveProxySync(configured_service, &info));
  EXPECT_EQ("PROXY native.test:8080", info.ToPacString());
  EXPECT_EQ(1, resolver.request_count());
  EXPECT_FALSE(test_mojo_proxy_resolver_factory_.resolver_created());
}

TEST_P(URLRequestContextBuilderMojoSystemProxyTest, BrowserConfiguredBaseline) {
  TestSystemProxyResolver resolver;
  mojo::Receiver<proxy_resolver::mojom::SystemProxyResolver> receiver(
      &resolver);
  builder_.SetMojoSystemProxyResolver(receiver.BindNewPipeAndPassRemote());
  if (GetParam()) {
    builder_.SetMojoProxyResolverFactory(
        test_mojo_proxy_resolver_factory_.CreateFactoryRemote());
  }
  net::ProxyConfig config;
  config.set_from_system(false);
  config.proxy_rules().ParseFromString("browser.test:8080");
  builder_.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation(config,
                                         TRAFFIC_ANNOTATION_FOR_TESTS)));
  std::unique_ptr<net::URLRequestContext> context = builder_.Build();
  ASSERT_TRUE(context->proxy_resolution_service());
  net::ConfiguredProxyResolutionService* configured_service = nullptr;
  ASSERT_TRUE(
      context->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&configured_service));

  net::ProxyInfo info;
  EXPECT_EQ(net::OK, ResolveProxySync(configured_service, &info));
  EXPECT_EQ("PROXY browser.test:8080", info.ToPacString());
  EXPECT_EQ(0, resolver.request_count());
  EXPECT_FALSE(test_mojo_proxy_resolver_factory_.resolver_created());
}

INSTANTIATE_TEST_SUITE_P(All,
                         URLRequestContextBuilderMojoSystemProxyTest,
                         testing::Bool());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Makes sure that pending PAC requests are correctly shut down during teardown.
TEST_F(URLRequestContextBuilderMojoTest, ShutdownWithHungRequest) {
  net::test_server::SimpleConnectionListener connection_listener(
      1, net::test_server::SimpleConnectionListener::
             FAIL_ON_ADDITIONAL_CONNECTIONS);
  test_server_.SetConnectionListener(&connection_listener);
  EXPECT_TRUE(test_server_.Start());

  builder_.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation(
              net::ProxyConfig::CreateFromCustomPacURL(
                  test_server_.GetURL("/hung")),
              TRAFFIC_ANNOTATION_FOR_TESTS)));
  builder_.SetMojoProxyResolverFactory(
      test_mojo_proxy_resolver_factory_.CreateFactoryRemote());

#if BUILDFLAG(IS_CHROMEOS)
  builder_.SetDhcpWpadUrlClient(
      MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(std::string()));
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<net::URLRequestContext> context(builder_.Build());
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request(context->CreateRequest(
      GURL("http://hats:12345/echoheader?Foo"), net::DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS,
      net::handles::kInvalidNetworkHandle));
  request->Start();
  connection_listener.WaitForConnections();

  // Tearing down the URLRequestContext should not cause an AssertNoURLRequests
  // failure.
  request.reset();
  context.reset();

  // Have to shut down the test server before |connection_listener| falls out of
  // scope.
  EXPECT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
}

}  // namespace

}  // namespace network
