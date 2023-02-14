// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_state.h"
#include "net/net_buildflags.h"
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
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
              base::EscapeQueryParamValue(value, false));
}

mojom::NetworkContextParamsPtr CreateContextParams() {
  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
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
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(params));
  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

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
  EXPECT_TRUE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kBasicAuthScheme));
  EXPECT_TRUE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kDigestAuthScheme));
  EXPECT_TRUE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kNtlmAuthScheme));

#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(auth_handler_factory->IsSchemeAllowedForTesting(
      net::kNegotiateAuthScheme));
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ("", auth_handler_factory->GetNegotiateLibraryNameForTesting());
#endif
#endif  // BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)

  EXPECT_FALSE(auth_handler_factory->http_auth_preferences()
                   ->NegotiateDisableCnameLookup());
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->NegotiateEnablePort());
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_TRUE(auth_handler_factory->http_auth_preferences()->NtlmV2Enabled());
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("", auth_handler_factory->http_auth_preferences()
                    ->AuthAndroidNegotiateAccountType());
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(NetworkServiceTest, AuthSchemesDynamicallyChanging) {
  service()->SetUpHttpAuth(mojom::HttpAuthStaticParams::New());

  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory);

  EXPECT_TRUE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kBasicAuthScheme));
  EXPECT_TRUE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kDigestAuthScheme));
  EXPECT_TRUE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kNtlmAuthScheme));
#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(auth_handler_factory->IsSchemeAllowedForTesting(
      net::kNegotiateAuthScheme));
#else
  EXPECT_FALSE(auth_handler_factory->IsSchemeAllowedForTesting(
      net::kNegotiateAuthScheme));
#endif
  {
    mojom::HttpAuthDynamicParamsPtr auth_params =
        mojom::HttpAuthDynamicParams::New();
    auth_params->allowed_schemes = std::vector<std::string>{};
    service()->ConfigureHttpAuthPrefs(std::move(auth_params));

    EXPECT_FALSE(
        auth_handler_factory->IsSchemeAllowedForTesting(net::kBasicAuthScheme));
    EXPECT_FALSE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kDigestAuthScheme));
    EXPECT_FALSE(
        auth_handler_factory->IsSchemeAllowedForTesting(net::kNtlmAuthScheme));
    EXPECT_FALSE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kNegotiateAuthScheme));
  }
  {
    mojom::HttpAuthDynamicParamsPtr auth_params =
        mojom::HttpAuthDynamicParams::New();
    auth_params->allowed_schemes =
        std::vector<std::string>{net::kDigestAuthScheme, net::kNtlmAuthScheme};
    service()->ConfigureHttpAuthPrefs(std::move(auth_params));

    EXPECT_FALSE(
        auth_handler_factory->IsSchemeAllowedForTesting(net::kBasicAuthScheme));
    EXPECT_TRUE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kDigestAuthScheme));
    EXPECT_TRUE(
        auth_handler_factory->IsSchemeAllowedForTesting(net::kNtlmAuthScheme));
    EXPECT_FALSE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kNegotiateAuthScheme));
  }
  {
    mojom::HttpAuthDynamicParamsPtr auth_params =
        mojom::HttpAuthDynamicParams::New();
    service()->ConfigureHttpAuthPrefs(std::move(auth_params));

    EXPECT_TRUE(
        auth_handler_factory->IsSchemeAllowedForTesting(net::kBasicAuthScheme));
    EXPECT_TRUE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kDigestAuthScheme));
    EXPECT_TRUE(
        auth_handler_factory->IsSchemeAllowedForTesting(net::kNtlmAuthScheme));
#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
    EXPECT_TRUE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kNegotiateAuthScheme));
#else
    EXPECT_FALSE(auth_handler_factory->IsSchemeAllowedForTesting(
        net::kNegotiateAuthScheme));
#endif
  }
}

TEST_F(NetworkServiceTest, AuthSchemesNone) {
  service()->SetUpHttpAuth(mojom::HttpAuthStaticParams::New());

  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory);

  // An empty list means to support no schemes.
  mojom::HttpAuthDynamicParamsPtr auth_params =
      mojom::HttpAuthDynamicParams::New();
  auth_params->allowed_schemes = std::vector<std::string>{};
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));

  EXPECT_FALSE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kBasicAuthScheme));
  EXPECT_FALSE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kDigestAuthScheme));
  EXPECT_FALSE(
      auth_handler_factory->IsSchemeAllowedForTesting(net::kNtlmAuthScheme));
}

#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
TEST_F(NetworkServiceTest, AuthGssapiLibraryName) {
  const std::string kGssapiLibraryName = "Jim";
  mojom::HttpAuthStaticParamsPtr static_auth_params =
      mojom::HttpAuthStaticParams::New();
  static_auth_params->gssapi_library_name = kGssapiLibraryName;
  service()->SetUpHttpAuth(std::move(static_auth_params));

  mojom::HttpAuthDynamicParamsPtr dynamic_auth_params =
      mojom::HttpAuthDynamicParams::New();
  dynamic_auth_params->allowed_schemes =
      std::vector<std::string>{net::kNegotiateAuthScheme};
  service()->ConfigureHttpAuthPrefs(std::move(dynamic_auth_params));

  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::HttpAuthHandlerRegistryFactory* auth_handler_factory =
      reinterpret_cast<net::HttpAuthHandlerRegistryFactory*>(
          network_context.url_request_context()->http_auth_handler_factory());
  ASSERT_TRUE(auth_handler_factory->IsSchemeAllowedForTesting(
      net::kNegotiateAuthScheme));
  EXPECT_EQ(kGssapiLibraryName,
            auth_handler_factory->GetNegotiateLibraryNameForTesting());
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
          url::SchemeHostPort(GURL("https://server1/"))));
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          url::SchemeHostPort(GURL("https://server2/"))));

  // Change allowlist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->server_allowlist = "server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_FALSE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          url::SchemeHostPort(GURL("https://server1/"))));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          url::SchemeHostPort(GURL("https://server2/"))));

  // Change allowlist to have multiple servers. The pre-existing NetworkContext
  // should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->server_allowlist = "server1,server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          url::SchemeHostPort(GURL("https://server1/"))));
  EXPECT_TRUE(
      auth_handler_factory->http_auth_preferences()->CanUseDefaultCredentials(
          url::SchemeHostPort(GURL("https://server2/"))));
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
            auth_prefs->GetDelegationType(
                url::SchemeHostPort(GURL("https://server1/"))));
  EXPECT_EQ(DelegationType::kNone,
            auth_prefs->GetDelegationType(
                url::SchemeHostPort(GURL("https://server2/"))));

  // Change allowlist to only have a different server on it. The pre-existing
  // NetworkContext should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_allowlist = "server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_EQ(DelegationType::kNone,
            auth_prefs->GetDelegationType(
                url::SchemeHostPort(GURL("https://server1/"))));
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(
                url::SchemeHostPort(GURL("https://server2/"))));

  // Change allowlist to have multiple servers. The pre-existing NetworkContext
  // should be using the new list.
  auth_params = mojom::HttpAuthDynamicParams::New();
  auth_params->delegate_allowlist = "server1,server2";
  service()->ConfigureHttpAuthPrefs(std::move(auth_params));
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(
                url::SchemeHostPort(GURL("https://server1/"))));
  EXPECT_EQ(DelegationType::kUnconstrained,
            auth_prefs->GetDelegationType(
                url::SchemeHostPort(GURL("https://server2/"))));
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
#if !BUILDFLAG(IS_IOS)

TEST_F(NetworkServiceTest, DnsClientEnableDisable) {
  // Create valid DnsConfig.
  net::DnsConfig config;
  config.nameservers.emplace_back();
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::DnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/true, net::SecureDnsMode::kOff,
      /*dns_over_https_config=*/{},
      /*additional_dns_types_enabled=*/true);
  EXPECT_TRUE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kOff,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/false, net::SecureDnsMode::kOff,
      /*dns_over_https_config=*/{},
      /*additional_dns_types_enabled=*/true);
  EXPECT_FALSE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kOff,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/false, net::SecureDnsMode::kAutomatic,
      /*dns_over_https_config=*/{},
      /*additional_dns_types_enabled=*/true);
  EXPECT_FALSE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/false, net::SecureDnsMode::kAutomatic,
      *net::DnsOverHttpsConfig::FromString("https://foo/"),
      /*additional_dns_types_enabled=*/true);
  EXPECT_FALSE(dns_client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic,
            dns_client_ptr->GetEffectiveConfig()->secure_dns_mode);
}

TEST_F(NetworkServiceTest, HandlesAdditionalDnsQueryTypesEnableDisable) {
  // Create valid DnsConfig.
  net::DnsConfig config;
  config.nameservers.emplace_back();
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  const net::DnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/true, net::SecureDnsMode::kOff,
      /*dns_over_https_config=*/{},
      /*additional_dns_types_enabled=*/true);
  EXPECT_TRUE(dns_client_ptr->CanQueryAdditionalTypesViaInsecureDns());

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/true, net::SecureDnsMode::kOff,
      /*dns_over_https_config=*/{},
      /*additional_dns_types_enabled=*/false);
  EXPECT_FALSE(dns_client_ptr->CanQueryAdditionalTypesViaInsecureDns());
}

TEST_F(NetworkServiceTest, DnsOverHttpsEnableDisable) {
  const auto kConfig1 = *net::DnsOverHttpsConfig::FromString("https://foo/");
  const auto kConfig2 = *net::DnsOverHttpsConfig::FromString(
      "https://bar/dns-query{?dns} https://grapefruit/resolver/query{?dns}");

  // Create valid DnsConfig.
  net::DnsConfig config;
  config.nameservers.emplace_back();
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  // Enable DNS over HTTPS for one server.

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/false, net::SecureDnsMode::kAutomatic,
      kConfig1,
      /*additional_dns_types_enabled=*/true);
  EXPECT_TRUE(
      service()->host_resolver_manager()->GetDnsConfigAsValue().is_dict());
  EXPECT_EQ(kConfig1, dns_client_ptr->GetEffectiveConfig()->doh_config);

  // Enable DNS over HTTPS for two servers.

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/true, net::SecureDnsMode::kSecure,
      kConfig2,
      /*additional_dns_types_enabled=*/true);
  EXPECT_TRUE(
      service()->host_resolver_manager()->GetDnsConfigAsValue().is_dict());
  EXPECT_EQ(kConfig2, dns_client_ptr->GetEffectiveConfig()->doh_config);
}

TEST_F(NetworkServiceTest, DisableDohUpgradeProviders) {
  auto FindProviderFeature =
      [](base::StringPiece provider) -> base::test::FeatureRef {
    const auto it =
        base::ranges::find(net::DohProviderEntry::GetList(), provider,
                           &net::DohProviderEntry::provider);
    CHECK(it != net::DohProviderEntry::GetList().end())
        << "Provider named \"" << provider
        << "\" not found in DoH provider list.";
    return (*it)->feature;
  };

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kDnsOverHttpsUpgrade},
      /*disabled_features=*/{FindProviderFeature("CleanBrowsingSecure"),
                             FindProviderFeature("Cloudflare")});

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/true, net::SecureDnsMode::kAutomatic,
      /*dns_over_https_config=*/{},
      /*additional_dns_types_enabled=*/true);

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

  config.nameservers.emplace_back(dns_ip0, net::dns_protocol::kDefaultPort);
  config.nameservers.emplace_back(dns_ip1, net::dns_protocol::kDefaultPort);
  config.nameservers.emplace_back(dns_ip2, 54);
  config.nameservers.emplace_back(dns_ip3, net::dns_protocol::kDefaultPort);
  config.nameservers.emplace_back(dns_ip4, net::dns_protocol::kDefaultPort);

  auto dns_client = net::DnsClient::CreateClient(nullptr /* net_log */);
  dns_client->SetSystemConfig(config);
  net::DnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  auto expected_doh_config = *net::DnsOverHttpsConfig::FromString(
      "https://doh.cleanbrowsing.org/doh/family-filter{?dns}");
  EXPECT_TRUE(dns_client_ptr->GetEffectiveConfig());
  EXPECT_EQ(expected_doh_config,
            dns_client_ptr->GetEffectiveConfig()->doh_config);
}

TEST_F(NetworkServiceTest, DohProbe) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  net::DnsConfig config;
  config.nameservers.emplace_back();
  config.doh_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
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
  mojom::NetworkContextParamsPtr context_params1 = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context1;
  service()->CreateNetworkContext(network_context1.BindNewPipeAndPassReceiver(),
                                  std::move(context_params1));

  net::DnsConfig config;
  config.nameservers.emplace_back();
  config.doh_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
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
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(dns_client_ptr->factory()->doh_probes_running());

  network_context1.reset();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_ContextAddedBeforeTimeout) {
  net::DnsConfig config;
  config.nameservers.emplace_back();
  config.doh_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
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
  net::DnsConfig config;
  config.nameservers.emplace_back();
  config.doh_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
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
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  net::DnsConfig config;
  config.nameservers.emplace_back();
  config.doh_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
  auto dns_client = std::make_unique<net::MockDnsClient>(
      std::move(config), net::MockDnsClientRuleList());
  dns_client->set_ignore_system_config_changes(true);
  net::MockDnsClient* dns_client_ptr = dns_client.get();
  service()->host_resolver_manager()->SetDnsClientForTesting(
      std::move(dns_client));

  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  network_context.reset();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());

  task_environment()->FastForwardBy(NetworkService::kInitialDohProbeTimeout);
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());
}

TEST_F(NetworkServiceTest, DohProbe_ContextRemovedAfterTimeout) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(context_params));

  net::DnsConfig config;
  config.nameservers.emplace_back();
  config.doh_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(dns_client_ptr->factory()->doh_probes_running());
}

#endif  // !BUILDFLAG(IS_IOS)

// |ntlm_v2_enabled| is only supported on POSIX platforms.
#if BUILDFLAG(IS_POSIX)
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
#endif  // BUILDFLAG(IS_POSIX)

// |android_negotiate_account_type| is only supported on Android.
#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_CT_SUPPORTED)
// Tests that disabling CT enforcement disables the feature for both existing
// and new network contexts.
TEST_F(NetworkServiceTest, DisableCTEnforcement) {
  mojo::Remote<mojom::NetworkContext> network_context_remote;
  NetworkContext network_context(
      service(), network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  net::TransportSecurityState* transport_security_state =
      network_context.url_request_context()->transport_security_state();
  EXPECT_FALSE(
      transport_security_state->is_ct_emergency_disabled_for_testing());

  base::RunLoop run_loop;
  service()->SetCtEnforcementEnabled(false, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(transport_security_state->is_ct_emergency_disabled_for_testing());

  mojo::Remote<mojom::NetworkContext> new_network_context_remote;
  NetworkContext new_network_context(
      service(), new_network_context_remote.BindNewPipeAndPassReceiver(),
      CreateContextParams());
  transport_security_state =
      new_network_context.url_request_context()->transport_security_state();
  EXPECT_TRUE(transport_security_state->is_ct_emergency_disabled_for_testing());
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

class NetworkServiceTestWithService : public testing::Test {
 public:
  NetworkServiceTestWithService()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  NetworkServiceTestWithService(const NetworkServiceTestWithService&) = delete;
  NetworkServiceTestWithService& operator=(
      const NetworkServiceTestWithService&) = delete;

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
    client_ = std::make_unique<TestURLLoaderClient>();
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
        loader_.BindNewPipeAndPassReceiver(), 1, options, request,
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

  base::test::ScopedFeatureList scoped_features_;
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

  base::Value::Dict dict;
  dict.Set("amiatest", "iamatest");

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
  ASSERT_TRUE(log_dict->is_dict());
  ASSERT_EQ(*log_dict->GetDict().FindStringByDottedPath("constants.amiatest"),
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
  loader()->FollowRedirect({}, {}, {}, absl::nullopt);
  client()->RunUntilComplete();
}

class NetworkServiceTestWithResolverMap : public NetworkServiceTestWithService {
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        network::switches::kHostResolverRules, "MAP *.test 127.0.0.1");
    NetworkServiceTestWithService::SetUp();
  }
};

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
      mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb;
  expectation->id = 1;
  expectation->batch_size = 5;

  base::RunLoop run_loop;
  network_service_->SetTrustTokenKeyCommitments(
      R"( { "https://issuer.example": { "TrustTokenV3PMB": {
        "protocol_version": "TrustTokenV3PMB", "id": 1, "batchsize": 5 } } } )",
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
          [&](const absl::optional<std::vector<net::NetworkInterface>>& list) {
            EXPECT_NE(absl::nullopt, list);
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

  TestNetworkChangeManagerClient(const TestNetworkChangeManagerClient&) =
      delete;
  TestNetworkChangeManagerClient& operator=(
      const TestNetworkChangeManagerClient&) = delete;

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
#if BUILDFLAG(IS_IOS)
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

  NetworkServiceNetworkChangeTest(const NetworkServiceNetworkChangeTest&) =
      delete;
  NetworkServiceNetworkChangeTest& operator=(
      const NetworkServiceNetworkChangeTest&) = delete;

  ~NetworkServiceNetworkChangeTest() override {}

  mojom::NetworkService* service() { return network_service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  mojo::Remote<mojom::NetworkService> network_service_;
  std::unique_ptr<NetworkService> service_;
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

  NetworkServiceNetworkDelegateTest(const NetworkServiceNetworkDelegateTest&) =
      delete;
  NetworkServiceNetworkDelegateTest& operator=(
      const NetworkServiceNetworkDelegateTest&) = delete;

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

  void LoadURL(const GURL& url,
               int options = mojom::kURLLoadOptionNone,
               mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
                   url_loader_network_observer = mojo::NullRemote()) {
    ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();
    StartLoadingURL(request, 0 /* process_id */, options,
                    std::move(url_loader_network_observer));
    client_->RunUntilComplete();
  }

  void StartLoadingURL(
      const ResourceRequest& request,
      uint32_t process_id,
      int options = mojom::kURLLoadOptionNone,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer = mojo::NullRemote()) {
    client_ = std::make_unique<TestURLLoaderClient>();
    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id;
    params->is_corb_enabled = false;
    params->url_loader_network_observer =
        std::move(url_loader_network_observer);
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    loader_.reset();
    loader_factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 1, options, request,
        client_->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  TestURLLoaderClient* client() { return client_.get(); }

 protected:
  void SetUp() override {
    // Set up HTTPS server.
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
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
};

class ClearSiteDataAuthCertObserver : public TestURLLoaderNetworkObserver {
 public:
  ClearSiteDataAuthCertObserver() = default;
  ~ClearSiteDataAuthCertObserver() override = default;

  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
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
// w/ and w/o |NetworkContextCient|.
TEST_F(NetworkServiceNetworkDelegateTest, ClearSiteDataObserver) {
  const char kClearCookiesHeader[] = "Clear-Site-Data: \"cookies\"";
  CreateNetworkContext();

  // Null |url_loader_network_observer|. The request should complete without
  // being deferred.
  GURL url = https_server()->GetURL("/foo");
  url = AddQuery(url, "header", kClearCookiesHeader);
  LoadURL(url);
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  // With |url_loader_network_observer|. The request should go through
  // |ClearSiteDataAuthCertObserver| and complete.
  ClearSiteDataAuthCertObserver clear_site_observer;
  url = https_server()->GetURL("/bar");
  url = AddQuery(url, "header", kClearCookiesHeader);
  EXPECT_EQ(0, clear_site_observer.on_clear_site_data_counter());
  LoadURL(url, mojom::kURLLoadOptionNone, clear_site_observer.Bind());
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
  EXPECT_EQ(1, clear_site_observer.on_clear_site_data_counter());
}

// Check that headers are handled and passed to the client correctly.
TEST_F(NetworkServiceNetworkDelegateTest, HandleClearSiteDataHeaders) {
  const char kClearCookiesHeaderValue[] = "\"cookies\"";
  const char kClearCookiesHeader[] = "Clear-Site-Data: \"cookies\"";
  CreateNetworkContext();

  ClearSiteDataAuthCertObserver clear_site_observer;

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
    EXPECT_EQ(0, clear_site_observer.on_clear_site_data_counter());
    LoadURL(url, mojom::kURLLoadOptionNone, clear_site_observer.Bind());

    EXPECT_EQ(net::OK, client()->completion_status().error_code);
    if (test_case.should_call_client) {
      EXPECT_EQ(1, clear_site_observer.on_clear_site_data_counter());
      EXPECT_EQ(test_case.passed_header_value,
                clear_site_observer.last_on_clear_site_data_header_value());
    } else {
      EXPECT_EQ(0, clear_site_observer.on_clear_site_data_counter());
    }
    clear_site_observer.ClearOnClearSiteDataCounter();
  }
}

}  // namespace

}  // namespace network
