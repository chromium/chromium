// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
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
#include "net/log/file_net_log_observer.h"
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
#include "services/network/public/mojom/cookie_encryption_provider.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_annotation_monitor.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/system_dns_resolution.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_KERBEROS)
#include "net/http/http_auth_handler_negotiate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/network/mock_mojo_dhcp_wpad_url_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_WEBSOCKETS)
#include "services/network/test_mojo_proxy_resolver_factory.h"
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

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
  ~NetworkServiceTest() override = default;

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

TEST_F(NetworkServiceTest, CreateContextWithMaskedDomainListProxyConfig) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  masked_domain_list::MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  service()->UpdateMaskedDomainList(
      mojo_base::ProtoWrapper(mdl),
      /*exclusion_list=*/std::vector<std::string>());
  task_environment()->RunUntilIdle();

  mojom::NetworkContextParamsPtr params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(params));

  // TODO(aakallam): verify that the allow list is used

  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkServiceTest,
       CreateContextWithCustomProxyConfig_MdlConfigIsNotUsed) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  masked_domain_list::MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  service()->UpdateMaskedDomainList(
      mojo_base::ProtoWrapper(mdl),
      /*exclusion_list=*/std::vector<std::string>());
  task_environment()->RunUntilIdle();

  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->initial_custom_proxy_config =
      network::mojom::CustomProxyConfig::New();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(params));

  // TODO(aakallam): verify that the allow list isn't used

  network_context.reset();
  // Make sure the NetworkContext is destroyed.
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkServiceTest, CreateContextWithoutMaskedDomainListData) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  mojom::NetworkContextParamsPtr params = CreateContextParams();
  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(params));

  // TODO(aakallam): verify that the allow list isn't used

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
  EXPECT_EQ(kConfig1, dns_client_ptr->GetEffectiveConfig()->doh_config);

  // Enable DNS over HTTPS for two servers.

  service()->ConfigureStubHostResolver(
      /*insecure_dns_client_enabled=*/true, net::SecureDnsMode::kSecure,
      kConfig2,
      /*additional_dns_types_enabled=*/true);
  EXPECT_EQ(kConfig2, dns_client_ptr->GetEffectiveConfig()->doh_config);
}

TEST_F(NetworkServiceTest, DisableDohUpgradeProviders) {
  auto FindProviderFeature =
      [](std::string_view provider) -> base::test::FeatureRef {
    const auto it =
        base::ranges::find(net::DohProviderEntry::GetList(), provider,
                           &net::DohProviderEntry::provider);
    CHECK(it != net::DohProviderEntry::GetList().end())
        << "Provider named \"" << provider
        << "\" not found in DoH provider list.";
    return (*it)->feature.get();
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

static int GetGlobalMaxConnectionsPerProxyChain() {
  return net::ClientSocketPoolManager::max_sockets_per_proxy_chain(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL);
}

// Tests that NetworkService::SetMaxConnectionsPerProxyChain() (1) modifies
// globals in net::ClientSocketPoolManager (2) saturates out of bound values.
TEST_F(NetworkServiceTest, SetMaxConnectionsPerProxyChain) {
  const int kDefault = net::kDefaultMaxSocketsPerProxyChain;
  const int kMin = 6;
  const int kMax = 99;

  // Starts off at default value.
  EXPECT_EQ(net::kDefaultMaxSocketsPerProxyChain,
            GetGlobalMaxConnectionsPerProxyChain());

  // Anything less than kMin saturates to kMin.
  service()->SetMaxConnectionsPerProxyChain(kMin - 1);
  EXPECT_EQ(kMin, GetGlobalMaxConnectionsPerProxyChain());

  // Anything larger than kMax saturates to kMax
  service()->SetMaxConnectionsPerProxyChain(kMax + 1);
  EXPECT_EQ(kMax, GetGlobalMaxConnectionsPerProxyChain());

  // Anything in between kMin and kMax should be set exactly.
  service()->SetMaxConnectionsPerProxyChain(58);
  EXPECT_EQ(58, GetGlobalMaxConnectionsPerProxyChain());

  // Negative values select the default.
  service()->SetMaxConnectionsPerProxyChain(-2);
  EXPECT_EQ(kDefault, GetGlobalMaxConnectionsPerProxyChain());

  // Restore the default value to minize sideffects.
  service()->SetMaxConnectionsPerProxyChain(kDefault);
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

TEST_F(NetworkServiceTest, SetMaskedDomainList) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnableIpProtectionProxy,
       network::features::kMaskedDomainList},
      {});

  masked_domain_list::MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");

  service()->UpdateMaskedDomainList(
      mojo_base::ProtoWrapper(mdl),
      /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(service()->masked_domain_list_manager()->IsPopulated());
}

class TestCookieEncryptionProvider : public mojom::CookieEncryptionProvider {
 public:
  TestCookieEncryptionProvider() = default;

  mojo::PendingRemote<network::mojom::CookieEncryptionProvider> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  MOCK_METHOD(void, GetEncryptor, (GetEncryptorCallback callback), (override));

 private:
  mojo::Receiver<mojom::CookieEncryptionProvider> receiver_{this};
};

class NetworkServiceCookieTest
    : public NetworkServiceTest,
      public testing::WithParamInterface<
          std::tuple</*enable_encryption*/ bool, /*set_provider*/ bool>> {
 protected:
  bool IsEncryptionEnabled() const { return std::get<0>(GetParam()); }
  bool ShouldSetEncryptionProvider() const { return std::get<1>(GetParam()); }
};

// This test verifies that SetCookieEncryptionProvider API on the
// network_service functions correctly. In the case where
// SetCookieEncryptionProvider is called with a provider, and
// enable_encrypted_cookies is on, then the GetEncryptor method is called and
// the returned Encryptor is used for encryption.
TEST_P(NetworkServiceCookieTest, CookieEncryptionProvider) {
  const auto cookie_path = base::FilePath(FILE_PATH_LITERAL("Cookies"));
  testing::StrictMock<TestCookieEncryptionProvider> provider;
  std::optional<base::ScopedClosureRunner> maybe_teardown_os_crypt;

  mojom::NetworkContextParamsPtr params = CreateContextParams();

  if (ShouldSetEncryptionProvider()) {
    params->cookie_encryption_provider = provider.BindRemote();
    if (IsEncryptionEnabled()) {
      EXPECT_CALL(provider, GetEncryptor)
          .WillOnce(
              [](network::mojom::CookieEncryptionProvider::GetEncryptorCallback
                     callback) {
                std::move(callback).Run(
                    os_crypt_async::GetTestEncryptorForTesting());
              });
    }
  } else {
    if (IsEncryptionEnabled()) {
      // If encryption is enabled but a CookieEncryptionProvider is not
      // provided, then network service uses OSCrypt. This requires a valid key,
      // so obtain one from the mocker.
      OSCryptMocker::SetUp();
      maybe_teardown_os_crypt.emplace(base::ScopedClosureRunner(
          base::BindOnce([]() { OSCryptMocker::TearDown(); })));
    }
  }

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  params->enable_encrypted_cookies = IsEncryptionEnabled();
  params->file_paths->data_directory = temp_dir.GetPath();
  params->file_paths->cookie_database_name = cookie_path;

  mojo::Remote<mojom::NetworkContext> network_context;
  service()->CreateNetworkContext(network_context.BindNewPipeAndPassReceiver(),
                                  std::move(params));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  const char kSecretValue[] = "SUPERSECRET1234";
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "TestCookie", kSecretValue, "www.test.com", "/", base::Time::Now(),
      base::Time::Now() + base::Days(1), base::Time(), base::Time(),
      /*secure=*/true, /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT);
  base::test::TestFuture<net::CookieAccessResult> future;
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      net::CookieOptions(), future.GetCallback());
  ASSERT_TRUE(future.Take().status.IsInclude());

  base::RunLoop flush_loop;
  cookie_manager->FlushCookieStore(flush_loop.QuitClosure());
  flush_loop.Run();

  base::RunLoop run_loop;
  network_context.set_disconnect_handler(run_loop.QuitClosure());
  // This closes the cookie file, allowing the Cookie file to be safely read,
  // and the temp directory to be deleted.
  DestroyService();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_dir.GetPath().Append(cookie_path),
                                     &contents));
  bool expect_encrypted_data = IsEncryptionEnabled();

  if (IsEncryptionEnabled()) {
    if (ShouldSetEncryptionProvider()) {
      // The test os_crypt_async::Encryptor uses a key ring with '_' as the
      // provider name, so the encrypted text will always contain this marker.
      EXPECT_NE(contents.find("_"), std::string::npos);
    } else {
      // cookie_config::GetCookieCryptoDelegate only returns a valid OSCrypt
      // crypto delegate on some platforms. On other platforms, there is no
      // cookie crypto as it's handled by the OS.
#if !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
      BUILDFLAG(IS_CHROMEOS))
      expect_encrypted_data = false;
#endif
    }
  }

  if (expect_encrypted_data) {
    EXPECT_EQ(contents.find(kSecretValue), std::string::npos);
  } else {
    EXPECT_NE(contents.find(kSecretValue), std::string::npos);
  }
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         NetworkServiceCookieTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         [](const auto& info) {
                           return base::StringPrintf(
                               "%s_%s",
                               std::get<0>(info.param) ? "crypt" : "no_crypt",
                               std::get<1>(info.param) ? "provider"
                                                       : "no_provider");
                         });

class NetworkServiceTestWithService : public testing::Test {
 public:
  NetworkServiceTestWithService()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  NetworkServiceTestWithService(const NetworkServiceTestWithService&) = delete;
  NetworkServiceTestWithService& operator=(
      const NetworkServiceTestWithService&) = delete;

  ~NetworkServiceTestWithService() override = default;

  virtual mojom::NetworkServiceParamsPtr GetParams() {
    return mojom::NetworkServiceParams::New();
  }

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kServicesTestData));
    ASSERT_TRUE(test_server_.Start());
    service_ = std::make_unique<NetworkService>(
        nullptr, network_service_.BindNewPipeAndPassReceiver(),
        /*delay_initialization_until_set_client=*/true);
    service_->Initialize(GetParams());
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
    params->is_orb_enabled = false;
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

  base::File log_file(log_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  network_service_->StartNetLog(
      std::move(log_file), net::FileNetLogObserver::kNoLimit,
      net::NetLogCaptureMode::kDefault,
      base::Value::Dict().Set("amiatest", "iamatest"));
  CreateNetworkContext();
  LoadURL(test_server()->GetURL("/echo"));
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  // |log_file| is closed on destruction of the NetworkService.
  Shutdown();

  // |log_file| is closed on another thread, so have to wait for that to happen.
  task_environment_.RunUntilIdle();

  base::Value::Dict log_dict = base::test::ParseJsonDictFromFile(log_path);
  ASSERT_EQ(*log_dict.FindStringByDottedPath("constants.amiatest"), "iamatest");

  // The log should have a "polledData" list.
  ASSERT_TRUE(log_dict.FindList("polledData"));
}

// Verifies that a passed net log file is successfully opened and sane data
// written to it up until the max file size.
TEST_F(NetworkServiceTestWithService, StartsNetLogBounded) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_dir = temp_dir.GetPath();
  base::FilePath log_path =
      log_dir.Append(FILE_PATH_LITERAL("test_log_bounded.json"));

  // For testing, have a max log size of 1 MB. 1024*1024 == 2^20 == left shift
  // by 20 bits
  const uint64_t kMaxSizeBytes = 1 << 20;
  base::File log_file(log_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  network_service_->StartNetLog(std::move(log_file), kMaxSizeBytes,
                                net::NetLogCaptureMode::kEverything,
                                base::Value::Dict());
  CreateNetworkContext();

  // Through trial and error it was found that this looping navigation results
  // in a ~2MB unbounded net-log file. Since our bounded net-log is limited to
  // 1MB this is fine.

  // This string is roughly 8KB;
  const std::string kManyAs(8192, 'a');
  for (int i = 0; i < 30; i++) {
    LoadURL(test_server()->GetURL("/echo?" + kManyAs));
    EXPECT_EQ(net::OK, client()->completion_status().error_code);
  }

  // |log_file| is closed on destruction of the NetworkService.
  Shutdown();

  // |log_file| is closed on another thread, so have to wait for that to happen.
  task_environment_.RunUntilIdle();

  base::Value::Dict log_dict = base::test::ParseJsonDictFromFile(log_path);

  base::File log_file_read(log_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File::Info file_info;
  log_file_read.GetInfo(&file_info);

  // The max size is only a rough bound, so let's make sure the final file is
  // within a reasonable range from our max. Let's say 10%.
  const int64_t kMaxSizeUpper = kMaxSizeBytes * 1.1;
  const int64_t kMaxSizeLower = kMaxSizeBytes * 0.9;
  EXPECT_GT(file_info.size, kMaxSizeLower);
  EXPECT_LT(file_info.size, kMaxSizeUpper);
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
  loader()->FollowRedirect({}, {}, {}, std::nullopt);
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
      R"( { "https://issuer.example": { "PrivateStateTokenV3PMB": {
        "protocol_version": "PrivateStateTokenV3PMB", "id": 1,
        "batchsize": 5 } } } )",
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
          [&](const std::optional<std::vector<net::NetworkInterface>>& list) {
            EXPECT_NE(std::nullopt, list);
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

  ~TestNetworkChangeManagerClient() override = default;

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

  ~NetworkChangeTest() override = default;

  NetworkService* service() const { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<NetworkService> service_;
};

TEST_F(NetworkChangeTest, NetworkChangeManagerRequest) {
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

  ~NetworkServiceNetworkChangeTest() override = default;

  mojom::NetworkService* service() { return network_service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  mojo::Remote<mojom::NetworkService> network_service_;
  std::unique_ptr<NetworkService> service_;
};

TEST_F(NetworkServiceNetworkChangeTest, NetworkChangeManagerRequest) {
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

  void CreateNetworkContext(mojom::NetworkContextParamsPtr context_params =
                                mojom::NetworkContextParams::New()) {
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
    params->is_orb_enabled = false;
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
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
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

class TestNetworkAnnotationMonitor : public mojom::NetworkAnnotationMonitor {
 public:
  mojo::PendingRemote<mojom::NetworkAnnotationMonitor> GetClient() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  TestNetworkAnnotationMonitor() : expected_hash_code_(0) {}

  // Alternative constructor which allows waiting for `expected_hash_code` to be
  // reported via `WaitForHashCode()`.
  explicit TestNetworkAnnotationMonitor(int32_t expected_hash_code)
      : expected_hash_code_(expected_hash_code) {}

  void Report(int32_t hash_code) override {
    reported_hash_codes_.push_back(hash_code);
    if (hash_code == expected_hash_code_) {
      run_loop_.Quit();
    }
  }

  void WaitForHashCode() { run_loop_.Run(); }

  const std::vector<int32_t> reported_hash_codes() {
    return reported_hash_codes_;
  }

 private:
  mojo::Receiver<mojom::NetworkAnnotationMonitor> receiver_{this};
  std::vector<int32_t> reported_hash_codes_;
  const int32_t expected_hash_code_;
  base::RunLoop run_loop_;
};

TEST_F(NetworkServiceNetworkDelegateTest, NetworkAnnotationMonitor) {
  CreateNetworkContext();

  TestNetworkAnnotationMonitor monitor;
  service()->SetNetworkAnnotationMonitor(monitor.GetClient());
  LoadURL(https_server()->GetURL("/foo"));
  task_environment()->RunUntilIdle();

  std::vector<int32_t> expected_hash_codes = {
      TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code};
  EXPECT_EQ(expected_hash_codes, monitor.reported_hash_codes());
}

#if BUILDFLAG(ENABLE_WEBSOCKETS)
// Verify that network requests without a loader are reported to Network
// Annotation Monitor. This test uses a PAC fetch as an example of such request.
TEST_F(NetworkServiceNetworkDelegateTest,
       NetworkAnnotationMonitorWithoutLoader) {
  net::NetworkTrafficAnnotationTag kTestPacFetchAnnotation =
      net::DefineNetworkTrafficAnnotation("test_pac_fetch", "");
  TestNetworkAnnotationMonitor monitor(
      kTestPacFetchAnnotation.unique_id_hash_code);
  service()->SetNetworkAnnotationMonitor(monitor.GetClient());

  // Setup NetworkContext with proxy config. This will enable PAC fetch.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  TestMojoProxyResolverFactory proxy_resolver_factory;
  context_params->proxy_resolver_factory =
      proxy_resolver_factory.CreateFactoryRemote();
  context_params->initial_proxy_config =
      net::ProxyConfigWithAnnotation(net::ProxyConfig::CreateFromCustomPacURL(
                                         GURL("https://not.a.real.proxy.test")),
                                     kTestPacFetchAnnotation);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  CreateNetworkContext(std::move(context_params));

  // Load an arbitrary URL. This should trigger the PAC fetch.
  LoadURL(https_server()->GetURL("/foo"));

  // Verify PAC fetch annotation was reported.
  monitor.WaitForHashCode();
}
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

class NetworkServiceTestWithSystemDnsResolver
    : public NetworkServiceTestWithService {
 public:
  NetworkServiceTestWithSystemDnsResolver() = default;
  NetworkServiceTestWithSystemDnsResolver(
      const NetworkServiceTestWithSystemDnsResolver&) = delete;
  NetworkServiceTestWithSystemDnsResolver& operator=(
      const NetworkServiceTestWithSystemDnsResolver&) = delete;
  ~NetworkServiceTestWithSystemDnsResolver() override = default;

  mojom::NetworkServiceParamsPtr GetParams() override {
    auto params = mojom::NetworkServiceParams::New();
    params->system_dns_resolver =
        system_dns_resolver_pending_receiver_.InitWithNewPipeAndPassRemote();
    return params;
  }

 protected:
  mojo::PendingReceiver<mojom::SystemDnsResolver>
      system_dns_resolver_pending_receiver_;
};

class StubHostResolverClient : public mojom::ResolveHostClient {
 public:
  using ResolveHostCallback = base::OnceCallback<void(net::AddressList)>;

  explicit StubHostResolverClient(
      mojo::PendingReceiver<mojom::ResolveHostClient> receiver,
      ResolveHostCallback resolve_host_callback)
      : receiver_(this, std::move(receiver)),
        resolve_host_callback_(std::move(resolve_host_callback)) {}

  StubHostResolverClient(const StubHostResolverClient&) = delete;
  StubHostResolverClient& operator=(const StubHostResolverClient&) = delete;
  ~StubHostResolverClient() override = default;

  void OnTextResults(const std::vector<std::string>& text_results) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
  }
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    std::move(resolve_host_callback_)
        .Run(resolved_addresses.value_or(net::AddressList()));
  }

 private:
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_;
  ResolveHostCallback resolve_host_callback_;
};

TEST_F(NetworkServiceTestWithSystemDnsResolver,
       HandlesDeadSystemDnsResolverService) {
  CreateNetworkContext();

  // Kill the SystemDnsResolver pipe.
  system_dns_resolver_pending_receiver_.reset();

  // Call ResolveHost() and force it to use the SYSTEM dns resolver without
  // cache or DoH. This will attempt to call back into the SystemDnsResolver,
  // whose pipe is dead.
  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->initial_priority = net::RequestPriority::HIGHEST;
  // Use the SYSTEM resolver, and don't allow the cache or attempt DoH.
  parameters->source = net::HostResolverSource::SYSTEM;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
  parameters->secure_dns_policy = network::mojom::SecureDnsPolicy::DISABLE;
  mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver;
  network_context_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("hostname1", 80)),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      receiver.InitWithNewPipeAndPassRemote());

  // Wait until the ResolveHost() call is done and make sure it returns an empty
  // AddressList.
  base::RunLoop run_loop;
  auto stub_host_resolver_client = std::make_unique<StubHostResolverClient>(
      std::move(receiver),
      base::BindLambdaForTesting([&run_loop](net::AddressList address_list) {
        ASSERT_TRUE(address_list.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(NetworkServiceTest, NetworkAnnotationMonitor) {
  TestNetworkAnnotationMonitor monitor;

  // Hash codes should not be reported until NetworkAnnotationMonitor is set.
  service()->NotifyNetworkRequestWithAnnotation(TRAFFIC_ANNOTATION_FOR_TESTS);
  task_environment()->RunUntilIdle();
  EXPECT_THAT(monitor.reported_hash_codes(), testing::IsEmpty());

  service()->SetNetworkAnnotationMonitor(monitor.GetClient());
  service()->NotifyNetworkRequestWithAnnotation(TRAFFIC_ANNOTATION_FOR_TESTS);
  task_environment()->RunUntilIdle();
  std::vector<int32_t> expected_hash_codes = {
      TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code};
  EXPECT_EQ(expected_hash_codes, monitor.reported_hash_codes());
}

}  // namespace

}  // namespace network
