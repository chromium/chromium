// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/escape.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_network_service_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_KERBEROS)
#include "net/http/http_auth_handler_negotiate.h"
#endif

namespace network {

namespace {

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
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

class NetworkServiceTest : public testing::Test {
 public:
  explicit NetworkServiceTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          time_source),
        service_(NetworkService::CreateForTesting()) {}
  ~NetworkServiceTest() override {}

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  NetworkService* service() const { return service_.get(); }

  void DestroyService() { service_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> service_;
};

// Test shutdown in the case a NetworkContext is destroyed before the
// NetworkService.
TEST_F(NetworkServiceTest, CreateAndDestroyContext) {
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  CreateContextParams());
  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

// Test shutdown in the case there is still a live NetworkContext when the
// NetworkService is destroyed. The service should destroy the NetworkContext
// itself.
TEST_F(NetworkServiceTest, DestroyingServiceDestroysContext) {
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  CreateContextParams());
  base::RunLoop run_loop;
  network_context.set_disconnect_handler(run_loop.QuitClosure());
  DestroyService();

  // Destroying the service should destroy the context, causing a connection
  // error.
  run_loop.Run();
}

TEST_F(NetworkServiceTest, CreateContextWithoutChannelID) {
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->cookie_path = base::FilePath();
  params->enable_encrypted_cookies = false;
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
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
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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
#if defined(OS_POSIX) && !BUILDFLAG(IS_CHROMEOS_ASH)
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

  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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

  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory);

  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kBasicAuthScheme));
  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kDigestAuthScheme));
  EXPECT_FALSE(auth_handler_factory->GetSchemeFactory(net::kNtlmAuthScheme));
}

#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
TEST_F(NetworkServiceTest, AuthGssapiLibraryName) {
  const std::string kGssapiLibraryName = "Jim";
  mojom::HttpAuthStaticParamsPtr auth_params =
      mojom::HttpAuthStaticParams::New();
  auth_params->supported_schemes.push_back("negotiate");
  auth_params->gssapi_library_name = kGssapiLibraryName;
  service()->SetUpHttpAuth(std::move(auth_params));

  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  ASSERT_TRUE(GetNegotiateFactory(&network_context));
  EXPECT_EQ(kGssapiLibraryName,
            GetNegotiateFactory(&network_context)->GetLibraryNameForTesting());
}
#endif  // BUILDFLAG(USE_EXTERNAL_GSSAPI)

TEST_F(NetworkServiceTest, AuthServerAllowlist) {
  // Add one server to the allowlist before creating any NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->server_allowlist = "server1";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the allowlist.
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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

  // Change allowlist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->server_allowlist = "server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server1/")));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server2/")));

  // Change allowlist to have multiple servers. The pre-existing NetworkContext
  // should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->server_allowlist = "server1,server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server1/")));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          GURL("https://server2/")));
}

TEST_F(NetworkServiceTest, AuthDelegateAllowlist) {
  using DelegationType = net::HttpAuth::DelegationType;

  // Add one server to the allowlist before creating any NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_allowlist = "server1";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the allowlist.
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  const net::HttpAuthPreferences* auth_prefs =
      auth_handler_factory->http_auth_preferences();
  ASSERT_TRUE(auth_prefs);
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(GURL("https://server1/")));
  EXPECT_EQ(DelegationType::kNone,
            auth_prefs->GetDelegationType(GURL("https://server2/")));

  // Change allowlist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_allowlist = "server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_EQ(DelegationType::kNone,
            auth_prefs->GetDelegationType(GURL("https://server1/")));
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(GURL("https://server2/")));

  // Change allowlist to have multiple servers. The pre-existing NetworkContext
  // should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_allowlist = "server1,server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(GURL("https://server1/")));
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(GURL("https://server2/")));
}

TEST_F(NetworkServiceTest, DelegateByKdcPolicy) {
  // Create a network context, which should use default value.
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::HttpAuthHandlerFactory* auth_handler_factory =
      network_context.url_request_context()->http_auth_handler_factory();
  ASSERT_TRUE(auth_handler_factory);
  ASSERT_TRUE(auth_handler_factory->http_auth_preferences());
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->delegate_by_kdc_policy());

  // Change allowlist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_by_kdc_policy = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->delegate_by_kdc_policy());
}

TEST_F(NetworkServiceTest, AuthNegotiateCnameLookup) {
  // Set |negotiate_disable_cname_lookup| to true before creating any
  // NetworkContexts.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->negotiate_disable_cname_lookup = true;
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  // Create a network context, which should reflect the setting.
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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
  // Create valid DnsConfig.
  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::DnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  service()->ConfigureStubHostResolver(
      true /* insecure_dns_client_enabled */, net::SecureDnsMode::kOff,
      base::nullopt /* dns_over_https_servers */);
  EXPECT_TRUE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kOff,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);

  service()->ConfigureStubHostResolver(
      false /* insecure_dns_client_enabled */, net::SecureDnsMode::kOff,
      base::nullopt /* dns_over_https_servers */);
  EXPECT_FALSE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kOff,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);

  service()->ConfigureStubHostResolver(
      false /* insecure_dns_client_enabled */, net::SecureDnsMode::kAutomatic,
      base::nullopt /* dns_over_https_servers */);
  EXPECT_FALSE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);

  std::vector<mojom::DnsOverHttpsServerPtr> dns_over_https_servers_ptr;
  mojom::DnsOverHttpsServerPtr dns_over_https_server =
      mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = "https://foo/";
  dns_over_https_server->use_post = true;
  dns_over_https_servers_ptr.emplace_back(std::move(dns_over_https_server));
  service()->ConfigureStubHostResolver(false /* insecure_dns_client_enabled */,
                                       net::SecureDnsMode::kAutomatic,
                                       std::move(dns_over_https_servers_ptr));
  EXPECT_FALSE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);
}

TEST_F(NetworkServiceTest, DnsOverHttpsEnableDisable) {
  const std::string kServer1 = "https://foo/";
  const bool kServer1UsePost = false;
  const std::string kServer2 = "https://bar/dns-query{?dns}";
  const bool kServer2UsePost = true;
  const std::string kServer3 = "https://grapefruit/resolver/query{?dns}";
  const bool kServer3UsePost = false;

  // Create valid DnsConfig.
  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  // Enable DNS over HTTPS for one server.

  std::vector<mojom::DnsOverHttpsServerPtr> dns_over_https_servers_ptr;

  mojom::DnsOverHttpsServerPtr dns_over_https_server =
      mojom::DnsOverHttpsServer::New();
  dns_over_https_server->server_template = kServer1;
  dns_over_https_server->use_post = kServer1UsePost;
  dns_over_https_servers_ptr.emplace_back(std::move(dns_over_https_server));

  service()->ConfigureStubHostResolver(false /* insecure_dns_client_enabled */,
                                       net::SecureDnsMode::kAutomatic,
                                       std::move(dns_over_https_servers_ptr));
  EXPECT_TRUE(
      service()->host_resolver_manager()->GetDnsConfigAsValue().is_dict());
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers =
      dns_client_ptr->GetEffectiveConfig()->dns_over_https_servers;
  ASSERT_EQ(1u, dns_over_https_servers.size());
  EXPECT_EQ(kServer1, dns_over_https_servers[0].server_template);
  EXPECT_EQ(kServer1UsePost, dns_over_https_servers[0].use_post);

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

  service()->ConfigureStubHostResolver(true /* insecure_dns_client_enabled */,
                                       net::SecureDnsMode::kSecure,
                                       std::move(dns_over_https_servers_ptr));
  EXPECT_TRUE(
      service()->host_resolver_manager()->GetDnsConfigAsValue().is_dict());
  dns_over_https_servers =
      dns_client_ptr->GetEffectiveConfig()->dns_over_https_servers;
  ASSERT_EQ(2u, dns_over_https_servers.size());
  EXPECT_EQ(kServer2, dns_over_https_servers[0].server_template);
  EXPECT_EQ(kServer2UsePost, dns_over_https_servers[0].use_post);
  EXPECT_EQ(kServer3, dns_over_https_servers[1].server_template);
  EXPECT_EQ(kServer3UsePost, dns_over_https_servers[1].use_post);
}

TEST_F(NetworkServiceTest, DisableDohUpgradeProviders) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kDnsOverHttpsUpgrade,
      {{"DisabledProviders", "CleanBrowsingSecure, , Cloudflare,Unexpected"}});
  service()->ConfigureStubHostResolver(
      true /* insecure_dns_client_enabled */, net::SecureDnsMode::kAutomatic,
      base::nullopt /* dns_over_https_servers */);

  // Set valid DnsConfig.
  net::DnsConfig config;
  // Cloudflare upgradeable IPs
  net::IPAddress dns_ip0(1, 0, 0, 1);
  net::IPAddress dns_ip1;
  EXPECT_TRUE(dns_ip1.AssignFromIPLiteral("2606:4700:4700::1111"));
  // CleanBrowsing family filter upgradeable IP
  net::IPAddress dns_ip2;
  EXPECT_TRUE(dns_ip2.AssignFromIPLiteral("2a0d:2a00:2::"));
  // CleanBrowsing security filter upgradeable IP
  net::IPAddress dns_ip3(185, 228, 169, 9);
  // Non-upgradeable IP
  net::IPAddress dns_ip4(1, 2, 3, 4);

  config.nameservers.push_back(
      net::IPEndPoint(dns_ip0, net::dns_protocol::kDefaultPort));
  config.nameservers.push_back(
      net::IPEndPoint(dns_ip1, net::dns_protocol::kDefaultPort));
  config.nameservers.push_back(net::IPEndPoint(dns_ip2, 54));
  config.nameservers.push_back(
      net::IPEndPoint(dns_ip3, net::dns_protocol::kDefaultPort));
  config.nameservers.push_back(
      net::IPEndPoint(dns_ip4, net::dns_protocol::kDefaultPort));

  auto dns_client = net::DnsClient::CreateClient(nullptr /* net_log */);
  dns_client->SetSystemConfig(config);
  net::DnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  std::vector<net::DnsOverHttpsServerConfig> expected_doh_servers = {
      {"https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
       false /* use_post */}};
  EXPECT_TRUE(dns_client_ptr->GetEffectiveConfig());
  EXPECT_EQ(expected_doh_servers,
            dns_client_ptr->GetEffectiveConfig()->dns_over_https_servers);
}

TEST_F(NetworkServiceTest, DohProbe) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  config.dns_over_https_servers.emplace_back("example.com",
                                             true /* use_post */);
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_MultipleContexts) {
  service()->StopMetricsTimerForTesting();
  mojom::NetworkContextParamsPtr context_params1 = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context1;
  service()->CreateNetworkContext(network_context1.BindNewPipeAndPassReceiver(),
                                  std::move(context_params1));

  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  config.dns_over_https_servers.emplace_back("example.com",
                                             true /* use_post */);
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  ASSERT_TRUE(dns_client_ptr->factory()->doh_probes_running());

  mojom::NetworkContextParamsPtr context_params2 = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context2;
  service()->CreateNetworkContext(network_context2.BindNewPipeAndPassReceiver(),
                                  std::move(context_params2));
  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());

  network_context2.reset();
  task_environment()->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());

  network_context1.reset();
  task_environment()->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_ContextAddedBeforeTimeout) {
  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  config.dns_over_https_servers.emplace_back("example.com",
                                             true /* use_post */);
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_ContextAddedAfterTimeout) {
  service()->StopMetricsTimerForTesting();
  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  config.dns_over_https_servers.emplace_back("example.com",
                                             true /* use_post */);
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_ContextRemovedBeforeTimeout) {
  service()->StopMetricsTimerForTesting();
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  config.dns_over_https_servers.emplace_back("example.com",
                                             true /* use_post */);
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  network_context.reset();
  task_environment()->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_ContextRemovedAfterTimeout) {
  service()->StopMetricsTimerForTesting();
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  net::DnsConfig config;
  config.nameservers.push_back(net::IPEndPoint());
  config.dns_over_https_servers.emplace_back("example.com",
                                             true /* use_post */);
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());

  network_context.reset();
  task_environment()->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());
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
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
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

static int GetGlobalMaxConnectionsPerProxy() {
  return net::ClientSocketPoolManager::max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL);
}

// Tests that NetworkService::SetMaxConnectionsPerProxy() (1) modifies globals
// in net::ClientSocketPoolManager (2) saturates out of bound values.
TEST_F(NetworkServiceTest, SetMaxConnectionsPerProxy) {
  const int kDefault = net::kDefaultMaxSocketsPerProxyServer;
  const int kMin = 6;
  const int kMax = 99;

  // Starts off at default value.
  EXPECT_EQ(net::kDefaultMaxSocketsPerProxyServer,
            GetGlobalMaxConnectionsPerProxy());

  // Anything less than kMin saturates to kMin.
  service()->SetMaxConnectionsPerProxy(kMin - 1);
  EXPECT_EQ(kMin, GetGlobalMaxConnectionsPerProxy());

  // Anything larger than kMax saturates to kMax
  service()->SetMaxConnectionsPerProxy(kMax + 1);
  EXPECT_EQ(kMax, GetGlobalMaxConnectionsPerProxy());

  // Anything in between kMin and kMax should be set exactly.
  service()->SetMaxConnectionsPerProxy(58);
  EXPECT_EQ(58, GetGlobalMaxConnectionsPerProxy());

  // Negative values select the default.
  service()->SetMaxConnectionsPerProxy(-2);
  EXPECT_EQ(kDefault, GetGlobalMaxConnectionsPerProxy());

  // Restore the default value to minize sideffects.
  service()->SetMaxConnectionsPerProxy(kDefault);
}

class NetworkServiceTestWithService : public testing::Test {
 public:
  NetworkServiceTestWithService()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~NetworkServiceTestWithService() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kServicesTestData));
    ASSERT_TRUE(test_server_.Start());
    service_ = NetworkService::CreateForTesting();
    service_->Bind(network_service_.BindNewPipeAndPassReceiver());
  }

  void CreateNetworkContext() {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_service_->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
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
    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id;
    params->request_initiator_origin_lock =
        url::Origin::Create(GURL("https://initiator.example.com"));
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    loader_.reset();
    loader_factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 1, 1, options, request,
        client_->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void Shutdown() { service_.reset(); }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  TestURLLoaderClient* client() { return client_.get(); }
  mojom::URLLoader* loader() { return loader_.get(); }
  mojom::NetworkService* service() { return network_service_.get(); }
  mojom::NetworkContext* context() { return network_context_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> service_;

  net::EmbeddedTestServer test_server_;
  std::unique_ptr<TestURLLoaderClient> client_;
  mojo::Remote<mojom::NetworkService> network_service_;
  mojo::Remote<mojom::NetworkContext> network_context_;
  mojo::Remote<mojom::URLLoader> loader_;

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
  network_service_->StartNetLog(
      std::move(log_file), net::NetLogCaptureMode::kDefault, std::move(dict));
  CreateNetworkContext();
  LoadURL(test_server()->GetURL("/echo"));
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  // |log_file| is closed on destruction of the NetworkService.
  Shutdown();

  // |log_file| is closed on another thread, so have to wait for that to happen.
  task_environment_.RunUntilIdle();

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
  EXPECT_TRUE(!client()->response_head()->raw_request_response_info);
  loader()->FollowRedirect({}, {}, {}, base::nullopt);
  client()->RunUntilComplete();
  EXPECT_TRUE(!client()->response_head()->raw_request_response_info);
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
    auto& request_response_info =
        client()->response_head()->raw_request_response_info;
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
  loader()->FollowRedirect({}, {}, {}, base::nullopt);
  client()->RunUntilComplete();
  {
    auto& request_response_info =
        client()->response_head()->raw_request_response_info;
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
  EXPECT_FALSE(client()->response_head()->raw_request_response_info);
  service()->SetRawHeadersAccess(
      process_id,
      {url::Origin::CreateFromNormalizedTuple("http", "example.com", 80),
       url::Origin::Create(request.url)});
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  {
    auto& request_response_info =
        client()->response_head()->raw_request_response_info;
    ASSERT_TRUE(request_response_info);
    EXPECT_EQ(200, request_response_info->http_status_code);
    EXPECT_EQ("OK", request_response_info->http_status_text);
  }

  service()->SetRawHeadersAccess(process_id, {});
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  EXPECT_FALSE(client()->response_head()->raw_request_response_info.get());

  service()->SetRawHeadersAccess(
      process_id,
      {url::Origin::CreateFromNormalizedTuple("http", "example.com", 80)});
  StartLoadingURL(request, process_id);
  client()->RunUntilComplete();
  EXPECT_FALSE(client()->response_head()->raw_request_response_info.get());
}

class NetworkServiceTestWithResolverMap : public NetworkServiceTestWithService {
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        network::switches::kHostResolverRules, "MAP *.test 127.0.0.1");
    NetworkServiceTestWithService::SetUp();
  }
};

TEST_F(NetworkServiceTestWithResolverMap, RawRequestAccessControlWithRedirect) {
  CreateNetworkContext();

  const uint32_t process_id = 42;
  // initial_url in a.test redirects to b_url (in b.test) that then redirects to
  // url_a in a.test.
  GURL url_a = test_server()->GetURL("a.test", "/echo");
  GURL url_b =
      test_server()->GetURL("b.test", "/server-redirect?" + url_a.spec());
  GURL initial_url =
      test_server()->GetURL("a.test", "/server-redirect?" + url_b.spec());
  ResourceRequest request;
  request.url = initial_url;
  request.method = "GET";
  request.report_raw_headers = true;
  request.request_initiator = url::Origin();

  service()->SetRawHeadersAccess(process_id, {url::Origin::Create(url_a)});

  StartLoadingURL(request, process_id);
  client()->RunUntilRedirectReceived();  // from a.test to b.test
  EXPECT_TRUE(client()->response_head()->raw_request_response_info);

  loader()->FollowRedirect({}, {}, {}, base::nullopt);
  client()->ClearHasReceivedRedirect();
  client()->RunUntilRedirectReceived();  // from b.test to a.test
  EXPECT_FALSE(client()->response_head()->raw_request_response_info);

  loader()->FollowRedirect({}, {}, {}, base::nullopt);
  client()->RunUntilComplete();  // Done loading a.test
  EXPECT_TRUE(client()->response_head()->raw_request_response_info.get());

  service()->SetRawHeadersAccess(process_id, {url::Origin::Create(url_b)});

  StartLoadingURL(request, process_id);
  client()->RunUntilRedirectReceived();  // from a.test to b.test
  EXPECT_FALSE(client()->response_head()->raw_request_response_info);

  loader()->FollowRedirect({}, {}, {}, base::nullopt);
  client()->ClearHasReceivedRedirect();
  client()->RunUntilRedirectReceived();  // from b.test to a.test
  EXPECT_TRUE(client()->response_head()->raw_request_response_info);

  loader()->FollowRedirect({}, {}, {}, base::nullopt);
  client()->RunUntilComplete();  // Done loading a.test
  EXPECT_FALSE(client()->response_head()->raw_request_response_info.get());
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
  request.request_initiator =
      url::Origin::Create(GURL("https://initiator.example.com"));
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

// Integration test confirming that the SetTrustTokenKeyCommitments IPC is wired
// up correctly by verifying that it's possible to read a value previously
// passed to the setter.
TEST_F(NetworkServiceTestWithService, SetsTrustTokenKeyCommitments) {
  ASSERT_TRUE(service_->trust_token_key_commitments());

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  expectation->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  expectation->id = 1;
  expectation->batch_size = 5;

  base::RunLoop run_loop;
  network_service_->SetTrustTokenKeyCommitments(
      R"( { "https://issuer.example": { "protocol_version": "TrustTokenV2PMB", "id": 1, "batchsize": 5 } } )",
      run_loop.QuitClosure());
  run_loop.Run();

  mojom::TrustTokenKeyCommitmentResultPtr result;
  bool ran = false;

  service_->trust_token_key_commitments()->Get(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example")),
      base::BindLambdaForTesting(
          [&](mojom::TrustTokenKeyCommitmentResultPtr ptr) {
            result = std::move(ptr);
            ran = true;
          }));

  ASSERT_TRUE(ran);

  EXPECT_TRUE(result.Equals(expectation));
}

TEST_F(NetworkServiceTestWithService, GetDnsConfigChangeManager) {
  mojo::Remote<mojom::DnsConfigChangeManager> remote;
  ASSERT_FALSE(remote.is_bound());

  network_service_->GetDnsConfigChangeManager(
      remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(remote.is_bound());
}

TEST_F(NetworkServiceTestWithService, GetNetworkList) {
  base::RunLoop run_loop;
  network_service_->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindLambdaForTesting(
          [&](const base::Optional<std::vector<net::NetworkInterface>>& list) {
            EXPECT_NE(base::nullopt, list);
            for (auto it = list->begin(); it != list->end(); ++it) {
              // Verify that names are not empty.
              EXPECT_FALSE(it->name.empty());
              EXPECT_FALSE(it->friendly_name.empty());

              // Verify that the address is correct.
              EXPECT_TRUE(it->address.IsValid());

              EXPECT_FALSE(it->address.IsZero());
              EXPECT_GT(it->prefix_length, 1u);
              EXPECT_LE(it->prefix_length, it->address.size() * 8);
            }
            run_loop.Quit();
          }));
  run_loop.Run();
}

class TestNetworkChangeManagerClient
    : public mojom::NetworkChangeManagerClient {
 public:
  explicit TestNetworkChangeManagerClient(
      mojom::NetworkService* network_service)
      : connection_type_(mojom::ConnectionType::CONNECTION_UNKNOWN) {
    mojo::Remote<mojom::NetworkChangeManager> manager_remote;
    network_service->GetNetworkChangeManager(
        manager_remote.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::NetworkChangeManagerClient> client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver());
    manager_remote->RequestNotifications(std::move(client_remote));
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

  void Flush() { receiver_.FlushForTesting(); }

 private:
  base::RunLoop run_loop_;
  mojom::ConnectionType connection_type_;
  mojo::Receiver<mojom::NetworkChangeManagerClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeManagerClient);
};

class NetworkChangeTest : public testing::Test {
 public:
  NetworkChangeTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        service_(NetworkService::CreateForTesting()) {}

  ~NetworkChangeTest() override {}

  NetworkService* service() const { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<NetworkService> service_;
};

// mojom:NetworkChangeManager isn't supported on iOS.
// See the same ifdef in CreateNetworkChangeNotifierIfNeeded.
#if defined(OS_IOS)
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

class NetworkServiceNetworkChangeTest : public testing::Test {
 public:
  NetworkServiceNetworkChangeTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        service_(NetworkService::CreateForTesting()) {
    service_->Bind(network_service_.BindNewPipeAndPassReceiver());
  }

  ~NetworkServiceNetworkChangeTest() override {}

  mojom::NetworkService* service() { return network_service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  mojo::Remote<mojom::NetworkService> network_service_;
  std::unique_ptr<NetworkService> service_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceNetworkChangeTest);
};

TEST_F(NetworkServiceNetworkChangeTest, MAYBE_NetworkChangeManagerRequest) {
  TestNetworkChangeManagerClient manager_client(service());

  // Wait for the NetworkChangeManagerClient registration to be processed within
  // the NetworkService impl before simulating a change. Flushing guarantees
  // end-to-end connection of the client interface.
  manager_client.Flush();

  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_3G);

  manager_client.WaitForNotification(mojom::ConnectionType::CONNECTION_3G);
}

class NetworkServiceNetworkDelegateTest : public NetworkServiceTest {
 public:
  NetworkServiceNetworkDelegateTest()
      : NetworkServiceTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}
  ~NetworkServiceNetworkDelegateTest() override = default;

  void CreateNetworkContext() {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    service()->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
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
    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    loader_.reset();
    loader_factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 1, 1, options, request,
        client_->CreateRemote(),
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
  mojo::Remote<mojom::NetworkContext> network_context_;
  mojo::Remote<mojom::URLLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceNetworkDelegateTest);
};

class ClearSiteDataNetworkContextClient : public TestNetworkContextClient {
 public:
  explicit ClearSiteDataNetworkContextClient(
      mojo::PendingReceiver<mojom::NetworkContextClient> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~ClearSiteDataNetworkContextClient() override = default;

  void OnClearSiteData(int32_t process_id,
                       int32_t routing_id,
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
  mojo::Receiver<mojom::NetworkContextClient> receiver_;
};

// Check that |NetworkServiceNetworkDelegate| handles Clear-Site-Data header
// w/ and w/o |NetworkContextCient|.
TEST_F(NetworkServiceNetworkDelegateTest, ClearSiteDataNetworkContextCient) {
  const char kClearCookiesHeader[] = "Clear-Site-Data: \"cookies\"";
  CreateNetworkContext();

  // Null |NetworkContextCient|. The request should complete without being
  // deferred.
  GURL url = https_server()->GetURL("/foo");
  url = AddQuery(url, "header", kClearCookiesHeader);
  LoadURL(url);
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  // With |NetworkContextCient|. The request should go through
  // |ClearSiteDataNetworkContextClient| and complete.
  mojo::PendingRemote<mojom::NetworkContextClient> client_remote;
  auto client_impl = std::make_unique<ClearSiteDataNetworkContextClient>(
      client_remote.InitWithNewPipeAndPassReceiver());
  network_context_->SetClient(std::move(client_remote));
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

  mojo::PendingRemote<mojom::NetworkContextClient> client_remote;
  auto client_impl = std::make_unique<ClearSiteDataNetworkContextClient>(
      client_remote.InitWithNewPipeAndPassReceiver());
  network_context_->SetClient(std::move(client_remote));

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
