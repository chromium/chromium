// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/cache_type.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_resolver_impl.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_manager.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/udp_socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_test_util.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace network {

namespace {

const GURL kURL("http://foo.com");
const GURL kOtherURL("http://other.com");
constexpr char kMockHost[] = "mock.host";

void StoreBool(bool* result, const base::Closure& callback, bool value) {
  *result = value;
  callback.Run();
}

void StoreValue(base::Value* result,
                const base::Closure& callback,
                base::Value value) {
  *result = std::move(value);
  callback.Run();
}

mojom::NetworkContextParamsPtr CreateContextParams() {
  mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

void SetContentSetting(const GURL& primary_pattern,
                       const GURL& secondary_pattern,
                       ContentSetting setting,
                       NetworkContext* network_context) {
  network_context->cookie_manager()->SetContentSettings(
      {ContentSettingPatternSource(
          ContentSettingsPattern::FromURL(primary_pattern),
          ContentSettingsPattern::FromURL(secondary_pattern),
          base::Value(setting), std::string(), false)});
}

void SetDefaultContentSetting(ContentSetting setting,
                              NetworkContext* network_context) {
  network_context->cookie_manager()->SetContentSettings(
      {ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                   ContentSettingsPattern::Wildcard(),
                                   base::Value(setting), std::string(),
                                   false)});
}

std::unique_ptr<TestURLLoaderClient> FetchRequest(
    const ResourceRequest& request,
    NetworkContext* network_context) {
  mojom::URLLoaderFactoryPtr loader_factory;
  auto params = mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                          std::move(params));

  auto client = std::make_unique<TestURLLoaderClient>();
  mojom::URLLoaderPtr loader;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client->CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client->RunUntilComplete();
  return client;
}

// ProxyLookupClient that drives proxy lookups and can wait for the responses to
// be received.
class TestProxyLookupClient : public mojom::ProxyLookupClient {
 public:
  TestProxyLookupClient() : binding_(this) {}
  ~TestProxyLookupClient() override = default;

  void StartLookUpProxyForURL(const GURL& url,
                              mojom::NetworkContext* network_context) {
    // Make sure this method is called at most once.
    EXPECT_FALSE(binding_.is_bound());

    mojom::ProxyLookupClientPtr proxy_lookup_client;
    binding_.Bind(mojo::MakeRequest(&proxy_lookup_client));
    network_context->LookUpProxyForURL(url, std::move(proxy_lookup_client));
  }

  void WaitForResult() { run_loop_.Run(); }

  // mojom::ProxyLookupClient implementation:
  void OnProxyLookupComplete(
      const base::Optional<net::ProxyInfo>& proxy_info) override {
    EXPECT_FALSE(is_done_);
    EXPECT_FALSE(proxy_info_);

    is_done_ = true;
    proxy_info_ = proxy_info;
    binding_.Close();
    run_loop_.Quit();
  }

  const base::Optional<net::ProxyInfo>& proxy_info() const {
    return proxy_info_;
  }
  bool is_done() const { return is_done_; }

 private:
  mojo::Binding<mojom::ProxyLookupClient> binding_;

  bool is_done_ = false;
  base::Optional<net::ProxyInfo> proxy_info_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyLookupClient);
};

class NetworkContextTest : public testing::Test,
                           public net::SSLConfigService::Observer {
 public:
  NetworkContextTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()) {}
  ~NetworkContextTest() override {}

  std::unique_ptr<NetworkContext> CreateContextWithParams(
      mojom::NetworkContextParamsPtr context_params) {
    return std::make_unique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
        std::move(context_params));
  }

  // Searches through |backend|'s stats to discover its type. Only supports
  // blockfile and simple caches.
  net::URLRequestContextBuilder::HttpCacheParams::Type GetBackendType(
      disk_cache::Backend* backend) {
    base::StringPairs stats;
    backend->GetStats(&stats);
    for (const auto& pair : stats) {
      if (pair.first != "Cache type")
        continue;

      if (pair.second == "Simple Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
      if (pair.second == "Blockfile Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE;
      break;
    }

    NOTREACHED();
    return net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
  }

  mojom::NetworkService* network_service() const {
    return network_service_.get();
  }

  void OnSSLConfigChanged() override { ++ssl_config_changed_count_; }

  // Looks up a value with the given name from the NetworkContext's
  // TransportSocketPool info dictionary.
  int GetSocketPoolInfo(NetworkContext* context, base::StringPiece name) {
    int value = -1;
    context->url_request_context()
        ->http_transaction_factory()
        ->GetSession()
        ->GetTransportSocketPool(
            net::HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL)
        ->GetInfoAsValue("", "", false)
        ->GetInteger(name, &value);
    return value;
  }

  // Looks up a value with the given name from the NetworkContext's
  // SSLSocketPool info dictionary.
  int GetSSLSocketPoolInfo(NetworkContext* context, base::StringPiece name) {
    int value;
    context->url_request_context()
        ->http_transaction_factory()
        ->GetSession()
        ->GetSSLSocketPool(
            net::HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL)
        ->GetInfoAsValue("", "", false)
        ->GetInteger(name, &value);
    return value;
  }

  int GetSocketCount(NetworkContext* network_context) {
    return GetSocketPoolInfo(network_context, "idle_socket_count") +
           GetSocketPoolInfo(network_context, "connecting_socket_count") +
           GetSocketPoolInfo(network_context, "handed_out_socket_count");
  }

  int GetSSLSocketCount(NetworkContext* network_context) {
    return GetSSLSocketPoolInfo(network_context, "idle_socket_count") +
           GetSSLSocketPoolInfo(network_context, "connecting_socket_count") +
           GetSSLSocketPoolInfo(network_context, "handed_out_socket_count");
  }

  GURL GetHttpUrlFromHttps(const GURL& https_url) {
    url::Replacements<char> replacements;
    const char http[] = "http";
    replacements.SetScheme(http, url::Component(0, strlen(http)));
    return https_url.ReplaceComponents(replacements);
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  // Stores the NetworkContextPtr of the most recently created NetworkContext.
  // Not strictly needed, but seems best to mimic real-world usage.
  mojom::NetworkContextPtr network_context_ptr_;
  int ssl_config_changed_count_ = 0;
};

TEST_F(NetworkContextTest, DestroyContextWithLiveRequest) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/hung-after-headers");

  mojom::URLLoaderFactoryPtr loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                          std::move(params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilResponseReceived();
  EXPECT_TRUE(client.has_received_response());
  EXPECT_FALSE(client.has_received_completion());

  // Destroying the loader factory should not delete the URLLoader.
  loader_factory.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client.has_received_completion());

  // Destroying the NetworkContext should result in destroying the loader and
  // the client receiving a connection error.
  network_context.reset();

  client.RunUntilConnectionError();
  EXPECT_FALSE(client.has_received_completion());
}

TEST_F(NetworkContextTest, DisableQuic) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  // By default, QUIC should be enabled for new NetworkContexts when the command
  // line indicates it should be.
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .enable_quic);

  // Disabling QUIC should disable it on existing NetworkContexts.
  network_service()->DisableQuic();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC should disable it new NetworkContexts.
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context2->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC again should be harmless.
  network_service()->DisableQuic();
  std::unique_ptr<NetworkContext> network_context3 =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context3->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);
}

TEST_F(NetworkContextTest, UserAgentAndLanguage) {
  const char kUserAgent[] = "Chromium Unit Test";
  const char kAcceptLanguage[] = "en-US,en;q=0.9,uk;q=0.8";
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->user_agent = kUserAgent;
  // Not setting accept_language, to test the default.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  EXPECT_EQ(kUserAgent, network_context->url_request_context()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ("", network_context->url_request_context()
                    ->http_user_agent_settings()
                    ->GetAcceptLanguage());

  // Change accept-language.
  network_context->SetAcceptLanguage(kAcceptLanguage);
  EXPECT_EQ(kUserAgent, network_context->url_request_context()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ(kAcceptLanguage, network_context->url_request_context()
                                 ->http_user_agent_settings()
                                 ->GetAcceptLanguage());

  // Create with custom accept-language configured.
  params = CreateContextParams();
  params->user_agent = kUserAgent;
  params->accept_language = kAcceptLanguage;
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(std::move(params));
  EXPECT_EQ(kUserAgent, network_context2->url_request_context()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ(kAcceptLanguage, network_context2->url_request_context()
                                 ->http_user_agent_settings()
                                 ->GetAcceptLanguage());
}

TEST_F(NetworkContextTest, EnableBrotli) {
  for (bool enable_brotli : {true, false}) {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    context_params->enable_brotli = enable_brotli;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));
    EXPECT_EQ(enable_brotli,
              network_context->url_request_context()->enable_brotli());
  }
}

TEST_F(NetworkContextTest, ContextName) {
  const char kContextName[] = "Jim";
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->context_name = std::string(kContextName);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kContextName, network_context->url_request_context()->name());
}

TEST_F(NetworkContextTest, QuicUserAgentId) {
  const char kQuicUserAgentId[] = "007";
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->quic_user_agent_id = kQuicUserAgentId;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kQuicUserAgentId, network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->params()
                                  .quic_user_agent_id);
}

TEST_F(NetworkContextTest, DisableDataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_data_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, EnableDataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_data_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, DisableFileUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_file_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
TEST_F(NetworkContextTest, EnableFileUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_file_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}
#endif  // !BUILDFLAG(DISABLE_FILE_SUPPORT)

TEST_F(NetworkContextTest, DisableFtpUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_ftp_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST_F(NetworkContextTest, EnableFtpUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_ftp_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(NetworkContextTest, DisableReporting) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context->url_request_context()->reporting_service());
}

TEST_F(NetworkContextTest, EnableReporting) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(network_context->url_request_context()->reporting_service());
}

TEST_F(NetworkContextTest, DisableNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kNetworkErrorLogging);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(
      network_context->url_request_context()->network_error_logging_service());
}

TEST_F(NetworkContextTest, EnableNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(
      network_context->url_request_context()->network_error_logging_service());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

TEST_F(NetworkContextTest, Http09Disabled) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_09_on_non_default_ports_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .http_09_on_non_default_ports_enabled);
}

TEST_F(NetworkContextTest, Http09Enabled) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_09_on_non_default_ports_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .http_09_on_non_default_ports_enabled);
}

TEST_F(NetworkContextTest, DefaultHttpNetworkSessionParams) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const net::HttpNetworkSession::Params& params =
      network_context->url_request_context()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_TRUE(params.enable_http2);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
  EXPECT_TRUE(params.origins_to_force_quic_on.empty());
  EXPECT_FALSE(params.enable_user_alternate_protocol_ports);
  EXPECT_FALSE(params.ignore_certificate_errors);
  EXPECT_EQ(0, params.testing_fixed_http_port);
  EXPECT_EQ(0, params.testing_fixed_https_port);
}

// Make sure that network_session_configurator is hooked up.
TEST_F(NetworkContextTest, FixedHttpPort) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpPort, "800");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpsPort, "801");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const net::HttpNetworkSession::Params& params =
      network_context->url_request_context()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_EQ(800, params.testing_fixed_http_port);
  EXPECT_EQ(801, params.testing_fixed_https_port);
}

TEST_F(NetworkContextTest, NoCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetCache());
}

TEST_F(NetworkContextTest, MemoryCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::MEMORY_CACHE, backend->GetCacheType());
}

TEST_F(NetworkContextTest, DiskCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::DISK_CACHE, backend->GetCacheType());
  EXPECT_EQ(network_session_configurator::ChooseCacheType(
                *base::CommandLine::ForCurrentProcess()),
            GetBackendType(backend));
}

// This makes sure that network_session_configurator::ChooseCacheType is
// connected to NetworkContext.
TEST_F(NetworkContextTest, SimpleCache) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseSimpleCacheBackend, "on");
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            GetBackendType(backend));
}

TEST_F(NetworkContextTest, HttpServerPropertiesToDisk) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo");
  EXPECT_FALSE(base::PathExists(file_path));

  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  // Create a context with on-disk storage of HTTP server properties.
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk, and sanity check initial state.
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));

  // Set a property.
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, true);
  // Deleting the context will cause it to flush state. Wait for the pref
  // service to flush to disk.
  network_context.reset();
  scoped_task_environment_.RunUntilIdle();

  // Create a new NetworkContext using the same path for HTTP server properties.
  context_params = mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  network_context = CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(network_context->url_request_context()
                  ->http_server_properties()
                  ->GetSupportsSpdy(kSchemeHostPort));

  // Now check that ClearNetworkingHistorySince clears the data.
  base::RunLoop run_loop2;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));

  // Clear destroy the network context and let any pending writes complete
  // before destroying |temp_dir|, to avoid leaking any files.
  network_context.reset();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir.Delete());
}

// Checks that ClearNetworkingHistorySince() works clears in-memory pref stores,
// and invokes the closure passed to it.
TEST_F(NetworkContextTest, ClearHttpServerPropertiesInMemory) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());

  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, true);
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_server_properties()
                  ->GetSupportsSpdy(kSchemeHostPort));

  base::RunLoop run_loop;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));
}

// Test that TransportSecurity state is persisted (or not) as expected.
TEST_F(NetworkContextTest, TransportSecurityStatePersisted) {
  const char kDomain[] = "foo.test";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath transport_security_persister_path = temp_dir.GetPath();
  base::FilePath transport_security_persister_file_path =
      transport_security_persister_path.AppendASCII("TransportSecurity");
  EXPECT_FALSE(base::PathExists(transport_security_persister_file_path));

  for (bool on_disk : {false, true}) {
    // Create a NetworkContext.
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    if (on_disk) {
      context_params->transport_security_persister_path =
          transport_security_persister_path;
    }
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    // Add an STS entry.
    net::TransportSecurityState::STSState sts_state;
    net::TransportSecurityState* state =
        network_context->url_request_context()->transport_security_state();
    EXPECT_FALSE(state->GetDynamicSTSState(kDomain, &sts_state));
    state->AddHSTS(kDomain,
                   base::Time::Now() + base::TimeDelta::FromSecondsD(1000),
                   false /* include subdomains */);
    EXPECT_TRUE(state->GetDynamicSTSState(kDomain, &sts_state));
    ASSERT_EQ(kDomain, sts_state.domain);

    // Destroy the network context, and wait for all tasks to write state to
    // disk to finish running.
    network_context.reset();
    scoped_task_environment_.RunUntilIdle();
    EXPECT_EQ(on_disk,
              base::PathExists(transport_security_persister_file_path));

    // Create a new NetworkContext,with the same parameters, and check if the
    // added STS entry still exists.
    context_params = CreateContextParams();
    if (on_disk) {
      context_params->transport_security_persister_path =
          transport_security_persister_path;
    }
    network_context = CreateContextWithParams(std::move(context_params));
    // Wait for the entry to load.
    scoped_task_environment_.RunUntilIdle();
    state = network_context->url_request_context()->transport_security_state();
    ASSERT_EQ(on_disk, state->GetDynamicSTSState(kDomain, &sts_state));
    if (on_disk)
      EXPECT_EQ(kDomain, sts_state.domain);
  }
}

// Test that PKP failures are reported if and only if certificate reporting is
// enabled.
TEST_F(NetworkContextTest, CertReporting) {
  const char kPreloadedPKPHost[] = "with-report-uri-pkp.preloaded.test";
  const char kReportHost[] = "report-uri.preloaded.test";
  const char kReportPath[] = "/pkp";

  for (bool reporting_enabled : {false, true}) {
    // Server that PKP reports are sent to.
    net::test_server::EmbeddedTestServer report_test_server;
    net::test_server::ControllableHttpResponse controllable_response(
        &report_test_server, kReportPath);
    ASSERT_TRUE(report_test_server.Start());

    // Configure the TransportSecurityStateSource so that kPreloadedPKPHost will
    // have static PKP pins set, with a report URI on kReportHost.
    net::ScopedTransportSecurityStateSource scoped_security_state_source(
        report_test_server.port());

    // Configure a test HTTPS server.
    net::test_server::EmbeddedTestServer pkp_test_server(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    pkp_test_server.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(pkp_test_server.Start());

    // Configure mock cert verifier to cause the PKP check to fail.
    net::CertVerifyResult result;
    result.verified_cert = net::CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "ok_cert.pem",
        net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    ASSERT_TRUE(result.verified_cert);
    net::SHA256HashValue hash = {{0x00, 0x01}};
    result.public_key_hashes.push_back(net::HashValue(hash));
    result.is_issued_by_known_root = true;
    net::MockCertVerifier mock_verifier;
    mock_verifier.AddResultForCert(pkp_test_server.GetCertificate(), result,
                                   net::OK);
    NetworkContext::SetCertVerifierForTesting(&mock_verifier);

    // Configure a MockHostResolver to map requests to kPreloadedPKPHost and
    // kReportHost to the test servers:
    scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc =
        base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);
    mock_resolver_proc->AddIPLiteralRule(
        kPreloadedPKPHost, pkp_test_server.GetIPLiteralString(), std::string());
    mock_resolver_proc->AddIPLiteralRule(
        kReportHost, report_test_server.GetIPLiteralString(), std::string());
    net::ScopedDefaultHostResolverProc scoped_default_host_resolver(
        mock_resolver_proc.get());

    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    EXPECT_FALSE(context_params->enable_certificate_reporting);
    context_params->enable_certificate_reporting = reporting_enabled;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    // Enable static pins so that requests made to kPreloadedPKPHost will check
    // the pins, and send a report if the pinning check fails.
    network_context->url_request_context()
        ->transport_security_state()
        ->EnableStaticPinsForTesting();

    ResourceRequest request;
    request.url = pkp_test_server.GetURL(kPreloadedPKPHost, "/");

    mojom::URLLoaderFactoryPtr loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                            std::move(params));

    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
              client.completion_status().error_code);

    if (reporting_enabled) {
      // If reporting is enabled, wait to see the request from the ReportSender.
      // Don't respond to the request, effectively making it a hung request.
      controllable_response.WaitForRequest();
    } else {
      // Otherwise, there should be no pending URLRequest.
      // |controllable_response| will cause requests to hang, so if there's no
      // URLRequest, then either a reporting request was never started. This
      // relies on reported being sent immediately for correctness.
      network_context->url_request_context()->AssertNoURLRequests();
    }

    // Destroy the network context. This serves to check the case that reporting
    // requests are alive when a NetworkContext is torn down.
    network_context.reset();

    // Remove global reference to the MockCertVerifier before it falls out of
    // scope.
    NetworkContext::SetCertVerifierForTesting(nullptr);
  }
}

// Test that valid referrers are allowed, while invalid ones result in errors.
TEST_F(NetworkContextTest, Referrers) {
  const GURL kReferrer = GURL("http://referrer/");
  net::test_server::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  for (bool validate_referrer_policy_on_initial_request : {false, true}) {
    for (net::URLRequest::ReferrerPolicy referrer_policy :
         {net::URLRequest::NEVER_CLEAR_REFERRER,
          net::URLRequest::NO_REFERRER}) {
      mojom::NetworkContextParamsPtr context_params = CreateContextParams();
      context_params->validate_referrer_policy_on_initial_request =
          validate_referrer_policy_on_initial_request;
      std::unique_ptr<NetworkContext> network_context =
          CreateContextWithParams(std::move(context_params));

      mojom::URLLoaderFactoryPtr loader_factory;
      mojom::URLLoaderFactoryParamsPtr params =
          mojom::URLLoaderFactoryParams::New();
      params->process_id = 0;
      network_context->CreateURLLoaderFactory(
          mojo::MakeRequest(&loader_factory), std::move(params));

      ResourceRequest request;
      request.url = test_server.GetURL("/echoheader?Referer");
      request.referrer = kReferrer;
      request.referrer_policy = referrer_policy;

      mojom::URLLoaderPtr loader;
      TestURLLoaderClient client;
      loader_factory->CreateLoaderAndStart(
          mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
          0 /* options */, request, client.CreateInterfacePtr(),
          net::MutableNetworkTrafficAnnotationTag(
              TRAFFIC_ANNOTATION_FOR_TESTS));

      client.RunUntilComplete();
      EXPECT_TRUE(client.has_received_completion());

      // If validating referrers, and the referrer policy is not to send
      // referrers, the request should fail.
      if (validate_referrer_policy_on_initial_request &&
          referrer_policy == net::URLRequest::NO_REFERRER) {
        EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
                  client.completion_status().error_code);
        EXPECT_FALSE(client.response_body().is_valid());
        continue;
      }

      // Otherwise, the request should succeed.
      EXPECT_EQ(net::OK, client.completion_status().error_code);
      std::string response_body;
      ASSERT_TRUE(client.response_body().is_valid());
      EXPECT_TRUE(mojo::BlockingCopyToString(client.response_body_release(),
                                             &response_body));
      if (referrer_policy == net::URLRequest::NO_REFERRER) {
        // If not validating referrers, and the referrer policy is not to send
        // referrers, the referrer should be cleared.
        EXPECT_EQ("None", response_body);
      } else {
        // Otherwise, the referrer should be send.
        EXPECT_EQ(kReferrer.spec(), response_body);
      }
    }
  }
}

TEST_F(NetworkContextTest, HttpRequestCompletionErrorCodes) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer https_test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(https_test_server.Start());

  const struct {
    const char* path;
    bool use_https;
    bool is_main_frame;
    int expected_net_error;
    int expected_request_completion_count;
    int expected_request_completion_main_frame_count;
  } kTests[] = {
      {"/", false /* use_https */, true /* is_main_frame */, net::OK,
       1 /* expected_request_completion_count */,
       1 /* expected_request_completion_main_frame_count */},
      {"/close-socket", false /* use_https */, true /* is_main_frame */,
       net::ERR_EMPTY_RESPONSE, 1 /* expected_request_completion_count */,
       1 /* expected_request_completion_main_frame_count */},
      {"/", false /* use_https */, false /* is_main_frame */, net::OK,
       1 /* expected_request_completion_count */,
       0 /* expected_request_completion_main_frame_count */},
      {"/", true /* use_https */, true /* is_main_frame */, net::OK,
       0 /* expected_request_completion_count */,
       0 /* expected_request_completion_main_frame_count */},
  };

  const char kHttpRequestCompletionErrorCode[] =
      "Net.HttpRequestCompletionErrorCodes";
  const char kHttpRequestCompletionErrorCodeMainFrame[] =
      "Net.HttpRequestCompletionErrorCodes.MainFrame";

  for (const auto& test : kTests) {
    base::HistogramTester histograms;

    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateContextParams());

    mojom::URLLoaderFactoryPtr loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                            std::move(params));

    ResourceRequest request;
    if (!test.use_https) {
      request.url = test_server.GetURL(test.path);
    } else {
      request.url = https_test_server.GetURL(test.path);
    }
    if (test.is_main_frame)
      request.load_flags = net::LOAD_MAIN_FRAME_DEPRECATED;

    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(test.expected_net_error, client.completion_status().error_code);

    histograms.ExpectTotalCount(kHttpRequestCompletionErrorCode,
                                test.expected_request_completion_count);
    histograms.ExpectUniqueSample(kHttpRequestCompletionErrorCode,
                                  -test.expected_net_error,
                                  test.expected_request_completion_count);
    histograms.ExpectTotalCount(
        kHttpRequestCompletionErrorCodeMainFrame,
        test.expected_request_completion_main_frame_count);
    histograms.ExpectUniqueSample(
        kHttpRequestCompletionErrorCodeMainFrame, -test.expected_net_error,
        test.expected_request_completion_main_frame_count);
  }
}

// Validates that clearing the HTTP cache when no cache exists does complete.
TEST_F(NetworkContextTest, ClearHttpCacheWithNoCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_EQ(nullptr, cache);
  base::RunLoop run_loop;
  network_context->ClearHttpCache(base::Time(), base::Time(),
                                  nullptr /* filter */,
                                  base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearHttpCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();

  std::vector<std::string> entry_urls = {
      "http://www.google.com",    "https://www.google.com",
      "http://www.wikipedia.com", "https://www.wikipedia.com",
      "http://localhost:1234",    "https://localhost:1234",
  };
  ASSERT_TRUE(cache);
  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  for (const auto& url : entry_urls) {
    disk_cache::Entry* entry = nullptr;
    base::RunLoop run_loop;
    if (backend->CreateEntry(
            url, net::HIGHEST, &entry,
            base::Bind([](base::OnceClosure quit_loop,
                          int rv) { std::move(quit_loop).Run(); },
                       run_loop.QuitClosure())) == net::ERR_IO_PENDING) {
      run_loop.Run();
    }
    entry->Close();
  }
  EXPECT_EQ(entry_urls.size(), static_cast<size_t>(backend->GetEntryCount()));
  base::RunLoop run_loop;
  network_context->ClearHttpCache(base::Time(), base::Time(),
                                  nullptr /* filter */,
                                  base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(0U, static_cast<size_t>(backend->GetEntryCount()));
}

// Checks that when multiple calls are made to clear the HTTP cache, all
// callbacks are invoked.
TEST_F(NetworkContextTest, MultipleClearHttpCacheCalls) {
  constexpr int kNumberOfClearCalls = 10;

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      kNumberOfClearCalls /* num_closures */, run_loop.QuitClosure());
  for (int i = 0; i < kNumberOfClearCalls; i++) {
    network_context->ClearHttpCache(base::Time(), base::Time(),
                                    nullptr /* filter */,
                                    base::BindOnce(barrier_closure));
  }
  run_loop.Run();
  // If all the callbacks were invoked, we should terminate.
}

TEST_F(NetworkContextTest, CountHttpCache) {
  // Just ensure that a couple of concurrent calls go through, and produce
  // the expected "it's empty!" result. More detailed testing is left to
  // HttpCacheDataCounter unit tests.

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  int responses = 0;
  base::RunLoop run_loop;

  auto callback =
      base::BindLambdaForTesting([&](bool upper_bound, int64_t size_or_error) {
        // Don't expect approximation for full range.
        EXPECT_EQ(false, upper_bound);
        EXPECT_EQ(0, size_or_error);
        ++responses;
        if (responses == 2)
          run_loop.Quit();
      });

  network_context->ComputeHttpCacheSize(base::Time(), base::Time::Max(),
                                        callback);
  network_context->ComputeHttpCacheSize(base::Time(), base::Time::Max(),
                                        callback);
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearChannelIds) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  ASSERT_TRUE(network_context->url_request_context()->channel_id_service());

  net::ChannelIDStore* store = network_context->url_request_context()
                                   ->channel_id_service()
                                   ->GetChannelIDStore();
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "google.com", base::Time::FromDoubleT(123),
      crypto::ECPrivateKey::Create()));
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "chromium.org", base::Time::FromDoubleT(456),
      crypto::ECPrivateKey::Create()));

  ASSERT_EQ(2, store->GetChannelIDCount());

  base::RunLoop run_loop;
  network_context->ClearChannelIds(base::Time(), base::Time(),
                                   nullptr /* filter */,
                                   base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(0, store->GetChannelIDCount());
}

TEST_F(NetworkContextTest, ClearEmptyChannelIds) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  ASSERT_TRUE(network_context->url_request_context()->channel_id_service());

  net::ChannelIDStore* store = network_context->url_request_context()
                                   ->channel_id_service()
                                   ->GetChannelIDStore();
  ASSERT_EQ(0, store->GetChannelIDCount());

  base::RunLoop run_loop;
  network_context->ClearChannelIds(base::Time(), base::Time(),
                                   nullptr /* filter */,
                                   base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(0, store->GetChannelIDCount());
}

void GetAllChannelIdsCallback(
    base::RunLoop* run_loop,
    net::ChannelIDStore::ChannelIDList* dest,
    const net::ChannelIDStore::ChannelIDList& result) {
  *dest = result;
  run_loop->Quit();
}

TEST_F(NetworkContextTest, ClearChannelIdsWithKeepFilter) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  ASSERT_TRUE(network_context->url_request_context()->channel_id_service());

  net::ChannelIDStore* store = network_context->url_request_context()
                                   ->channel_id_service()
                                   ->GetChannelIDStore();
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "google.com", base::Time::FromDoubleT(123),
      crypto::ECPrivateKey::Create()));
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "chromium.org", base::Time::FromDoubleT(456),
      crypto::ECPrivateKey::Create()));

  ASSERT_EQ(2, store->GetChannelIDCount());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop1;
  network_context->ClearChannelIds(base::Time(), base::Time(),
                                   std::move(filter),
                                   base::BindOnce(run_loop1.QuitClosure()));
  run_loop1.Run();

  base::RunLoop run_loop2;
  net::ChannelIDStore::ChannelIDList channel_ids;
  store->GetAllChannelIDs(
      base::BindRepeating(&GetAllChannelIdsCallback, &run_loop2, &channel_ids));
  run_loop2.Run();
  ASSERT_EQ(1u, channel_ids.size());
  EXPECT_EQ("chromium.org", channel_ids.front().server_identifier());
}

TEST_F(NetworkContextTest, ClearChannelIdsWithDeleteFilter) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  ASSERT_TRUE(network_context->url_request_context()->channel_id_service());

  net::ChannelIDStore* store = network_context->url_request_context()
                                   ->channel_id_service()
                                   ->GetChannelIDStore();
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "google.com", base::Time::FromDoubleT(123),
      crypto::ECPrivateKey::Create()));
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "chromium.org", base::Time::FromDoubleT(456),
      crypto::ECPrivateKey::Create()));

  ASSERT_EQ(2, store->GetChannelIDCount());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop1;
  network_context->ClearChannelIds(base::Time(), base::Time(),
                                   std::move(filter),
                                   base::BindOnce(run_loop1.QuitClosure()));
  run_loop1.Run();

  base::RunLoop run_loop2;
  net::ChannelIDStore::ChannelIDList channel_ids;
  store->GetAllChannelIDs(
      base::BindRepeating(&GetAllChannelIdsCallback, &run_loop2, &channel_ids));
  run_loop2.Run();
  ASSERT_EQ(1u, channel_ids.size());
  EXPECT_EQ("google.com", channel_ids.front().server_identifier());
}

TEST_F(NetworkContextTest, ClearChannelIdsWithTimeRange) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  ASSERT_TRUE(network_context->url_request_context()->channel_id_service());

  net::ChannelIDStore* store = network_context->url_request_context()
                                   ->channel_id_service()
                                   ->GetChannelIDStore();
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "google.com", base::Time::FromDoubleT(123),
      crypto::ECPrivateKey::Create()));
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "chromium.org", base::Time::FromDoubleT(456),
      crypto::ECPrivateKey::Create()));
  store->SetChannelID(std::make_unique<net::ChannelIDStore::ChannelID>(
      "gmail.com", base::Time::FromDoubleT(789),
      crypto::ECPrivateKey::Create()));

  ASSERT_EQ(3, store->GetChannelIDCount());

  base::RunLoop run_loop1;
  network_context->ClearChannelIds(
      base::Time::FromDoubleT(450), base::Time::FromDoubleT(460),
      nullptr /* filter */, base::BindOnce(run_loop1.QuitClosure()));
  run_loop1.Run();

  base::RunLoop run_loop2;
  net::ChannelIDStore::ChannelIDList channel_ids;
  store->GetAllChannelIDs(
      base::BindRepeating(&GetAllChannelIdsCallback, &run_loop2, &channel_ids));
  run_loop2.Run();

  std::vector<std::string> identifiers;
  for (const auto& id : channel_ids) {
    identifiers.push_back(id.server_identifier());
  }
  EXPECT_THAT(identifiers,
              testing::UnorderedElementsAre("google.com", "gmail.com"));
}

TEST_F(NetworkContextTest, ClearChannelIdTriggersSslChangeNotification) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  ASSERT_TRUE(network_context->url_request_context()->channel_id_service());

  network_context->url_request_context()->ssl_config_service()->AddObserver(
      this);

  ASSERT_EQ(0, ssl_config_changed_count_);

  base::RunLoop run_loop;
  network_context->ClearChannelIds(base::Time(), base::Time(),
                                   nullptr /* filter */,
                                   base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(1, ssl_config_changed_count_);
}

TEST_F(NetworkContextTest, ClearChannelIdWithNoService) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_channel_id_service(nullptr);

  base::RunLoop run_loop;
  network_context->ClearChannelIds(base::Time(), base::Time(),
                                   nullptr /* filter */,
                                   base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearHostCache) {
  // List of domains added to the host cache before running each test case.
  const char* kDomains[] = {
      "domain0", "domain1", "domain2", "domain3",
  };

  // Each bit correponds to one of the 4 domains above.
  enum Domains {
    NO_DOMAINS = 0x0,
    DOMAIN0 = 0x1,
    DOMAIN1 = 0x2,
    DOMAIN2 = 0x4,
    DOMAIN3 = 0x8,
  };

  const struct {
    // True if the ClearDataFilter should be a nullptr.
    bool null_filter;
    mojom::ClearDataFilter::Type type;
    // Bit field of Domains that appear in the filter. The origin vector is
    // never populated.
    int filter_domains;
    // Only domains that are expected to remain in the host cache.
    int expected_cached_domains;
  } kTestCases[] = {
      // A null filter should delete everything. The filter type and filter
      // domain lists are ignored.
      {
          true /* null_filter */, mojom::ClearDataFilter::Type::KEEP_MATCHES,
          NO_DOMAINS /* filter_domains */,
          NO_DOMAINS /* expected_cached_domains */
      },
      // An empty DELETE_MATCHES filter should delete nothing.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::DELETE_MATCHES,
          NO_DOMAINS /* filter_domains */,
          DOMAIN0 | DOMAIN1 | DOMAIN2 | DOMAIN3 /* expected_cached_domains */
      },
      // An empty KEEP_MATCHES filter should delete everything.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::KEEP_MATCHES,
          NO_DOMAINS /* filter_domains */,
          NO_DOMAINS /* expected_cached_domains */
      },
      // Test a non-empty DELETE_MATCHES filter.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::DELETE_MATCHES,
          DOMAIN0 | DOMAIN2 /* filter_domains */,
          DOMAIN1 | DOMAIN3 /* expected_cached_domains */
      },
      // Test a non-empty KEEP_MATCHES filter.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::KEEP_MATCHES,
          DOMAIN0 | DOMAIN2 /* filter_domains */,
          DOMAIN0 | DOMAIN2 /* expected_cached_domains */
      },
  };

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateContextParams());
    net::HostCache* host_cache =
        network_context->url_request_context()->host_resolver()->GetHostCache();
    ASSERT_TRUE(host_cache);

    // Add the 4 test domains to the host cache.
    for (const auto* domain : kDomains) {
      host_cache->Set(
          net::HostCache::Key(domain, net::ADDRESS_FAMILY_UNSPECIFIED, 0),
          net::HostCache::Entry(net::OK, net::AddressList(),
                                net::HostCache::Entry::SOURCE_UNKNOWN),
          base::TimeTicks::Now(), base::TimeDelta::FromDays(1));
    }
    // Sanity check.
    EXPECT_EQ(base::size(kDomains), host_cache->entries().size());

    // Set up and run the filter, according to |test_case|.
    mojom::ClearDataFilterPtr clear_data_filter;
    if (!test_case.null_filter) {
      clear_data_filter = mojom::ClearDataFilter::New();
      clear_data_filter->type = test_case.type;
      for (size_t i = 0; i < base::size(kDomains); ++i) {
        if (test_case.filter_domains & (1 << i))
          clear_data_filter->domains.push_back(kDomains[i]);
      }
    }
    base::RunLoop run_loop;
    network_context->ClearHostCache(std::move(clear_data_filter),
                                    run_loop.QuitClosure());
    run_loop.Run();

    // Check that only the expected domains remain in the cache.
    for (size_t i = 0; i < base::size(kDomains); ++i) {
      bool expect_domain_cached =
          ((test_case.expected_cached_domains & (1 << i)) != 0);
      EXPECT_EQ(expect_domain_cached,
                host_cache->HasEntry(kDomains[i], nullptr /* source_out */,
                                     nullptr /* stale_out */));
    }
  }
}

TEST_F(NetworkContextTest, ClearHttpAuthCache) {
  GURL origin("http://google.com");
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:00", &start_time));
  base::SimpleTestClock test_clock;
  test_clock.SetNow(start_time);
  cache->set_clock_for_testing(&test_clock);

  base::string16 user = base::ASCIIToUTF16("user");
  base::string16 password = base::ASCIIToUTF16("pass");
  cache->Add(origin, "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
             "basic realm=Realm1", net::AuthCredentials(user, password), "/");

  test_clock.Advance(base::TimeDelta::FromHours(1));  // Time now 13:00
  cache->Add(origin, "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
             "basic realm=Realm2", net::AuthCredentials(user, password), "/");

  ASSERT_EQ(2u, cache->GetEntriesSizeForTesting());
  ASSERT_NE(nullptr,
            cache->Lookup(origin, "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC));
  ASSERT_NE(nullptr,
            cache->Lookup(origin, "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC));

  base::RunLoop run_loop;
  base::Time test_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:30:00", &test_time));
  network_context->ClearHttpAuthCache(test_time, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(1u, cache->GetEntriesSizeForTesting());
  EXPECT_NE(nullptr,
            cache->Lookup(origin, "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache->Lookup(origin, "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC));
}

TEST_F(NetworkContextTest, ClearAllHttpAuthCache) {
  GURL origin("http://google.com");
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:00", &start_time));
  base::SimpleTestClock test_clock;
  test_clock.SetNow(start_time);
  cache->set_clock_for_testing(&test_clock);

  base::string16 user = base::ASCIIToUTF16("user");
  base::string16 password = base::ASCIIToUTF16("pass");
  cache->Add(origin, "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
             "basic realm=Realm1", net::AuthCredentials(user, password), "/");

  test_clock.Advance(base::TimeDelta::FromHours(1));  // Time now 13:00
  cache->Add(origin, "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
             "basic realm=Realm2", net::AuthCredentials(user, password), "/");

  ASSERT_EQ(2u, cache->GetEntriesSizeForTesting());
  ASSERT_NE(nullptr,
            cache->Lookup(origin, "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC));
  ASSERT_NE(nullptr,
            cache->Lookup(origin, "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC));

  base::RunLoop run_loop;
  network_context->ClearHttpAuthCache(base::Time(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0u, cache->GetEntriesSizeForTesting());
  EXPECT_EQ(nullptr,
            cache->Lookup(origin, "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache->Lookup(origin, "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC));
}

TEST_F(NetworkContextTest, ClearEmptyHttpAuthCache) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  ASSERT_EQ(0u, cache->GetEntriesSizeForTesting());

  base::RunLoop run_loop;
  network_context->ClearHttpAuthCache(base::Time::UnixEpoch(),
                                      base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(0u, cache->GetEntriesSizeForTesting());
}

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(NetworkContextTest, ClearReportingCacheReports) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain("http://google.com");
  reporting_service->QueueReport(domain, "Mozilla/1.0", "group", "type",
                                 nullptr, 0);

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_EQ(0u, reports.size());
}

TEST_F(NetworkContextTest, ClearReportingCacheReportsWithFilter) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain1("http://google.com");
  reporting_service->QueueReport(domain1, "Mozilla/1.0", "group", "type",
                                 nullptr, 0);
  GURL domain2("http://chromium.org");
  reporting_service->QueueReport(domain2, "Mozilla/1.0", "group", "type",
                                 nullptr, 0);

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_EQ(2u, reports.size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
  EXPECT_EQ(domain2, reports.front()->url);
}

TEST_F(NetworkContextTest,
       ClearReportingCacheReportsWithNonRegisterableFilter) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain1("http://192.168.0.1");
  reporting_service->QueueReport(domain1, "Mozilla/1.0", "group", "type",
                                 nullptr, 0);
  GURL domain2("http://192.168.0.2");
  reporting_service->QueueReport(domain2, "Mozilla/1.0", "group", "type",
                                 nullptr, 0);

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_EQ(2u, reports.size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("192.168.0.2");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
  EXPECT_EQ(domain2, reports.front()->url);
}

TEST_F(NetworkContextTest, ClearEmptyReportingCacheReports) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_TRUE(reports.empty());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(NetworkContextTest, ClearReportingCacheReportsWithNoService) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ASSERT_EQ(nullptr,
            network_context->url_request_context()->reporting_service());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearReportingCacheClients) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain("https://google.com");
  reporting_cache->SetClient(url::Origin::Create(domain), domain,
                             net::ReportingClient::Subdomains::EXCLUDE, "group",
                             base::TimeTicks::Max(), 0, 1);

  std::vector<const net::ReportingClient*> clients;
  reporting_cache->GetClients(&clients);
  ASSERT_EQ(1u, clients.size());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetClients(&clients);
  EXPECT_EQ(0u, clients.size());
}

TEST_F(NetworkContextTest, ClearReportingCacheClientsWithFilter) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain1("https://google.com");
  reporting_cache->SetClient(url::Origin::Create(domain1), domain1,
                             net::ReportingClient::Subdomains::EXCLUDE, "group",
                             base::TimeTicks::Max(), 0, 1);
  GURL domain2("https://chromium.org");
  reporting_cache->SetClient(url::Origin::Create(domain2), domain2,
                             net::ReportingClient::Subdomains::EXCLUDE, "group",
                             base::TimeTicks::Max(), 0, 1);

  std::vector<const net::ReportingClient*> clients;
  reporting_cache->GetClients(&clients);
  ASSERT_EQ(2u, clients.size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetClients(&clients);
  EXPECT_EQ(1u, clients.size());
  EXPECT_EQ(domain2, clients.front()->endpoint);
}

TEST_F(NetworkContextTest, ClearEmptyReportingCacheClients) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  std::vector<const net::ReportingClient*> clients;
  reporting_cache->GetClients(&clients);
  ASSERT_TRUE(clients.empty());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetClients(&clients);
  EXPECT_TRUE(clients.empty());
}

TEST_F(NetworkContextTest, ClearReportingCacheClientsWithNoService) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ASSERT_EQ(nullptr,
            network_context->url_request_context()->reporting_service());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  net::NetworkErrorLoggingService* logging_service =
      network_context->url_request_context()->network_error_logging_service();
  ASSERT_TRUE(logging_service);

  GURL domain("https://google.com");
  logging_service->OnHeader(url::Origin::Create(domain),
                            net::IPAddress(192, 168, 0, 1),
                            "{\"report_to\":\"group\",\"max_age\":86400}");

  ASSERT_EQ(1u, logging_service->GetPolicyOriginsForTesting().size());

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(nullptr /* filter */,
                                            run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(logging_service->GetPolicyOriginsForTesting().empty());
}

TEST_F(NetworkContextTest, ClearNetworkErrorLoggingWithFilter) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  net::NetworkErrorLoggingService* logging_service =
      network_context->url_request_context()->network_error_logging_service();
  ASSERT_TRUE(logging_service);

  GURL domain1("https://google.com");
  logging_service->OnHeader(url::Origin::Create(domain1),
                            net::IPAddress(192, 168, 0, 1),
                            "{\"report_to\":\"group\",\"max_age\":86400}");
  GURL domain2("https://chromium.org");
  logging_service->OnHeader(url::Origin::Create(domain2),
                            net::IPAddress(192, 168, 0, 1),
                            "{\"report_to\":\"group\",\"max_age\":86400}");

  ASSERT_EQ(2u, logging_service->GetPolicyOriginsForTesting().size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(std::move(filter),
                                            run_loop.QuitClosure());
  run_loop.Run();

  std::set<url::Origin> policy_origins =
      logging_service->GetPolicyOriginsForTesting();
  EXPECT_EQ(1u, policy_origins.size());
  EXPECT_NE(policy_origins.end(),
            policy_origins.find(url::Origin::Create(domain2)));
}

TEST_F(NetworkContextTest, ClearEmptyNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  net::NetworkErrorLoggingService* logging_service =
      network_context->url_request_context()->network_error_logging_service();
  ASSERT_TRUE(logging_service);

  ASSERT_TRUE(logging_service->GetPolicyOriginsForTesting().empty());

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(nullptr /* filter */,
                                            run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(logging_service->GetPolicyOriginsForTesting().empty());
}

TEST_F(NetworkContextTest, ClearEmptyNetworkErrorLoggingWithNoService) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ASSERT_FALSE(
      network_context->url_request_context()->network_error_logging_service());

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(nullptr /* filter */,
                                            run_loop.QuitClosure());
  run_loop.Run();
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void SetCookieCallback(base::RunLoop* run_loop, bool* result_out, bool result) {
  *result_out = result;
  run_loop->Quit();
}

void GetCookieListCallback(base::RunLoop* run_loop,
                           net::CookieList* result_out,
                           const net::CookieList& result) {
  *result_out = result;
  run_loop->Quit();
}

TEST_F(NetworkContextTest, CookieManager) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());

  mojom::CookieManagerPtr cookie_manager_ptr;
  mojom::CookieManagerRequest cookie_manager_request(
      mojo::MakeRequest(&cookie_manager_ptr));
  network_context->GetCookieManager(std::move(cookie_manager_request));

  // Set a cookie through the cookie interface.
  base::RunLoop run_loop1;
  bool result = false;
  cookie_manager_ptr->SetCanonicalCookie(
      net::CanonicalCookie("TestCookie", "1", "www.test.com", "/", base::Time(),
                           base::Time(), base::Time(), false, false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_LOW),
      true, true, base::BindOnce(&SetCookieCallback, &run_loop1, &result));
  run_loop1.Run();
  EXPECT_TRUE(result);

  // Confirm that cookie is visible directly through the store associated with
  // the network context.
  base::RunLoop run_loop2;
  net::CookieList cookies;
  network_context->url_request_context()
      ->cookie_store()
      ->GetCookieListWithOptionsAsync(
          GURL("http://www.test.com/whatever"), net::CookieOptions(),
          base::Bind(&GetCookieListCallback, &run_loop2, &cookies));
  run_loop2.Run();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("TestCookie", cookies[0].Name());
}

TEST_F(NetworkContextTest, ProxyConfig) {
  // Each ProxyConfigSet consists of a net::ProxyConfig, and the net::ProxyInfos
  // that it will result in for http and ftp URLs. All that matters is that each
  // ProxyConfig is different. It's important that none of these configs require
  // fetching a PAC scripts, as this test checks
  // ProxyResolutionService::config(), which is only updated after fetching PAC
  // scripts (if applicable).
  struct ProxyConfigSet {
    net::ProxyConfig proxy_config;
    net::ProxyInfo http_proxy_info;
    net::ProxyInfo ftp_proxy_info;
  } proxy_config_sets[3];

  proxy_config_sets[0].proxy_config.proxy_rules().ParseFromString(
      "http=foopy:80");
  proxy_config_sets[0].http_proxy_info.UsePacString("PROXY foopy:80");
  proxy_config_sets[0].ftp_proxy_info.UseDirect();

  proxy_config_sets[1].proxy_config.proxy_rules().ParseFromString(
      "http=foopy:80;ftp=foopy2");
  proxy_config_sets[1].http_proxy_info.UsePacString("PROXY foopy:80");
  proxy_config_sets[1].ftp_proxy_info.UsePacString("PROXY foopy2");

  proxy_config_sets[2].proxy_config = net::ProxyConfig::CreateDirect();
  proxy_config_sets[2].http_proxy_info.UseDirect();
  proxy_config_sets[2].ftp_proxy_info.UseDirect();

  // Sanity check.
  EXPECT_FALSE(proxy_config_sets[0].proxy_config.Equals(
      proxy_config_sets[1].proxy_config));
  EXPECT_FALSE(proxy_config_sets[0].proxy_config.Equals(
      proxy_config_sets[2].proxy_config));
  EXPECT_FALSE(proxy_config_sets[1].proxy_config.Equals(
      proxy_config_sets[2].proxy_config));

  // Try each proxy config as the initial config, to make sure setting the
  // initial config works.
  for (const auto& initial_proxy_config_set : proxy_config_sets) {
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        initial_proxy_config_set.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    mojom::ProxyConfigClientPtr config_client;
    context_params->proxy_config_client_request =
        mojo::MakeRequest(&config_client);
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    net::ProxyResolutionService* proxy_resolution_service =
        network_context->url_request_context()->proxy_resolution_service();
    // Need to do proxy resolutions before can check the ProxyConfig, as the
    // ProxyService doesn't start updating its config until it's first used.
    // This also gives some test coverage of LookUpProxyForURL.
    TestProxyLookupClient http_proxy_lookup_client;
    http_proxy_lookup_client.StartLookUpProxyForURL(GURL("http://foo"),
                                                    network_context.get());
    http_proxy_lookup_client.WaitForResult();
    ASSERT_TRUE(http_proxy_lookup_client.proxy_info());
    EXPECT_EQ(initial_proxy_config_set.http_proxy_info.ToPacString(),
              http_proxy_lookup_client.proxy_info()->ToPacString());

    TestProxyLookupClient ftp_proxy_lookup_client;
    ftp_proxy_lookup_client.StartLookUpProxyForURL(GURL("ftp://foo"),
                                                   network_context.get());
    ftp_proxy_lookup_client.WaitForResult();
    ASSERT_TRUE(ftp_proxy_lookup_client.proxy_info());
    EXPECT_EQ(initial_proxy_config_set.ftp_proxy_info.ToPacString(),
              ftp_proxy_lookup_client.proxy_info()->ToPacString());

    EXPECT_TRUE(proxy_resolution_service->config());
    EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(
        initial_proxy_config_set.proxy_config));

    // Always go through the other configs in the same order. This has the
    // advantage of testing the case where there's no change, for
    // proxy_config[0].
    for (const auto& proxy_config_set : proxy_config_sets) {
      config_client->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
          proxy_config_set.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
      scoped_task_environment_.RunUntilIdle();

      TestProxyLookupClient http_proxy_lookup_client2;
      http_proxy_lookup_client2.StartLookUpProxyForURL(GURL("http://foo"),
                                                       network_context.get());
      http_proxy_lookup_client2.WaitForResult();
      ASSERT_TRUE(http_proxy_lookup_client2.proxy_info());
      EXPECT_EQ(proxy_config_set.http_proxy_info.ToPacString(),
                http_proxy_lookup_client2.proxy_info()->ToPacString());

      TestProxyLookupClient ftp_proxy_lookup_client2;
      ftp_proxy_lookup_client2.StartLookUpProxyForURL(GURL("ftp://foo"),
                                                      network_context.get());
      ftp_proxy_lookup_client2.WaitForResult();
      ASSERT_TRUE(ftp_proxy_lookup_client2.proxy_info());
      EXPECT_EQ(proxy_config_set.ftp_proxy_info.ToPacString(),
                ftp_proxy_lookup_client2.proxy_info()->ToPacString());

      EXPECT_TRUE(proxy_resolution_service->config());
      EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(
          proxy_config_set.proxy_config));
    }
  }
}

// Verify that a proxy config works without a ProxyConfigClientRequest.
TEST_F(NetworkContextTest, StaticProxyConfig) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ProxyResolutionService* proxy_resolution_service =
      network_context->url_request_context()->proxy_resolution_service();
  // Kick the ProxyResolutionService into action, as it doesn't start updating
  // its config until it's first used.
  proxy_resolution_service->ForceReloadProxyConfig();
  EXPECT_TRUE(proxy_resolution_service->config());
  EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(proxy_config));
}

TEST_F(NetworkContextTest, NoInitialProxyConfig) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config.reset();
  mojom::ProxyConfigClientPtr config_client;
  context_params->proxy_config_client_request =
      mojo::MakeRequest(&config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ProxyResolutionService* proxy_resolution_service =
      network_context->url_request_context()->proxy_resolution_service();
  EXPECT_FALSE(proxy_resolution_service->config());
  EXPECT_FALSE(proxy_resolution_service->fetched_config());

  // Before there's a proxy configuration, proxy requests should hang.
  // Create two lookups, to make sure two simultaneous lookups can be handled at
  // once.
  TestProxyLookupClient http_proxy_lookup_client;
  http_proxy_lookup_client.StartLookUpProxyForURL(GURL("http://foo/"),
                                                  network_context.get());
  TestProxyLookupClient ftp_proxy_lookup_client;
  ftp_proxy_lookup_client.StartLookUpProxyForURL(GURL("ftp://foo/"),
                                                 network_context.get());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_resolution_service->config());
  EXPECT_FALSE(proxy_resolution_service->fetched_config());
  EXPECT_FALSE(http_proxy_lookup_client.is_done());
  EXPECT_FALSE(ftp_proxy_lookup_client.is_done());
  EXPECT_EQ(2u, network_context->pending_proxy_lookup_requests_for_testing());

  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80");
  config_client->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));

  http_proxy_lookup_client.WaitForResult();
  ASSERT_TRUE(http_proxy_lookup_client.proxy_info());
  EXPECT_EQ("PROXY foopy:80",
            http_proxy_lookup_client.proxy_info()->ToPacString());

  ftp_proxy_lookup_client.WaitForResult();
  ASSERT_TRUE(ftp_proxy_lookup_client.proxy_info());
  EXPECT_EQ("DIRECT", ftp_proxy_lookup_client.proxy_info()->ToPacString());

  EXPECT_EQ(0u, network_context->pending_proxy_lookup_requests_for_testing());
}

TEST_F(NetworkContextTest, DestroyedWithoutProxyConfig) {
  // Create a NetworkContext without an initial proxy configuration.
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config.reset();
  mojom::ProxyConfigClientPtr config_client;
  context_params->proxy_config_client_request =
      mojo::MakeRequest(&config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Proxy requests should hang.
  TestProxyLookupClient proxy_lookup_client;
  proxy_lookup_client.StartLookUpProxyForURL(GURL("http://foo/"),
                                             network_context.get());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, network_context->pending_proxy_lookup_requests_for_testing());
  EXPECT_FALSE(proxy_lookup_client.is_done());

  // Destroying the NetworkContext should cause the pending lookup to fail with
  // ERR_ABORTED.
  network_context.reset();
  proxy_lookup_client.WaitForResult();
  EXPECT_FALSE(proxy_lookup_client.proxy_info());
}

TEST_F(NetworkContextTest, CancelPendingProxyLookup) {
  // Create a NetworkContext without an initial proxy configuration.
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config.reset();
  mojom::ProxyConfigClientPtr config_client;
  context_params->proxy_config_client_request =
      mojo::MakeRequest(&config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Proxy requests should hang.
  std::unique_ptr<TestProxyLookupClient> proxy_lookup_client =
      std::make_unique<TestProxyLookupClient>();
  proxy_lookup_client->StartLookUpProxyForURL(GURL("http://foo/"),
                                              network_context.get());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_lookup_client->is_done());
  EXPECT_EQ(1u, network_context->pending_proxy_lookup_requests_for_testing());

  // Cancelling the proxy lookup should cause the proxy lookup request objects
  // to be deleted.
  proxy_lookup_client.reset();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, network_context->pending_proxy_lookup_requests_for_testing());
}

TEST_F(NetworkContextTest, PacQuickCheck) {
  // Check the default value.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(network_context->url_request_context()
                  ->proxy_resolution_service()
                  ->quick_check_enabled_for_testing());

  // Explicitly enable.
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->pac_quick_check_enabled = true;
  network_context = CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(network_context->url_request_context()
                  ->proxy_resolution_service()
                  ->quick_check_enabled_for_testing());

  // Explicitly disable.
  context_params = CreateContextParams();
  context_params->pac_quick_check_enabled = false;
  network_context = CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->url_request_context()
                   ->proxy_resolution_service()
                   ->quick_check_enabled_for_testing());
}

TEST_F(NetworkContextTest, DangerouslyAllowPacAccessToSecureURLs) {
  // Check the default value.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_EQ(net::ProxyResolutionService::SanitizeUrlPolicy::SAFE,
            network_context->url_request_context()
                ->proxy_resolution_service()
                ->sanitize_url_policy_for_testing());

  // Explicitly disable.
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->dangerously_allow_pac_access_to_secure_urls = false;
  network_context = CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(net::ProxyResolutionService::SanitizeUrlPolicy::SAFE,
            network_context->url_request_context()
                ->proxy_resolution_service()
                ->sanitize_url_policy_for_testing());

  // Explicitly enable.
  context_params = CreateContextParams();
  context_params->dangerously_allow_pac_access_to_secure_urls = true;
  network_context = CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(net::ProxyResolutionService::SanitizeUrlPolicy::UNSAFE,
            network_context->url_request_context()
                ->proxy_resolution_service()
                ->sanitize_url_policy_for_testing());
}

net::IPEndPoint GetLocalHostWithAnyPort() {
  return net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 0);
}

std::vector<uint8_t> CreateTestMessage(uint8_t initial, size_t size) {
  std::vector<uint8_t> array(size);
  for (size_t i = 0; i < size; ++i)
    array[i] = static_cast<uint8_t>((i + initial) % 256);
  return array;
}

TEST_F(NetworkContextTest, CreateUDPSocket) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  mojom::UDPSocketPtr server_socket;
  network_context->CreateUDPSocket(mojo::MakeRequest(&server_socket),
                                   std::move(receiver_interface_ptr));
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, nullptr, &server_addr));

  // Create a client socket to send datagrams.
  mojom::UDPSocketPtr client_socket;
  mojom::UDPSocketRequest client_socket_request(
      mojo::MakeRequest(&client_socket));
  network_context->CreateUDPSocket(std::move(client_socket_request), nullptr);

  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper client_helper(&client_socket);
  ASSERT_EQ(net::OK,
            client_helper.ConnectSync(server_addr, nullptr, &client_addr));

  // This test assumes that the loopback interface doesn't drop UDP packets for
  // a small number of packets.
  const size_t kDatagramCount = 6;
  const size_t kDatagramSize = 255;
  server_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize));
    int result = client_helper.SendSync(test_msg);
    EXPECT_EQ(net::OK, result);
  }

  receiver.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, receiver.results().size());

  int i = 0;
  for (const auto& result : receiver.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(result.src_addr, client_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
              result.data.value());
    i++;
  }
}

TEST_F(NetworkContextTest, CreateNetLogExporter) {
  // Basic flow around start/stop.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  mojom::NetLogExporterPtr net_log_exporter;
  network_context->CreateNetLogExporter(mojo::MakeRequest(&net_log_exporter));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_path(temp_dir.GetPath().AppendASCII("out.json"));
  base::File out_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(out_file.IsValid());

  base::Value dict_start(base::Value::Type::DICTIONARY);
  const char kKeyEarly[] = "early";
  const char kValEarly[] = "morning";
  dict_start.SetKey(kKeyEarly, base::Value(kValEarly));

  net::TestCompletionCallback cb;
  net_log_exporter->Start(std::move(out_file), std::move(dict_start),
                          mojom::NetLogCaptureMode::DEFAULT, 100 * 1024,
                          cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());

  base::Value dict_late(base::Value::Type::DICTIONARY);
  const char kKeyLate[] = "late";
  const char kValLate[] = "snowval";
  dict_late.SetKey(kKeyLate, base::Value(kValLate));

  net_log_exporter->Stop(std::move(dict_late), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());

  // Check that file got written.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));

  // Contents should have net constants, without the client needing any
  // net:: methods.
  EXPECT_NE(std::string::npos, contents.find("ERR_IO_PENDING")) << contents;

  // The additional stuff inject should also occur someplace.
  EXPECT_NE(std::string::npos, contents.find(kKeyEarly)) << contents;
  EXPECT_NE(std::string::npos, contents.find(kValEarly)) << contents;
  EXPECT_NE(std::string::npos, contents.find(kKeyLate)) << contents;
  EXPECT_NE(std::string::npos, contents.find(kValLate)) << contents;
}

TEST_F(NetworkContextTest, CreateNetLogExporterUnbounded) {
  // Make sure that exporting without size limit works.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  mojom::NetLogExporterPtr net_log_exporter;
  network_context->CreateNetLogExporter(mojo::MakeRequest(&net_log_exporter));

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File out_file(temp_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(out_file.IsValid());

  net::TestCompletionCallback cb;
  net_log_exporter->Start(
      std::move(out_file), base::Value(base::Value::Type::DICTIONARY),
      mojom::NetLogCaptureMode::DEFAULT,
      mojom::NetLogExporter::kUnlimitedFileSize, cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());

  net_log_exporter->Stop(base::Value(base::Value::Type::DICTIONARY),
                         cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());

  // Check that file got written.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_path, &contents));

  // Contents should have net constants, without the client needing any
  // net:: methods.
  EXPECT_NE(std::string::npos, contents.find("ERR_IO_PENDING")) << contents;

  base::DeleteFile(temp_path, false);
}

TEST_F(NetworkContextTest, CreateNetLogExporterErrors) {
  // Some basic state machine misuses.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  mojom::NetLogExporterPtr net_log_exporter;
  network_context->CreateNetLogExporter(mojo::MakeRequest(&net_log_exporter));

  net::TestCompletionCallback cb;
  net_log_exporter->Stop(base::Value(base::Value::Type::DICTIONARY),
                         cb.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, cb.WaitForResult());

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file.IsValid());

  net_log_exporter->Start(
      std::move(temp_file), base::Value(base::Value::Type::DICTIONARY),
      mojom::NetLogCaptureMode::DEFAULT, 100 * 1024, cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());

  // Can't start twice.
  base::FilePath temp_path2;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path2));
  base::File temp_file2(
      temp_path2, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file2.IsValid());

  net_log_exporter->Start(
      std::move(temp_file2), base::Value(base::Value::Type::DICTIONARY),
      mojom::NetLogCaptureMode::DEFAULT, 100 * 1024, cb.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, cb.WaitForResult());

  base::DeleteFile(temp_path, false);
  base::DeleteFile(temp_path2, false);

  // Forgetting to stop is recovered from.
}

TEST_F(NetworkContextTest, DestroyNetLogExporterWhileCreatingScratchDir) {
  // Make sure that things behave OK if NetLogExporter is destroyed during the
  // brief window it owns the scratch directory.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  std::unique_ptr<NetLogExporter> net_log_exporter =
      std::make_unique<NetLogExporter>(network_context.get());

  base::WaitableEvent block_mktemp(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath path = dir.Take();
  EXPECT_TRUE(base::PathExists(path));

  net_log_exporter->SetCreateScratchDirHandlerForTesting(base::BindRepeating(
      [](base::WaitableEvent* block_on,
         const base::FilePath& path) -> base::FilePath {
        base::ScopedAllowBaseSyncPrimitivesForTesting need_to_block;
        block_on->Wait();
        return path;
      },
      &block_mktemp, path));

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file.IsValid());

  net_log_exporter->Start(
      std::move(temp_file), base::Value(base::Value::Type::DICTIONARY),
      mojom::NetLogCaptureMode::DEFAULT, 100, base::BindOnce([](int) {}));
  net_log_exporter = nullptr;
  block_mktemp.Signal();

  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(path));
  base::DeleteFile(temp_path, false);
}

net::IPEndPoint CreateExpectedEndPoint(const std::string& address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

class TestResolveHostClient : public mojom::ResolveHostClient {
 public:
  TestResolveHostClient(mojom::ResolveHostClientPtr* interface_ptr,
                        base::RunLoop* run_loop)
      : binding_(this, mojo::MakeRequest(interface_ptr)),
        complete_(false),
        run_loop_(run_loop) {
    DCHECK(run_loop_);
  }

  void CloseBinding() { binding_.Close(); }

  void OnComplete(int error,
                  const base::Optional<net::AddressList>& addresses) override {
    DCHECK(!complete_);

    complete_ = true;
    result_error_ = error;
    result_addresses_ = addresses;
    run_loop_->Quit();
  }

  bool complete() const { return complete_; }

  int result_error() const {
    DCHECK(complete_);
    return result_error_;
  }

  const base::Optional<net::AddressList>& result_addresses() const {
    DCHECK(complete_);
    return result_addresses_;
  }

 private:
  mojo::Binding<mojom::ResolveHostClient> binding_;

  bool complete_;
  int result_error_;
  base::Optional<net::AddressList> result_addresses_;
  base::RunLoop* const run_loop_;
};

TEST_F(NetworkContextTest, ResolveHost_Sync) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto resolver = std::make_unique<net::MockHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->set_synchronous_mode(true);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 160),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Async) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto resolver = std::make_unique<net::MockHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->set_synchronous_mode(false);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 160),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Failure_Sync) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto resolver = std::make_unique<net::MockHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->rules()->AddSimulatedFailure("example.com");
  resolver->set_synchronous_mode(true);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("example.com", 160),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Failure_Async) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto resolver = std::make_unique<net::MockHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->rules()->AddSimulatedFailure("example.com");
  resolver->set_synchronous_mode(false);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("example.com", 160),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_NoControlHandle) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  network_context->ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                               std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_CloseControlHandle) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  network_context->ResolveHost(net::HostPortPair("localhost", 160),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));
  control_handle = nullptr;
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160),
                                    CreateExpectedEndPoint("::1", 160)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Cancellation) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_EQ(0, resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 80),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.Run();

  // On cancellation, should receive an ERR_FAILED result, and the internal
  // resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_ABORTED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_DestroyContext) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_EQ(0, resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 80),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  network_context = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(NetworkContextTest, ResolveHost_CloseClient) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_EQ(0, resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 80),
                               std::move(optional_parameters),
                               std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  response_client.CloseBinding();
  run_loop.RunUntilIdle();

  // Response pipe is closed, so no results to check. Internal request should be
  // cancelled.
  EXPECT_FALSE(response_client.complete());
  EXPECT_EQ(1, resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

// Test factory of net::HostResolvers. Creates standard net::HostResolverImpl.
// Keeps pointers to all created resolvers.
class TestResolverFactory : public net::HostResolver::Factory {
 public:
  static TestResolverFactory* CreateAndSetFactory(NetworkContext* context) {
    auto factory = std::make_unique<TestResolverFactory>();
    auto* factory_ptr = factory.get();
    context->set_host_resolver_factory_for_testing(std::move(factory));
    return factory_ptr;
  }

  std::unique_ptr<net::HostResolver> CreateResolver(
      const net::HostResolver::Options& options,
      net::NetLog* net_log) override {
    std::unique_ptr<net::HostResolverImpl> resolver =
        net::HostResolver::CreateSystemResolverImpl(options, net_log);
    resolvers_.push_back(resolver.get());
    return resolver;
  }

  const std::vector<net::HostResolverImpl*>& resolvers() const {
    return resolvers_;
  }

 private:
  std::vector<net::HostResolverImpl*> resolvers_;
};

TEST_F(NetworkContextTest, CreateHostResolver) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Inject a factory to control and capture created net::HostResolvers.
  TestResolverFactory* factory =
      TestResolverFactory::CreateAndSetFactory(network_context.get());

  mojom::HostResolverPtr resolver;
  network_context->CreateHostResolver(base::nullopt,
                                      mojo::MakeRequest(&resolver));

  // Expected to use shared internal HostResolver.
  EXPECT_TRUE(factory->resolvers().empty());

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                        std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, CreateHostResolver_CloseResolver) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto internal_resolver = std::make_unique<net::HangingHostResolver>();
  network_context->url_request_context()->set_host_resolver(
      internal_resolver.get());

  mojom::HostResolverPtr resolver;
  network_context->CreateHostResolver(base::nullopt,
                                      mojo::MakeRequest(&resolver));

  ASSERT_EQ(0, internal_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80),
                        std::move(optional_parameters),
                        std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  resolver = nullptr;
  run_loop.Run();

  // On resolver destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, internal_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(NetworkContextTest, CreateHostResolver_CloseContext) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto internal_resolver = std::make_unique<net::HangingHostResolver>();
  network_context->url_request_context()->set_host_resolver(
      internal_resolver.get());

  mojom::HostResolverPtr resolver;
  network_context->CreateHostResolver(base::nullopt,
                                      mojo::MakeRequest(&resolver));

  ASSERT_EQ(0, internal_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80),
                        std::move(optional_parameters),
                        std::move(response_client_ptr));
  // Run a bit to ensure the resolve request makes it to the resolver. Otherwise
  // the resolver will be destroyed and close its pipe before it even knows
  // about the request to send a failure.
  scoped_task_environment_.RunUntilIdle();

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  bool resolver_closed = false;
  auto resolver_closed_callback =
      base::BindLambdaForTesting([&]() { resolver_closed = true; });
  resolver.set_connection_error_handler(resolver_closed_callback);

  network_context = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, internal_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_TRUE(resolver_closed);
}

TEST_F(NetworkContextTest, CreateHostResolverWithConfigOverrides) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Inject a factory to control and capture created net::HostResolvers.
  TestResolverFactory* factory =
      TestResolverFactory::CreateAndSetFactory(network_context.get());

  net::DnsConfigOverrides overrides;
  overrides.nameservers = std::vector<net::IPEndPoint>{
      CreateExpectedEndPoint("100.100.100.100", 22)};

  mojom::HostResolverPtr resolver;
  network_context->CreateHostResolver(overrides, mojo::MakeRequest(&resolver));

  // Should create 1 private resolver with a DnsClient (if DnsClient is
  // enablable for the build config).
  ASSERT_EQ(1u, factory->resolvers().size());
  net::HostResolverImpl* internal_resolver = factory->resolvers().front();
#if defined(ENABLE_BUILT_IN_DNS)
  EXPECT_TRUE(internal_resolver->GetDnsConfigAsValue());
#endif

  // Override DnsClient with a basic mock.
  const std::string kQueryHostname = "example.com";
  const std::string kResult = "1.2.3.4";
  net::IPAddress result;
  CHECK(result.AssignFromIPLiteral(kResult));
  net::MockDnsClientRuleList rules{
      net::MockDnsClientRule(kQueryHostname, net::dns_protocol::kTypeA,
                             net::MockDnsClientRule::Result(result), false),
      net::MockDnsClientRule(kQueryHostname, net::dns_protocol::kTypeAAAA,
                             net::MockDnsClientRule::Result(
                                 net::MockDnsClientRule::ResultType::EMPTY),
                             false)};
  auto mock_dns_client =
      std::make_unique<net::MockDnsClient>(net::DnsConfig(), rules);
  auto* mock_dns_client_ptr = mock_dns_client.get();
  internal_resolver->SetDnsClient(std::move(mock_dns_client));

  // Force the base configuration to ensure consistent overriding.
  net::DnsConfig base_configuration;
  base_configuration.nameservers = {CreateExpectedEndPoint("12.12.12.12", 53)};
  internal_resolver->SetBaseDnsConfigForTesting(base_configuration);

  // Test that the DnsClient is getting the overridden configuration.
  EXPECT_TRUE(overrides.ApplyOverrides(base_configuration)
                  .Equals(*mock_dns_client_ptr->GetConfig()));

  // Ensure we are using the private resolver by testing that we get results
  // from the overridden DnsClient.
  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::HostResolver::DnsQueryType::A;
  optional_parameters->source = net::HostResolverSource::DNS;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);
  resolver->ResolveHost(net::HostPortPair(kQueryHostname, 80),
                        std::move(optional_parameters),
                        std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kResult, 80)));
}

TEST_F(NetworkContextTest, PrivacyModeDisabledByDefault) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkContextTest, PrivacyModeEnabledIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK,
                    network_context.get());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_delegate()
                  ->CanEnablePrivacyMode(kURL, kOtherURL));
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->CanEnablePrivacyMode(kOtherURL, kURL));
}

TEST_F(NetworkContextTest, PrivacyModeDisabledIfCookiesAllowed) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW,
                    network_context.get());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkContextTest, PrivacyModeDisabledIfCookiesSettingForOtherURL) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // URLs are switched so setting should not apply.
  SetContentSetting(kOtherURL, kURL, CONTENT_SETTING_BLOCK,
                    network_context.get());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkContextTest, PrivacyModeEnabledIfThirdPartyCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::NetworkDelegate* delegate =
      network_context->url_request_context()->network_delegate();

  network_context->cookie_manager()->BlockThirdPartyCookies(true);
  EXPECT_TRUE(delegate->CanEnablePrivacyMode(kURL, kOtherURL));
  EXPECT_FALSE(delegate->CanEnablePrivacyMode(kURL, kURL));

  network_context->cookie_manager()->BlockThirdPartyCookies(false);
  EXPECT_FALSE(delegate->CanEnablePrivacyMode(kURL, kOtherURL));
  EXPECT_FALSE(delegate->CanEnablePrivacyMode(kURL, kURL));
}

TEST_F(NetworkContextTest, CanSetCookieFalseIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request = context.CreateRequest(
      kURL, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  net::CanonicalCookie cookie("TestCookie", "1", "www.test.com", "/",
                              base::Time(), base::Time(), base::Time(), false,
                              false, net::CookieSameSite::NO_RESTRICTION,
                              net::COOKIE_PRIORITY_LOW);

  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, cookie, nullptr, true));
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK, network_context.get());
  EXPECT_FALSE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, cookie, nullptr, true));
}

TEST_F(NetworkContextTest, CanSetCookieTrueIfCookiesAllowed) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request = context.CreateRequest(
      kURL, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  net::CanonicalCookie cookie("TestCookie", "1", "www.test.com", "/",
                              base::Time(), base::Time(), base::Time(), false,
                              false, net::CookieSameSite::NO_RESTRICTION,
                              net::COOKIE_PRIORITY_LOW);

  SetDefaultContentSetting(CONTENT_SETTING_ALLOW, network_context.get());
  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, cookie, nullptr, true));
}

TEST_F(NetworkContextTest, CanGetCookiesFalseIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request = context.CreateRequest(
      kURL, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanGetCookies(
          *request, {}, true));
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK, network_context.get());
  EXPECT_FALSE(
      network_context->url_request_context()->network_delegate()->CanGetCookies(
          *request, {}, true));
}

TEST_F(NetworkContextTest, CanGetCookiesTrueIfCookiesAllowed) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request = context.CreateRequest(
      kURL, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);

  SetDefaultContentSetting(CONTENT_SETTING_ALLOW, network_context.get());
  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanGetCookies(
          *request, {}, true));
}

// Gets notified by the EmbeddedTestServer on incoming connections being
// accepted or read from, keeps track of them and exposes that info to
// the tests.
// A port being reused is currently considered an error.  If a test
// needs to verify multiple connections are opened in sequence, that will need
// to be changed.
class ConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  ConnectionListener()
      : total_sockets_seen_(0),
        total_sockets_waited_for_(0),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        num_accepted_connections_needed_(0),
        num_accepted_connections_loop_(nullptr) {}

  ~ConnectionListener() override {}

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  void AcceptedSocket(const net::StreamSocket& connection) override {
    base::AutoLock lock(lock_);
    uint16_t socket = GetPort(connection);
    EXPECT_TRUE(sockets_.find(socket) == sockets_.end());

    sockets_[socket] = SOCKET_ACCEPTED;
    total_sockets_seen_++;
    CheckAccepted();
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {
    EXPECT_GE(rv, net::OK);
  }

  // Wait for exactly |n| items in |sockets_|. |n| must be greater than 0.
  void WaitForAcceptedConnections(size_t num_connections) {
    DCHECK(!num_accepted_connections_loop_);
    DCHECK_GT(num_connections, 0u);
    base::RunLoop run_loop;
    {
      base::AutoLock lock(lock_);
      EXPECT_GE(num_connections, sockets_.size() - total_sockets_waited_for_);
      num_accepted_connections_loop_ = &run_loop;
      num_accepted_connections_needed_ = num_connections;
      CheckAccepted();
    }
    // Note that the previous call to CheckAccepted can quit this run loop
    // before this call, which will make this call a no-op.
    run_loop.Run();

    // Grab the mutex again and make sure that the number of accepted sockets is
    // indeed |num_connections|.
    base::AutoLock lock(lock_);
    total_sockets_waited_for_ += num_connections;
    EXPECT_EQ(total_sockets_seen_, total_sockets_waited_for_);
  }

  // Helper function to stop the waiting for sockets to be accepted for
  // WaitForAcceptedConnections. |num_accepted_connections_loop_| spins
  // until |num_accepted_connections_needed_| sockets are accepted by the test
  // server. The values will be null/0 if the loop is not running.
  void CheckAccepted() {
    lock_.AssertAcquired();
    // |num_accepted_connections_loop_| null implies
    // |num_accepted_connections_needed_| == 0.
    DCHECK(num_accepted_connections_loop_ ||
           num_accepted_connections_needed_ == 0);
    if (!num_accepted_connections_loop_ ||
        num_accepted_connections_needed_ !=
            sockets_.size() - total_sockets_waited_for_) {
      return;
    }

    task_runner_->PostTask(FROM_HERE,
                           num_accepted_connections_loop_->QuitClosure());
    num_accepted_connections_needed_ = 0;
    num_accepted_connections_loop_ = nullptr;
  }

 private:
  static uint16_t GetPort(const net::StreamSocket& connection) {
    // Get the remote port of the peer, since the local port will always be the
    // port the test server is listening on. This isn't strictly correct - it's
    // possible for multiple peers to connect with the same remote port but
    // different remote IPs - but the tests here assume that connections to the
    // test server (running on localhost) will always come from localhost, and
    // thus the peer port is all thats needed to distinguish two connections.
    // This also would be problematic if the OS reused ports, but that's not
    // something to worry about for these tests.
    net::IPEndPoint address;
    EXPECT_EQ(net::OK, connection.GetPeerAddress(&address));
    return address.port();
  }

  int total_sockets_seen_;
  int total_sockets_waited_for_;

  enum SocketStatus { SOCKET_ACCEPTED, SOCKET_READ_FROM };

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This lock protects all the members below, which each are used on both the
  // IO and UI thread. Members declared after the lock are protected by it.
  mutable base::Lock lock_;
  typedef std::map<uint16_t, SocketStatus> SocketContainer;
  SocketContainer sockets_;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before quitting the |num_accepted_connections_loop_|.
  size_t num_accepted_connections_needed_;
  base::RunLoop* num_accepted_connections_loop_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionListener);
};

TEST_F(NetworkContextTest, PreconnectOne) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(1, test_server.base_url(),
                                     net::LOAD_NORMAL, true);
  connection_listener.WaitForAcceptedConnections(1u);
}

TEST_F(NetworkContextTest, PreconnectHSTS) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  const GURL server_http_url = GetHttpUrlFromHttps(test_server.base_url());
  network_context->PreconnectSockets(1, server_http_url, net::LOAD_NORMAL,
                                     true);
  connection_listener.WaitForAcceptedConnections(1u);

  int num_sockets = GetSocketCount(network_context.get());
  EXPECT_EQ(num_sockets, 1);
  int num_ssl_sockets = GetSSLSocketCount(network_context.get());
  EXPECT_EQ(num_ssl_sockets, 0);

  const base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  network_context->url_request_context()->transport_security_state()->AddHSTS(
      server_http_url.host(), expiry, false);
  network_context->PreconnectSockets(1, server_http_url, net::LOAD_NORMAL,
                                     true);
  connection_listener.WaitForAcceptedConnections(1u);

  num_sockets = GetSocketCount(network_context.get());
  EXPECT_EQ(num_sockets, 2);
  num_ssl_sockets = GetSSLSocketCount(network_context.get());
  EXPECT_EQ(num_ssl_sockets, 1);
}

TEST_F(NetworkContextTest, PreconnectZero) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(0, test_server.base_url(),
                                     net::LOAD_NORMAL, true);
  base::RunLoop().RunUntilIdle();

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 0);
  int num_connecting_sockets =
      GetSocketPoolInfo(network_context.get(), "connecting_socket_count");
  ASSERT_EQ(num_connecting_sockets, 0);
}

TEST_F(NetworkContextTest, PreconnectTwo) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(2, test_server.base_url(),
                                     net::LOAD_NORMAL, true);
  connection_listener.WaitForAcceptedConnections(2u);

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 2);
}

TEST_F(NetworkContextTest, PreconnectFour) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(4, test_server.base_url(),
                                     net::LOAD_NORMAL, true);

  connection_listener.WaitForAcceptedConnections(4u);

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 4);
}

// Flaky; see http://crbug.com/874419
TEST_F(NetworkContextTest, DISABLED_PreconnectMax) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  int max_num_sockets =
      GetSocketPoolInfo(network_context.get(), "max_sockets_per_group");
  EXPECT_GT(76, max_num_sockets);

  network_context->PreconnectSockets(76, test_server.base_url(),
                                     net::LOAD_NORMAL, true);
  base::RunLoop().RunUntilIdle();

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, max_num_sockets);
}

TEST_F(NetworkContextTest, CloseAllConnections) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(2, test_server.base_url(),
                                     net::LOAD_NORMAL, true);
  connection_listener.WaitForAcceptedConnections(2u);

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  EXPECT_EQ(num_sockets, 2);

  base::RunLoop run_loop;
  network_context->CloseAllConnections(run_loop.QuitClosure());
  run_loop.Run();

  num_sockets = GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  EXPECT_EQ(num_sockets, 0);
}

TEST_F(NetworkContextTest, CloseIdleConnections) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  // Create a hung (i.e. non-idle) socket.
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      network_context->url_request_context()->CreateRequest(
          test_server.GetURL("/hung"), net::DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  connection_listener.WaitForAcceptedConnections(1u);
  EXPECT_EQ(0, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
  EXPECT_EQ(
      0, GetSocketPoolInfo(network_context.get(), "connecting_socket_count"));
  EXPECT_EQ(
      1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

  // Create an idle socket.
  network_context->PreconnectSockets(2, test_server.base_url(),
                                     net::LOAD_NORMAL, true);
  connection_listener.WaitForAcceptedConnections(2u);
  EXPECT_EQ(2, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
  EXPECT_EQ(
      0, GetSocketPoolInfo(network_context.get(), "connecting_socket_count"));
  EXPECT_EQ(
      1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

  base::RunLoop run_loop;
  network_context->CloseIdleConnections(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
  EXPECT_EQ(
      0, GetSocketPoolInfo(network_context.get(), "connecting_socket_count"));
  EXPECT_EQ(
      1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));
}

TEST_F(NetworkContextTest, ExpectCT) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const char kTestDomain[] = "example.com";
  const base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  const bool enforce = true;
  const GURL report_uri = GURL("https://example.com/foo/bar");

  // Assert we start with no data for the test host.
  {
    base::Value state;
    base::RunLoop run_loop;
    network_context->GetExpectCTState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(state.is_dict());

    const base::Value* result =
        state.FindKeyOfType("result", base::Value::Type::BOOLEAN);
    ASSERT_TRUE(result != nullptr);
    EXPECT_FALSE(result->GetBool());
  }

  // Add the host data.
  {
    base::RunLoop run_loop;
    bool result = false;
    network_context->AddExpectCT(
        kTestDomain, expiry, enforce, report_uri,
        base::BindOnce(&StoreBool, &result, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(result);
  }

  // Assert added host data is returned.
  {
    base::Value state;
    base::RunLoop run_loop;
    network_context->GetExpectCTState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(state.is_dict());

    const base::Value* value = state.FindKeyOfType("dynamic_expect_ct_domain",
                                                   base::Value::Type::STRING);
    ASSERT_TRUE(value != nullptr);
    EXPECT_EQ(kTestDomain, value->GetString());

    value = state.FindKeyOfType("dynamic_expect_ct_expiry",
                                base::Value::Type::DOUBLE);
    ASSERT_TRUE(value != nullptr);
    EXPECT_EQ(expiry.ToDoubleT(), value->GetDouble());

    value = state.FindKeyOfType("dynamic_expect_ct_enforce",
                                base::Value::Type::BOOLEAN);
    ASSERT_TRUE(value != nullptr);
    EXPECT_EQ(enforce, value->GetBool());

    value = state.FindKeyOfType("dynamic_expect_ct_report_uri",
                                base::Value::Type::STRING);
    ASSERT_TRUE(value != nullptr);
    EXPECT_EQ(report_uri, value->GetString());
  }

  // Delete host data.
  {
    bool result;
    base::RunLoop run_loop;
    network_context->DeleteDynamicDataForHost(
        kTestDomain,
        base::BindOnce(&StoreBool, &result, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(result);
  }

  // Assert data is removed.
  {
    base::Value state;
    base::RunLoop run_loop;
    network_context->GetExpectCTState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(state.is_dict());

    const base::Value* result =
        state.FindKeyOfType("result", base::Value::Type::BOOLEAN);
    ASSERT_TRUE(result != nullptr);
    EXPECT_FALSE(result->GetBool());
  }
}

TEST_F(NetworkContextTest, SetExpectCTTestReport) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::EmbeddedTestServer test_server;

  std::set<GURL> requested_urls;
  auto monitor_callback = base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        requested_urls.insert(request.GetURL());
      });
  test_server.RegisterRequestMonitor(monitor_callback);
  ASSERT_TRUE(test_server.Start());
  const GURL kReportURL = test_server.base_url().Resolve("/report/path");

  base::RunLoop run_loop;
  bool result = false;
  network_context->SetExpectCTTestReport(
      kReportURL, base::BindOnce(&StoreBool, &result, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(result);

  EXPECT_TRUE(base::ContainsKey(requested_urls, kReportURL));
}

TEST_F(NetworkContextTest, QueryHSTS) {
  const char kTestDomain[] = "example.com";

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  bool result = false, got_result = false;
  network_context->IsHSTSActiveForHost(
      kTestDomain, base::BindLambdaForTesting([&](bool is_hsts) {
        result = is_hsts;
        got_result = true;
      }));
  EXPECT_TRUE(got_result);
  EXPECT_FALSE(result);

  base::RunLoop run_loop;
  network_context->AddHSTS(
      kTestDomain, base::Time::Now() + base::TimeDelta::FromDays(1000),
      false /*include_subdomains*/, run_loop.QuitClosure());
  run_loop.Run();

  bool result2 = false, got_result2 = false;
  network_context->IsHSTSActiveForHost(
      kTestDomain, base::BindLambdaForTesting([&](bool is_hsts) {
        result2 = is_hsts;
        got_result2 = true;
      }));
  EXPECT_TRUE(got_result2);
  EXPECT_TRUE(result2);
}

TEST_F(NetworkContextTest, GetHSTSState) {
  const char kTestDomain[] = "example.com";
  const base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  const GURL report_uri = GURL("https://example.com/foo/bar");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  base::Value state;
  {
    base::RunLoop run_loop;
    network_context->GetHSTSState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_TRUE(state.is_dict());

  const base::Value* result =
      state.FindKeyOfType("result", base::Value::Type::BOOLEAN);
  ASSERT_TRUE(result != nullptr);
  EXPECT_FALSE(result->GetBool());

  {
    base::RunLoop run_loop;
    network_context->AddHSTS(kTestDomain, expiry, false /*include_subdomains*/,
                             run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    network_context->GetHSTSState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_TRUE(state.is_dict());

  result = state.FindKeyOfType("result", base::Value::Type::BOOLEAN);
  ASSERT_TRUE(result != nullptr);
  EXPECT_TRUE(result->GetBool());

  // Not checking all values - only enough to ensure the underlying call
  // was made.
  const base::Value* value =
      state.FindKeyOfType("dynamic_sts_domain", base::Value::Type::STRING);
  ASSERT_TRUE(value != nullptr);
  EXPECT_EQ(kTestDomain, value->GetString());

  value = state.FindKeyOfType("dynamic_sts_expiry", base::Value::Type::DOUBLE);
  ASSERT_TRUE(value != nullptr);
  EXPECT_EQ(expiry.ToDoubleT(), value->GetDouble());
}

TEST_F(NetworkContextTest, ForceReloadProxyConfig) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  auto net_log_exporter =
      std::make_unique<network::NetLogExporter>(network_context.get());
  base::FilePath net_log_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&net_log_path));

  {
    base::File net_log_file(
        net_log_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    EXPECT_TRUE(net_log_file.IsValid());
    base::RunLoop run_loop;
    int32_t start_param = 0;
    auto start_callback = base::BindLambdaForTesting([&](int32_t result) {
      start_param = result;
      run_loop.Quit();
    });
    net_log_exporter->Start(
        std::move(net_log_file),
        /*extra_constants=*/base::Value(base::Value::Type::DICTIONARY),
        network::mojom::NetLogCaptureMode::DEFAULT,
        network::mojom::NetLogExporter::kUnlimitedFileSize, start_callback);
    run_loop.Run();
    EXPECT_EQ(net::OK, start_param);
  }

  {
    base::RunLoop run_loop;
    network_context->ForceReloadProxyConfig(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    int32_t stop_param = 0;
    auto stop_callback = base::BindLambdaForTesting([&](int32_t result) {
      stop_param = result;
      run_loop.Quit();
    });
    net_log_exporter->Stop(
        /*polled_data=*/base::Value(base::Value::Type::DICTIONARY),
        stop_callback);
    run_loop.Run();
    EXPECT_EQ(net::OK, stop_param);
  }

  std::string log_contents;
  EXPECT_TRUE(base::ReadFileToString(net_log_path, &log_contents));

  EXPECT_NE(std::string::npos, log_contents.find("\"new_config\""))
      << log_contents;
  base::DeleteFile(net_log_path, false);
}

TEST_F(NetworkContextTest, ClearBadProxiesCache) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  net::ProxyResolutionService* proxy_resolution_service =
      network_context->url_request_context()->proxy_resolution_service();

  // Very starting conditions: zero bad proxies.
  EXPECT_EQ(0UL, proxy_resolution_service->proxy_retry_info().size());

  // Simulate network error to add one proxy to the bad proxy list.
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("http://foo1.com");
  proxy_resolution_service->ReportSuccess(proxy_info);
  std::vector<net::ProxyServer> proxies;
  proxies.push_back(net::ProxyServer::FromURI("http://foo1.com",
                                              net::ProxyServer::SCHEME_HTTP));
  proxy_resolution_service->MarkProxiesAsBadUntil(
      proxy_info, base::TimeDelta::FromDays(1), proxies,
      net::NetLogWithSource());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1UL, proxy_resolution_service->proxy_retry_info().size());

  // Clear the bad proxies.
  base::RunLoop run_loop;
  network_context->ClearBadProxiesCache(run_loop.QuitClosure());
  run_loop.Run();

  // Verify all cleared.
  EXPECT_EQ(0UL, proxy_resolution_service->proxy_retry_info().size());
}

// This is a test ProxyErrorClient that records the sequence of calls made to
// OnPACScriptError() and OnRequestMaybeFailedDueToProxySettings().
class TestProxyErrorClient final : public mojom::ProxyErrorClient {
 public:
  struct PacScriptError {
    int line = -1;
    std::string details;
  };

  TestProxyErrorClient() : binding_(this) {}

  ~TestProxyErrorClient() override {}

  void OnPACScriptError(int32_t line_number,
                        const std::string& details) override {
    on_pac_script_error_calls_.push_back({line_number, details});
  }

  void OnRequestMaybeFailedDueToProxySettings(int32_t net_error) override {
    on_request_maybe_failed_calls_.push_back(net_error);
  }

  const std::vector<int>& on_request_maybe_failed_calls() const {
    return on_request_maybe_failed_calls_;
  }

  const std::vector<PacScriptError>& on_pac_script_error_calls() const {
    return on_pac_script_error_calls_;
  }

  // Creates an InterfacePtrInfo, binds it to |*this| and returns it.
  mojom::ProxyErrorClientPtrInfo CreateInterfacePtrInfo() {
    mojom::ProxyErrorClientPtrInfo client_ptr_info;

    binding_.Bind(mojo::MakeRequest(&client_ptr_info));
    binding_.set_connection_error_handler(base::BindOnce(
        &TestProxyErrorClient::OnMojoPipeError, base::Unretained(this)));
    return client_ptr_info;
  }

  // Runs until the message pipe is closed due to an error.
  void RunUntilMojoPipeError() {
    if (has_received_mojo_pipe_error_)
      return;
    base::RunLoop run_loop;
    quit_closure_for_on_mojo_pipe_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnMojoPipeError() {
    if (has_received_mojo_pipe_error_)
      return;
    has_received_mojo_pipe_error_ = true;
    if (quit_closure_for_on_mojo_pipe_error_)
      std::move(quit_closure_for_on_mojo_pipe_error_).Run();
  }

  mojo::Binding<mojom::ProxyErrorClient> binding_;

  base::OnceClosure quit_closure_for_on_mojo_pipe_error_;
  bool has_received_mojo_pipe_error_ = false;
  std::vector<int> on_request_maybe_failed_calls_;
  std::vector<PacScriptError> on_pac_script_error_calls_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyErrorClient);
};

// While in scope, all host resolutions will fail with ERR_NAME_NOT_RESOLVED,
// including localhost (so this precludes the use of embedded test server).
class ScopedFailAllHostResolutions {
 public:
  ScopedFailAllHostResolutions()
      : mock_resolver_proc_(new net::RuleBasedHostResolverProc(nullptr)),
        default_resolver_proc_(mock_resolver_proc_.get()) {
    mock_resolver_proc_->AddSimulatedFailure("*");
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc_;
  net::ScopedDefaultHostResolverProc default_resolver_proc_;
};

// Tests that when a ProxyErrorClient is provided to NetworkContextParams, this
// client's OnRequestMaybeFailedDueToProxySettings() method is called exactly
// once when a request fails due to a proxy server connectivity failure.
TEST_F(NetworkContextTest, ProxyErrorClientNotifiedOfProxyConnection) {
  // Avoid the test having a network dependency on DNS.
  ScopedFailAllHostResolutions fail_dns;

  // Set up the NetworkContext, such that it uses an unreachable proxy
  // (proxy and is configured to send "proxy errors" to
  // |proxy_error_client|.
  TestProxyErrorClient proxy_error_client;
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->proxy_error_client =
      proxy_error_client.CreateInterfacePtrInfo();
  net::ProxyConfig proxy_config;
  // Set the proxy to an unreachable address (host resolution fails).
  proxy_config.proxy_rules().ParseFromString("proxy.bad.dns");
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request. It doesn't matter exactly what the URL is, since it
  // will be sent to the proxy.
  ResourceRequest request;
  request.url = GURL("http://example.test");

  mojom::URLLoaderFactoryPtr loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                          std::move(loader_params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Confirm the the resource request failed due to an unreachable proxy.
  client.RunUntilComplete();
  EXPECT_THAT(client.completion_status().error_code,
              net::test::IsError(net::ERR_PROXY_CONNECTION_FAILED));

  // Tear down the network context and wait for a pipe error to ensure
  // that all queued messages on |proxy_error_client| have been processed.
  network_context.reset();
  proxy_error_client.RunUntilMojoPipeError();

  // Confirm that the ProxyErrorClient received the expected calls.
  const auto& request_errors =
      proxy_error_client.on_request_maybe_failed_calls();
  const auto& pac_errors = proxy_error_client.on_pac_script_error_calls();

  ASSERT_EQ(1u, request_errors.size());
  EXPECT_THAT(request_errors[0],
              net::test::IsError(net::ERR_PROXY_CONNECTION_FAILED));
  EXPECT_EQ(0u, pac_errors.size());
}

// Tests that when a ProxyErrorClient is provided to NetworkContextParams, this
// client's OnRequestMaybeFailedDueToProxySettings() method is
// NOT called when a request fails due to a non-proxy related error (in this
// case the target host is unreachable).
TEST_F(NetworkContextTest, ProxyErrorClientNotNotifiedOfUnreachableError) {
  // Avoid the test having a network dependency on DNS.
  ScopedFailAllHostResolutions fail_dns;

  // Set up the NetworkContext that uses the default DIRECT proxy
  // configuration.
  TestProxyErrorClient proxy_error_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->proxy_error_client =
      proxy_error_client.CreateInterfacePtrInfo();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request to an unreachable URL.
  ResourceRequest request;
  request.url = GURL("http://server.bad.dns/fail");

  mojom::URLLoaderFactoryPtr loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                          std::move(loader_params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Confirm the the resource request failed.
  client.RunUntilComplete();
  EXPECT_THAT(client.completion_status().error_code,
              net::test::IsError(net::ERR_NAME_NOT_RESOLVED));

  // Tear down the network context and wait for a pipe error to ensure
  // that all queued messages on |proxy_error_client| have been processed.
  network_context.reset();
  proxy_error_client.RunUntilMojoPipeError();

  // Confirm that the ProxyErrorClient received no calls.
  const auto& request_errors =
      proxy_error_client.on_request_maybe_failed_calls();
  const auto& pac_errors = proxy_error_client.on_pac_script_error_calls();

  EXPECT_EQ(0u, request_errors.size());
  EXPECT_EQ(0u, pac_errors.size());
}

// Test mojom::ProxyResolver that completes calls to GetProxyForUrl() with a
// DIRECT "proxy". It additionall emits a script error on line 42 for every call
// to GetProxyForUrl().
class MockMojoProxyResolver : public proxy_resolver::mojom::ProxyResolver {
 public:
  MockMojoProxyResolver() {}

 private:
  // Overridden from proxy_resolver::mojom::ProxyResolver:
  void GetProxyForUrl(
      const GURL& url,
      proxy_resolver::mojom::ProxyResolverRequestClientPtr client) override {
    // Report a Javascript error and then complete the request successfully,
    // having chosen DIRECT connections.
    client->OnError(42, "Failed: FindProxyForURL(url=" + url.spec() + ")");

    net::ProxyInfo result;
    result.UseDirect();

    client->ReportResult(net::OK, result);
  }

  DISALLOW_COPY_AND_ASSIGN(MockMojoProxyResolver);
};

// Test mojom::ProxyResolverFactory implementation that successfully completes
// any CreateResolver() requests, and binds the request to a new
// MockMojoProxyResolver.
class MockMojoProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory {
 public:
  MockMojoProxyResolverFactory() {}

  // Binds and returns a mock ProxyResolverFactory whose lifetime is bound to
  // the message pipe.
  static proxy_resolver::mojom::ProxyResolverFactoryPtrInfo Create() {
    proxy_resolver::mojom::ProxyResolverFactoryPtrInfo ptr_info;
    mojo::MakeStrongBinding(std::make_unique<MockMojoProxyResolverFactory>(),
                            mojo::MakeRequest(&ptr_info));
    return ptr_info;
  }

 private:
  void CreateResolver(
      const std::string& pac_url,
      mojo::InterfaceRequest<proxy_resolver::mojom::ProxyResolver> request,
      proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr client)
      override {
    // Bind |request| to a new MockMojoProxyResolver, and return success.
    mojo::MakeStrongBinding(std::make_unique<MockMojoProxyResolver>(),
                            std::move(request));
    client->ReportResult(net::OK);
  }

  DISALLOW_COPY_AND_ASSIGN(MockMojoProxyResolverFactory);
};

// Tests that when a ProxyErrorClient is provided to NetworkContextParams, this
// client's OnPACScriptError() method is called whenever the PAC script throws
// an error.
TEST_F(NetworkContextTest, ProxyErrorClientNotifiedOfPacError) {
  // Avoid the test having a network dependency on DNS.
  ScopedFailAllHostResolutions fail_dns;

  // Set up the NetworkContext so that it sends "proxy errors" to
  // |proxy_error_client|, and uses a mock ProxyResolverFactory that emits
  // script errors.
  TestProxyErrorClient proxy_error_client;
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->proxy_error_client =
      proxy_error_client.CreateInterfacePtrInfo();
  // The PAC URL doesn't matter, since the test is configured to use a
  // mock ProxyResolverFactory which doesn't actually evaluate it. It just
  // needs to be a data: URL to ensure the network fetch doesn't fail.
  //
  // That said, the mock PAC evalulator being used behaves similarly to the
  // script embedded in the data URL below.
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateFromCustomPacURL(
      GURL("data:,function FindProxyForURL(url,host){throw url}"));
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  context_params->proxy_resolver_factory =
      MockMojoProxyResolverFactory::Create();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request. This will end up being sent DIRECT since the PAC
  // script is broken.
  ResourceRequest request;
  request.url = GURL("http://server.bad.dns");

  mojom::URLLoaderFactoryPtr loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                          std::move(loader_params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Confirm the the resource request failed.
  client.RunUntilComplete();
  EXPECT_THAT(client.completion_status().error_code,
              net::test::IsError(net::ERR_NAME_NOT_RESOLVED));

  // Tear down the network context and wait for a pipe error to ensure
  // that all queued messages on |proxy_error_client| have been processed.
  network_context.reset();
  proxy_error_client.RunUntilMojoPipeError();

  // Confirm that the ProxyErrorClient received the expected calls.
  const auto& request_errors =
      proxy_error_client.on_request_maybe_failed_calls();
  const auto& pac_errors = proxy_error_client.on_pac_script_error_calls();

  EXPECT_EQ(0u, request_errors.size());

  ASSERT_EQ(1u, pac_errors.size());
  EXPECT_EQ(pac_errors[0].line, 42);
  EXPECT_EQ(pac_errors[0].details,
            "Failed: FindProxyForURL(url=http://server.bad.dns/)");
}

// Test ensures that ProxyServer data is populated correctly across Mojo calls.
// Basically it performs a set of URLLoader network requests, whose requests
// configure proxies. Then it checks whether the expected proxy scheme is
// respected.
TEST_F(NetworkContextTest, EnsureProperProxyServerIsUsed) {
  net::test_server::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  struct ProxyConfigSet {
    net::ProxyConfig proxy_config;
    GURL url;
    net::ProxyServer::Scheme expected_proxy_config_scheme;
  } proxy_config_set[2];

  proxy_config_set[0].proxy_config.proxy_rules().ParseFromString(
      base::StringPrintf("http=%s",
                         test_server.host_port_pair().ToString().c_str()));
  // The domain here is irrelevant, and it is the path that matters.
  proxy_config_set[0].url = GURL("http://does.not.matter/echo");
  proxy_config_set[0].expected_proxy_config_scheme =
      net::ProxyServer::SCHEME_HTTP;

  proxy_config_set[1].proxy_config.proxy_rules().ParseFromString(
      "http=direct://");
  proxy_config_set[1].url = test_server.GetURL("/echo");
  proxy_config_set[1].expected_proxy_config_scheme =
      net::ProxyServer::SCHEME_DIRECT;

  for (const auto& proxy_data : proxy_config_set) {
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_data.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    mojom::ProxyConfigClientPtr config_client;
    context_params->proxy_config_client_request =
        mojo::MakeRequest(&config_client);
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    mojom::URLLoaderFactoryPtr loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = 0;
    network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                            std::move(params));

    ResourceRequest request;
    // The domain here is irrelevant, and it is the path that matters.
    request.url = proxy_data.url;  // test_server.GetURL("/echo");

    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(client.response_head().proxy_server.scheme(),
              proxy_data.expected_proxy_config_scheme);
  }
}

// Custom proxy does not apply to localhost, so resolve kMockHost to localhost,
// and use that instead.
class NetworkContextMockHostTest : public NetworkContextTest {
 public:
  NetworkContextMockHostTest() {
    auto host_resolver = std::make_unique<net::MockHostResolver>();
    host_resolver->rules()->AddRule(kMockHost, "127.0.0.1");
    network_service_->SetHostResolver(std::move(host_resolver));
  }

 protected:
  GURL GetURLWithMockHost(const net::EmbeddedTestServer& server,
                          const std::string& relative_url) {
    GURL server_base_url = server.base_url();
    GURL base_url =
        GURL(base::StrCat({server_base_url.scheme(), "://", kMockHost, ":",
                           server_base_url.port()}));
    EXPECT_TRUE(base_url.is_valid()) << base_url.possibly_invalid_spec();
    return base_url.Resolve(relative_url);
  }

  net::ProxyServer ConvertToProxyServer(const net::EmbeddedTestServer& server) {
    std::string base_url = server.base_url().spec();
    // Remove slash from URL.
    base_url.pop_back();
    auto proxy_server =
        net::ProxyServer::FromURI(base_url, net::ProxyServer::SCHEME_HTTP);
    EXPECT_TRUE(proxy_server.is_valid()) << base_url;
    return proxy_server;
  }
};

TEST_F(NetworkContextMockHostTest, CustomProxyAddsHeaders) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  config->pre_cache_headers.SetHeader("pre_foo", "pre_foo_value");
  config->post_cache_headers.SetHeader("post_foo", "post_foo_value");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("pre_bar", "pre_bar_value");
  request.custom_proxy_post_cache_headers.SetHeader("post_bar",
                                                    "post_bar_value");
  request.url = GetURLWithMockHost(
      test_server, "/echoheader?pre_foo&post_foo&pre_bar&post_bar");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"post_bar_value", "post_foo_value",
                                        "pre_bar_value", "pre_foo_value"},
                                       "\n"));
  EXPECT_EQ(client->response_head().proxy_server, proxy_server);
}

TEST_F(NetworkContextMockHostTest,
       CustomProxyRequestHeadersOverrideConfigHeaders) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  config->pre_cache_headers.SetHeader("foo", "bad");
  config->post_cache_headers.SetHeader("bar", "bad");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("foo", "foo_value");
  request.custom_proxy_post_cache_headers.SetHeader("bar", "bar_value");
  request.url = GetURLWithMockHost(test_server, "/echoheader?foo&bar");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_EQ(client->response_head().proxy_server, proxy_server);
}

TEST_F(NetworkContextMockHostTest, CustomProxyConfigHeadersAddedBeforeCache) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  config->pre_cache_headers.SetHeader("foo", "foo_value");
  config->post_cache_headers.SetHeader("bar", "bar_value");
  proxy_config_client->OnCustomProxyConfigUpdated(config->Clone());
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = GetURLWithMockHost(test_server, "/echoheadercache?foo&bar");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_EQ(client->response_head().proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head().was_fetched_via_cache);

  // post_cache_headers should not break caching.
  config->post_cache_headers.SetHeader("bar", "new_bar");
  proxy_config_client->OnCustomProxyConfigUpdated(config->Clone());
  scoped_task_environment_.RunUntilIdle();

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_TRUE(client->response_head().was_fetched_via_cache);

  // pre_cache_headers should invalidate cache.
  config->pre_cache_headers.SetHeader("foo", "new_foo");
  proxy_config_client->OnCustomProxyConfigUpdated(config->Clone());
  scoped_task_environment_.RunUntilIdle();

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"new_bar", "new_foo"}, "\n"));
  EXPECT_EQ(client->response_head().proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head().was_fetched_via_cache);
}

TEST_F(NetworkContextMockHostTest, CustomProxyRequestHeadersAddedBeforeCache) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = GetURLWithMockHost(test_server, "/echoheadercache?foo&bar");
  request.custom_proxy_pre_cache_headers.SetHeader("foo", "foo_value");
  request.custom_proxy_post_cache_headers.SetHeader("bar", "bar_value");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_EQ(client->response_head().proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head().was_fetched_via_cache);

  // custom_proxy_post_cache_headers should not break caching.
  request.custom_proxy_post_cache_headers.SetHeader("bar", "new_bar");

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_TRUE(client->response_head().was_fetched_via_cache);

  // custom_proxy_pre_cache_headers should invalidate cache.
  request.custom_proxy_pre_cache_headers.SetHeader("foo", "new_foo");

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"new_bar", "new_foo"}, "\n"));
  EXPECT_EQ(client->response_head().proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head().was_fetched_via_cache);
}

TEST_F(NetworkContextMockHostTest,
       CustomProxyDoesNotAddHeadersWhenNoProxyUsed) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->pre_cache_headers.SetHeader("pre_foo", "bad");
  config->post_cache_headers.SetHeader("post_foo", "bad");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("pre_bar", "bad");
  request.custom_proxy_post_cache_headers.SetHeader("post_bar", "bad");
  request.url = GetURLWithMockHost(
      test_server, "/echoheader?pre_foo&post_foo&pre_bar&post_bar");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"None", "None", "None", "None"}, "\n"));
  EXPECT_TRUE(client->response_head().proxy_server.is_direct());
}

TEST_F(NetworkContextMockHostTest,
       CustomProxyDoesNotAddHeadersWhenOtherProxyUsed) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  // Set up a proxy to be used by the proxy config service.
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(
      "http=" + ConvertToProxyServer(proxy_test_server).ToURI());
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->pre_cache_headers.SetHeader("pre_foo", "bad");
  config->post_cache_headers.SetHeader("post_foo", "bad");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("pre_bar", "bad");
  request.custom_proxy_post_cache_headers.SetHeader("post_bar", "bad");
  request.url = GetURLWithMockHost(
      test_server, "/echoheader?pre_foo&post_foo&pre_bar&post_bar");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"None", "None", "None", "None"}, "\n"));
  EXPECT_EQ(client->response_head().proxy_server,
            ConvertToProxyServer(proxy_test_server));
}

TEST_F(NetworkContextMockHostTest, CustomProxyUsesAlternateProxyList) {
  net::EmbeddedTestServer invalid_server;
  ASSERT_TRUE(invalid_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojom::CustomProxyConfigClientPtr proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=" +
                                ConvertToProxyServer(invalid_server).ToURI());

  config->alternate_proxy_list.AddProxyServer(
      ConvertToProxyServer(proxy_test_server));
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  scoped_task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = GURL("http://does.not.resolve/echo");
  request.custom_proxy_use_alternate_proxy_list = true;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  // |invalid_server| has no handlers set up so would return an empty response.
  EXPECT_EQ(response, "Echo");
  EXPECT_EQ(client->response_head().proxy_server,
            ConvertToProxyServer(proxy_test_server));
}

}  // namespace

}  // namespace network
