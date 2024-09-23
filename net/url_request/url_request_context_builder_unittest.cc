// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_REPORTING)
#include "base/files/scoped_temp_dir.h"

#if !BUILDFLAG(CRONET_BUILD)
// gn check does not account for BUILDFLAG(). So, for Cronet builds, it will
// complain about a missing dependency on the target exposing this header. Add a
// nogncheck to stop it from yelling.
#include "net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h"  // nogncheck
#endif  // !BUILDFLAG(CRONET_BUILD)

#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_uploader.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {

namespace {

class MockHttpAuthHandlerFactory : public HttpAuthHandlerFactory {
 public:
  MockHttpAuthHandlerFactory(std::string supported_scheme, int return_code)
      : return_code_(return_code), supported_scheme_(supported_scheme) {}
  ~MockHttpAuthHandlerFactory() override = default;

  int CreateAuthHandler(
      HttpAuthChallengeTokenizer* challenge,
      HttpAuth::Target target,
      const SSLInfo& ssl_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::SchemeHostPort& scheme_host_port,
      CreateReason reason,
      int nonce_count,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler) override {
    handler->reset();

    return challenge->auth_scheme() == supported_scheme_
               ? return_code_
               : ERR_UNSUPPORTED_AUTH_SCHEME;
  }

 private:
  int return_code_;
  std::string supported_scheme_;
};

class URLRequestContextBuilderTest : public PlatformTest,
                                     public WithTaskEnvironment {
 protected:
  URLRequestContextBuilderTest() {
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
    SetUpURLRequestContextBuilder(builder_);
  }

  void SetUpURLRequestContextBuilder(URLRequestContextBuilder& builder) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    builder.set_proxy_config_service(std::make_unique<ProxyConfigServiceFixed>(
        ProxyConfigWithAnnotation::CreateDirect()));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
  }

  std::unique_ptr<HostResolver> host_resolver_ =
      std::make_unique<MockHostResolver>();
  EmbeddedTestServer test_server_;
  URLRequestContextBuilder builder_;
};

TEST_F(URLRequestContextBuilderTest, DefaultSettings) {
  ASSERT_TRUE(test_server_.Start());

  std::unique_ptr<URLRequestContext> context(builder_.Build());
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      test_server_.GetURL("/echoheader?Foo"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->set_method("GET");
  request->SetExtraRequestHeaderByName("Foo", "Bar", false);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ("Bar", delegate.data_received());
}

TEST_F(URLRequestContextBuilderTest, UserAgent) {
  ASSERT_TRUE(test_server_.Start());

  builder_.set_user_agent("Bar");
  std::unique_ptr<URLRequestContext> context(builder_.Build());
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      test_server_.GetURL("/echoheader?User-Agent"), DEFAULT_PRIORITY,
      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->set_method("GET");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ("Bar", delegate.data_received());
}

TEST_F(URLRequestContextBuilderTest, DefaultHttpAuthHandlerFactory) {
  url::SchemeHostPort scheme_host_port(GURL("https://www.google.com"));
  std::unique_ptr<HttpAuthHandler> handler;
  std::unique_ptr<URLRequestContext> context(builder_.Build());
  SSLInfo null_ssl_info;

  // Verify that the default basic handler is present
  EXPECT_EQ(OK,
            context->http_auth_handler_factory()->CreateAuthHandlerFromString(
                "basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resolver_.get(), &handler));
}

TEST_F(URLRequestContextBuilderTest, CustomHttpAuthHandlerFactory) {
  url::SchemeHostPort scheme_host_port(GURL("https://www.google.com"));
  const int kBasicReturnCode = OK;
  std::unique_ptr<HttpAuthHandler> handler;
  builder_.SetHttpAuthHandlerFactory(
      std::make_unique<MockHttpAuthHandlerFactory>("extrascheme",
                                                   kBasicReturnCode));
  std::unique_ptr<URLRequestContext> context(builder_.Build());
  SSLInfo null_ssl_info;
  // Verify that a handler is returned for a custom scheme.
  EXPECT_EQ(kBasicReturnCode,
            context->http_auth_handler_factory()->CreateAuthHandlerFromString(
                "ExtraScheme", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resolver_.get(), &handler));

  // Verify that the default basic handler isn't present
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            context->http_auth_handler_factory()->CreateAuthHandlerFromString(
                "basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resolver_.get(), &handler));

  // Verify that a handler isn't returned for a bogus scheme.
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            context->http_auth_handler_factory()->CreateAuthHandlerFromString(
                "Bogus", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resolver_.get(), &handler));
}

#if BUILDFLAG(ENABLE_REPORTING)
// See crbug.com/935209. This test ensures that shutdown occurs correctly and
// does not crash while destoying the NEL and Reporting services in the process
// of destroying the URLRequestContext whilst Reporting has a pending upload.
TEST_F(URLRequestContextBuilderTest, ShutDownNELAndReportingWithPendingUpload) {
  std::unique_ptr<MockHostResolver> host_resolver =
      std::make_unique<MockHostResolver>();
  host_resolver->set_ondemand_mode(true);
  MockHostResolver* mock_host_resolver = host_resolver.get();
  builder_.set_host_resolver(std::move(host_resolver));
  builder_.set_proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateDirect());
  builder_.set_reporting_policy(std::make_unique<ReportingPolicy>());
  builder_.set_network_error_logging_enabled(true);

  std::unique_ptr<URLRequestContext> context(builder_.Build());
  ASSERT_TRUE(context->network_error_logging_service());
  ASSERT_TRUE(context->reporting_service());

  // Queue a pending upload.
  GURL url("https://www.foo.test");
  context->reporting_service()->GetContextForTesting()->uploader()->StartUpload(
      url::Origin::Create(url), url, IsolationInfo::CreateTransient(),
      "report body", 0,
      /*eligible_for_credentials=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, context->reporting_service()
                   ->GetContextForTesting()
                   ->uploader()
                   ->GetPendingUploadCountForTesting());
  ASSERT_TRUE(mock_host_resolver->has_pending_requests());

  // This should shut down and destroy the NEL and Reporting services, including
  // the PendingUpload, and should not cause a crash.
  context.reset();
}

#if !BUILDFLAG(CRONET_BUILD)
// See crbug.com/935209. This test ensures that shutdown occurs correctly and
// does not crash while destoying the NEL and Reporting services in the process
// of destroying the URLRequestContext whilst Reporting has a pending upload.
TEST_F(URLRequestContextBuilderTest,
       ShutDownNELAndReportingWithPendingUploadAndPersistentStorage) {
  std::unique_ptr<MockHostResolver> host_resolver =
      std::make_unique<MockHostResolver>();
  host_resolver->set_ondemand_mode(true);
  MockHostResolver* mock_host_resolver = host_resolver.get();
  builder_.set_host_resolver(std::move(host_resolver));
  builder_.set_proxy_resolution_service(
      ConfiguredProxyResolutionService::CreateDirect());
  builder_.set_reporting_policy(std::make_unique<ReportingPolicy>());
  builder_.set_network_error_logging_enabled(true);
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  builder_.set_persistent_reporting_and_nel_store(
      std::make_unique<SQLitePersistentReportingAndNelStore>(
          scoped_temp_dir.GetPath().Append(
              FILE_PATH_LITERAL("ReportingAndNelStore")),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(),
               net::GetReportingAndNelStoreBackgroundSequencePriority(),
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})));

  std::unique_ptr<URLRequestContext> context(builder_.Build());
  ASSERT_TRUE(context->network_error_logging_service());
  ASSERT_TRUE(context->reporting_service());
  ASSERT_TRUE(context->network_error_logging_service()
                  ->GetPersistentNelStoreForTesting());
  ASSERT_TRUE(context->reporting_service()->GetContextForTesting()->store());

  // Queue a pending upload.
  GURL url("https://www.foo.test");
  context->reporting_service()->GetContextForTesting()->uploader()->StartUpload(
      url::Origin::Create(url), url, IsolationInfo::CreateTransient(),
      "report body", 0,
      /*eligible_for_credentials=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, context->reporting_service()
                   ->GetContextForTesting()
                   ->uploader()
                   ->GetPendingUploadCountForTesting());
  ASSERT_TRUE(mock_host_resolver->has_pending_requests());

  // This should shut down and destroy the NEL and Reporting services, including
  // the PendingUpload, and should not cause a crash.
  context.reset();
}
#endif  // !BUILDFLAG(CRONET_BUILD)

TEST_F(URLRequestContextBuilderTest,
       BuilderSetEnterpriseReportingEndpointsWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  builder_.set_reporting_policy(std::make_unique<ReportingPolicy>());
  builder_.set_enterprise_reporting_endpoints(test_enterprise_endpoints);
  std::unique_ptr<URLRequestContext> context(builder_.Build());
  ASSERT_TRUE(context->reporting_service());
  std::vector<net::ReportingEndpoint> expected_enterprise_endpoints = {
      {net::ReportingEndpointGroupKey(net::NetworkAnonymizationKey(),
                                      /*reporting_source=*/std::nullopt,
                                      /*origin=*/std::nullopt, "endpoint-1",
                                      net::ReportingTargetType::kEnterprise),
       {.url = GURL("https://example.com/reports")}},
      {net::ReportingEndpointGroupKey(net::NetworkAnonymizationKey(),
                                      /*reporting_source=*/std::nullopt,
                                      /*origin=*/std::nullopt, "endpoint-2",
                                      net::ReportingTargetType::kEnterprise),
       {.url = GURL("https://reporting.example/cookie-issues")}},
      {net::ReportingEndpointGroupKey(net::NetworkAnonymizationKey(),
                                      /*reporting_source=*/std::nullopt,
                                      /*origin=*/std::nullopt, "endpoint-3",
                                      net::ReportingTargetType::kEnterprise),
       {.url = GURL("https://report-collector.example")}}};

  EXPECT_EQ(expected_enterprise_endpoints,
            context->reporting_service()
                ->GetContextForTesting()
                ->cache()
                ->GetEnterpriseEndpointsForTesting());
}

TEST_F(URLRequestContextBuilderTest,
       BuilderSetEnterpriseReportingEndpointsWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  builder_.set_reporting_policy(std::make_unique<ReportingPolicy>());
  builder_.set_enterprise_reporting_endpoints(test_enterprise_endpoints);
  std::unique_ptr<URLRequestContext> context(builder_.Build());
  ASSERT_TRUE(context->reporting_service());

  EXPECT_EQ(0u, context->reporting_service()
                    ->GetContextForTesting()
                    ->cache()
                    ->GetEnterpriseEndpointsForTesting()
                    .size());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

TEST_F(URLRequestContextBuilderTest, ShutdownHostResolverWithPendingRequest) {
  auto mock_host_resolver = std::make_unique<MockHostResolver>();
  mock_host_resolver->rules()->AddRule("example.com", "1.2.3.4");
  mock_host_resolver->set_ondemand_mode(true);
  auto state = mock_host_resolver->state();
  builder_.set_host_resolver(std::move(mock_host_resolver));
  std::unique_ptr<URLRequestContext> context(builder_.Build());

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      context->host_resolver()->CreateRequest(HostPortPair("example.com", 1234),
                                              NetworkAnonymizationKey(),
                                              NetLogWithSource(), std::nullopt);
  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  ASSERT_TRUE(state->has_pending_requests());

  context.reset();

  EXPECT_FALSE(state->has_pending_requests());

  // Request should never complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback.have_result());
}

TEST_F(URLRequestContextBuilderTest, DefaultHostResolver) {
  auto manager = std::make_unique<HostResolverManager>(
      HostResolver::ManagerOptions(), nullptr /* system_dns_config_notifier */,
      nullptr /* net_log */);

  // Use a stack allocated builder instead of `builder_` to avoid dangling
  // pointer of `manager`.
  URLRequestContextBuilder builder;
  SetUpURLRequestContextBuilder(builder);
  builder.set_host_resolver_manager(manager.get());
  std::unique_ptr<URLRequestContext> context = builder.Build();

  EXPECT_EQ(context.get(), context->host_resolver()->GetContextForTesting());
  EXPECT_EQ(manager.get(), context->host_resolver()->GetManagerForTesting());
}

TEST_F(URLRequestContextBuilderTest, CustomHostResolver) {
  std::unique_ptr<HostResolver> resolver =
      HostResolver::CreateStandaloneResolver(nullptr);
  ASSERT_FALSE(resolver->GetContextForTesting());

  builder_.set_host_resolver(std::move(resolver));
  std::unique_ptr<URLRequestContext> context = builder_.Build();

  EXPECT_EQ(context.get(), context->host_resolver()->GetContextForTesting());
}

TEST_F(URLRequestContextBuilderTest, BindToNetworkFinalConfiguration) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    GTEST_SKIP()
        << "BindToNetwork is supported starting from Android Marshmallow";
  }

  // The actual network handle doesn't really matter, this test just wants to
  // check that all the pieces are in place and configured correctly.
  constexpr handles::NetworkHandle network = 2;
  auto scoped_mock_network_change_notifier =
      std::make_unique<test::ScopedMockNetworkChangeNotifier>();
  test::MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();

  builder_.BindToNetwork(network);
  std::unique_ptr<URLRequestContext> context = builder_.Build();

  EXPECT_EQ(context->bound_network(), network);
  EXPECT_EQ(context->host_resolver()->GetTargetNetworkForTesting(), network);
  EXPECT_EQ(context->host_resolver()
                ->GetManagerForTesting()
                ->target_network_for_testing(),
            network);
  ASSERT_TRUE(context->GetNetworkSessionContext());
  // A special factory that bind sockets to `network` is needed. We don't need
  // to check exactly for that, the fact that we are not using the default one
  // should be good enough.
  EXPECT_NE(context->GetNetworkSessionContext()->client_socket_factory,
            ClientSocketFactory::GetDefaultFactory());

  const auto* quic_params = context->quic_context()->params();
  EXPECT_FALSE(quic_params->close_sessions_on_ip_change);
  EXPECT_FALSE(quic_params->goaway_sessions_on_ip_change);
  EXPECT_FALSE(quic_params->migrate_sessions_on_network_change_v2);

  const auto* network_session_params = context->GetNetworkSessionParams();
  EXPECT_TRUE(network_session_params->ignore_ip_address_changes);
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "BindToNetwork is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(URLRequestContextBuilderTest, BindToNetworkCustomManagerOptions) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    GTEST_SKIP()
        << "BindToNetwork is supported starting from Android Marshmallow";
  }

  // The actual network handle doesn't really matter, this test just wants to
  // check that all the pieces are in place and configured correctly.
  constexpr handles::NetworkHandle network = 2;
  auto scoped_mock_network_change_notifier =
      std::make_unique<test::ScopedMockNetworkChangeNotifier>();
  test::MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();

  // Set non-default value for check_ipv6_on_wifi and check that this is what
  // HostResolverManager receives.
  HostResolver::ManagerOptions options;
  options.check_ipv6_on_wifi = !options.check_ipv6_on_wifi;
  builder_.BindToNetwork(network, options);
  std::unique_ptr<URLRequestContext> context = builder_.Build();
  EXPECT_EQ(context->host_resolver()
                ->GetManagerForTesting()
                ->check_ipv6_on_wifi_for_testing(),
            options.check_ipv6_on_wifi);
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "BindToNetwork is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(URLRequestContextBuilderTest, MigrateSessionsOnNetworkChangeV2Default) {
  std::unique_ptr<URLRequestContext> context = builder_.Build();

  const QuicParams* quic_params = context->quic_context()->params();
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(quic_params->migrate_sessions_on_network_change_v2);
#else   // !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(quic_params->migrate_sessions_on_network_change_v2);
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(URLRequestContextBuilderTest, MigrateSessionsOnNetworkChangeV2Override) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      net::features::kMigrateSessionsOnNetworkChangeV2);
  std::unique_ptr<URLRequestContext> context = builder_.Build();

  const QuicParams* quic_params = context->quic_context()->params();
  EXPECT_FALSE(quic_params->migrate_sessions_on_network_change_v2);
}

}  // namespace

}  // namespace net
