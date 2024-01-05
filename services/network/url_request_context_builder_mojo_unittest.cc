// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_request_context_builder_mojo.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "net/base/host_port_pair.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/network/mock_mojo_dhcp_wpad_url_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  builder_.SetDhcpWpadUrlClient(
      MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(std::string()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<net::URLRequestContext> context(builder_.Build());
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request(context->CreateRequest(
      GURL("http://hats:12345/echoheader?Foo"), net::DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetExtraRequestHeaderByName("Foo", "Bar", false);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ("Bar", delegate.data_received());

  // Make sure that the Mojo factory was used.
  EXPECT_TRUE(test_mojo_proxy_resolver_factory_.resolver_created());
}

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  builder_.SetDhcpWpadUrlClient(
      MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(std::string()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<net::URLRequestContext> context(builder_.Build());
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request(context->CreateRequest(
      GURL("http://hats:12345/echoheader?Foo"), net::DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
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
