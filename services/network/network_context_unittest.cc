// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
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
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "net/http/http_auth.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/mock_http_cache.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/gtest_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_manager.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_qualities_pref_delegate.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
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

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
#include "net/ftp/ftp_auth_cache.h"
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_test_util.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if defined(OS_CHROMEOS)
#include "services/network/mock_mojo_dhcp_wpad_url_client.h"
#endif  // defined(OS_CHROMEOS)

namespace network {

namespace {

const GURL kURL("http://foo.com");
const GURL kOtherURL("http://other.com");
const url::Origin kOrigin = url::Origin::Create(kURL);
const url::Origin kOtherOrigin = url::Origin::Create(kOtherURL);
constexpr char kMockHost[] = "mock.host";
constexpr char kCustomProxyResponse[] = "CustomProxyResponse";
constexpr int kProcessId = 11;
constexpr int kRouteId = 12;

#if BUILDFLAG(ENABLE_REPORTING)
const base::FilePath::CharType kFilename[] =
    FILE_PATH_LITERAL("TempReportingAndNelStore");
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_CT_SUPPORTED)
void StoreBool(bool* result, const base::Closure& callback, bool value) {
  *result = value;
  callback.Run();
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

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
    NetworkContext* network_context,
    int url_loader_options = mojom::kURLLoadOptionNone,
    int process_id = mojom::kBrowserProcessId) {
  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  auto params = mojom::URLLoaderFactoryParams::New();
  params->process_id = process_id;
  params->is_corb_enabled = false;
  params->network_isolation_key =
      net::NetworkIsolationKey(url::Origin::Create(GURL("https://abc.com")),
                               url::Origin::Create(GURL("https://xyz.com")));
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  auto client = std::make_unique<TestURLLoaderClient>();
  mojom::URLLoaderPtr loader;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      url_loader_options, request, client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client->RunUntilComplete();
  return client;
}

// Creates a URLLoaderPtr and fetches |request| after going through
// |redirect_counts| redirects.
std::unique_ptr<TestURLLoaderClient> FetchRedirectedRequest(
    size_t redirect_counts,
    const ResourceRequest& request,
    NetworkContext* network_context) {
  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  auto params = mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  auto client = std::make_unique<TestURLLoaderClient>();
  mojom::URLLoaderPtr loader;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  while (redirect_counts > 0) {
    client->RunUntilRedirectReceived();
    loader->FollowRedirect({}, {}, base::nullopt);
    client->ClearHasReceivedRedirect();
    --redirect_counts;
  }

  client->RunUntilComplete();
  return client;
}

// Returns a response that redirects to the next URL in |redirect_cycle|.
std::unique_ptr<net::test_server::HttpResponse>
RedirectThroughCycleProxyResponse(
    const std::vector<GURL>& redirect_cycle,
    const net::test_server::HttpRequest& request) {
  DCHECK_LE(1u, redirect_cycle.size());

  // Compute the requested URL from the "Host" header. It's not possible
  // to use the request URL directly since that contains the hostname of the
  // proxy server.
  const GURL kOriginUrl(
      base::StrCat({"http://", request.headers.find("Host")->second +
                                   request.GetURL().path()}));

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();

  GURL redirect_location;
  // Compute |redirect_location| by first finding kOriginUrl in
  // |redirect_cycle|.
  for (size_t i = 0; i < redirect_cycle.size(); ++i) {
    if (redirect_cycle[i] == kOriginUrl) {
      // Set |redirect_location| to the next URL in |redirect_cycle|.
      redirect_location = redirect_cycle[(i + 1) % redirect_cycle.size()];
      break;
    }
  }
  DCHECK(redirect_location.is_valid());

  response->AddCustomHeader("Location", redirect_location.spec());
  response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  response->set_content_type("text/plain");
  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> CustomProxyResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(kCustomProxyResponse);
  return std::move(response);
}

// ProxyLookupClient that drives proxy lookups and can wait for the responses to
// be received.
class TestProxyLookupClient : public mojom::ProxyLookupClient {
 public:
  TestProxyLookupClient() = default;
  ~TestProxyLookupClient() override = default;

  void StartLookUpProxyForURL(const GURL& url,
                              mojom::NetworkContext* network_context) {
    // Make sure this method is called at most once.
    EXPECT_FALSE(receiver_.is_bound());

    network_context->LookUpProxyForURL(url,
                                       receiver_.BindNewPipeAndPassRemote());
  }

  void WaitForResult() { run_loop_.Run(); }

  // mojom::ProxyLookupClient implementation:
  void OnProxyLookupComplete(
      int32_t net_error,
      const base::Optional<net::ProxyInfo>& proxy_info) override {
    EXPECT_FALSE(is_done_);
    EXPECT_FALSE(proxy_info_);

    EXPECT_EQ(net_error == net::OK, proxy_info.has_value());

    is_done_ = true;
    proxy_info_ = proxy_info;
    net_error_ = net_error;
    receiver_.reset();
    run_loop_.Quit();
  }

  const base::Optional<net::ProxyInfo>& proxy_info() const {
    return proxy_info_;
  }

  int32_t net_error() const { return net_error_; }
  bool is_done() const { return is_done_; }

 private:
  mojo::Receiver<mojom::ProxyLookupClient> receiver_{this};

  bool is_done_ = false;
  base::Optional<net::ProxyInfo> proxy_info_;
  int32_t net_error_ = net::ERR_UNEXPECTED;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyLookupClient);
};

class NetworkContextTest : public testing::Test {
 public:
  NetworkContextTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        network_service_(NetworkService::CreateForTesting()) {}
  ~NetworkContextTest() override {}

  std::unique_ptr<NetworkContext> CreateContextWithParams(
      mojom::NetworkContextParamsPtr context_params) {
    network_context_remote_.reset();
    return std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
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

  // Looks up a value with the given name from the NetworkContext's
  // TransportSocketPool info dictionary.
  int GetSocketPoolInfo(NetworkContext* context, base::StringPiece name) {
    return context->url_request_context()
        ->http_transaction_factory()
        ->GetSession()
        ->GetSocketPool(
            net::HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
            net::ProxyServer::Direct())
        ->GetInfoAsValue("", "")
        .FindIntPath(name)
        .value_or(-1);
  }

  int GetSocketCountForGroup(NetworkContext* context,
                             const std::string& group_name) {
    base::Value pool_info =
        context->url_request_context()
            ->http_transaction_factory()
            ->GetSession()
            ->GetSocketPool(
                net::HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
                net::ProxyServer::Direct())
            ->GetInfoAsValue("", "");

    int count = 0;
    base::Value* active_socket_count = pool_info.FindPathOfType(
        base::span<const base::StringPiece>{
            {"groups", group_name, "active_socket_count"}},
        base::Value::Type::INTEGER);
    if (active_socket_count)
      count += active_socket_count->GetInt();
    base::Value* idle_sockets = pool_info.FindPathOfType(
        base::span<const base::StringPiece>{
            {"groups", group_name, "idle_sockets"}},
        base::Value::Type::LIST);
    if (idle_sockets)
      count += idle_sockets->GetList().size();
    base::Value* connect_jobs = pool_info.FindPathOfType(
        base::span<const base::StringPiece>{
            {"groups", group_name, "connect_jobs"}},
        base::Value::Type::LIST);
    if (connect_jobs)
      count += connect_jobs->GetList().size();
    return count;
  }

  GURL GetHttpUrlFromHttps(const GURL& https_url) {
    url::Replacements<char> replacements;
    const char http[] = "http";
    replacements.SetScheme(http, url::Component(0, strlen(http)));
    return https_url.ReplaceComponents(replacements);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<NetworkService> network_service_;
  // Stores the mojo::Remote<NetworkContext> of the most recently created
  // NetworkContext. Not strictly needed, but seems best to mimic real-world
  // usage.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
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

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
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

  client.RunUntilDisconnect();
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
                                  .quic_params.user_agent_id);
}

TEST_F(NetworkContextTest, DataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, FileUrlSupportDisabled) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}

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

TEST_F(NetworkContextTest, EnableReportingWithoutStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(network_context->url_request_context()->reporting_service());
  EXPECT_FALSE(network_context->url_request_context()
                   ->reporting_service()
                   ->GetContextForTesting()
                   ->store());
}

TEST_F(NetworkContextTest, EnableReportingWithStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kReporting);

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->reporting_and_nel_store_path =
      temp_dir.GetPath().Append(kFilename);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(network_context->url_request_context()->reporting_service());
  EXPECT_TRUE(network_context->url_request_context()
                  ->reporting_service()
                  ->GetContextForTesting()
                  ->store());
}

TEST_F(NetworkContextTest, DisableNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kNetworkErrorLogging);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(
      network_context->url_request_context()->network_error_logging_service());
}

TEST_F(NetworkContextTest, EnableNetworkErrorLoggingWithoutStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kNetworkErrorLogging, features::kReporting}, {});

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(
      network_context->url_request_context()->network_error_logging_service());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_error_logging_service()
                   ->GetPersistentNelStoreForTesting());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_error_logging_service()
                   ->GetReportingServiceForTesting()
                   ->GetContextForTesting()
                   ->store());
}

TEST_F(NetworkContextTest, EnableNetworkErrorLoggingWithStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kNetworkErrorLogging, features::kReporting}, {});

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->reporting_and_nel_store_path =
      temp_dir.GetPath().Append(kFilename);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->network_error_logging_service());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_error_logging_service()
                  ->GetPersistentNelStoreForTesting());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_error_logging_service()
                  ->GetReportingServiceForTesting()
                  ->GetContextForTesting()
                  ->store());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

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
  EXPECT_EQ(1350u, params.quic_params.max_packet_length);
  EXPECT_TRUE(params.quic_params.origins_to_force_quic_on.empty());
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
  EXPECT_EQ(network_session_configurator::ChooseCacheType(),
            GetBackendType(backend));
}

// This makes sure that network_session_configurator::ChooseCacheType is
// connected to NetworkContext.
TEST_F(NetworkContextTest, SimpleCache) {
  base::FieldTrialList::CreateFieldTrial("SimpleCacheTrial", "ExperimentYes");
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
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey()));

  // Set a property.
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey(), true);
  // Deleting the context will cause it to flush state. Wait for the pref
  // service to flush to disk.
  network_context.reset();
  task_environment_.RunUntilIdle();

  // Create a new NetworkContext using the same path for HTTP server properties.
  context_params = mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  network_context = CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey()));

  // Now check that ClearNetworkingHistorySince clears the data.
  base::RunLoop run_loop2;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey()));

  // Destroy the network context and let any pending writes complete before
  // destroying |temp_dir|, to avoid leaking any files.
  network_context.reset();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir.Delete());
}

// Checks that ClearNetworkingHistorySince() clears in-memory pref stores and
// invokes the closure passed to it.
TEST_F(NetworkContextTest, ClearHttpServerPropertiesInMemory) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());

  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey()));
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey(), true);
  EXPECT_TRUE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey()));

  base::RunLoop run_loop;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkIsolationKey()));
}

// Checks that ClearNetworkingHistorySince() clears network quality prefs.
TEST_F(NetworkContextTest, ClearingNetworkingHistoryClearNetworkQualityPrefs) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);
  net::TestNetworkQualityEstimator estimator;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());
  TestingPrefServiceSimple pref_service_simple;
  NetworkQualitiesPrefDelegate::RegisterPrefs(pref_service_simple.registry());

  std::unique_ptr<NetworkQualitiesPrefDelegate>
      network_qualities_pref_delegate =
          std::make_unique<NetworkQualitiesPrefDelegate>(&pref_service_simple,
                                                         &estimator);
  NetworkQualitiesPrefDelegate* network_qualities_pref_delegate_ptr =
      network_qualities_pref_delegate.get();
  network_context->set_network_qualities_pref_delegate_for_testing(
      std::move(network_qualities_pref_delegate));

  // Running the loop allows prefs to be set.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_qualities_pref_delegate_ptr->ForceReadPrefsForTesting().empty());

  // Clear the networking history.
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop.QuitClosure());
  run_loop.Run();

  // Running the loop should clear the network quality prefs.
  base::RunLoop().RunUntilIdle();
  // Prefs should be empty now.
  EXPECT_TRUE(
      network_qualities_pref_delegate_ptr->ForceReadPrefsForTesting().empty());
  histogram_tester.ExpectTotalCount("NQE.PrefsSizeOnClearing", 1);
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
    task_environment_.RunUntilIdle();
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
    task_environment_.RunUntilIdle();
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

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
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

      mojo::Remote<mojom::URLLoaderFactory> loader_factory;
      mojom::URLLoaderFactoryParamsPtr params =
          mojom::URLLoaderFactoryParams::New();
      params->process_id = 0;
      network_context->CreateURLLoaderFactory(
          loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

      ResourceRequest request;
      request.url = test_server.GetURL("/echoheader?Referer");
      request.referrer = kReferrer;
      request.referrer_policy = referrer_policy;

      mojom::URLLoaderPtr loader;
      TestURLLoaderClient client;
      loader_factory->CreateLoaderAndStart(
          mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
          0 /* options */, request, client.CreateRemote(),
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
    disk_cache::EntryResult result;
    base::RunLoop run_loop;

    result = backend->CreateEntry(
        url, net::HIGHEST,
        base::BindLambdaForTesting([&](disk_cache::EntryResult got_result) {
          result = std::move(got_result);
          run_loop.Quit();
        }));
    if (result.net_error() == net::ERR_IO_PENDING)
      run_loop.Run();

    result.ReleaseEntry()->Close();
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

TEST_F(NetworkContextTest, NotifyExternalCacheHit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  net::MockHttpCache mock_cache;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  network_context->url_request_context()->set_http_transaction_factory(
      mock_cache.http_cache());

  std::vector<std::string> entry_urls = {
      "http://www.google.com",    "https://www.google.com",
      "http://www.wikipedia.com", "https://www.wikipedia.com",
      "http://localhost:1234",    "https://localhost:1234",
  };

  // The disk cache is lazily instanitated, force it and ensure it's valid.
  ASSERT_TRUE(mock_cache.disk_cache());
  EXPECT_EQ(0U, mock_cache.disk_cache()->GetExternalCacheHits().size());

  for (size_t i = 0; i < entry_urls.size(); i++) {
    GURL test_url(entry_urls[i]);

    net::NetworkIsolationKey key;
    network_context->NotifyExternalCacheHit(test_url, test_url.scheme(), key);
    EXPECT_EQ(i + 1, mock_cache.disk_cache()->GetExternalCacheHits().size());

    // Note: if this breaks check HttpCache::GenerateCacheKey() for changes.
    EXPECT_EQ(test_url, mock_cache.disk_cache()->GetExternalCacheHits().back());
  }
}

TEST_F(NetworkContextTest, NotifyExternalCacheHit_Split) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);
  url::Origin origin_a = url::Origin::Create(GURL("http://a.com"));

  net::MockHttpCache mock_cache;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  network_context->url_request_context()->set_http_transaction_factory(
      mock_cache.http_cache());

  std::vector<std::string> entry_urls = {
      "http://www.google.com",    "https://www.google.com",
      "http://www.wikipedia.com", "https://www.wikipedia.com",
      "http://localhost:1234",    "https://localhost:1234",
  };

  // The disk cache is lazily instanitated, force it and ensure it's valid.
  ASSERT_TRUE(mock_cache.disk_cache());
  EXPECT_EQ(0U, mock_cache.disk_cache()->GetExternalCacheHits().size());

  for (size_t i = 0; i < entry_urls.size(); i++) {
    GURL test_url(entry_urls[i]);

    net::NetworkIsolationKey key = net::NetworkIsolationKey(origin_a, origin_a);
    network_context->NotifyExternalCacheHit(test_url, test_url.scheme(), key);
    EXPECT_EQ(i + 1, mock_cache.disk_cache()->GetExternalCacheHits().size());

    // Since this is splitting the cache, the key also includes the top-level
    // frame origin.
    EXPECT_EQ(base::StrCat({"_dk_http://a.com ", test_url.spec()}),
              mock_cache.disk_cache()->GetExternalCacheHits().back());
  }
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

TEST_F(NetworkContextTest, ClearHostCache) {
  // List of domains added to the host cache before running each test case.
  const char* kDomains[] = {
      "domain0",
      "domain1",
      "domain2",
      "domain3",
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
          net::HostCache::Key(domain, net::DnsQueryType::UNSPECIFIED, 0,
                              net::HostResolverSource::ANY,
                              net::NetworkIsolationKey()),
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
      EXPECT_EQ(
          expect_domain_cached,
          (host_cache->GetMatchingKey(kDomains[i], nullptr /* source_out */,
                                      nullptr /* stale_out */) != nullptr));
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
  cache->Add(origin, net::HttpAuth::AUTH_SERVER, "Realm1",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey(),
             "basic realm=Realm1", net::AuthCredentials(user, password), "/");

  test_clock.Advance(base::TimeDelta::FromHours(1));  // Time now 13:00
  cache->Add(origin, net::HttpAuth::AUTH_PROXY, "Realm2",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey(),
             "basic realm=Realm2", net::AuthCredentials(user, password), "/");

  ASSERT_EQ(2u, cache->GetEntriesSizeForTesting());
  ASSERT_NE(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_SERVER, "Realm1",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));
  ASSERT_NE(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_PROXY, "Realm2",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));

  base::RunLoop run_loop;
  base::Time test_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:30:00", &test_time));
  network_context->ClearHttpAuthCache(test_time, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(1u, cache->GetEntriesSizeForTesting());
  EXPECT_NE(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_SERVER, "Realm1",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));
  EXPECT_EQ(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_PROXY, "Realm2",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));
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
  cache->Add(origin, net::HttpAuth::AUTH_SERVER, "Realm1",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey(),
             "basic realm=Realm1", net::AuthCredentials(user, password), "/");

  test_clock.Advance(base::TimeDelta::FromHours(1));  // Time now 13:00
  cache->Add(origin, net::HttpAuth::AUTH_PROXY, "Realm2",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey(),
             "basic realm=Realm2", net::AuthCredentials(user, password), "/");

  ASSERT_EQ(2u, cache->GetEntriesSizeForTesting());
  ASSERT_NE(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_SERVER, "Realm1",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));
  ASSERT_NE(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_PROXY, "Realm2",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));

  base::RunLoop run_loop;
  network_context->ClearHttpAuthCache(base::Time(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0u, cache->GetEntriesSizeForTesting());
  EXPECT_EQ(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_SERVER, "Realm1",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));
  EXPECT_EQ(nullptr, cache->Lookup(origin, net::HttpAuth::AUTH_PROXY, "Realm2",
                                   net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkIsolationKey()));
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

base::Optional<net::AuthCredentials> GetAuthCredentials(
    NetworkContext* network_context,
    const GURL& origin,
    const net::NetworkIsolationKey& network_isolation_key) {
  base::RunLoop run_loop;
  base::Optional<net::AuthCredentials> result;
  network_context->LookupServerBasicAuthCredentials(
      origin, network_isolation_key,
      base::BindLambdaForTesting(
          [&](const base::Optional<net::AuthCredentials>& credentials) {
            result = credentials;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

TEST_F(NetworkContextTest, LookupServerBasicAuthCredentials) {
  GURL origin("http://foo.test");
  GURL origin2("http://bar.test");
  GURL origin3("http://baz.test");
  net::NetworkIsolationKey network_isolation_key1(url::Origin::Create(origin),
                                                  url::Origin::Create(origin));
  net::NetworkIsolationKey network_isolation_key2(url::Origin::Create(origin2),
                                                  url::Origin::Create(origin2));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->SetSplitAuthCacheByNetworkIsolationKey(true);
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  base::string16 user = base::ASCIIToUTF16("user");
  base::string16 password = base::ASCIIToUTF16("pass");
  cache->Add(origin, net::HttpAuth::AUTH_SERVER, "Realm",
             net::HttpAuth::AUTH_SCHEME_BASIC, network_isolation_key1,
             "basic realm=Realm", net::AuthCredentials(user, password), "/");
  cache->Add(origin2, net::HttpAuth::AUTH_PROXY, "Realm",
             net::HttpAuth::AUTH_SCHEME_BASIC, network_isolation_key1,
             "basic realm=Realm", net::AuthCredentials(user, password), "/");

  base::Optional<net::AuthCredentials> result =
      GetAuthCredentials(network_context.get(), origin, network_isolation_key1);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(user, result->username());
  EXPECT_EQ(password, result->password());

  // Nothing should be returned when using a different NIK.
  EXPECT_FALSE(
      GetAuthCredentials(network_context.get(), origin, network_isolation_key2)
          .has_value());

  // Proxy credentials should not be returned
  result = GetAuthCredentials(network_context.get(), origin2,
                              network_isolation_key1);
  EXPECT_FALSE(result.has_value());
}

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(NetworkContextTest, ClearReportingCacheReports) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
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
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
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
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
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
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
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
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain("https://google.com");
  reporting_cache->SetEndpointForTesting(url::Origin::Create(domain), "group",
                                         domain, net::OriginSubdomains::DEFAULT,
                                         base::Time::Max(), 1 /* priority */,
                                         1 /* weight */);

  ASSERT_EQ(1u, reporting_cache->GetEndpointCount());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0u, reporting_cache->GetEndpointCount());
}

TEST_F(NetworkContextTest, ClearReportingCacheClientsWithFilter) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  GURL domain1("https://google.com");
  reporting_cache->SetEndpointForTesting(
      url::Origin::Create(domain1), "group", domain1,
      net::OriginSubdomains::DEFAULT, base::Time::Max(), 1 /* priority */,
      1 /* weight */);
  GURL domain2("https://chromium.org");
  reporting_cache->SetEndpointForTesting(
      url::Origin::Create(domain2), "group", domain2,
      net::OriginSubdomains::DEFAULT, base::Time::Max(), 1 /* priority */,
      1 /* weight */);

  ASSERT_EQ(2u, reporting_cache->GetEndpointCount());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(1u, reporting_cache->GetEndpointCount());
  EXPECT_TRUE(reporting_cache->GetEndpointForTesting(
      url::Origin::Create(domain2), "group", domain2));
  EXPECT_FALSE(reporting_cache->GetEndpointForTesting(
      url::Origin::Create(domain1), "group", domain1));
}

TEST_F(NetworkContextTest, ClearEmptyReportingCacheClients) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<net::ReportingService> reporting_service =
      net::ReportingService::CreateForTesting(std::move(reporting_context));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_reporting_service(
      reporting_service.get());

  ASSERT_EQ(0u, reporting_cache->GetEndpointCount());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(0u, reporting_cache->GetEndpointCount());
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

void SetCookieCallback(base::RunLoop* run_loop,
                       bool* result_out,
                       net::CanonicalCookie::CookieInclusionStatus result) {
  *result_out = result.IsInclude();
  run_loop->Quit();
}

void GetCookieListCallback(base::RunLoop* run_loop,
                           net::CookieList* result_out,
                           const net::CookieStatusList& result,
                           const net::CookieStatusList& excluded_cookies) {
  *result_out = net::cookie_util::StripStatuses(result);
  run_loop->Quit();
}

bool SetCookieHelper(NetworkContext* network_context,
                     const GURL& url,
                     const std::string& key,
                     const std::string& value) {
  mojo::Remote<mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());
  base::RunLoop run_loop;
  bool result = false;
  cookie_manager->SetCanonicalCookie(
      net::CanonicalCookie(key, value, url.host(), "/", base::Time(),
                           base::Time(), base::Time(), true, false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_LOW),
      url.scheme(), net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&SetCookieCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

TEST_F(NetworkContextTest, CookieManager) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());

  mojo::Remote<mojom::CookieManager> cookie_manager_remote;
  network_context->GetCookieManager(
      cookie_manager_remote.BindNewPipeAndPassReceiver());

  // Set a cookie through the cookie interface.
  base::RunLoop run_loop1;
  bool result = false;
  cookie_manager_remote->SetCanonicalCookie(
      net::CanonicalCookie("TestCookie", "1", "www.test.com", "/", base::Time(),
                           base::Time(), base::Time(), false, false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_LOW),
      "https", net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&SetCookieCallback, &run_loop1, &result));
  run_loop1.Run();
  EXPECT_TRUE(result);

  // Confirm that cookie is visible directly through the store associated with
  // the network context.
  base::RunLoop run_loop2;
  net::CookieList cookies;
  network_context->url_request_context()
      ->cookie_store()
      ->GetCookieListWithOptionsAsync(
          GURL("http://www.test.com/whatever"),
          net::CookieOptions::MakeAllInclusive(),
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
    mojo::Remote<mojom::ProxyConfigClient> config_client;
    context_params->proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
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
      task_environment_.RunUntilIdle();

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
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
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
  task_environment_.RunUntilIdle();
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
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Proxy requests should hang.
  TestProxyLookupClient proxy_lookup_client;
  proxy_lookup_client.StartLookUpProxyForURL(GURL("http://foo/"),
                                             network_context.get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, network_context->pending_proxy_lookup_requests_for_testing());
  EXPECT_FALSE(proxy_lookup_client.is_done());

  // Destroying the NetworkContext should cause the pending lookup to fail with
  // ERR_ABORTED.
  network_context.reset();
  proxy_lookup_client.WaitForResult();
  EXPECT_FALSE(proxy_lookup_client.proxy_info());
  EXPECT_EQ(net::ERR_ABORTED, proxy_lookup_client.net_error());
}

TEST_F(NetworkContextTest, CancelPendingProxyLookup) {
  // Create a NetworkContext without an initial proxy configuration.
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config.reset();
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Proxy requests should hang.
  std::unique_ptr<TestProxyLookupClient> proxy_lookup_client =
      std::make_unique<TestProxyLookupClient>();
  proxy_lookup_client->StartLookUpProxyForURL(GURL("http://foo/"),
                                              network_context.get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_lookup_client->is_done());
  EXPECT_EQ(1u, network_context->pending_proxy_lookup_requests_for_testing());

  // Cancelling the proxy lookup should cause the proxy lookup request objects
  // to be deleted.
  proxy_lookup_client.reset();
  task_environment_.RunUntilIdle();

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
  test::UDPSocketListenerImpl listener;
  mojo::Receiver<mojom::UDPSocketListener> listener_receiver(&listener);

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  mojo::Remote<mojom::UDPSocket> server_socket;
  network_context->CreateUDPSocket(
      server_socket.BindNewPipeAndPassReceiver(),
      listener_receiver.BindNewPipeAndPassRemote());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, nullptr, &server_addr));

  // Create a client socket to send datagrams.
  mojo::Remote<mojom::UDPSocket> client_socket;
  network_context->CreateUDPSocket(client_socket.BindNewPipeAndPassReceiver(),
                                   mojo::NullRemote());

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

  listener.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, listener.results().size());

  int i = 0;
  for (const auto& result : listener.results()) {
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

  mojo::Remote<mojom::NetLogExporter> net_log_exporter;
  network_context->CreateNetLogExporter(
      net_log_exporter.BindNewPipeAndPassReceiver());

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

  net::TestCompletionCallback start_callback;
  net_log_exporter->Start(std::move(out_file), std::move(dict_start),
                          net::NetLogCaptureMode::kDefault, 100 * 1024,
                          start_callback.callback());
  EXPECT_EQ(net::OK, start_callback.WaitForResult());

  base::Value dict_late(base::Value::Type::DICTIONARY);
  const char kKeyLate[] = "late";
  const char kValLate[] = "snowval";
  dict_late.SetKey(kKeyLate, base::Value(kValLate));

  net::TestCompletionCallback stop_callback;
  net_log_exporter->Stop(std::move(dict_late), stop_callback.callback());
  EXPECT_EQ(net::OK, stop_callback.WaitForResult());

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

  mojo::Remote<mojom::NetLogExporter> net_log_exporter;
  network_context->CreateNetLogExporter(
      net_log_exporter.BindNewPipeAndPassReceiver());

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File out_file(temp_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(out_file.IsValid());

  net::TestCompletionCallback start_callback;
  net_log_exporter->Start(
      std::move(out_file), base::Value(base::Value::Type::DICTIONARY),
      net::NetLogCaptureMode::kDefault,
      mojom::NetLogExporter::kUnlimitedFileSize, start_callback.callback());
  EXPECT_EQ(net::OK, start_callback.WaitForResult());

  net::TestCompletionCallback stop_callback;
  net_log_exporter->Stop(base::Value(base::Value::Type::DICTIONARY),
                         stop_callback.callback());
  EXPECT_EQ(net::OK, stop_callback.WaitForResult());

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

  mojo::Remote<mojom::NetLogExporter> net_log_exporter;
  network_context->CreateNetLogExporter(
      net_log_exporter.BindNewPipeAndPassReceiver());

  net::TestCompletionCallback stop_callback;
  net_log_exporter->Stop(base::Value(base::Value::Type::DICTIONARY),
                         stop_callback.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, stop_callback.WaitForResult());

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file.IsValid());

  net::TestCompletionCallback start_callback;
  net_log_exporter->Start(
      std::move(temp_file), base::Value(base::Value::Type::DICTIONARY),
      net::NetLogCaptureMode::kDefault, 100 * 1024, start_callback.callback());
  EXPECT_EQ(net::OK, start_callback.WaitForResult());

  // Can't start twice.
  base::FilePath temp_path2;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path2));
  base::File temp_file2(
      temp_path2, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file2.IsValid());

  net::TestCompletionCallback start_callback2;
  net_log_exporter->Start(
      std::move(temp_file2), base::Value(base::Value::Type::DICTIONARY),
      net::NetLogCaptureMode::kDefault, 100 * 1024, start_callback2.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, start_callback2.WaitForResult());

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
      net::NetLogCaptureMode::kDefault, 100, base::BindOnce([](int) {}));
  net_log_exporter = nullptr;
  block_mktemp.Signal();

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(path));
  base::DeleteFile(temp_path, false);
}

net::IPEndPoint CreateExpectedEndPoint(const std::string& address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

class TestResolveHostClient : public ResolveHostClientBase {
 public:
  TestResolveHostClient(mojo::PendingRemote<mojom::ResolveHostClient>* remote,
                        base::RunLoop* run_loop)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()),
        complete_(false),
        run_loop_(run_loop) {
    DCHECK(run_loop_);
  }

  void CloseReceiver() { receiver_.reset(); }

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
  mojo::Receiver<mojom::ResolveHostClient> receiver_;

  bool complete_;
  int result_error_;
  base::Optional<net::AddressList> result_addresses_;
  base::RunLoop* const run_loop_;
};

TEST_F(NetworkContextTest, ResolveHost_Sync) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->set_synchronous_mode(true);

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 160),
                               std::move(optional_parameters),
                               std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Async) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->set_synchronous_mode(false);

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 160),
                               std::move(optional_parameters),
                               std::move(pending_response_client));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
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
  auto resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->rules()->AddSimulatedFailure("example.com");
  resolver->set_synchronous_mode(true);

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("example.com", 160),
                               std::move(optional_parameters),
                               std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Failure_Async) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  network_context->url_request_context()->set_host_resolver(resolver.get());
  resolver->rules()->AddSimulatedFailure("example.com");
  resolver->set_synchronous_mode(false);

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("example.com", 160),
                               std::move(optional_parameters),
                               std::move(pending_response_client));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
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
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  network_context->ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                               std::move(pending_response_client));
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
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  network_context->ResolveHost(net::HostPortPair("localhost", 160),
                               std::move(optional_parameters),
                               std::move(pending_response_client));
  control_handle.reset();
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
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_EQ(0, resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 80),
                               std::move(optional_parameters),
                               std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

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
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_EQ(0, resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 80),
                               std::move(optional_parameters),
                               std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

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
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_EQ(0, resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(net::HostPortPair("localhost", 80),
                               std::move(optional_parameters),
                               std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  response_client.CloseReceiver();
  run_loop.RunUntilIdle();

  // Response pipe is closed, so no results to check. Internal request should be
  // cancelled.
  EXPECT_FALSE(response_client.complete());
  EXPECT_EQ(1, resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

// Test factory of net::HostResolvers. Creates standard (but potentially non-
// caching) net::ContextHostResolver. Keeps pointers to all created resolvers.
class TestResolverFactory : public net::HostResolver::Factory {
 public:
  static TestResolverFactory* CreateAndSetFactory(NetworkService* service) {
    auto factory = std::make_unique<TestResolverFactory>();
    auto* factory_ptr = factory.get();
    service->set_host_resolver_factory_for_testing(std::move(factory));
    return factory_ptr;
  }

  std::unique_ptr<net::HostResolver> CreateResolver(
      net::HostResolverManager* manager,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override {
    DCHECK(host_mapping_rules.empty());
    auto resolver = std::make_unique<net::ContextHostResolver>(
        manager, nullptr /* host_cache */);
    resolvers_.push_back(resolver.get());
    return resolver;
  }

  std::unique_ptr<net::HostResolver> CreateStandaloneResolver(
      net::NetLog* net_log,
      const net::HostResolver::ManagerOptions& options,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override {
    DCHECK(host_mapping_rules.empty());
    std::unique_ptr<net::ContextHostResolver> resolver =
        net::HostResolver::CreateStandaloneContextResolver(net_log, options,
                                                           enable_caching);
    resolvers_.push_back(resolver.get());
    return resolver;
  }

  const std::vector<net::ContextHostResolver*>& resolvers() const {
    return resolvers_;
  }

  void ForgetResolvers() { resolvers_.clear(); }

 private:
  std::vector<net::ContextHostResolver*> resolvers_;
};

TEST_F(NetworkContextTest, CreateHostResolver) {
  // Inject a factory to control and capture created net::HostResolvers.
  TestResolverFactory* factory =
      TestResolverFactory::CreateAndSetFactory(network_service_.get());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Creates single shared (within the NetworkContext) internal HostResolver.
  EXPECT_EQ(1u, factory->resolvers().size());
  factory->ForgetResolvers();

  mojo::Remote<mojom::HostResolver> resolver;
  network_context->CreateHostResolver(base::nullopt,
                                      resolver.BindNewPipeAndPassReceiver());

  // Expected to reuse shared (within the NetworkContext) internal HostResolver.
  EXPECT_TRUE(factory->resolvers().empty());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                        std::move(pending_response_client));
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
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto internal_resolver = std::make_unique<net::HangingHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_host_resolver(
      internal_resolver.get());

  mojo::Remote<mojom::HostResolver> resolver;
  network_context->CreateHostResolver(base::nullopt,
                                      resolver.BindNewPipeAndPassReceiver());

  ASSERT_EQ(0, internal_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80),
                        std::move(optional_parameters),
                        std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  resolver.reset();
  run_loop.Run();

  // On resolver destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, internal_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(NetworkContextTest, CreateHostResolver_CloseContext) {
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto internal_resolver = std::make_unique<net::HangingHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_host_resolver(
      internal_resolver.get());

  mojo::Remote<mojom::HostResolver> resolver;
  network_context->CreateHostResolver(base::nullopt,
                                      resolver.BindNewPipeAndPassReceiver());

  ASSERT_EQ(0, internal_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80),
                        std::move(optional_parameters),
                        std::move(pending_response_client));
  // Run a bit to ensure the resolve request makes it to the resolver. Otherwise
  // the resolver will be destroyed and close its pipe before it even knows
  // about the request to send a failure.
  task_environment_.RunUntilIdle();

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
  bool resolver_closed = false;
  auto resolver_closed_callback =
      base::BindLambdaForTesting([&]() { resolver_closed = true; });
  resolver.set_disconnect_handler(resolver_closed_callback);

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

// Config overrides are not supported on iOS.
#if !defined(OS_IOS)
TEST_F(NetworkContextTest, CreateHostResolverWithConfigOverrides) {
  // Inject a factory to control and capture created net::HostResolvers.
  TestResolverFactory* factory =
      TestResolverFactory::CreateAndSetFactory(network_service_.get());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Creates single shared (within the NetworkContext) internal HostResolver.
  EXPECT_EQ(1u, factory->resolvers().size());
  factory->ForgetResolvers();

  net::DnsConfigOverrides overrides;
  overrides.nameservers = std::vector<net::IPEndPoint>{
      CreateExpectedEndPoint("100.100.100.100", 22)};

  mojo::Remote<mojom::HostResolver> resolver;
  network_context->CreateHostResolver(overrides,
                                      resolver.BindNewPipeAndPassReceiver());

  // Should create 1 private resolver with a DnsClient (if DnsClient is
  // enablable for the build config).
  ASSERT_EQ(1u, factory->resolvers().size());
  net::ContextHostResolver* internal_resolver = factory->resolvers().front();

  EXPECT_TRUE(internal_resolver->GetDnsConfigAsValue());

  // Override DnsClient with a basic mock.
  net::DnsConfig base_configuration;
  base_configuration.nameservers = {CreateExpectedEndPoint("12.12.12.12", 53)};
  const std::string kQueryHostname = "example.com";
  const std::string kResult = "1.2.3.4";
  net::IPAddress result;
  CHECK(result.AssignFromIPLiteral(kResult));
  net::MockDnsClientRuleList rules;
  rules.emplace_back(kQueryHostname, net::dns_protocol::kTypeA,
                     false /* secure */,
                     net::MockDnsClientRule::Result(
                         net::BuildTestDnsResponse(kQueryHostname, result)),
                     false /* delay */);
  rules.emplace_back(
      kQueryHostname, net::dns_protocol::kTypeAAAA, false /* secure */,
      net::MockDnsClientRule::Result(net::MockDnsClientRule::ResultType::EMPTY),
      false /* delay */);
  auto mock_dns_client = std::make_unique<net::MockDnsClient>(
      base_configuration, std::move(rules));
  mock_dns_client->SetInsecureEnabled(true);
  mock_dns_client->set_ignore_system_config_changes(true);
  auto* mock_dns_client_ptr = mock_dns_client.get();
  internal_resolver->GetManagerForTesting()->SetDnsClientForTesting(
      std::move(mock_dns_client));

  // Test that the DnsClient is getting the overridden configuration.
  EXPECT_TRUE(overrides.ApplyOverrides(base_configuration)
                  .Equals(*mock_dns_client_ptr->GetEffectiveConfig()));

  // Ensure we are using the private resolver by testing that we get results
  // from the overridden DnsClient.
  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::DnsQueryType::A;
  optional_parameters->source = net::HostResolverSource::DNS;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);
  resolver->ResolveHost(net::HostPortPair(kQueryHostname, 80),
                        std::move(optional_parameters),
                        std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kResult, 80)));
}
#endif  // defined(OS_IOS)

TEST_F(NetworkContextTest, ActivateDohProbes) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->primary_network_context = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_FALSE(resolver->IsDohProbeRunning());

  network_context->ActivateDohProbes();
  EXPECT_TRUE(resolver->IsDohProbeRunning());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(resolver->IsDohProbeRunning());

  network_context.reset();

  EXPECT_FALSE(resolver->IsDohProbeRunning());
}

TEST_F(NetworkContextTest, ActivateDohProbes_NotPrimaryContext) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->url_request_context()->set_host_resolver(resolver.get());

  ASSERT_FALSE(resolver->IsDohProbeRunning());

  network_context->ActivateDohProbes();
  EXPECT_FALSE(resolver->IsDohProbeRunning());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(resolver->IsDohProbeRunning());
}

TEST_F(NetworkContextTest, PrivacyModeDisabledByDefault) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->ForcePrivacyMode(kURL, kOtherURL, kOtherOrigin));
}

TEST_F(NetworkContextTest, PrivacyModeEnabledIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK,
                    network_context.get());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_delegate()
                  ->ForcePrivacyMode(kURL, kOtherURL, kOtherOrigin));
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->ForcePrivacyMode(kOtherURL, kURL, kOrigin));
}

TEST_F(NetworkContextTest, PrivacyModeDisabledIfCookiesAllowed) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW,
                    network_context.get());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->ForcePrivacyMode(kURL, kOtherURL, kOtherOrigin));
}

TEST_F(NetworkContextTest, PrivacyModeDisabledIfCookiesSettingForOtherURL) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // URLs are switched so setting should not apply.
  SetContentSetting(kOtherURL, kURL, CONTENT_SETTING_BLOCK,
                    network_context.get());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->ForcePrivacyMode(kURL, kOtherURL, kOtherOrigin));
}

TEST_F(NetworkContextTest, PrivacyModeEnabledIfThirdPartyCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::NetworkDelegate* delegate =
      network_context->url_request_context()->network_delegate();

  network_context->cookie_manager()->BlockThirdPartyCookies(true);
  EXPECT_TRUE(delegate->ForcePrivacyMode(kURL, kOtherURL, kOtherOrigin));
  EXPECT_FALSE(delegate->ForcePrivacyMode(kURL, kURL, kOrigin));

  network_context->cookie_manager()->BlockThirdPartyCookies(false);
  EXPECT_FALSE(delegate->ForcePrivacyMode(kURL, kOtherURL, kOtherOrigin));
  EXPECT_FALSE(delegate->ForcePrivacyMode(kURL, kURL, kOrigin));
}

TEST_F(NetworkContextTest, CanSetCookieFalseIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request = context.CreateRequest(
      kURL, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  net::CanonicalCookie cookie("TestCookie", "1", "www.test.com", "/",
                              base::Time(), base::Time(), base::Time(), false,
                              false, net::CookieSameSite::LAX_MODE,
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
                              false, net::CookieSameSite::LAX_MODE,
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
  ConnectionListener() = default;

  ~ConnectionListener() override = default;

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
    DCHECK(on_done_accepting_connections_.is_null());
    DCHECK_GT(num_connections, 0u);
    base::RunLoop run_loop;
    {
      base::AutoLock lock(lock_);
      EXPECT_GE(num_connections, sockets_.size() - total_sockets_waited_for_);
      // QuitWhenIdle() instead of regular Quit() because in Preconnect tests we
      // count "idle_socket_count" but tasks posted synchronously after
      // AcceptedSocket() need to resolve before the new sockets are considered
      // idle.
      on_done_accepting_connections_ = run_loop.QuitWhenIdleClosure();
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
    DCHECK(!on_done_accepting_connections_.is_null() ||
           num_accepted_connections_needed_ == 0);
    if (on_done_accepting_connections_.is_null() ||
        num_accepted_connections_needed_ !=
            sockets_.size() - total_sockets_waited_for_) {
      return;
    }

    num_accepted_connections_needed_ = 0;
    std::move(on_done_accepting_connections_).Run();
  }

  int GetTotalSocketsSeen() const {
    base::AutoLock lock(lock_);
    return total_sockets_seen_;
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

  int total_sockets_seen_ = 0;
  int total_sockets_waited_for_ = 0;

  enum SocketStatus { SOCKET_ACCEPTED, SOCKET_READ_FROM };

  // This lock protects all the members below, which each are used on both the
  // IO and UI thread. Members declared after the lock are protected by it.
  mutable base::Lock lock_;
  typedef std::map<uint16_t, SocketStatus> SocketContainer;
  SocketContainer sockets_;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before invoking |on_done_accepting_connections_|.
  size_t num_accepted_connections_needed_ = 0;
  base::OnceClosure on_done_accepting_connections_;

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
                                     /*allow_credentials=*/true,
                                     net::NetworkIsolationKey());
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
  network_context->PreconnectSockets(1, server_http_url,
                                     /*allow_credentials=*/false,
                                     net::NetworkIsolationKey());
  connection_listener.WaitForAcceptedConnections(1u);

  int num_sockets = GetSocketCountForGroup(
      network_context.get(),
      "pm/" + net::HostPortPair::FromURL(server_http_url).ToString());
  EXPECT_EQ(num_sockets, 1);

  const base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  network_context->url_request_context()->transport_security_state()->AddHSTS(
      server_http_url.host(), expiry, false);
  network_context->PreconnectSockets(1, server_http_url,
                                     /*allow_credentials=*/false,
                                     net::NetworkIsolationKey());
  connection_listener.WaitForAcceptedConnections(1u);

  // If HSTS weren't respected, the initial connection would have been reused.
  num_sockets = GetSocketCountForGroup(
      network_context.get(),
      "pm/ssl/" + net::HostPortPair::FromURL(server_http_url).ToString());
  EXPECT_EQ(num_sockets, 1);
}

TEST_F(NetworkContextTest, PreconnectZero) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(0, test_server.base_url(),
                                     /*allow_credentials=*/true,
                                     net::NetworkIsolationKey());
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
                                     /*allow_credentials=*/true,
                                     net::NetworkIsolationKey());
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
                                     /*allow_credentials=*/true,
                                     net::NetworkIsolationKey());

  connection_listener.WaitForAcceptedConnections(4u);

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 4);
}

TEST_F(NetworkContextTest, PreconnectMax) {
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
                                     /*allow_credentials=*/true,
                                     net::NetworkIsolationKey());

  // Wait until |max_num_sockets| have been connected.
  connection_listener.WaitForAcceptedConnections(max_num_sockets);

  // This is not guaranteed to wait long enough if more than |max_num_sockets|
  // connections are actually made, but experimentally, it fails consistently if
  // that's the case.
  base::RunLoop().RunUntilIdle();

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, max_num_sockets);
}

// Make sure preconnects for the same URL but with different network isolation
// keys are not merged.
TEST_F(NetworkContextTest, PreconnectNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kPartitionConnectionsByNetworkIsolationKey);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  const auto kOriginFoo = url::Origin::Create(GURL("http://foo.test"));
  const auto kOriginBar = url::Origin::Create(GURL("http://bar.test"));
  const net::NetworkIsolationKey kKey1(kOriginFoo, kOriginFoo);
  const net::NetworkIsolationKey kKey2(kOriginBar, kOriginBar);
  network_context->PreconnectSockets(1, test_server.base_url(),
                                     /*allow_credentials=*/false, kKey1);
  network_context->PreconnectSockets(2, test_server.base_url(),
                                     /*allow_credentials=*/false, kKey2);
  connection_listener.WaitForAcceptedConnections(3u);

  net::ClientSocketPool::GroupId group_id1(
      test_server.host_port_pair(), net::ClientSocketPool::SocketType::kHttp,
      net::PrivacyMode::PRIVACY_MODE_ENABLED, kKey1,
      false /* disable_secure_dns */);
  EXPECT_EQ(
      1, GetSocketCountForGroup(network_context.get(), group_id1.ToString()));
  net::ClientSocketPool::GroupId group_id2(
      test_server.host_port_pair(), net::ClientSocketPool::SocketType::kHttp,
      net::PrivacyMode::PRIVACY_MODE_ENABLED, kKey2,
      false /* disable_secure_dns */);
  EXPECT_EQ(
      2, GetSocketCountForGroup(network_context.get(), group_id2.ToString()));
}

// This tests both ClostAllConnetions and CloseIdleConnections.
TEST_F(NetworkContextTest, CloseConnections) {
  // Have to close all connections first, as CloseIdleConnections leaves around
  // a connection at the end of the test.
  for (bool close_all_connections : {true, false}) {
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateContextParams());

    // Use different paths to avoid running into the cache lock.
    const char kPath1[] = "/foo";
    const char kPath2[] = "/bar";
    const char kPath3[] = "/baz";
    net::EmbeddedTestServer test_server;
    net::test_server::ControllableHttpResponse controllable_response1(
        &test_server, kPath1);
    net::test_server::ControllableHttpResponse controllable_response2(
        &test_server, kPath2);
    net::test_server::ControllableHttpResponse controllable_response3(
        &test_server, kPath3);
    ASSERT_TRUE(test_server.Start());

    // Start three network requests. Requests have to all be started before any
    // one of them receives a response to be sure none of them tries to reuse
    // the socket created by another one.

    net::TestDelegate delegate1;
    base::RunLoop run_loop1;
    delegate1.set_on_complete(run_loop1.QuitClosure());
    std::unique_ptr<net::URLRequest> request1 =
        network_context->url_request_context()->CreateRequest(
            test_server.GetURL(kPath1), net::DEFAULT_PRIORITY, &delegate1,
            TRAFFIC_ANNOTATION_FOR_TESTS);
    request1->Start();
    controllable_response1.WaitForRequest();
    EXPECT_EQ(
        1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    net::TestDelegate delegate2;
    base::RunLoop run_loop2;
    delegate2.set_on_complete(run_loop2.QuitClosure());
    std::unique_ptr<net::URLRequest> request2 =
        network_context->url_request_context()->CreateRequest(
            test_server.GetURL(kPath2), net::DEFAULT_PRIORITY, &delegate2,
            TRAFFIC_ANNOTATION_FOR_TESTS);
    request2->Start();
    controllable_response2.WaitForRequest();
    EXPECT_EQ(
        2, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    net::TestDelegate delegate3;
    base::RunLoop run_loop3;
    delegate3.set_on_complete(run_loop3.QuitClosure());
    std::unique_ptr<net::URLRequest> request3 =
        network_context->url_request_context()->CreateRequest(
            test_server.GetURL(kPath3), net::DEFAULT_PRIORITY, &delegate3,
            TRAFFIC_ANNOTATION_FOR_TESTS);
    request3->Start();
    controllable_response3.WaitForRequest();
    EXPECT_EQ(
        3, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    // Complete the first two requests successfully, with a keep-alive response.
    // The EmbeddedTestServer doesn't actually support connection reuse, but
    // this will send a raw response that will make the network stack think it
    // does, and will cause the connection not to be closed.
    controllable_response1.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n\r\n");
    controllable_response2.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n\r\n");
    run_loop1.Run();
    run_loop2.Run();
    // There should now be 2 idle and one handed out socket.
    EXPECT_EQ(2, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
    EXPECT_EQ(
        1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    // Closing all or idle connections should result in closing the idle
    // sockets, but the handed out socket can't be closed.
    base::RunLoop run_loop;
    if (close_all_connections) {
      network_context->CloseAllConnections(run_loop.QuitClosure());
    } else {
      network_context->CloseIdleConnections(run_loop.QuitClosure());
    }
    run_loop.Run();
    EXPECT_EQ(0, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
    EXPECT_EQ(
        1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    // The final request completes. In the close all connections case, its
    // socket should be closed as soon as it is returned to the pool, but in the
    // CloseIdleConnections case, it is added to the pool as an idle socket.
    controllable_response3.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n\r\n");
    run_loop3.Run();
    EXPECT_EQ(close_all_connections ? 0 : 1,
              GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
    EXPECT_EQ(
        0, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));
  }
}

// Test that only trusted URLLoaderFactories accept
// ResourceRequest::trusted_params.
TEST_F(NetworkContextTest, TrustedParams) {
  for (bool trusted_factory : {false, true}) {
    ConnectionListener connection_listener;
    net::EmbeddedTestServer test_server;
    test_server.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    test_server.SetConnectionListener(&connection_listener);
    ASSERT_TRUE(test_server.Start());

    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateContextParams());

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    // URLLoaderFactories should not be trusted by default.
    EXPECT_FALSE(params->is_trusted);
    params->is_trusted = trusted_factory;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    ResourceRequest request;
    request.url = test_server.GetURL("/echo");
    request.trusted_params = ResourceRequest::TrustedParams();
    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    // If the factory was trusted, the request should have succeeded. Otherwise,
    // it should have failed.
    EXPECT_EQ(trusted_factory, client.has_received_response());

    if (trusted_factory) {
      EXPECT_THAT(client.completion_status().error_code, net::test::IsOk());
      EXPECT_EQ(1, connection_listener.GetTotalSocketsSeen());
    } else {
      EXPECT_THAT(client.completion_status().error_code,
                  net::test::IsError(net::ERR_INVALID_ARGUMENT));
      // No connection should have been made to the test server.
      EXPECT_EQ(0, connection_listener.GetTotalSocketsSeen());
    }
  }
}

// Test that the disable_secure_dns trusted param is passed through to the
// host resolver.
TEST_F(NetworkContextTest, TrustedParams_DisableSecureDns) {
  std::unique_ptr<net::MockHostResolver> resolver =
      std::make_unique<net::MockHostResolver>();
  std::unique_ptr<net::TestURLRequestContext> url_request_context =
      std::make_unique<net::TestURLRequestContext>(
          true /* delay_initialization */);
  url_request_context->set_host_resolver(resolver.get());
  url_request_context->Init();

  network_context_remote_.reset();
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(
          network_service_.get(),
          network_context_remote_.BindNewPipeAndPassReceiver(),
          url_request_context.get(),
          /*cors_exempt_header_list=*/std::vector<std::string>());

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  params->is_trusted = true;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  for (bool disable_secure_dns : {false, true}) {
    ResourceRequest request;
    request.url = GURL("http://example.test");
    request.load_flags = net::LOAD_BYPASS_CACHE;
    request.trusted_params = ResourceRequest::TrustedParams();
    request.trusted_params->disable_secure_dns = disable_secure_dns;
    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_EQ(disable_secure_dns,
              resolver->last_secure_dns_mode_override().has_value());
    if (disable_secure_dns) {
      EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF,
                resolver->last_secure_dns_mode_override().value());
    }
  }
}

#if BUILDFLAG(IS_CT_SUPPORTED)
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

  EXPECT_TRUE(base::Contains(requested_urls, kReportURL));
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

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
        net::NetLogCaptureMode::kDefault,
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

  TestProxyErrorClient() = default;

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

  // Creates an mojo::PendingRemote, binds it to |*this| and returns it.
  mojo::PendingRemote<mojom::ProxyErrorClient> CreateRemote() {
    mojo::PendingRemote<mojom::ProxyErrorClient> client_remote =
        receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestProxyErrorClient::OnMojoPipeError, base::Unretained(this)));
    return client_remote;
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

  mojo::Receiver<mojom::ProxyErrorClient> receiver_{this};

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
  context_params->proxy_error_client = proxy_error_client.CreateRemote();
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

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(loader_params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
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
  context_params->proxy_error_client = proxy_error_client.CreateRemote();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request to an unreachable URL.
  ResourceRequest request;
  request.url = GURL("http://server.bad.dns/fail");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(loader_params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
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
// DIRECT "proxy". It additionally emits a script error on line 42 for every
// call to GetProxyForUrl().
class MockMojoProxyResolver : public proxy_resolver::mojom::ProxyResolver {
 public:
  MockMojoProxyResolver() {}

 private:
  // Overridden from proxy_resolver::mojom::ProxyResolver:
  void GetProxyForUrl(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverRequestClient>
          pending_client) override {
    // Report a Javascript error and then complete the request successfully,
    // having chosen DIRECT connections.
    mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient> client(
        std::move(pending_client));
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
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojoProxyResolverFactory>(),
        remote.InitWithNewPipeAndPassReceiver());
    return std::move(remote);
  }

 private:
  void CreateResolver(
      const std::string& pac_url,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
          pending_client) override {
    // Bind |receiver| to a new MockMojoProxyResolver, and return success.
    mojo::MakeSelfOwnedReceiver(std::make_unique<MockMojoProxyResolver>(),
                                std::move(receiver));

    mojo::Remote<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
        client(std::move(pending_client));
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
  context_params->proxy_error_client = proxy_error_client.CreateRemote();

#if defined(OS_CHROMEOS)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // defined(OS_CHROMEOS)

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

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(loader_params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
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
      "http=" + test_server.host_port_pair().ToString());
  proxy_config_set[0].url = GURL("http://does.not.matter/echo");
  proxy_config_set[0].expected_proxy_config_scheme =
      net::ProxyServer::SCHEME_HTTP;

  proxy_config_set[1].proxy_config.proxy_rules().ParseFromString(
      "http=direct://");
  proxy_config_set[1]
      .proxy_config.proxy_rules()
      .bypass_rules.AddRulesToSubtractImplicit();
  proxy_config_set[1].url = test_server.GetURL("/echo");
  proxy_config_set[1].expected_proxy_config_scheme =
      net::ProxyServer::SCHEME_DIRECT;

  for (const auto& proxy_data : proxy_config_set) {
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_data.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    mojo::Remote<mojom::ProxyConfigClient> config_client;
    context_params->proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = 0;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    ResourceRequest request;
    request.url = proxy_data.url;

    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(client.response_head()->proxy_server.scheme(),
              proxy_data.expected_proxy_config_scheme);
  }
}

class TestURLLoaderHeaderClient : public mojom::TrustedURLLoaderHeaderClient {
 public:
  class TestHeaderClient : public mojom::TrustedHeaderClient {
   public:
    TestHeaderClient() {}

    // network::mojom::TrustedHeaderClient:
    void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                             OnBeforeSendHeadersCallback callback) override {
      auto new_headers = headers;
      new_headers.SetHeader("foo", "bar");
      std::move(callback).Run(on_before_send_headers_result_, new_headers);
    }

    void OnHeadersReceived(const std::string& headers,
                           const net::IPEndPoint& endpoint,
                           OnHeadersReceivedCallback callback) override {
      auto new_headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(headers);
      new_headers->AddHeader("baz: qux");
      std::move(callback).Run(on_headers_received_result_,
                              new_headers->raw_headers(), GURL());
    }

    void set_on_before_send_headers_result(int result) {
      on_before_send_headers_result_ = result;
    }

    void set_on_headers_received_result(int result) {
      on_headers_received_result_ = result;
    }

    void Bind(
        mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
      receiver_.reset();
      receiver_.Bind(std::move(receiver));
    }

   private:
    int on_before_send_headers_result_ = net::OK;
    int on_headers_received_result_ = net::OK;
    mojo::Receiver<mojom::TrustedHeaderClient> receiver_{this};

    DISALLOW_COPY_AND_ASSIGN(TestHeaderClient);
  };

  explicit TestURLLoaderHeaderClient(
      mojo::PendingReceiver<mojom::TrustedURLLoaderHeaderClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    header_client_.Bind(std::move(receiver));
  }
  void OnLoaderForCorsPreflightCreated(
      const ResourceRequest& request,
      mojo::PendingReceiver<mojom::TrustedHeaderClient> receiver) override {
    header_client_.Bind(std::move(receiver));
  }

  void set_on_before_send_headers_result(int result) {
    header_client_.set_on_before_send_headers_result(result);
  }

  void set_on_headers_received_result(int result) {
    header_client_.set_on_headers_received_result(result);
  }

 private:
  TestHeaderClient header_client_;
  mojo::Receiver<mojom::TrustedURLLoaderHeaderClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderHeaderClient);
};

TEST_F(NetworkContextTest, HeaderClientModifiesHeaders) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  TestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  // First, do a request with kURLLoadOptionUseHeaderClient set.
  {
    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    // Make sure request header was modified. The value will be in the body
    // since we used the /echoheader endpoint.
    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client.response_body_release(), &response));
    EXPECT_EQ(response, "bar");

    // Make sure response header was modified.
    EXPECT_TRUE(client.response_head()->headers->HasHeaderValue("baz", "qux"));
  }

  // Next, do a request without kURLLoadOptionUseHeaderClient set, headers
  // should not be modified.
  {
    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    // Make sure request header was not set.
    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client.response_body_release(), &response));
    EXPECT_EQ(response, "None");

    // Make sure response header was not set.
    EXPECT_FALSE(client.response_head()->headers->HasHeaderValue("foo", "bar"));
  }
}

TEST_F(NetworkContextTest, HeaderClientFailsRequest) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/echo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  TestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  // First, fail request on OnBeforeSendHeaders.
  {
    header_client.set_on_before_send_headers_result(net::ERR_FAILED);
    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
  }

  // Next, fail request on OnHeadersReceived.
  {
    header_client.set_on_before_send_headers_result(net::OK);
    header_client.set_on_headers_received_result(net::ERR_FAILED);
    mojom::URLLoaderPtr loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
  }
}

class HangingTestURLLoaderHeaderClient
    : public mojom::TrustedURLLoaderHeaderClient {
 public:
  class TestHeaderClient : public mojom::TrustedHeaderClient {
   public:
    TestHeaderClient() {}

    // network::mojom::TrustedHeaderClient:
    void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                             OnBeforeSendHeadersCallback callback) override {
      saved_request_headers_ = headers;
      saved_on_before_send_headers_callback_ = std::move(callback);
      on_before_send_headers_loop_.Quit();
    }

    void OnHeadersReceived(const std::string& headers,
                           const net::IPEndPoint& endpoint,
                           OnHeadersReceivedCallback callback) override {
      saved_received_headers_ = headers;
      saved_on_headers_received_callback_ = std::move(callback);
      on_headers_received_loop_.Quit();
    }

    void CallOnBeforeSendHeadersCallback() {
      net::HttpRequestHeaders new_headers = std::move(saved_request_headers_);
      new_headers.SetHeader("foo", "bar");
      std::move(saved_on_before_send_headers_callback_)
          .Run(net::OK, new_headers);
    }

    void WaitForOnBeforeSendHeaders() { on_before_send_headers_loop_.Run(); }

    void CallOnHeadersReceivedCallback() {
      auto new_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          saved_received_headers_);
      new_headers->AddHeader("baz: qux");
      std::move(saved_on_headers_received_callback_)
          .Run(net::OK, new_headers->raw_headers(), GURL());
    }

    void WaitForOnHeadersReceived() { on_headers_received_loop_.Run(); }

    void Bind(
        mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
      receiver_.Bind(std::move(receiver));
    }

   private:
    base::RunLoop on_before_send_headers_loop_;
    net::HttpRequestHeaders saved_request_headers_;
    OnBeforeSendHeadersCallback saved_on_before_send_headers_callback_;

    base::RunLoop on_headers_received_loop_;
    std::string saved_received_headers_;
    OnHeadersReceivedCallback saved_on_headers_received_callback_;
    mojo::Receiver<mojom::TrustedHeaderClient> receiver_{this};

    DISALLOW_COPY_AND_ASSIGN(TestHeaderClient);
  };

  explicit HangingTestURLLoaderHeaderClient(
      mojo::PendingReceiver<mojom::TrustedURLLoaderHeaderClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    header_client_.Bind(std::move(receiver));
  }
  void OnLoaderForCorsPreflightCreated(
      const ResourceRequest& request,
      mojo::PendingReceiver<mojom::TrustedHeaderClient> receiver) override {
    header_client_.Bind(std::move(receiver));
  }

  void CallOnBeforeSendHeadersCallback() {
    header_client_.CallOnBeforeSendHeadersCallback();
  }

  void WaitForOnBeforeSendHeaders() {
    header_client_.WaitForOnBeforeSendHeaders();
  }

  void CallOnHeadersReceivedCallback() {
    header_client_.CallOnHeadersReceivedCallback();
  }

  void WaitForOnHeadersReceived() { header_client_.WaitForOnHeadersReceived(); }

 private:
  TestHeaderClient header_client_;
  mojo::Receiver<mojom::TrustedURLLoaderHeaderClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(HangingTestURLLoaderHeaderClient);
};

// Test waiting on the OnHeadersReceived event, then proceeding to call the
// OnHeadersReceivedCallback asynchronously. This mostly just verifies that
// HangingTestURLLoaderHeaderClient works.
TEST_F(NetworkContextTest, HangingHeaderClientModifiesHeadersAsynchronously) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  HangingTestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  header_client.WaitForOnBeforeSendHeaders();
  header_client.CallOnBeforeSendHeadersCallback();

  header_client.WaitForOnHeadersReceived();
  header_client.CallOnHeadersReceivedCallback();

  client.RunUntilComplete();

  EXPECT_EQ(client.completion_status().error_code, net::OK);
  // Make sure request header was modified. The value will be in the body
  // since we used the /echoheader endpoint.
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client.response_body_release(), &response));
  EXPECT_EQ(response, "bar");

  // Make sure response header was modified.
  EXPECT_TRUE(client.response_head()->headers->HasHeaderValue("baz", "qux"));
}

// Test destroying the mojom::URLLoader after the OnBeforeSendHeaders event and
// then calling the OnBeforeSendHeadersCallback.
TEST_F(NetworkContextTest, HangingHeaderClientAbortDuringOnBeforeSendHeaders) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  HangingTestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  header_client.WaitForOnBeforeSendHeaders();

  loader.reset();

  // Ensure the loader is destroyed before the callback is run.
  base::RunLoop().RunUntilIdle();

  header_client.CallOnBeforeSendHeadersCallback();

  client.RunUntilComplete();

  // The reported error differs, but eventually URLLoader returns
  // net::ERR_ABORTED once OOR-CORS clean-up is finished.
  if (features::ShouldEnableOutOfBlinkCorsForTesting())
    EXPECT_EQ(client.completion_status().error_code, net::ERR_ABORTED);
  else
    EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
}

// Test destroying the mojom::URLLoader after the OnHeadersReceived event and
// then calling the OnHeadersReceivedCallback.
TEST_F(NetworkContextTest, HangingHeaderClientAbortDuringOnHeadersReceived) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  HangingTestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  header_client.WaitForOnBeforeSendHeaders();
  header_client.CallOnBeforeSendHeadersCallback();

  header_client.WaitForOnHeadersReceived();

  loader.reset();

  // Ensure the loader is destroyed before the callback is run.
  base::RunLoop().RunUntilIdle();

  header_client.CallOnHeadersReceivedCallback();

  client.RunUntilComplete();

  // The reported error differs, but eventually URLLoader returns
  // net::ERR_ABORTED once OOR-CORS clean-up is finished.
  if (features::ShouldEnableOutOfBlinkCorsForTesting())
    EXPECT_EQ(client.completion_status().error_code, net::ERR_ABORTED);
  else
    EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
}

// Custom proxy does not apply to localhost, so resolve kMockHost to localhost,
// and use that instead.
class NetworkContextMockHostTest : public NetworkContextTest {
 public:
  NetworkContextMockHostTest() {
    scoped_refptr<net::RuleBasedHostResolverProc> rules =
        net::CreateCatchAllHostResolverProc();
    rules->AddRule(kMockHost, "127.0.0.1");

    network_service_->set_host_resolver_factory_for_testing(
        std::make_unique<net::MockHostResolverFactory>(std::move(rules)));
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

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  config->pre_cache_headers.SetHeader("pre_foo", "pre_foo_value");
  config->post_cache_headers.SetHeader("post_foo", "post_foo_value");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("pre_bar", "pre_bar_value");
  request.custom_proxy_post_cache_headers.SetHeader("post_bar",
                                                    "post_bar_value");
  request.url = GetURLWithMockHost(
      test_server, "/echoheader?pre_foo&post_foo&pre_bar&post_bar");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"post_bar_value", "post_foo_value",
                                        "pre_bar_value", "pre_foo_value"},
                                       "\n"));
  EXPECT_EQ(client->response_head()->proxy_server, proxy_server);
}

// Tests that if using a custom proxy results in redirect loop, then
// the proxy is bypassed, and the request is fetched directly.
TEST_F(NetworkContextMockHostTest, CanUseProxyOnHttpSelfRedirect) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  const GURL kUrl = GetURLWithMockHost(test_server, "/echo");

  net::EmbeddedTestServer proxy_test_server;
  // |redirect_cycle| has length 1 implying that fetching kUrl will result in
  // redirect to kUrl.
  const std::vector<GURL> kRedirectCycle({kUrl});

  proxy_test_server.RegisterRequestHandler(
      base::BindRepeating(&RedirectThroughCycleProxyResponse, kRedirectCycle));

  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  // Set |can_use_proxy_on_http_url_redirect_cycles| to false.
  // This allows proxy delegate to bypass custom proxies if there
  // is a redirect loop.
  config->can_use_proxy_on_http_url_redirect_cycles = false;
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = kUrl;
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client = FetchRedirectedRequest(
      kRedirectCycle.size(), request, network_context.get());
  task_environment_.RunUntilIdle();
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ("Echo", response);
}

// Tests that if using a custom proxy results in a long redirect loop, then
// the proxy is bypassed, and the request is fetched directly.
TEST_F(NetworkContextMockHostTest, CanUseProxyOnHttpRedirectCycles) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  const GURL kUrl1 = GetURLWithMockHost(test_server, "/echo");
  const GURL kUrl2 = GetURLWithMockHost(test_server, "/2/echo");
  const GURL kUrl3 = GetURLWithMockHost(test_server, "/3/echo");

  // Create a redirect cycle of length 3. Note that fetching kUrl3 will cause
  // redirect back to kUrl1.
  const std::vector<GURL> kRedirectCycle({kUrl1, kUrl2, kUrl3});

  net::EmbeddedTestServer proxy_test_server;

  proxy_test_server.RegisterRequestHandler(
      base::BindRepeating(&RedirectThroughCycleProxyResponse, kRedirectCycle));

  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  // Set |can_use_proxy_on_http_url_redirect_cycles| to false.
  // This allows proxy delegate to bypass custom proxies if there
  // is a redirect loop.
  config->can_use_proxy_on_http_url_redirect_cycles = false;
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = kUrl1;
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client = FetchRedirectedRequest(
      kRedirectCycle.size(), request, network_context.get());
  task_environment_.RunUntilIdle();
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ("Echo", response);
}

TEST_F(NetworkContextMockHostTest, CustomProxyHeadersAreMerged) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  config->pre_cache_headers.SetHeader("foo", "first_foo_key=value1");
  config->post_cache_headers.SetHeader("bar", "first_bar_key=value2");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("foo",
                                                   "foo_next_key=value3");
  request.custom_proxy_post_cache_headers.SetHeader("bar",
                                                    "bar_next_key=value4");
  request.url = GetURLWithMockHost(test_server, "/echoheader?foo&bar");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response,
            base::JoinString({"first_bar_key=value2, bar_next_key=value4",
                              "first_foo_key=value1, foo_next_key=value3"},
                             "\n"));
  EXPECT_EQ(client->response_head()->proxy_server, proxy_server);
}

TEST_F(NetworkContextMockHostTest, CustomProxyConfigHeadersAddedBeforeCache) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  config->pre_cache_headers.SetHeader("foo", "foo_value");
  config->post_cache_headers.SetHeader("bar", "bar_value");
  proxy_config_client->OnCustomProxyConfigUpdated(config->Clone());
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = GetURLWithMockHost(test_server, "/echoheadercache?foo&bar");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_EQ(client->response_head()->proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head()->was_fetched_via_cache);

  // post_cache_headers should not break caching.
  config->post_cache_headers.SetHeader("bar", "new_bar");
  proxy_config_client->OnCustomProxyConfigUpdated(config->Clone());
  task_environment_.RunUntilIdle();

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_TRUE(client->response_head()->was_fetched_via_cache);

  // pre_cache_headers should invalidate cache.
  config->pre_cache_headers.SetHeader("foo", "new_foo");
  proxy_config_client->OnCustomProxyConfigUpdated(config->Clone());
  task_environment_.RunUntilIdle();

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"new_bar", "new_foo"}, "\n"));
  EXPECT_EQ(client->response_head()->proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head()->was_fetched_via_cache);
}

TEST_F(NetworkContextMockHostTest, CustomProxyRequestHeadersAddedBeforeCache) {
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
  config->rules.ParseFromString("http=" + proxy_server.ToURI());
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = GetURLWithMockHost(test_server, "/echoheadercache?foo&bar");
  request.custom_proxy_pre_cache_headers.SetHeader("foo", "foo_value");
  request.custom_proxy_post_cache_headers.SetHeader("bar", "bar_value");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_EQ(client->response_head()->proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head()->was_fetched_via_cache);

  // custom_proxy_post_cache_headers should not break caching.
  request.custom_proxy_post_cache_headers.SetHeader("bar", "new_bar");

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"bar_value", "foo_value"}, "\n"));
  EXPECT_TRUE(client->response_head()->was_fetched_via_cache);

  // custom_proxy_pre_cache_headers should invalidate cache.
  request.custom_proxy_pre_cache_headers.SetHeader("foo", "new_foo");

  client = FetchRequest(request, network_context.get());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"new_bar", "new_foo"}, "\n"));
  EXPECT_EQ(client->response_head()->proxy_server, proxy_server);
  EXPECT_FALSE(client->response_head()->was_fetched_via_cache);
}

TEST_F(NetworkContextMockHostTest,
       CustomProxyDoesNotAddHeadersWhenNoProxyUsed) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->pre_cache_headers.SetHeader("pre_foo", "bad");
  config->post_cache_headers.SetHeader("post_foo", "bad");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("pre_bar", "bad");
  request.custom_proxy_post_cache_headers.SetHeader("post_bar", "bad");
  request.url = GetURLWithMockHost(
      test_server, "/echoheader?pre_foo&post_foo&pre_bar&post_bar");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  ASSERT_TRUE(client->response_body());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"None", "None", "None", "None"}, "\n"));
  EXPECT_TRUE(client->response_head()->proxy_server.is_direct());
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

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->pre_cache_headers.SetHeader("pre_foo", "bad");
  config->post_cache_headers.SetHeader("post_foo", "bad");
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.custom_proxy_pre_cache_headers.SetHeader("pre_bar", "bad");
  request.custom_proxy_post_cache_headers.SetHeader("post_bar", "bad");
  request.url = GetURLWithMockHost(
      test_server, "/echoheader?pre_foo&post_foo&pre_bar&post_bar");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  EXPECT_EQ(response, base::JoinString({"None", "None", "None", "None"}, "\n"));
  EXPECT_EQ(client->response_head()->proxy_server,
            ConvertToProxyServer(proxy_test_server));
}

TEST_F(NetworkContextMockHostTest, CustomProxyUsesSpecifiedProxyList) {
  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString(
      "http=" + ConvertToProxyServer(proxy_test_server).ToURI());
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
  task_environment_.RunUntilIdle();

  ResourceRequest request;
  request.url = GURL("http://does.not.resolve/echo");
  request.render_frame_id = kRouteId;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  // |invalid_server| has no handlers set up so would return an empty response.
  EXPECT_EQ(response, "Echo");
  EXPECT_EQ(client->response_head()->proxy_server,
            ConvertToProxyServer(proxy_test_server));
}

// Verifies that custom proxy is used only for requests with process id and
// render frame id.
// TODO(https://crbug.com/991035): Crash flakes due to NetworkService'
// UploadLoadInfo() timer firing during FetchRequest().
TEST_F(NetworkContextMockHostTest,
       DISABLED_UseCustomProxyForNavigationAndRenderFrameRequest) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  net::EmbeddedTestServer proxy_test_server;
  proxy_test_server.RegisterRequestHandler(
      base::BindRepeating(&CustomProxyResponse));
  ASSERT_TRUE(proxy_test_server.Start());

  struct TestCase {
    int process_id;
    int render_frame_id;
    bool expected_custom_proxy_used;
  };
  const TestCase test_cases[] = {
      // When process id and renderer id are invalid, custom proxy is not used.
      {0, MSG_ROUTING_NONE, false},

      {kProcessId, kRouteId, true},
      {0, kRouteId, true},
      {kProcessId, MSG_ROUTING_NONE, true},

      // render_frame_id = MSG_ROUTING_CONTROL provides a temporary way to use
      // the custom proxy for specific requests.
      {0, MSG_ROUTING_CONTROL, true},
  };

  for (const TestCase& test_case : test_cases) {
    mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->custom_proxy_config_client_receiver =
        proxy_config_client.BindNewPipeAndPassReceiver();
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));
    auto config = mojom::CustomProxyConfig::New();
    net::ProxyServer proxy_server = ConvertToProxyServer(proxy_test_server);
    config->rules.ParseFromString("http=" + proxy_server.ToURI());
    // Set |can_use_proxy_on_http_url_redirect_cycles| to false.
    // This allows proxy delegate to bypass custom proxies if disable cache load
    // flag is set.
    config->can_use_proxy_on_http_url_redirect_cycles = false;
    proxy_config_client->OnCustomProxyConfigUpdated(std::move(config));
    task_environment_.RunUntilIdle();

    ResourceRequest request;
    request.url = GetURLWithMockHost(test_server, "/echo");
    request.render_frame_id = test_case.render_frame_id;
    std::unique_ptr<TestURLLoaderClient> client =
        FetchRequest(request, network_context.get(), mojom::kURLLoadOptionNone,
                     test_case.process_id);
    task_environment_.RunUntilIdle();
    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client->response_body_release(), &response));

    if (test_case.expected_custom_proxy_used)
      EXPECT_EQ(kCustomProxyResponse, response);
    else
      EXPECT_EQ("Echo", response);
  }
}

TEST_F(NetworkContextTest, MaximumCount) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));

  const char kPath1[] = "/foobar";
  const char kPath2[] = "/hung";
  const char kPath3[] = "/hello.html";
  net::test_server::ControllableHttpResponse controllable_response1(
      &test_server, kPath1);

  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->set_max_loaders_per_process_for_testing(2);

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  ResourceRequest request;
  request.url = test_server.GetURL(kPath1);
  auto client1 = std::make_unique<TestURLLoaderClient>();
  mojom::URLLoaderPtr loader1;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader1), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client1->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  request.url = test_server.GetURL(kPath2);
  auto client2 = std::make_unique<TestURLLoaderClient>();
  mojom::URLLoaderPtr loader2;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader2), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client2->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // A third request should fail, since the first two are outstanding and the
  // limit is 2.
  request.url = test_server.GetURL(kPath3);
  auto client3 = std::make_unique<TestURLLoaderClient>();
  mojom::URLLoaderPtr loader3;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader3), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client3->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client3->RunUntilComplete();
  ASSERT_EQ(client3->completion_status().error_code,
            net::ERR_INSUFFICIENT_RESOURCES);

  // Complete the first request and try the third again.
  controllable_response1.WaitForRequest();
  controllable_response1.Send("HTTP/1.1 200 OK\r\n");
  controllable_response1.Done();

  client1->RunUntilComplete();
  ASSERT_EQ(client1->completion_status().error_code, net::OK);

  client3 = std::make_unique<TestURLLoaderClient>();
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader3), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client3->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client3->RunUntilComplete();
  ASSERT_EQ(client3->completion_status().error_code, net::OK);
}

TEST_F(NetworkContextTest, AllowAllCookies) {
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL server_url = test_server.GetURL("/echoheader?Cookie");
  GURL first_party_url(server_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  EXPECT_TRUE(
      SetCookieHelper(network_context.get(), server_url, "TestCookie", "1"));

  int url_loader_options = mojom::kURLLoadOptionNone;

  ResourceRequest first_party_request;
  first_party_request.url = server_url;
  first_party_request.site_for_cookies = first_party_url;

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      first_party_request, network_context.get(), url_loader_options);

  std::string response_body;
  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("TestCookie=1", response_body);

  ResourceRequest third_party_request;
  third_party_request.url = server_url;
  third_party_request.site_for_cookies = third_party_url;

  client = FetchRequest(third_party_request, network_context.get(),
                        url_loader_options);

  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("TestCookie=1", response_body);
}

TEST_F(NetworkContextTest, BlockThirdPartyCookies) {
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL server_url = test_server.GetURL("/echoheader?Cookie");
  GURL first_party_url(server_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  EXPECT_TRUE(
      SetCookieHelper(network_context.get(), server_url, "TestCookie", "1"));

  int url_loader_options = mojom::kURLLoadOptionBlockThirdPartyCookies;

  ResourceRequest first_party_request;
  first_party_request.url = server_url;
  first_party_request.site_for_cookies = first_party_url;

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      first_party_request, network_context.get(), url_loader_options);

  std::string response_body;
  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("TestCookie=1", response_body);

  ResourceRequest third_party_request;
  third_party_request.url = server_url;
  third_party_request.site_for_cookies = third_party_url;

  client = FetchRequest(third_party_request, network_context.get(),
                        url_loader_options);

  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("None", response_body);
}

TEST_F(NetworkContextTest, BlockAllCookies) {
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL server_url = test_server.GetURL("/echoheader?Cookie");
  GURL first_party_url(server_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  EXPECT_TRUE(
      SetCookieHelper(network_context.get(), server_url, "TestCookie", "1"));

  int url_loader_options = mojom::kURLLoadOptionBlockAllCookies;

  ResourceRequest first_party_request;
  first_party_request.url = server_url;
  first_party_request.site_for_cookies = first_party_url;

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      first_party_request, network_context.get(), url_loader_options);

  std::string response_body;
  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("None", response_body);

  ResourceRequest third_party_request;
  third_party_request.url = server_url;
  third_party_request.site_for_cookies = third_party_url;

  client = FetchRequest(third_party_request, network_context.get(),
                        url_loader_options);

  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("None", response_body);
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST_F(NetworkContextTest, AddFtpAuthCacheEntry) {
  GURL url("ftp://example.test/");
  const char kUsername[] = "test_user";
  const char kPassword[] = "test_pass";
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->enable_ftp_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = url::Origin::Create(url);

  ASSERT_TRUE(network_context->url_request_context()->ftp_auth_cache());
  ASSERT_FALSE(
      network_context->url_request_context()->ftp_auth_cache()->Lookup(url));
  base::RunLoop run_loop;
  network_context->AddAuthCacheEntry(
      challenge, net::NetworkIsolationKey(),
      net::AuthCredentials(base::ASCIIToUTF16(kUsername),
                           base::ASCIIToUTF16(kPassword)),
      run_loop.QuitClosure());
  run_loop.Run();
  net::FtpAuthCache::Entry* entry =
      network_context->url_request_context()->ftp_auth_cache()->Lookup(url);
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->origin);
  EXPECT_EQ(base::ASCIIToUTF16(kUsername), entry->credentials.username());
  EXPECT_EQ(base::ASCIIToUTF16(kPassword), entry->credentials.password());
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(IS_CT_SUPPORTED)
TEST_F(NetworkContextTest, CertificateTransparencyConfig) {
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->enforce_chrome_ct_policy = true;
  params->ct_log_update_time = base::Time::Now();

  // The log public keys do not matter for the test, so invalid keys are used.
  // However, because the log IDs are derived from the SHA-256 hash of the log
  // key, the log keys are generated such that qualified logs are in the form
  // of four digits (e.g. "0000", "1111"), while disqualified logs are in the
  // form of four letters (e.g. "AAAA", "BBBB").

  for (int i = 0; i < 6; ++i) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    // Shift to ASCII '0' (0x30)
    log_info->public_key = std::string(4, 0x30 + static_cast<char>(i));
    log_info->name = std::string(4, 0x30 + static_cast<char>(i));
    log_info->operated_by_google = i % 2;

    params->ct_logs.push_back(std::move(log_info));
  }
  for (int i = 0; i < 3; ++i) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    // Shift to ASCII 'A' (0x41)
    log_info->public_key = std::string(4, 0x41 + static_cast<char>(i));
    log_info->name = std::string(4, 0x41 + static_cast<char>(i));
    log_info->operated_by_google = false;
    log_info->disqualified_at = base::TimeDelta::FromSeconds(i);

    params->ct_logs.push_back(std::move(log_info));
  }
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));

  net::CTPolicyEnforcer* request_enforcer =
      network_context->url_request_context()->ct_policy_enforcer();
  ASSERT_TRUE(request_enforcer);

  // Completely unsafe if |enforce_chrome_ct_policy| is false.
  certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
      reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
          request_enforcer);

  EXPECT_TRUE(std::is_sorted(
      policy_enforcer->operated_by_google_logs_for_testing().begin(),
      policy_enforcer->operated_by_google_logs_for_testing().end()));
  EXPECT_TRUE(
      std::is_sorted(policy_enforcer->disqualified_logs_for_testing().begin(),
                     policy_enforcer->disqualified_logs_for_testing().end()));

  EXPECT_THAT(
      policy_enforcer->operated_by_google_logs_for_testing(),
      ::testing::UnorderedElementsAreArray({crypto::SHA256HashString("1111"),
                                            crypto::SHA256HashString("3333"),
                                            crypto::SHA256HashString("5555")}));
  EXPECT_THAT(policy_enforcer->disqualified_logs_for_testing(),
              ::testing::UnorderedElementsAre(
                  ::testing::Pair(crypto::SHA256HashString("AAAA"),
                                  base::TimeDelta::FromSeconds(0)),
                  ::testing::Pair(crypto::SHA256HashString("BBBB"),
                                  base::TimeDelta::FromSeconds(1)),
                  ::testing::Pair(crypto::SHA256HashString("CCCC"),
                                  base::TimeDelta::FromSeconds(2))));
}
#endif

TEST_F(NetworkContextTest, AddHttpAuthCacheEntry) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();
  ASSERT_TRUE(cache);
  // |key_server_entries_by_network_isolation_key| should be disabled by
  // default, so the passed in NetworkIsolationKeys don't matter.
  EXPECT_FALSE(cache->key_server_entries_by_network_isolation_key());

  // Add an AUTH_SERVER cache entry.
  GURL url("http://example.test/");
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = url::Origin::Create(url);
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char kUsername[] = "test_user";
  const char kPassword[] = "test_pass";
  ASSERT_FALSE(cache->Lookup(url, net::HttpAuth::AUTH_SERVER, challenge.realm,
                             net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkIsolationKey()));
  base::RunLoop run_loop;
  network_context->AddAuthCacheEntry(
      challenge, net::NetworkIsolationKey(),
      net::AuthCredentials(base::ASCIIToUTF16(kUsername),
                           base::ASCIIToUTF16(kPassword)),
      run_loop.QuitClosure());
  run_loop.Run();
  net::HttpAuthCache::Entry* entry = cache->Lookup(
      url, net::HttpAuth::AUTH_SERVER, challenge.realm,
      net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->origin());
  EXPECT_EQ(challenge.realm, entry->realm());
  EXPECT_EQ(net::HttpAuth::StringToScheme(challenge.scheme), entry->scheme());
  EXPECT_EQ(base::ASCIIToUTF16(kUsername), entry->credentials().username());
  EXPECT_EQ(base::ASCIIToUTF16(kPassword), entry->credentials().password());
  // Entry should only have been added for server auth.
  EXPECT_FALSE(cache->Lookup(url, net::HttpAuth::AUTH_PROXY, challenge.realm,
                             net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkIsolationKey()));

  // Add an AUTH_PROXY cache entry.
  GURL proxy_url("http://proxy.test/");
  challenge.is_proxy = true;
  challenge.challenger = url::Origin::Create(proxy_url);
  const char kProxyUsername[] = "test_proxy_user";
  const char kProxyPassword[] = "test_proxy_pass";
  ASSERT_FALSE(cache->Lookup(proxy_url, net::HttpAuth::AUTH_PROXY,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkIsolationKey()));
  base::RunLoop run_loop2;
  network_context->AddAuthCacheEntry(
      challenge, net::NetworkIsolationKey(),
      net::AuthCredentials(base::ASCIIToUTF16(kProxyUsername),
                           base::ASCIIToUTF16(kProxyPassword)),
      run_loop2.QuitClosure());
  run_loop2.Run();
  entry = cache->Lookup(proxy_url, net::HttpAuth::AUTH_PROXY, challenge.realm,
                        net::HttpAuth::AUTH_SCHEME_BASIC,
                        net::NetworkIsolationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(proxy_url, entry->origin());
  EXPECT_EQ(challenge.realm, entry->realm());
  EXPECT_EQ(net::HttpAuth::StringToScheme(challenge.scheme), entry->scheme());
  EXPECT_EQ(base::ASCIIToUTF16(kProxyUsername),
            entry->credentials().username());
  EXPECT_EQ(base::ASCIIToUTF16(kProxyPassword),
            entry->credentials().password());
  // Entry should only have been added for proxy auth.
  EXPECT_FALSE(cache->Lookup(proxy_url, net::HttpAuth::AUTH_SERVER,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkIsolationKey()));
}

TEST_F(NetworkContextTest, AddHttpAuthCacheEntryWithNetworkIsolationKey) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  network_context->SetSplitAuthCacheByNetworkIsolationKey(true);

  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();
  ASSERT_TRUE(cache);
  // If this isn't true, the rest of this test is pretty meaningless.
  ASSERT_TRUE(cache->key_server_entries_by_network_isolation_key());

  // Add an AUTH_SERVER cache entry.
  GURL url("http://example.test/");
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = origin;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char kUsername[] = "test_user";
  const char kPassword[] = "test_pass";
  ASSERT_FALSE(cache->Lookup(url, net::HttpAuth::AUTH_SERVER, challenge.realm,
                             net::HttpAuth::AUTH_SCHEME_BASIC,
                             network_isolation_key));
  base::RunLoop run_loop;
  network_context->AddAuthCacheEntry(
      challenge, network_isolation_key,
      net::AuthCredentials(base::ASCIIToUTF16(kUsername),
                           base::ASCIIToUTF16(kPassword)),
      run_loop.QuitClosure());
  run_loop.Run();
  net::HttpAuthCache::Entry* entry =
      cache->Lookup(url, net::HttpAuth::AUTH_SERVER, challenge.realm,
                    net::HttpAuth::AUTH_SCHEME_BASIC, network_isolation_key);
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->origin());
  EXPECT_EQ(challenge.realm, entry->realm());
  EXPECT_EQ(net::HttpAuth::StringToScheme(challenge.scheme), entry->scheme());
  EXPECT_EQ(base::ASCIIToUTF16(kUsername), entry->credentials().username());
  EXPECT_EQ(base::ASCIIToUTF16(kPassword), entry->credentials().password());
  // Entry should only be accessibly when using the correct NetworkIsolationKey.
  EXPECT_FALSE(cache->Lookup(url, net::HttpAuth::AUTH_SERVER, challenge.realm,
                             net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkIsolationKey()));
}

TEST_F(NetworkContextTest, CopyHttpAuthCacheProxyEntries) {
  std::unique_ptr<NetworkContext> network_context1 =
      CreateContextWithParams(CreateContextParams());

  net::AuthChallengeInfo challenge;
  challenge.is_proxy = true;
  challenge.challenger = kOrigin;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char kProxyUsername[] = "proxy_user";
  const char kProxyPassword[] = "proxy_pass";

  base::RunLoop run_loop1;
  network_context1->AddAuthCacheEntry(
      challenge, net::NetworkIsolationKey(),
      net::AuthCredentials(base::ASCIIToUTF16(kProxyUsername),
                           base::ASCIIToUTF16(kProxyPassword)),
      run_loop1.QuitClosure());
  run_loop1.Run();

  challenge.is_proxy = false;
  const char kServerUsername[] = "server_user";
  const char kServerPassword[] = "server_pass";

  base::RunLoop run_loop2;
  network_context1->AddAuthCacheEntry(
      challenge, net::NetworkIsolationKey(),
      net::AuthCredentials(base::ASCIIToUTF16(kServerUsername),
                           base::ASCIIToUTF16(kServerPassword)),
      run_loop2.QuitClosure());
  run_loop2.Run();

  base::UnguessableToken token;
  base::RunLoop run_loop3;
  network_context1->SaveHttpAuthCacheProxyEntries(base::BindLambdaForTesting(
      [&](const base::UnguessableToken& returned_token) {
        token = returned_token;
        run_loop3.Quit();
      }));
  run_loop3.Run();

  // Delete first NetworkContext, to make sure saved credentials outlast it.
  network_context1.reset();
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(CreateContextParams());

  base::RunLoop run_loop4;
  network_context2->LoadHttpAuthCacheProxyEntries(token,
                                                  run_loop4.QuitClosure());
  run_loop4.Run();

  // Check cached credentials directly, since there's no API to check proxy
  // credentials.
  net::HttpAuthCache* cache = network_context2->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();
  // The server credentials should not have been copied.
  EXPECT_FALSE(cache->Lookup(kURL, net::HttpAuth::AUTH_SERVER, challenge.realm,
                             net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkIsolationKey()));
  net::HttpAuthCache::Entry* entry = cache->Lookup(
      kURL, net::HttpAuth::AUTH_PROXY, challenge.realm,
      net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(base::ASCIIToUTF16(kProxyUsername),
            entry->credentials().username());
  EXPECT_EQ(base::ASCIIToUTF16(kProxyPassword),
            entry->credentials().password());
}

TEST_F(NetworkContextTest, SplitAuthCacheByNetworkIsolationKey) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  EXPECT_FALSE(cache->key_server_entries_by_network_isolation_key());

  // Add proxy credentials, which should never be deleted.
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = true;
  challenge.challenger = kOrigin;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char kProxyUsername[] = "proxy_user";
  const char kProxyPassword[] = "proxy_pass";
  base::RunLoop run_loop1;
  network_context->AddAuthCacheEntry(
      challenge, net::NetworkIsolationKey(),
      net::AuthCredentials(base::ASCIIToUTF16(kProxyUsername),
                           base::ASCIIToUTF16(kProxyPassword)),
      run_loop1.QuitClosure());
  run_loop1.Run();

  // Set up challenge to add server credentials.
  challenge.is_proxy = false;

  for (bool set_split_cache_by_network_isolation_key : {true, false}) {
    // In each loop iteration, the setting should change, which should clear
    // server credentials.
    EXPECT_NE(set_split_cache_by_network_isolation_key,
              cache->key_server_entries_by_network_isolation_key());

    // Add server credentials.
    const char kServerUsername[] = "server_user";
    const char kServerPassword[] = "server_pass";
    base::RunLoop run_loop2;
    network_context->AddAuthCacheEntry(
        challenge, net::NetworkIsolationKey(),
        net::AuthCredentials(base::ASCIIToUTF16(kServerUsername),
                             base::ASCIIToUTF16(kServerPassword)),
        run_loop2.QuitClosure());
    run_loop2.Run();

    // Toggle setting.
    network_context->SetSplitAuthCacheByNetworkIsolationKey(
        set_split_cache_by_network_isolation_key);
    EXPECT_EQ(set_split_cache_by_network_isolation_key,
              cache->key_server_entries_by_network_isolation_key());

    // The server credentials should have been deleted.
    EXPECT_FALSE(cache->Lookup(
        kURL, net::HttpAuth::AUTH_SERVER, challenge.realm,
        net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey()));

    // The proxy credentials should still be in the cache.
    net::HttpAuthCache::Entry* entry = cache->Lookup(
        kURL, net::HttpAuth::AUTH_PROXY, challenge.realm,
        net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey());
    ASSERT_TRUE(entry);
    EXPECT_EQ(base::ASCIIToUTF16(kProxyUsername),
              entry->credentials().username());
    EXPECT_EQ(base::ASCIIToUTF16(kProxyPassword),
              entry->credentials().password());
  }
}

TEST_F(NetworkContextTest, HSTSPolicyBypassList) {
  // The default test preload list includes "example" as a preloaded TLD
  // (including subdomains).
  net::ScopedTransportSecurityStateSource scoped_security_state_source;

  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->hsts_policy_bypass_list.push_back("example");
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  net::TransportSecurityState* transport_security_state =
      network_context->url_request_context()->transport_security_state();
  // With the policy set, example should no longer upgrade to HTTPS.
  EXPECT_FALSE(transport_security_state->ShouldUpgradeToSSL("example"));
  // But the policy shouldn't apply to subdomains.
  EXPECT_TRUE(transport_security_state->ShouldUpgradeToSSL("sub.example"));
}

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
TEST_F(NetworkContextTest, UseCertVerifierBuiltin) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  // This just happens to be the only histogram that directly records which
  // verifier was used.
  const char kBuiltinVerifierHistogram[] =
      "Net.CertVerifier.NameNormalizationPrivateRoots.Builtin";

  for (bool builtin_verifier_enabled : {false, true}) {
    SCOPED_TRACE(builtin_verifier_enabled);

    mojom::NetworkContextParamsPtr params = CreateContextParams();
    params->use_builtin_cert_verifier = builtin_verifier_enabled;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(params));

    ResourceRequest request;
    request.url = test_server.GetURL("/nocontent");
    base::HistogramTester histogram_tester;
    std::unique_ptr<TestURLLoaderClient> client =
        FetchRequest(request, network_context.get());
    EXPECT_EQ(net::OK, client->completion_status().error_code);
    histogram_tester.ExpectTotalCount(kBuiltinVerifierHistogram,
                                      builtin_verifier_enabled ? 1 : 0);
  }
}
#endif

static ResourceRequest CreateResourceRequest(const char* method,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies = url;  // bypass third-party cookie blocking
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator is set
  return request;
}

class NetworkContextSplitCacheTest : public NetworkContextTest {
 protected:
  NetworkContextSplitCacheTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    EXPECT_TRUE(test_server_.Start());

    // Set up a scoped host resolver to access other origins.
    scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc =
        base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);
    mock_resolver_proc->AddRule("*", "127.0.0.1");
    mock_host_resolver_ = std::make_unique<net::ScopedDefaultHostResolverProc>(
        mock_resolver_proc.get());

    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    network_context_ = CreateContextWithParams(std::move(context_params));
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }

  void LoadAndVerifyCached(
      const GURL& url,
      const net::NetworkIsolationKey& key,
      bool was_cached,
      bool is_navigation,
      mojom::UpdateNetworkIsolationKeyOnRedirect
          update_network_isolation_key_on_redirect =
              mojom::UpdateNetworkIsolationKeyOnRedirect::kDoNotUpdate,
      bool expect_redirect = false,
      base::Optional<GURL> new_url = base::nullopt) {
    ResourceRequest request = CreateResourceRequest("GET", url);
    request.load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    auto params = mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    if (is_navigation) {
      request.trusted_params = ResourceRequest::TrustedParams();
      request.trusted_params->network_isolation_key = key;
      request.trusted_params->update_network_isolation_key_on_redirect =
          update_network_isolation_key_on_redirect;
      params->is_trusted = true;
    } else {
      // Different |update_network_isolation_key_on_redirect| values may only be
      // set for navigations.
      DCHECK_EQ(mojom::UpdateNetworkIsolationKeyOnRedirect::kDoNotUpdate,
                update_network_isolation_key_on_redirect);
      params->network_isolation_key = key;
    }
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));
    auto client = std::make_unique<TestURLLoaderClient>();
    mojom::URLLoaderPtr loader;
    loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionNone, request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    if (expect_redirect) {
      client->RunUntilRedirectReceived();
      loader->FollowRedirect({}, {}, new_url);
      client->ClearHasReceivedRedirect();
    }

    if (new_url) {
      client->RunUntilRedirectReceived();
      loader->FollowRedirect({}, {}, base::nullopt);
    }

    client->RunUntilComplete();

    EXPECT_EQ(net::OK, client->completion_status().error_code);
    EXPECT_EQ(was_cached, client->completion_status().exists_in_cache);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_;
  std::unique_ptr<NetworkContext> network_context_;
};

TEST_F(NetworkContextSplitCacheTest, CachedUsingNetworkIsolationKey) {
  GURL url = test_server()->GetURL("/resource");
  url::Origin origin_a = url::Origin::Create(GURL("http://a.test/"));
  net::NetworkIsolationKey key_a(origin_a, origin_a);
  LoadAndVerifyCached(url, key_a, false /* was_cached */,
                      false /* is_navigation */);

  // Load again with a different isolation key. The cached entry should not be
  // loaded.
  url::Origin origin_b = url::Origin::Create(GURL("http://b.test/"));
  net::NetworkIsolationKey key_b(origin_b, origin_b);
  LoadAndVerifyCached(url, key_b, false /* was_cached */,
                      false /* is_navigation */);

  // Load again with the same isolation key. The cached entry should be loaded.
  LoadAndVerifyCached(url, key_b, true /* was_cached */,
                      false /* is_navigation */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceCachedUsingNetworkIsolationKey) {
  GURL url = test_server()->GetURL("othersite.test", "/main.html");
  url::Origin origin_a = url::Origin::Create(url);
  net::NetworkIsolationKey key_a(origin_a, origin_a);
  LoadAndVerifyCached(url, key_a, false /* was_cached */,
                      true /* is_navigation */);

  // Load again with a different isolation key. The cached entry should not be
  // loaded.
  GURL url_b = test_server()->GetURL("/main.html");
  url::Origin origin_b = url::Origin::Create(url_b);
  net::NetworkIsolationKey key_b(origin_b, origin_b);
  LoadAndVerifyCached(url_b, key_b, false /* was_cached */,
                      true /* is_navigation */);

  // Load again with the same isolation key. The cached entry should be loaded.
  LoadAndVerifyCached(url_b, key_b, true /* was_cached */,
                      true /* is_navigation */);
}

TEST_F(NetworkContextSplitCacheTest,
       CachedUsingNetworkIsolationKeyWithFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {net::features::kSplitCacheByNetworkIsolationKey,
       net::features::kAppendFrameOriginToNetworkIsolationKey},
      {});

  GURL url = test_server()->GetURL("/resource");
  url::Origin origin_a = url::Origin::Create(GURL("http://a.test/"));
  net::NetworkIsolationKey key_a(origin_a, origin_a);
  LoadAndVerifyCached(url, key_a, false /* was_cached */,
                      false /* is_navigation */);

  // Load again with a different isolation key. The cached entry should not be
  // loaded.
  url::Origin origin_b = url::Origin::Create(GURL("http://b.test/"));
  net::NetworkIsolationKey key_b(origin_a, origin_b);
  LoadAndVerifyCached(url, key_b, false /* was_cached */,
                      false /* is_navigation */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceRedirectNetworkIsolationKey) {
  // Create a request that redirects.
  GURL url = test_server()->GetURL(
      "/server-redirect?" +
      test_server()->GetURL("othersite.test", "/title1.html").spec());
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey key(origin, origin);
  LoadAndVerifyCached(
      url, key, false /* was_cached */, true /* is_navigation */,
      mojom::UpdateNetworkIsolationKeyOnRedirect::kUpdateTopFrameAndFrameOrigin,
      true /* expect_redirect */);

  // Now directly load with the key using the redirected URL. This should be a
  // cache hit.
  GURL redirected_url = test_server()->GetURL("othersite.test", "/title1.html");
  url::Origin redirected_origin = url::Origin::Create(redirected_url);
  LoadAndVerifyCached(
      redirected_url,
      net::NetworkIsolationKey(redirected_origin, redirected_origin),
      true /* was_cached */, true /* is_navigation */);

  // A non-navigation resource with the same key and url should also be cached.
  LoadAndVerifyCached(
      redirected_url,
      net::NetworkIsolationKey(redirected_origin, redirected_origin),
      true /* was_cached */, false /* is_navigation */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceRedirectNetworkIsolationKeyWithNewUrl) {
  // Create a request that redirects to othersite.test/title1.html.
  GURL url = test_server()->GetURL(
      "/server-redirect?" +
      test_server()->GetURL("othersite.test", "/title1.html").spec());
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey key(origin, origin);

  // Create a new url that should be used in the network isolation key computed
  // in FollowRedirect instead of the redirected url.
  GURL new_url = test_server()->GetURL("othersite.test", "/title2.html");
  LoadAndVerifyCached(
      url, key, false /* was_cached */, true /* is_navigation */,
      mojom::UpdateNetworkIsolationKeyOnRedirect::kUpdateTopFrameAndFrameOrigin,
      true /* expect_redirect */, new_url);

  // Load with the key using the new url should be a cache hit.
  origin = url::Origin::Create(new_url);
  LoadAndVerifyCached(new_url, net::NetworkIsolationKey(origin, origin),
                      true /* was_cached */, true /* is_navigation */);

  // Now directly load with the key using the redirected URL. This should be a
  // cache miss.
  GURL redirected_url = test_server()->GetURL("othersite.test", "/title1.html");
  origin = url::Origin::Create(redirected_url);
  LoadAndVerifyCached(redirected_url, net::NetworkIsolationKey(origin, origin),
                      false /* was_cached */, true /* is_navigation */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceCachedUsingNetworkIsolationKeyWithFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {net::features::kSplitCacheByNetworkIsolationKey,
       net::features::kAppendFrameOriginToNetworkIsolationKey},
      {});
  GURL url = test_server()->GetURL("othersite.test", "/main.html");
  url::Origin origin_a = url::Origin::Create(url);
  net::NetworkIsolationKey key_a(origin_a, origin_a);
  LoadAndVerifyCached(url, key_a, false /* was_cached */,
                      true /* is_navigation */);

  // Load again with a isolation key using a different subframe origin. The
  // cached entry should not be loaded.
  url::Origin origin_b = url::Origin::Create(test_server()->base_url());
  net::NetworkIsolationKey key_b(origin_a, origin_b);
  LoadAndVerifyCached(url, key_b, false /* was_cached */,
                      true /* is_navigation */);

  // Load again with the same isolation key. The cached entry should be loaded.
  LoadAndVerifyCached(url, key_b, true /* was_cached */,
                      true /* is_navigation */);

  // Same for a non-navigation entry.
  LoadAndVerifyCached(url, key_b, true /* was_cached */,
                      false /* is_navigation */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceRedirectNetworkIsolationKeyWithFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {net::features::kSplitCacheByNetworkIsolationKey,
       net::features::kAppendFrameOriginToNetworkIsolationKey},
      {});
  // Create a request that redirects to othersite.test/title1.html.
  GURL url = test_server()->GetURL(
      "/server-redirect?" +
      test_server()->GetURL("othersite.test", "/title1.html").spec());
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey key(origin, origin);
  LoadAndVerifyCached(
      url, key, false /* was_cached */, true /* is_navigation */,
      mojom::UpdateNetworkIsolationKeyOnRedirect::kUpdateFrameOrigin,
      true /* expect_redirect */);

  // Now directly load with the key using the redirected URL. This should be a
  // cache hit.
  GURL redirected_url = test_server()->GetURL("othersite.test", "/title1.html");
  LoadAndVerifyCached(
      redirected_url,
      net::NetworkIsolationKey(origin, url::Origin::Create(redirected_url)),
      true /* was_cached */, true /* is_navigation */);
}

}  // namespace

}  // namespace network
