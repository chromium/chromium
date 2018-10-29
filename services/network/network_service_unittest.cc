// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/escape.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_scheme.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/test/test_network_service_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_KERBEROS)
#include "net/http/http_auth_handler_negotiate.h"
#endif

namespace network {

namespace {

const char kNetworkServiceName[] = "network";

const base::FilePath::CharType kServicesTestData[] =
    FILE_PATH_LITERAL("services/test/data");

// Returns a new URL with key=value pair added to the query.
GURL AddQuery(const GURL& url,
              const std::string& key,
              const std::string& value) {
  return GURL(url.spec() + (url.has_query() ? "&" : "?") + key + "=" +
              net::EscapeQueryParamValue(value, false));
}

mojom::NetworkContextParamsPtr CreateContextParams() {
  mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

class NetworkServiceTest : public testing::Test {
 public:
  NetworkServiceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        service_(NetworkService::CreateForTesting()) {}
  ~NetworkServiceTest() override {}

  NetworkService* service() const { return service_.get(); }

  void DestroyService() { service_.reset(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkService> service_;
};

// Test shutdown in the case a NetworkContext is destroyed before the
// NetworkService.
TEST_F(NetworkServiceTest, CreateAndDestroyContext) {
  mojom::NetworkContextPtr network_context;
  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  CreateContextParams());
  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

// Test shutdown in the case there is still a live NetworkContext when the
// NetworkService is destroyed. The service should destroy the NetworkContext
// itself.
TEST_F(NetworkServiceTest, DestroyingServiceDestroysContext) {
  mojom::NetworkContextPtr network_context;
  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  CreateContextParams());
  base::RunLoop run_loop;
  network_context.set_connection_error_handler(run_loop.QuitClosure());
  DestroyService();

  // Destroying the service should destroy the context, causing a connection
  // error.
  run_loop.Run();
}

TEST_F(NetworkServiceTest, CreateContextWithoutChannelID) {
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->cookie_path = base::FilePath();
  params->enable_encrypted_cookies = false;
  mojom::NetworkContextPtr network_context;
  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  std::move(params));
  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

// Platforms where Negotiate can be used.
#if BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)
// Returns the negotiate factory, if one exists, to query its configuration.
net::HttpAuthHandlerNegotiate::Factory* GetNegotiateFactory(
    NetworkContext* network_context) {
  net::HttpAuthHandlerFactory* auth_factory =
      network_context->url_request_context()->http_auth_handler_factory();
  return reinterpret_cast<net::HttpAuthHandlerNegotiate::Factory*>(
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(auth_factory)
          ->GetSchemeFactory(net::kNegotiateAuthScheme));
}

#endif  // BUILDFLAG(USE_KERBEROS)

TEST_F(NetworkServiceTest, AuthDefaultParams) {
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory);

  // These three factories should always be created by default.  Negotiate may
  // or may not be created, depending on other build flags.
  EXPECT_TRUE(auth_handler_factory->GetSchemeFactory(net::kBasicAuthScheme));
  EXPECT_TRUE(auth_handler_factory->GetSchemeFactory(net::kDigestAuthScheme));
  EXPECT_TRUE(auth_handler_factory->GetSchemeFactory(net::kNtlmAuthScheme));

#if BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)
  ASSERT_TRUE(GetNegotiateFactory(&network_context));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(GetNegotiateFactory(&network_context)
                  ->allow_gssapi_library_load_for_testing());
#elif defined(OS_POSIX)
  EXPECT_EQ("",
            GetNegotiateFactory(&network_context)->GetLibraryNameForTesting());
#endif
#endif  // BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)

  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()
                   ->NegotiateDisableCnameLookup());
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->NegotiateEnablePort());
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->NtlmV2Enabled());
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)
#if defined(OS_ANDROID)
  EXPECT_EQ("", auth_handler_factory->http_auth_preferences()
                    ->AuthAndroidNegotiateAccountType());
#endif  // defined(OS_ANDROID)
}

TEST_F(NetworkServiceTest, AuthSchemesDigestAndNtlmOnly) {
  mojom::HttpAuthStaticParamsPtr auth_params =
      mojom::HttpAuthStaticParams::New();
  auth_params->supported_schemes.push_back("digest");
  auth_params->supported_schemes.push_back("ntlm");
  service()->SetUpHttpAuth(std::move(auth_params));

  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory);

  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kBasicAuthScheme));
  EXPECT_TRUE(auth_handler_factory->GetSchemeFactory(net::kDigestAuthScheme));
  EXPECT_TRUE(auth_handler_factory->GetSchemeFactory(net::kNtlmAuthScheme));
  EXPECT_FALSE(
      auth_handler_factory->GetSchemeFactory(net::kNegotiateAuthScheme));
}

TEST_F(NetworkServiceTest, AuthSchemesNone) {
  // An empty list means to support no schemes.
  service()->SetUpHttpAuth(mojom::HttpAuthStaticParams::New());

  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory);

  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kBasicAuthScheme));
  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kDigestAuthScheme));
  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kNtlmAuthScheme));
}

// |allow_gssapi_library_load| is only supported on ChromeOS.
#if defined(OS_CHROMEOS)
TEST_F(NetworkServiceTest, AuthGssapiLibraryDisabled) {
  mojom::HttpAuthStaticParamsPtr auth_params =
      mojom::HttpAuthStaticParams::New();
  auth_params->supported_schemes.push_back("negotiate");
  auth_params->allow_gssapi_library_load = true;
  service()->SetUpHttpAuth(std::move(auth_params));

  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  ASSERT_TRUE(GetNegotiateFactory(&network_context));
  EXPECT_TRUE(GetNegotiateFactory(&network_context)
                  ->allow_gssapi_library_load_for_testing());
}
#endif  // defined(OS_CHROMEOS)

// |gssapi_library_name| is only supported on certain POSIX platforms.
#if BUILDFLAG(USE_KERBEROS) && defined(OS_POSIX) && !defined(OS_ANDROID) && \
    !defined(OS_CHROMEOS)
TEST_F(NetworkServiceTest, AuthGssapiLibraryName) {
  const std::string kGssapiLibraryName = "Jim";
  mojom::HttpAuthStaticParamsPtr auth_params =
      mojom::HttpAuthStaticParams::New();
  auth_params->supported_schemes.push_back("negotiate");
  auth_params->gssapi_library_name = kGssapiLibraryName;
  service()->SetUpHttpAuth(std::move(auth_params));

  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  ASSERT_TRUE(GetNegotiateFactory(&network_context));
  EXPECT_EQ(kGssapiLibraryName,
            GetNegotiateFactory(&network_context)->GetLibraryNameForTesting());
}
#endif

TEST_F(NetworkServiceTest, AuthServerWhitelist) {
  // Add one server to the whitelist before creating any NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->server_whitelist = "server1";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the whitelist.
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server1/")));
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server2/")));

  // Change whitelist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->server_whitelist = "server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server1/")));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server2/")));

  // Change whitelist to have multiple servers. The pre-existing NetworkContext
  // should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->server_whitelist = "server1,server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server1/")));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server2/")));
}

TEST_F(NetworkServiceTest, AuthDelegateWhitelist) {
  // Add one server to the whitelist before creating any NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_whitelist = "server1";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the whitelist.
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->CanDelegate(
      GURL("https://server1/")));
  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()->CanDelegate(
      GURL("https://server2/")));

  // Change whitelist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_whitelist = "server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()->CanDelegate(
      GURL("https://server1/")));
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->CanDelegate(
      GURL("https://server2/")));

  // Change whitelist to have multiple servers. The pre-existing NetworkContext
  // should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_whitelist = "server1,server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->CanDelegate(
      GURL("https://server1/")));
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->CanDelegate(
      GURL("https://server2/")));
}

TEST_F(NetworkServiceTest, AuthNegotiateCnameLookup) {
  // Set |negotiate_disable_cname_lookup| to true before creating any
  // NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->negotiate_disable_cname_lookup = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the setting.
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()
                  ->NegotiateDisableCnameLookup());

  // Set it to false. The pre-existing NetworkContext should be using the new
  // setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->negotiate_disable_cname_lookup = false;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()
                   ->NegotiateDisableCnameLookup());

  // Set it back to true. The pre-existing NetworkContext should be using the
  // new setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->negotiate_disable_cname_lookup = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()
                  ->NegotiateDisableCnameLookup());
}

TEST_F(NetworkServiceTest, AuthEnableNegotiatePort) {
  // Set |enable_negotiate_port| to true before creating any NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->enable_negotiate_port = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the setting.
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->NegotiateEnablePort());

  // Set it to false. The pre-existing NetworkContext should be using the new
  // setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->enable_negotiate_port = false;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->NegotiateEnablePort());

  // Set it back to true. The pre-existing NetworkContext should be using the
  // new setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->enable_negotiate_port = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->NegotiateEnablePort());
}

// DnsClient isn't supported on iOS.
#if !defined(OS_IOS)

TEST_F(NetworkServiceTest, DnsClientEnableDisable) {
  // HostResolver::GetDnsConfigAsValue() returns nullptr if the stub resolver is
  // disabled.
  EXPECT_FALSE(service()->host_resolver()->GetDnsConfigAsValue());
  service()->ConfigureStubHostResolver(
      true /* stub_resolver_enabled */,
      base::nullopt /* dns_over_https_servers */);
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  service()->ConfigureStubHostResolver(
      false /* stub_resolver_enabled */,
      base::nullopt /* dns_over_https_servers */);
  EXPECT_FALSE(service()->host_resolver()->GetDnsConfigAsValue());
}

TEST_F(NetworkServiceTest, DnsOverHttpsEnableDisable) {
  const std::string kServer1 = "https://foo/";
  const bool kServer1UsePost = false;
  const std::string kServer2 = "https://bar/dns-query{?dns}";
  const bool kServer2UsePost = true;
  const std::string kServer3 = "https://grapefruit/resolver/query{?dns}";
  const bool kServer3UsePost = false;

  // HostResolver::GetDnsClientForTesting() returns nullptr if the stub resolver
  // is disabled.
  EXPECT_FALSE(service()->host_resolver()->GetDnsConfigAsValue());

  // Create the primary NetworkContext before enabling DNS over HTTPS.
  mojom::NetworkContextPtr network_context;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->primary_network_context = true;
  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  std::move(context_params));

  // Enable DNS over HTTPS for one server.

  std::vector<mojom::DnsOverHttpsServerPtr> dns_over_https_servers_ptr;

  mojom::DnsOverHttpsServerPtr dns_over_https_server =
      mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = kServer1;
  dns_over_https_server->use_post = kServer1UsePost;
  dns_over_https_servers_ptr.emplace_back(std::move(dns_over_https_server));

  service()->ConfigureStubHostResolver(true /* stub_resolver_enabled */,
                                       std::move(dns_over_https_servers_ptr));
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  const auto* dns_over_https_servers =
      service()->host_resolver()->GetDnsOverHttpsServersForTesting();
  ASSERT_TRUE(dns_over_https_servers);
  ASSERT_EQ(1u, dns_over_https_servers->size());
  EXPECT_EQ(kServer1, (*dns_over_https_servers)[0].server_template);
  EXPECT_EQ(kServer1UsePost, (*dns_over_https_servers)[0].use_post);

  // Enable DNS over HTTPS for two servers.

  dns_over_https_servers_ptr.clear();
  dns_over_https_server = mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = kServer2;
  dns_over_https_server->use_post = kServer2UsePost;
  dns_over_https_servers_ptr.emplace_back(std::move(dns_over_https_server));

  dns_over_https_server = mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = kServer3;
  dns_over_https_server->use_post = kServer3UsePost;
  dns_over_https_servers_ptr.emplace_back(std::move(dns_over_https_server));

  service()->ConfigureStubHostResolver(true /* stub_resolver_enabled */,
                                       std::move(dns_over_https_servers_ptr));
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  dns_over_https_servers =
      service()->host_resolver()->GetDnsOverHttpsServersForTesting();
  ASSERT_TRUE(dns_over_https_servers);
  ASSERT_EQ(2u, dns_over_https_servers->size());
  EXPECT_EQ(kServer2, (*dns_over_https_servers)[0].server_template);
  EXPECT_EQ(kServer2UsePost, (*dns_over_https_servers)[0].use_post);
  EXPECT_EQ(kServer3, (*dns_over_https_servers)[1].server_template);
  EXPECT_EQ(kServer3UsePost, (*dns_over_https_servers)[1].use_post);

  // Destroying the primary NetworkContext should disable DNS over HTTPS.
  network_context.reset();
  base::RunLoop().RunUntilIdle();
  // DnsClient is still enabled.
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  // DNS over HTTPS is not.
  EXPECT_FALSE(service()->host_resolver()->GetDnsOverHttpsServersForTesting());
}

// Make sure that enabling DNS over HTTP without a primary NetworkContext fails.
TEST_F(NetworkServiceTest,
       DnsOverHttpsEnableDoesNothingWithoutPrimaryNetworkContext) {
  // HostResolver::GetDnsClientForTesting() returns nullptr if the stub resolver
  // is disabled.
  EXPECT_FALSE(service()->host_resolver()->GetDnsConfigAsValue());

  // Try to enable DnsClient and DNS over HTTPS. Only the first should take
  // effect.
  std::vector<mojom::DnsOverHttpsServerPtr> dns_over_https_servers;
  mojom::DnsOverHttpsServerPtr dns_over_https_server =
      mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = "https://foo/{?dns}";
  dns_over_https_servers.emplace_back(std::move(dns_over_https_server));
  service()->ConfigureStubHostResolver(true /* stub_resolver_enabled */,
                                       std::move(dns_over_https_servers));
  // DnsClient is enabled.
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  // DNS over HTTPS is not.
  EXPECT_FALSE(service()->host_resolver()->GetDnsOverHttpsServersForTesting());

  // Create a NetworkContext that is not the primary one.
  mojom::NetworkContextPtr network_context;
  service()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                  CreateContextParams());
  // There should be no change in host resolver state.
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  EXPECT_FALSE(service()->host_resolver()->GetDnsOverHttpsServersForTesting());

  // Try to enable DNS over HTTPS again, which should not work, since there's
  // still no primary NetworkContext.
  dns_over_https_servers.clear();
  dns_over_https_server = mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = "https://foo2/{?dns}";
  dns_over_https_servers.emplace_back(std::move(dns_over_https_server));
  service()->ConfigureStubHostResolver(true /* stub_resolver_enabled */,
                                       std::move(dns_over_https_servers));
  // There should be no change in host resolver state.
  EXPECT_TRUE(service()->host_resolver()->GetDnsConfigAsValue());
  EXPECT_FALSE(service()->host_resolver()->GetDnsOverHttpsServersForTesting());
}

#endif  // !defined(OS_IOS)

// |ntlm_v2_enabled| is only supported on POSIX platforms.
#if defined(OS_POSIX)
TEST_F(NetworkServiceTest, AuthNtlmV2Enabled) {
  // Set |ntlm_v2_enabled| to false before creating any NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->ntlm_v2_enabled = false;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the setting.
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()->NtlmV2Enabled());

  // Set it to true. The pre-existing NetworkContext should be using the new
  // setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->ntlm_v2_enabled = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->NtlmV2Enabled());

  // Set it back to false. The pre-existing NetworkContext should be using the
  // new setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->ntlm_v2_enabled = false;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()->NtlmV2Enabled());
}
#endif  // defined(OS_POSIX)

// |android_negotiate_account_type| is only supported on Android.
#if defined(OS_ANDROID)
TEST_F(NetworkServiceTest, AuthAndroidNegotiateAccountType) {
  const char kInitialAccountType[] = "Scorpio";
  const char kFinalAccountType[] = "Pisces";
  // Set |android_negotiate_account_type| to before creating any
  // NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->android_negotiate_account_type = kInitialAccountType;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the setting.
  mojom::NetworkContextPtr network_context_ptr;
  NetworkContext network_context(service(),
                                 mojo::MakeRequest(&network_context_ptr),
                                 CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_EQ(kInitialAccountType, auth_handler_factory->http_auth_preferences()
                                     ->AuthAndroidNegotiateAccountType());

  // Change |android_negotiate_account_type|. The pre-existing NetworkContext
  // should be using the new setting.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->android_negotiate_account_type = kFinalAccountType;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_EQ(kFinalAccountType, auth_handler_factory->http_auth_preferences()
                                   ->AuthAndroidNegotiateAccountType());
}
#endif  // defined(OS_ANDROID)

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  explicit ServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(base::Bind(
        &ServiceTestClient::BindServiceFactoryRequest, base::Unretained(this)));
  }
  ~ServiceTestClient() override {}

 protected:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    if (name == kNetworkServiceName) {
      service_context_.reset(new service_manager::ServiceContext(
          NetworkService::CreateForTesting(), std::move(request)));
    }
  }

  void BindServiceFactoryRequest(
      service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  std::unique_ptr<service_manager::ServiceContext> service_context_;

 private:
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
};

class NetworkServiceTestWithService
    : public service_manager::test::ServiceTest {
 public:
  NetworkServiceTestWithService()
      : ServiceTest("network_unittests",
                    base::test::ScopedTaskEnvironment::MainThreadType::IO) {}
  ~NetworkServiceTestWithService() override {}

  void CreateNetworkContext() {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    network_service_->CreateNetworkContext(mojo::MakeRequest(&network_context_),
                                           std::move(context_params));
  }

  void LoadURL(const GURL& url, int options = mojom::kURLLoadOptionNone) {
    ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();
    StartLoadingURL(request, 0 /* process_id */, options);
    client_->RunUntilComplete();
  }

  void StartLoadingURL(const ResourceRequest& request,
                       uint32_t process_id,
                       int options = mojom::kURLLoadOptionNone) {
    client_.reset(new TestURLLoaderClient());
    mojom::URLLoaderFactoryPtr loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                             std::move(params));

    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader_), 1, 1, options, request,
        client_->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  TestURLLoaderClient* client() { return client_.get(); }
  mojom::URLLoader* loader() { return loader_.get(); }
  mojom::NetworkService* service() { return network_service_.get(); }
  mojom::NetworkContext* context() { return network_context_.get(); }

 protected:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return std::make_unique<ServiceTestClient>(this);
  }

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kServicesTestData));
    ASSERT_TRUE(test_server_.Start());
    service_manager::test::ServiceTest::SetUp();
    connector()->BindInterface(kNetworkServiceName, &network_service_);
  }

  net::EmbeddedTestServer test_server_;
  std::unique_ptr<TestURLLoaderClient> client_;
  mojom::NetworkServicePtr network_service_;
  mojom::NetworkContextPtr network_context_;
  mojom::URLLoaderPtr loader_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestWithService);
};

// Verifies that loading a URL through the network service's mojo interface
// works.
TEST_F(NetworkServiceTestWithService, Basic) {
  CreateNetworkContext();
  LoadURL(test_server()->GetURL("/echo"));
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

// Verifies that a passed net log file is successfully opened and sane data
// written to it.
TEST_F(NetworkServiceTestWithService, StartsNetLog) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_dir = temp_dir.GetPath();
  base::FilePath log_path = log_dir.Append(FILE_PATH_LITERAL("test_log.json"));

  base::DictionaryValue dict;
  dict.SetString("amiatest", "iamatest");

  base::File log_file(log_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  network_service_->StartNetLog(std::move(log_file),
                                network::mojom::NetLogCaptureMode::DEFAULT,
                                std::move(dict));
  CreateNetworkContext();
  LoadURL(test_server()->GetURL("/echo"));
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  // |log_file| is closed on destruction of the NetworkService.
  Shutdown();

  // |log_file| is closed on another thread, so have to wait for that to happen.
  RunUntilIdle();

  JSONFileValueDeserializer deserializer(log_path);
  std::unique_ptr<base::Value> log_dict =
      deserializer.Deserialize(nullptr, nullptr);
  ASSERT_TRUE(log_dict);
  ASSERT_EQ(log_dict->FindKey("constants")->FindKey("amiatest")->GetString(),
            "iamatest");
}

// Verifies that raw headers are only reported if requested.
TEST_F(NetworkServiceTestWithService, RawRequestHeadersAbsent) {
  CreateNetworkContext();
  ResourceRequest request;
  request.url = test_server()->GetURL("/server-redirect?/echo");
  request.method = "GET";
  request.request_initiator = url::Origin();
  StartLoadingURL(request, 0);
  client()->RunUntilRedirectReceived();
  EXPECT_TRUE(client()->has_received_redirect());
  EXPECT_TRUE(!client()->response_head().raw_request_response_info);
  loader()->FollowRedirect(base::nullopt, base::nullopt);
  client()->RunUntilComplete();
  EXPECT_TRUE(!client()->response_head().raw_request_response_info);
}

TEST_F(NetworkServiceTestWithService, RawRequestHeadersPresent) {
  CreateNetworkContext();
  ResourceRequest request;
  request.url = test_server()->GetURL("/server-redirect?/echo");
  request.method = "GET";
  request.report_raw_headers = true;
  request.request_initiator = url::Origin();
  StartLoadingURL(request, 0);
  client()->RunUntilRedirectReceived();
  EXPECT_TRUE(client()->has_received_redirect());
  {
    scoped_refptr<HttpRawRequestResponseInfo> request_response_info =
        client()->response_head().raw_request_response_info;
    ASSERT_TRUE(request_response_info);
    EXPECT_EQ(301, request_response_info->http_status_code);
    EXPECT_EQ("Moved Permanently", request_response_info->http_status_text);
    EXPECT_TRUE(base::StartsWith(request_response_info->request_headers_text,
                                 "GET /server-redirect?/echo HTTP/1.1\r\n",
                                 base::CompareCase::SENSITIVE));
    EXPECT_GE(request_response_info->request_headers.size(), 1lu);
    EXPECT_GE(request_response_info->response_headers.size(), 1lu);
    EXPECT_TRUE(base::StartsWith(request_response_info->response_headers_text,
                                 "HTTP/1.1 301 Moved Permanently\r",
                                 base::CompareCase::SENSITIVE));
  }
  loader()->FollowRedirect(base::nullopt, base::nullopt);
  client()->RunUntilComplete();
  {
    scoped_refptr<HttpRawRequestResponseInfo> request_response_info =
        client()->response_head().raw_request_response_info;
    EXPECT_EQ(200, request_response_info->http_status_code);
    EXPECT_EQ("OK", request_response_info->http_status_text);
    EXPECT_TRUE(base::StartsWith(request_response_info->request_headers_text,
                                 "GET /echo HTTP/1.1\r\n",
                                 base::CompareCase::SENSITIVE));
    EXPECT_GE(request_response_info->request_headers.size(), 1lu);
    EXPECT_GE(request_response_info->response_headers.size(), 1lu);
    EXPECT_TRUE(base::StartsWith(request_response_info->response_headers_text,
                                 "HTTP/1.1 200 OK\r",
                                 base::CompareCase::SENSITIVE));
  }
}

TEST_F(NetworkServiceTestWithService, RawRequestAccessControl) {
  const uint32_t process_id = 42;
  CreateNetworkContext();
  ResourceRequest request;
  request.url = test_server()->GetURL("/nocache.html");
  request.method = "GET";
  request.report_raw_headers = true;
  request.request_initiator = url::Origin();

  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  EXPECT_FALSE(client()->response_head().raw_request_response_info);
  service()->SetRawHeadersAccess(process_id, true);
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  {
    scoped_refptr<HttpRawRequestResponseInfo> request_response_info =
        client()->response_head().raw_request_response_info;
    ASSERT_TRUE(request_response_info);
    EXPECT_EQ(200, request_response_info->http_status_code);
    EXPECT_EQ("OK", request_response_info->http_status_text);
  }

  service()->SetRawHeadersAccess(process_id, false);
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  EXPECT_FALSE(client()->response_head().raw_request_response_info.get());
}

TEST_F(NetworkServiceTestWithService, SetNetworkConditions) {
  const base::UnguessableToken profile_id = base::UnguessableToken::Create();
  CreateNetworkContext();
  mojom::NetworkConditionsPtr network_conditions =
      mojom::NetworkConditions::New();
  network_conditions->offline = true;
  context()->SetNetworkConditions(profile_id, std::move(network_conditions));

  ResourceRequest request;
  request.url = test_server()->GetURL("/nocache.html");
  request.method = "GET";

  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  request.throttling_profile_id = profile_id;
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_INTERNET_DISCONNECTED,
            client()->completion_status().error_code);

  network_conditions = mojom::NetworkConditions::New();
  network_conditions->offline = false;
  context()->SetNetworkConditions(profile_id, std::move(network_conditions));
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  network_conditions = mojom::NetworkConditions::New();
  network_conditions->offline = true;
  context()->SetNetworkConditions(profile_id, std::move(network_conditions));

  request.throttling_profile_id = profile_id;
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_INTERNET_DISCONNECTED,
            client()->completion_status().error_code);
  context()->SetNetworkConditions(profile_id, nullptr);
  StartLoadingURL(request, 0);
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

// CRLSets are not supported on iOS and Android system verifiers.
#if !defined(OS_IOS) && !defined(OS_ANDROID)

// Verifies CRLSets take effect if configured on the service.
TEST_F(NetworkServiceTestWithService, CRLSetIsApplied) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  test_server.AddDefaultHandlers(base::FilePath(kServicesTestData));
  ASSERT_TRUE(test_server.Start());

  CreateNetworkContext();

  uint32_t options = mojom::kURLLoadOptionSendSSLInfoWithResponse |
                     mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  // Make sure the test server loads fine with no CRLSet.
  LoadURL(test_server.GetURL("/echo"), options);
  ASSERT_EQ(net::OK, client()->completion_status().error_code);

  // Send a CRLSet that blocks the leaf cert.
  std::string crl_set_bytes;
  EXPECT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
      &crl_set_bytes));

  service()->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
  network_service_.FlushForTesting();

  // Flush all connections in the context, to force a new connection. A new
  // verification should be attempted, due to the configuration having
  // changed, thus forcing the CRLSet to be checked.
  base::RunLoop run_loop;
  context()->CloseAllConnections(run_loop.QuitClosure());
  run_loop.Run();

  // Make sure the connection fails, due to the certificate being revoked.
  LoadURL(test_server.GetURL("/echo"), options);
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
            client()->completion_status().error_code);
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info->cert_status &
              net::CERT_STATUS_REVOKED);
}

// Verifies CRLSets configured before creating a new network context are
// applied to that network context.
TEST_F(NetworkServiceTestWithService, CRLSetIsPassedToNewContexts) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  test_server.AddDefaultHandlers(base::FilePath(kServicesTestData));
  ASSERT_TRUE(test_server.Start());

  // Send a CRLSet that blocks the leaf cert, even while no NetworkContexts
  // exist.
  std::string crl_set_bytes;
  EXPECT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
      &crl_set_bytes));

  service()->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
  network_service_.FlushForTesting();

  // Configure a new NetworkContext.
  CreateNetworkContext();

  uint32_t options = mojom::kURLLoadOptionSendSSLInfoWithResponse |
                     mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  // Make sure the connection fails, due to the certificate being revoked.
  LoadURL(test_server.GetURL("/echo"), options);
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
            client()->completion_status().error_code);
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info->cert_status &
              net::CERT_STATUS_REVOKED);
}

// Verifies newer CRLSets (by sequence number) are applied.
TEST_F(NetworkServiceTestWithService, CRLSetIsUpdatedIfNewer) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  test_server.AddDefaultHandlers(base::FilePath(kServicesTestData));
  ASSERT_TRUE(test_server.Start());

  // Send a CRLSet that only allows the root cert if it matches a known SPKI
  // hash (that matches the test server chain)
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("crlset_by_root_subject.raw"),
      &crl_set_bytes));

  service()->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
  network_service_.FlushForTesting();

  CreateNetworkContext();

  uint32_t options = mojom::kURLLoadOptionSendSSLInfoWithResponse |
                     mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  // Make sure the connection loads, due to the root being whitelisted.
  LoadURL(test_server.GetURL("/echo"), options);
  ASSERT_EQ(net::OK, client()->completion_status().error_code);

  // Send a new CRLSet that removes trust in the root.
  ASSERT_TRUE(base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                                         "crlset_by_root_subject_no_spki.raw"),
                                     &crl_set_bytes));

  service()->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
  network_service_.FlushForTesting();

  // Flush all connections in the context, to force a new connection. A new
  // verification should be attempted, due to the configuration having
  // changed, thus forcing the CRLSet to be checked.
  base::RunLoop run_loop;
  context()->CloseAllConnections(run_loop.QuitClosure());
  run_loop.Run();

  // Make sure the connection fails, due to the certificate being revoked.
  LoadURL(test_server.GetURL("/echo"), options);
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
            client()->completion_status().error_code);
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info->cert_status &
              net::CERT_STATUS_REVOKED);
}

// Verifies that attempting to send an older CRLSet (by sequence number)
// does not apply to existing or new contexts.
TEST_F(NetworkServiceTestWithService, CRLSetDoesNotDowngrade) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  test_server.AddDefaultHandlers(base::FilePath(kServicesTestData));
  ASSERT_TRUE(test_server.Start());

  // Send a CRLSet that blocks the root certificate by subject name.
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                                         "crlset_by_root_subject_no_spki.raw"),
                                     &crl_set_bytes));

  service()->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
  network_service_.FlushForTesting();

  CreateNetworkContext();

  uint32_t options = mojom::kURLLoadOptionSendSSLInfoWithResponse |
                     mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  // Make sure the connection fails, due to the certificate being revoked.
  LoadURL(test_server.GetURL("/echo"), options);
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
            client()->completion_status().error_code);
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info->cert_status &
              net::CERT_STATUS_REVOKED);

  // Attempt to configure an older CRLSet that allowed trust in the root.
  ASSERT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("crlset_by_root_subject.raw"),
      &crl_set_bytes));

  service()->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
  network_service_.FlushForTesting();

  // Flush all connections in the context, to force a new connection. A new
  // verification should be attempted, due to the configuration having
  // changed, thus forcing the CRLSet to be checked.
  base::RunLoop run_loop;
  context()->CloseAllConnections(run_loop.QuitClosure());
  run_loop.Run();

  // Make sure the connection still fails, due to the newer CRLSet still
  // applying.
  LoadURL(test_server.GetURL("/echo"), options);
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
            client()->completion_status().error_code);
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info->cert_status &
              net::CERT_STATUS_REVOKED);

  // Create a new NetworkContext and ensure the latest CRLSet is still
  // applied.
  network_context_.reset();
  CreateNetworkContext();

  // The newer CRLSet that blocks the connection should still apply, even to
  // new NetworkContexts.
  LoadURL(test_server.GetURL("/echo"), options);
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
            client()->completion_status().error_code);
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info->cert_status &
              net::CERT_STATUS_REVOKED);
}

#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)

// The SpawnedTestServer does not work on iOS.
#if !defined(OS_IOS)

// Test |primary_network_context|, which is required by AIA fetching, among
// other things.
TEST_F(NetworkServiceTestWithService, AIAFetching) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->primary_network_context = true;

  network_service_->CreateNetworkContext(mojo::MakeRequest(&network_context_),
                                         std::move(context_params));

  net::SpawnedTestServer::SSLOptions ssl_options(
      net::SpawnedTestServer::SSLOptions::CERT_AUTO_AIA_INTERMEDIATE);
  net::SpawnedTestServer test_server(net::SpawnedTestServer::TYPE_HTTPS,
                                     ssl_options,
                                     base::FilePath(kServicesTestData));
  ASSERT_TRUE(test_server.Start());

  LoadURL(test_server.GetURL("/echo"),
          mojom::kURLLoadOptionSendSSLInfoWithResponse);
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
  EXPECT_EQ(
      0u, client()->response_head().cert_status & net::CERT_STATUS_ALL_ERRORS);
  ASSERT_TRUE(client()->ssl_info());
  ASSERT_TRUE(client()->ssl_info()->cert);
  EXPECT_EQ(2u, client()->ssl_info()->cert->intermediate_buffers().size());
  ASSERT_TRUE(client()->ssl_info()->unverified_cert);
  EXPECT_EQ(
      0u, client()->ssl_info()->unverified_cert->intermediate_buffers().size());
}
#endif  // !defined(OS_IOS)

// Check that destroying a NetworkContext with |primary_network_context| set
// destroys all other NetworkContexts.
TEST_F(NetworkServiceTestWithService,
       DestroyingPrimaryNetworkContextDestroysOtherContexts) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->primary_network_context = true;
  mojom::NetworkContextPtr cert_validating_network_context;
  network_service_->CreateNetworkContext(
      mojo::MakeRequest(&cert_validating_network_context),
      std::move(context_params));

  base::RunLoop run_loop;
  mojom::NetworkContextPtr network_context;
  network_service_->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                         CreateContextParams());
  network_context.set_connection_error_handler(run_loop.QuitClosure());

  // Wait until the new NetworkContext has been created, so it's not created
  // after the primary NetworkContext is destroyed.
  network_service_.FlushForTesting();

  // Destroying |cert_validating_network_context| should result in destroying
  // |network_context| as well.
  cert_validating_network_context.reset();
  run_loop.Run();
  EXPECT_TRUE(network_context.encountered_error());
}

class TestNetworkChangeManagerClient
    : public mojom::NetworkChangeManagerClient {
 public:
  explicit TestNetworkChangeManagerClient(
      mojom::NetworkService* network_service)
      : connection_type_(mojom::ConnectionType::CONNECTION_UNKNOWN),
        binding_(this) {
    mojom::NetworkChangeManagerPtr manager_ptr;
    mojom::NetworkChangeManagerRequest request(mojo::MakeRequest(&manager_ptr));
    network_service->GetNetworkChangeManager(std::move(request));

    mojom::NetworkChangeManagerClientPtr client_ptr;
    mojom::NetworkChangeManagerClientRequest client_request(
        mojo::MakeRequest(&client_ptr));
    binding_.Bind(std::move(client_request));
    manager_ptr->RequestNotifications(std::move(client_ptr));
  }

  ~TestNetworkChangeManagerClient() override {}

  // NetworkChangeManagerClient implementation:
  void OnInitialConnectionType(mojom::ConnectionType type) override {
    if (type == connection_type_)
      run_loop_.Quit();
  }

  void OnNetworkChanged(mojom::ConnectionType type) override {
    if (type == connection_type_)
      run_loop_.Quit();
  }

  // Waits for the desired |connection_type| notification.
  void WaitForNotification(mojom::ConnectionType type) {
    connection_type_ = type;
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  mojom::ConnectionType connection_type_;
  mojo::Binding<mojom::NetworkChangeManagerClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeManagerClient);
};

class NetworkChangeTest : public testing::Test {
 public:
  NetworkChangeTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    service_ = NetworkService::CreateForTesting();
  }

  ~NetworkChangeTest() override {}

  NetworkService* service() const { return service_.get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
#if defined(OS_ANDROID)
  // On Android, NetworkChangeNotifier setup is more involved and needs to
  // to be split between UI thread and network thread. Use a mock
  // NetworkChangeNotifier in tests, so the test setup is simpler.
  net::test::MockNetworkChangeNotifier network_change_notifier_;
#endif
  std::unique_ptr<NetworkService> service_;
};

// mojom:NetworkChangeManager isn't supported on these platforms.
// See the same ifdef in CreateNetworkChangeNotifierIfNeeded.
#if defined(OS_CHROMEOS) || defined(OS_FUCHSIA) || defined(OS_IOS)
#define MAYBE_NetworkChangeManagerRequest DISABLED_NetworkChangeManagerRequest
#else
#define MAYBE_NetworkChangeManagerRequest NetworkChangeManagerRequest
#endif
TEST_F(NetworkChangeTest, MAYBE_NetworkChangeManagerRequest) {
  TestNetworkChangeManagerClient manager_client(service());
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_3G);
  manager_client.WaitForNotification(mojom::ConnectionType::CONNECTION_3G);
}

class NetworkServiceNetworkChangeTest
    : public service_manager::test::ServiceTest {
 public:
  NetworkServiceNetworkChangeTest()
      : ServiceTest("network_unittests",
                    base::test::ScopedTaskEnvironment::MainThreadType::IO) {}
  ~NetworkServiceNetworkChangeTest() override {}

  mojom::NetworkService* service() { return network_service_.get(); }

 private:
  // A ServiceTestClient that broadcasts a network change notification in the
  // network service's process.
  class ServiceTestClientWithNetworkChange : public ServiceTestClient {
   public:
    explicit ServiceTestClientWithNetworkChange(
        service_manager::test::ServiceTest* test)
        : ServiceTestClient(test) {}
    ~ServiceTestClientWithNetworkChange() override {}

   protected:
    void CreateService(
        service_manager::mojom::ServiceRequest request,
        const std::string& name,
        service_manager::mojom::PIDReceiverPtr pid_receiver) override {
      if (name == kNetworkServiceName) {
        service_context_.reset(new service_manager::ServiceContext(
            NetworkService::CreateForTesting(), std::move(request)));
        // Send a broadcast after NetworkService is actually created.
        // Otherwise, this NotifyObservers is a no-op.
        net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
            net::NetworkChangeNotifier::CONNECTION_3G);
      }
    }
  };
  std::unique_ptr<service_manager::Service> CreateService() override {
    return std::make_unique<ServiceTestClientWithNetworkChange>(this);
  }

  void SetUp() override {
    service_manager::test::ServiceTest::SetUp();
    connector()->BindInterface(kNetworkServiceName, &network_service_);
  }

  mojom::NetworkServicePtr network_service_;
#if defined(OS_ANDROID)
  // On Android, NetworkChangeNotifier setup is more involved and needs
  // to be split between UI thread and network thread. Use a mock
  // NetworkChangeNotifier in tests, so the test setup is simpler.
  net::test::MockNetworkChangeNotifier network_change_notifier_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceNetworkChangeTest);
};

TEST_F(NetworkServiceNetworkChangeTest, MAYBE_NetworkChangeManagerRequest) {
  TestNetworkChangeManagerClient manager_client(service());
  manager_client.WaitForNotification(mojom::ConnectionType::CONNECTION_3G);
}

class NetworkServiceNetworkDelegateTest : public NetworkServiceTest {
 public:
  NetworkServiceNetworkDelegateTest() {
    // |NetworkServiceNetworkDelegate::HandleClearSiteDataHeader| requires
    // Network Service.
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kNetworkService);
  }
  ~NetworkServiceNetworkDelegateTest() override = default;

  void CreateNetworkContext() {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    service()->CreateNetworkContext(mojo::MakeRequest(&network_context_),
                                    std::move(context_params));
  }

  void LoadURL(const GURL& url, int options = mojom::kURLLoadOptionNone) {
    ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();
    StartLoadingURL(request, 0 /* process_id */, options);
    client_->RunUntilComplete();
  }

  void StartLoadingURL(const ResourceRequest& request,
                       uint32_t process_id,
                       int options = mojom::kURLLoadOptionNone) {
    client_.reset(new TestURLLoaderClient());
    mojom::URLLoaderFactoryPtr loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                             std::move(params));

    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader_), 1, 1, options, request,
        client_->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  TestURLLoaderClient* client() { return client_.get(); }

 protected:
  void SetUp() override {
    // Set up HTTPS server.
    https_server_.reset(new net::EmbeddedTestServer(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &NetworkServiceNetworkDelegateTest::HandleHTTPSRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  // Responds with the header "<header>" if we have "header"=<header> query
  // parameters in the url.
  std::unique_ptr<net::test_server::HttpResponse> HandleHTTPSRequest(
      const net::test_server::HttpRequest& request) {
    std::string header;
    if (net::GetValueForKeyInQuery(request.GetURL(), "header", &header)) {
      std::unique_ptr<net::test_server::RawHttpResponse> response(
          new net::test_server::RawHttpResponse("HTTP/1.1 200 OK\r\n", ""));

      // Newlines are encoded as '%0A' in URLs.
      const std::string newline_escape = "%0A";
      std::size_t pos = header.find(newline_escape);
      while (pos != std::string::npos) {
        header.replace(pos, newline_escape.length(), "\r\n");
        pos = header.find(newline_escape);
      }

      response->AddHeader(header);
      return response;
    }

    return nullptr;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<TestURLLoaderClient> client_;
  mojom::NetworkContextPtr network_context_;
  mojom::URLLoaderPtr loader_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceNetworkDelegateTest);
};

class ClearSiteDataNetworkServiceClient : public TestNetworkServiceClient {
 public:
  explicit ClearSiteDataNetworkServiceClient(
      mojom::NetworkServiceClientRequest request)
      : TestNetworkServiceClient(std::move(request)) {}
  ~ClearSiteDataNetworkServiceClient() override = default;

  // Needed these two cookie overrides to avoid NOTREACHED().
  void OnCookiesRead(int process_id,
                     int routing_id,
                     const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override {}

  void OnCookieChange(int process_id,
                      int routing_id,
                      const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override {}

  void OnClearSiteData(int process_id,
                       int routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int load_flags,
                       OnClearSiteDataCallback callback) override {
    ++on_clear_site_data_counter_;
    last_on_clear_site_data_header_value_ = header_value;
    std::move(callback).Run();
  }

  int on_clear_site_data_counter() const { return on_clear_site_data_counter_; }
  const std::string& last_on_clear_site_data_header_value() const {
    return last_on_clear_site_data_header_value_;
  }

  void ClearOnClearSiteDataCounter() {
    on_clear_site_data_counter_ = 0;
    last_on_clear_site_data_header_value_.clear();
  }

 private:
  int on_clear_site_data_counter_ = 0;
  std::string last_on_clear_site_data_header_value_;
};

// Check that |NetworkServiceNetworkDelegate| handles Clear-Site-Data header
// w/ and w/o |NetworkServiceCient|.
TEST_F(NetworkServiceNetworkDelegateTest, ClearSiteDataNetworkServiceCient) {
  const char kClearCookiesHeader[] = "Clear-Site-Data: \"cookies\"";
  CreateNetworkContext();

  // Null |NetworkServiceCient|. The request should complete without being
  // deferred.
  EXPECT_EQ(nullptr, service()->client());
  GURL url = https_server()->GetURL("/foo");
  url = AddQuery(url, "header", kClearCookiesHeader);
  LoadURL(url);
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  // With |NetworkServiceCient|. The request should go through
  // |ClearSiteDataNetworkServiceClient| and complete.
  mojom::NetworkServiceClientPtr client_ptr;
  auto client_impl = std::make_unique<ClearSiteDataNetworkServiceClient>(
      mojo::MakeRequest(&client_ptr));
  service()->SetClient(std::move(client_ptr));
  url = https_server()->GetURL("/bar");
  url = AddQuery(url, "header", kClearCookiesHeader);
  EXPECT_EQ(0, client_impl->on_clear_site_data_counter());
  LoadURL(url);
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
  EXPECT_EQ(1, client_impl->on_clear_site_data_counter());
}

// Check that headers are handled and passed to the client correctly.
TEST_F(NetworkServiceNetworkDelegateTest, HandleClearSiteDataHeaders) {
  const char kClearCookiesHeaderValue[] = "\"cookies\"";
  const char kClearCookiesHeader[] = "Clear-Site-Data: \"cookies\"";
  CreateNetworkContext();

  mojom::NetworkServiceClientPtr client_ptr;
  auto client_impl = std::make_unique<ClearSiteDataNetworkServiceClient>(
      mojo::MakeRequest(&client_ptr));
  service()->SetClient(std::move(client_ptr));

  // |passed_header_value| are only checked if |should_call_client| is true.
  const struct TestCase {
    std::string response_headers;
    bool should_call_client;
    std::string passed_header_value;
  } kTestCases[] = {
      // The throttle does not defer requests if there are no interesting
      // response headers.
      {"", false, ""},
      {"Set-Cookie: abc=123;", false, ""},
      {"Content-Type: image/png;", false, ""},

      // Both malformed and valid Clear-Site-Data headers will defer requests
      // and be passed to the client. It's client's duty to detect malformed
      // headers.
      {"Clear-Site-Data: cookies", true, "cookies"},
      {"Clear-Site-Data: \"unknown type\"", true, "\"unknown type\""},
      {"Clear-Site-Data: \"cookies\", \"unknown type\"", true,
       "\"cookies\", \"unknown type\""},
      {kClearCookiesHeader, true, kClearCookiesHeaderValue},
      {base::StringPrintf("Content-Type: image/png;\n%s", kClearCookiesHeader),
       true, kClearCookiesHeaderValue},
      {base::StringPrintf("%s\nContent-Type: image/png;", kClearCookiesHeader),
       true, kClearCookiesHeaderValue},

      // Multiple instances of the header will be parsed correctly.
      {base::StringPrintf("%s\n%s", kClearCookiesHeader, kClearCookiesHeader),
       true, "\"cookies\", \"cookies\""},
  };

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(
        base::StringPrintf("Headers:\n%s", test_case.response_headers.c_str()));

    GURL url = https_server()->GetURL("/foo");
    url = AddQuery(url, "header", test_case.response_headers);
    EXPECT_EQ(0, client_impl->on_clear_site_data_counter());
    LoadURL(url);

    EXPECT_EQ(net::OK, client()->completion_status().error_code);
    if (test_case.should_call_client) {
      EXPECT_EQ(1, client_impl->on_clear_site_data_counter());
      EXPECT_EQ(test_case.passed_header_value,
                client_impl->last_on_clear_site_data_header_value());
    } else {
      EXPECT_EQ(0, client_impl->on_clear_site_data_counter());
    }
    client_impl->ClearOnClearSiteDataCounter();
  }
}

}  // namespace

}  // namespace network
