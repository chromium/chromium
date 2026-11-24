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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/test/gtest_util.h"
#include "services/network/proxy_config_service_mojo.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class TestSystemProxyResolver
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  TestSystemProxyResolver() { proxy_list_.Set("native.test:8080"); }
  explicit TestSystemProxyResolver(net::ProxyList proxy_list)
      : proxy_list_(std::move(proxy_list)) {}

  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override {
    ++request_count_;
    last_url_ = url;
    auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
    status->is_success = true;
    std::move(callback).Run(proxy_list_, std::move(status));
  }

  int request_count() const { return request_count_; }
  const GURL& last_url() const { return last_url_; }

 private:
  net::ProxyList proxy_list_;
  int request_count_ = 0;
  GURL last_url_;
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
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::string ResolveProxyForUrl(net::ProxyResolutionService* service,
                               const GURL& url) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolutionRequest> request;
  int rv = service->ResolveProxy(
      url, /*method=*/"GET", net::NetworkAnonymizationKey(),
      net::handles::kInvalidNetworkHandle, &info, callback.callback(), &request,
      net::NetLogWithSource(), net::DEFAULT_PRIORITY);
  EXPECT_THAT(callback.GetResult(rv), net::test::IsOk());
  return info.ToDebugString();
}

void UpdateAndFlushProxyConfig(
    mojo::Remote<mojom::ProxyConfigClient>& proxy_config_client,
    const net::ProxyConfig& config) {
  proxy_config_client->OnProxyConfigUpdated(
      net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  base::test::TestFuture<void> flushed;
  proxy_config_client->FlushProxyConfig(flushed.GetCallback());
  ASSERT_TRUE(flushed.Wait());
}

TEST_F(URLRequestContextBuilderMojoTest,
       SystemProxyResolverUsedOnlyForSystemConfigs) {
  ASSERT_TRUE(test_server_.Start());

  net::ProxyList portal_proxies;
  portal_proxies.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS portal-proxy:8443"));
  TestSystemProxyResolver resolver(portal_proxies);
  mojo::Receiver<proxy_resolver::mojom::SystemProxyResolver> receiver(
      &resolver);

  net::ProxyConfig system_config = net::ProxyConfig::CreateDirect();
  system_config.set_from_system(true);
  net::ProxyConfig::ProxyOverrideRule override_rule;
  override_rule.destination_matchers.ParseFromString("https://override.test");
  override_rule.proxy_list.SetSingleProxyServer(
      net::PacResultElementToProxyServer("PROXY override-proxy:8080"));
  system_config.set_proxy_override_rules({override_rule});

  mojo::Remote<mojom::ProxyConfigClient> proxy_config_client;
  builder_.set_proxy_config_service(std::make_unique<ProxyConfigServiceMojo>(
      proxy_config_client.BindNewPipeAndPassReceiver(),
      net::ProxyConfigWithAnnotation(system_config,
                                     TRAFFIC_ANNOTATION_FOR_TESTS),
      mojo::NullRemote()));
  builder_.SetMojoProxyResolverFactory(
      test_mojo_proxy_resolver_factory_.CreateFactoryRemote());
  builder_.SetMojoSystemProxyResolver(receiver.BindNewPipeAndPassRemote());

  std::unique_ptr<net::URLRequestContext> context(builder_.Build());
  net::ProxyResolutionService* proxy_service =
      context->proxy_resolution_service();

  // Enterprise ProxyOverrideRules apply before SystemProxyResolver, while
  // non-matching URLs delegate to SystemProxyResolver (with HTTPS path
  // sanitized to "/").
  EXPECT_EQ("PROXY override-proxy:8080",
            ResolveProxyForUrl(proxy_service, GURL("https://override.test/1")));
  EXPECT_EQ(0, resolver.request_count());
  EXPECT_EQ(
      "HTTPS portal-proxy:8443",
      ResolveProxyForUrl(proxy_service, GURL("https://a.example.test/path")));
  EXPECT_EQ(1, resolver.request_count());
  EXPECT_EQ(GURL("https://a.example.test/"), resolver.last_url());

  // Non-system fixed, direct, and custom PAC configs (from_system == false,
  // e.g. CLI switches, env vars, or policies) bypass SystemProxyResolver.
  net::ProxyConfig fixed_config;
  fixed_config.proxy_rules().ParseFromString("https=cli-proxy:8443");
  UpdateAndFlushProxyConfig(proxy_config_client, fixed_config);
  EXPECT_EQ("PROXY cli-proxy:8443",
            ResolveProxyForUrl(proxy_service, GURL("https://example.test/b")));
  EXPECT_EQ(1, resolver.request_count());

  UpdateAndFlushProxyConfig(proxy_config_client,
                            net::ProxyConfig::CreateDirect());
  EXPECT_EQ("DIRECT",
            ResolveProxyForUrl(proxy_service, GURL("https://example.test/c")));
  EXPECT_EQ(1, resolver.request_count());

  UpdateAndFlushProxyConfig(
      proxy_config_client,
      net::ProxyConfig::CreateFromCustomPacURL(test_server_.GetURL(kPacPath)));
  EXPECT_EQ("PROXY " + test_server_.host_port_pair().ToString(),
            ResolveProxyForUrl(proxy_service, GURL("https://example.test/d")));
  EXPECT_TRUE(test_mojo_proxy_resolver_factory_.resolver_created());
  EXPECT_EQ(1, resolver.request_count());

  // Reverting to a system-origin config resumes delegating to
  // SystemProxyResolver.
  UpdateAndFlushProxyConfig(proxy_config_client, system_config);
  EXPECT_EQ(
      "HTTPS portal-proxy:8443",
      ResolveProxyForUrl(proxy_service, GURL("https://e.example.test/path")));
  EXPECT_EQ(2, resolver.request_count());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace

}  // namespace network
