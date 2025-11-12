// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/storage_access_api/status.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/shared_resource_checker.h"
#include "services/network/test/url_loader_context_for_tests.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace content {

using testing::Optional;

class NavigationURLLoaderImplTest : public testing::Test {
 public:
  NavigationURLLoaderImplTest()
      : task_environment_(std::make_unique<BrowserTaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::IO,
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME)),
        network_change_notifier_(
            net::test::MockNetworkChangeNotifier::Create()) {
    browser_context_ = std::make_unique<TestBrowserContext>();
    http_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));

#if BUILDFLAG(ENABLE_PLUGINS)
    PluginService::GetInstance()->Init();
#endif
  }

  ~NavigationURLLoaderImplTest() override {
    browser_context_.reset();
    // Reset the BrowserTaskEnvironment to force destruction of the local
    // NetworkService, which is held in SequenceLocalStorage. This must happen
    // before destruction of |network_change_notifier_|, to allow observers to
    // be unregistered.
    task_environment_.reset();
  }

  void SetUp() override {
    // Do not create TestNavigationURLLoaderFactory as this tests creates
    // NavigationURLLoaders explicitly and TestNavigationURLLoaderFactory
    // interferes with that.
    rvh_test_enabler_ = std::make_unique<RenderViewHostTestEnabler>(
        RenderViewHostTestEnabler::NavigationURLLoaderFactoryType::kNone);
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
  }

  void TearDown() override {
    pending_navigation_.reset();
    web_contents_.reset();
    if (client_hints_controller_delegate_.get()) {
      browser_context_->SetClientHintsControllerDelegate(nullptr);
      client_hints_controller_delegate_.reset();
    }
    rvh_test_enabler_.reset();
  }

  std::unique_ptr<NavigationURLLoaderImpl> CreateTestLoader(
      const GURL& url,
      const std::string& headers,
      const std::string& method,
      NavigationURLLoaderDelegate* delegate,
      blink::NavigationDownloadPolicy download_policy =
          blink::NavigationDownloadPolicy(),
      bool is_main_frame = true,
      bool upgrade_if_insecure = false,
      bool is_ad_tagged = false,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors =
          {}) {
    // NavigationURLLoader assumes that the corresponding FrameTreeNode has an
    // associated NavigationRequest.
    // NOTE: This also creates and starts another `NavigationURLLoaderImpl`
    // (`NavigationRequest::loader_`) but it's not the `NavigationURLLoaderImpl`
    // being tested (=the return value of this method).
    pending_navigation_ = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://example.com"), web_contents_.get());
    pending_navigation_->Start();

    blink::mojom::BeginNavigationParamsPtr begin_params =
        blink::mojom::BeginNavigationParams::New(
            std::nullopt /* initiator_frame_token */, headers, net::LOAD_NORMAL,
            false /* skip_service_worker */,
            blink::mojom::RequestContextType::LOCATION,
            blink::mojom::MixedContentContextType::kBlockable,
            false /* is_form_submission */,
            false /* was_initiated_by_link_click */,
            blink::mojom::ForceHistoryPush::kNo,
            GURL() /* searchable_form_url */,
            std::string() /* searchable_form_encoding */,
            GURL() /* client_side_redirect_url */,
            std::nullopt /* devtools_initiator_info */,
            nullptr /* trust_token_params */, std::nullopt /* impression */,
            base::TimeTicks() /* renderer_before_unload_start */,
            base::TimeTicks() /* renderer_before_unload_end */,
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kDidNotStartWithTransientActivation,
            false /* is_container_initiated */,
            net::StorageAccessApiStatus::kNone, false /* has_rel_opener */);

    auto common_params = blink::CreateCommonNavigationParams();
    common_params->url = url;
    common_params->initiator_origin = url::Origin::Create(url);
    common_params->method = method;
    common_params->download_policy = download_policy;
    common_params->request_destination =
        network::mojom::RequestDestination::kDocument;
    url::Origin origin = url::Origin::Create(url);

    FrameTreeNodeId frame_tree_node_id =
        web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId();

    bool is_primary_main_frame = is_main_frame;
    bool is_outermost_main_frame = is_main_frame;

    std::unique_ptr<NavigationRequestInfo> request_info(
        std::make_unique<NavigationRequestInfo>(
            std::move(common_params), std::move(begin_params),
            network::mojom::WebSandboxFlags::kNone,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, origin, origin,
                net::SiteForCookies::FromUrl(url)),
            is_primary_main_frame, is_outermost_main_frame, is_main_frame,
            false /* are_ancestors_secure */, frame_tree_node_id,
            false /* report_raw_headers */,
            upgrade_if_insecure /* upgrade_if_insecure */,
            nullptr /* blob_url_loader_factory */,
            base::UnguessableToken::Create() /* devtools_navigation_token */,
            base::UnguessableToken::Create() /* devtools_frame_token */,
            nullptr /* client_security_state */,
            std::nullopt /* devtools_accepted_stream_types */,
            false /* is_pdf */,
            ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
            std::nullopt /* initiator_document_token */,
            GlobalRenderFrameHostId() /* previous_render_frame_host_id */,
            nullptr /* serving_page_metrics_container */,
            false /* allow_cookies_from_browser */, 0 /* navigation_id */,
            false /* shared_storage_writable */,
            is_ad_tagged /* is_ad_tagged */,
            false /* force_no_https_upgrade */));

    return std::make_unique<NavigationURLLoaderImpl>(
        browser_context_.get(), browser_context_->GetDefaultStoragePartition(),
        std::move(request_info), nullptr /* navigation_ui_data */,
        nullptr /* service_worker_handle */,
        nullptr /* prefetched_signed_exchange_cache */, delegate,
        mojo::NullRemote() /* cookie_access_obsever */,
        mojo::NullRemote() /* trust_token_observer */,
        mojo::NullRemote() /* shared_dictionary_observer */,
        mojo::NullRemote() /* url_loader_network_observer */,
        /*devtools_observer=*/mojo::NullRemote(),
        /*device_bound_session_observer=*/mojo::NullRemote(),
        std::move(interceptors));
  }

  // Requests |redirect_url|, which must return a HTTP 3xx redirect. It's also
  // used as the initial origin.
  // |request_method| is the method to use for the initial request.
  // |expected_redirect_method| is the method that is expected to be used for
  // the second request, after redirection.
  // |expected_origin_value| is the expected value for the Origin header after
  // redirection. If empty, expects that there will be no Origin header.
  // Returns the last `network::ResourceRequest` used.
  network::ResourceRequest HTTPRedirectOriginHeaderTest(
      const GURL& redirect_url,
      const std::string& request_method,
      const std::string& expected_redirect_method,
      const std::string& expected_origin_value,
      bool expect_request_fail = false) {
    TestNavigationURLLoaderDelegate delegate;
    auto loader = CreateTestLoader(
        redirect_url,
        base::StringPrintf(
            "%s: %s", net::HttpRequestHeaders::kOrigin,
            redirect_url.DeprecatedGetOriginAsURL().spec().c_str()),
        request_method, &delegate);
    loader->Start();
    delegate.WaitForRequestRedirected();
    loader->FollowRedirect({}, {}, {});

    EXPECT_EQ(expected_redirect_method, delegate.redirect_info().new_method);

    if (expect_request_fail) {
      delegate.WaitForRequestFailed();
    } else {
      delegate.WaitForResponseStarted();
    }

    network::ResourceRequest resource_request =
        loader->GetResourceRequestForTesting();

    // Note that there is no check for request success here because, for
    // purposes of testing, the request very well may fail. For example, if the
    // test redirects to an HTTPS server from an HTTP origin, thus it is cross
    // origin, there is not an HTTPS server in this unit test framework, so the
    // request would fail. However, that's fine, as long as the request headers
    // are in order and pass the checks below.
    if (expected_origin_value.empty()) {
      EXPECT_FALSE(
          resource_request.headers.HasHeader(net::HttpRequestHeaders::kOrigin));
    } else {
      EXPECT_THAT(
          resource_request.headers.GetHeader(net::HttpRequestHeaders::kOrigin),
          Optional(expected_origin_value));
    }
    return resource_request;
  }

  net::RedirectInfo NavigateAndReturnRedirectInfo(const GURL& url,
                                                  bool upgrade_if_insecure,
                                                  bool expect_request_fail) {
    TestNavigationURLLoaderDelegate delegate;
    auto loader = CreateTestLoader(
        url,
        base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                           url.DeprecatedGetOriginAsURL().spec().c_str()),
        "GET", &delegate, blink::NavigationDownloadPolicy(),
        true /*is_main_frame*/, upgrade_if_insecure);
    loader->Start();
    delegate.WaitForRequestRedirected();
    loader->FollowRedirect({}, {}, {});

    if (expect_request_fail) {
      delegate.WaitForRequestFailed();
    } else {
      delegate.WaitForResponseStarted();
    }
    return delegate.redirect_info();
  }

 protected:
  void SetupClientHintsControllerDelegate(
      const std::vector<network::mojom::WebClientHintsType>& client_hints) {
    blink::UserAgentMetadata ua_metadata;
    client_hints_controller_delegate_ =
        std::make_unique<MockClientHintsControllerDelegate>(ua_metadata);
    client_hints_controller_delegate_->SetAdditionalClientHints(client_hints);
    browser_context_->SetClientHintsControllerDelegate(
        client_hints_controller_delegate_.get());
  }

  std::unique_ptr<BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  std::unique_ptr<MockClientHintsControllerDelegate>
      client_hints_controller_delegate_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  net::EmbeddedTestServer http_test_server_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<TestWebContents> web_contents_;
  // NavigationURLLoaderImpl relies on the existence of the
  // |frame_tree_node->navigation_request()|.
  std::unique_ptr<NavigationSimulator> pending_navigation_;
};

// 304 responses should abort the navigation, rather than display the page.
TEST_F(NavigationURLLoaderImplTest, Response304) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL url = http_test_server_.GetURL("/page304.html");
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(url, std::string(), "GET", &delegate);
  loader->Start();
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_ABORTED, delegate.net_error());
}

TEST_F(NavigationURLLoaderImplTest, IsolationInfoOfMainFrameNavigation) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(
      url,
      base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                         url.DeprecatedGetOriginAsURL().spec().c_str()),
      "GET", &delegate, blink::NavigationDownloadPolicy(),
      true /*is_main_frame*/, false /*upgrade_if_insecure*/);
  loader->Start();
  delegate.WaitForResponseStarted();

  ASSERT_TRUE(loader->GetResourceRequestForTesting().trusted_params);
  EXPECT_TRUE(net::IsolationInfo::Create(
                  net::IsolationInfo::RequestType::kMainFrame, origin, origin,
                  net::SiteForCookies::FromOrigin(origin))
                  .IsEqualForTesting(loader->GetResourceRequestForTesting()
                                         .trusted_params->isolation_info));
}

TEST_F(NavigationURLLoaderImplTest,
       IsolationInfoOfRedirectedMainFrameNavigation) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  const url::Origin origin = url::Origin::Create(url);

  network::ResourceRequest resource_request = HTTPRedirectOriginHeaderTest(
      url, "GET", "GET", url.DeprecatedGetOriginAsURL().spec());

  ASSERT_TRUE(resource_request.trusted_params);
  EXPECT_TRUE(
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 origin, origin,
                                 net::SiteForCookies::FromOrigin(origin))
          .IsEqualForTesting(resource_request.trusted_params->isolation_info));
}

TEST_F(NavigationURLLoaderImplTest, EnsureEnabledClientHints) {
  base::test::ScopedFeatureList feature_list{
      network::features::kOffloadAcceptCHFrameCheck};
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  std::vector<network::mojom::WebClientHintsType> expected_client_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  SetupClientHintsControllerDelegate(expected_client_hints);
  TestNavigationURLLoaderDelegate delegate;
  auto loader =
      CreateTestLoader(url, /*headers=*/"", /*method=*/"GET", &delegate);
  loader->Start();
  delegate.WaitForResponseStarted();

  ASSERT_TRUE(loader->GetResourceRequestForTesting().trusted_params);
  EXPECT_TRUE(loader->GetResourceRequestForTesting()
                  .trusted_params->enabled_client_hints.has_value());
  // The default types are added in addition, and that is why `IsSupersetOf()`
  // is used.
  EXPECT_THAT(loader->GetResourceRequestForTesting()
                  .trusted_params->enabled_client_hints->hints,
              testing::IsSupersetOf(expected_client_hints));
  EXPECT_EQ(origin, loader->GetResourceRequestForTesting()
                        .trusted_params->enabled_client_hints->origin);
}

TEST_F(NavigationURLLoaderImplTest, EnsureEnabledClientHintsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {network::features::kOffloadAcceptCHFrameCheck});
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  std::vector<network::mojom::WebClientHintsType> client_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  SetupClientHintsControllerDelegate(client_hints);
  blink::UserAgentMetadata ua_metadata;
  TestNavigationURLLoaderDelegate delegate;
  auto loader =
      CreateTestLoader(url, /*headers=*/"", /*method=*/"GET", &delegate);
  loader->Start();
  delegate.WaitForResponseStarted();

  ASSERT_TRUE(loader->GetResourceRequestForTesting().trusted_params);
  EXPECT_FALSE(loader->GetResourceRequestForTesting()
                   .trusted_params->enabled_client_hints.has_value());
}

TEST_F(NavigationURLLoaderImplTest, EnsureEnabledClientHintsOnRedirect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      network::features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHOffloadWithRedirect", "true"}});
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  const url::Origin final_origin = url::Origin::Create(final_url);

  std::vector<network::mojom::WebClientHintsType> expected_client_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  SetupClientHintsControllerDelegate(expected_client_hints);
  TestNavigationURLLoaderDelegate delegate;
  auto loader =
      CreateTestLoader(url, /*headers=*/"", /*method=*/"GET", &delegate);
  loader->Start();
  delegate.WaitForRequestRedirected();
  loader->FollowRedirect({}, {}, {});
  delegate.WaitForResponseStarted();

  const auto& resource_request = loader->GetResourceRequestForTesting();
  ASSERT_TRUE(resource_request.trusted_params);
  EXPECT_TRUE(
      resource_request.trusted_params->enabled_client_hints.has_value());
  // The default types are added in addition, and that is why `IsSupersetOf()`
  // is used.
  EXPECT_THAT(resource_request.trusted_params->enabled_client_hints->hints,
              testing::IsSupersetOf(expected_client_hints));
  EXPECT_EQ(final_origin,
            resource_request.trusted_params->enabled_client_hints->origin);
}

TEST_F(NavigationURLLoaderImplTest,
       EnsureEnabledClientHintsOnCrossOriginRedirect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      network::features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHOffloadWithRedirect", "true"}});
  ASSERT_TRUE(http_test_server_.Start());
  net::EmbeddedTestServer http_test_server2;
  http_test_server2.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(http_test_server2.Start());

  const GURL final_url = http_test_server2.GetURL("/echo");
  const GURL url =
      http_test_server_.GetURL("/server-redirect?" + final_url.spec());
  const url::Origin final_origin = url::Origin::Create(final_url);

  std::vector<network::mojom::WebClientHintsType> expected_client_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  SetupClientHintsControllerDelegate(expected_client_hints);
  TestNavigationURLLoaderDelegate delegate;
  auto loader =
      CreateTestLoader(url, /*headers=*/"", /*method=*/"GET", &delegate);
  loader->Start();
  delegate.WaitForRequestRedirected();
  loader->FollowRedirect({}, {}, {});
  delegate.WaitForResponseStarted();

  const auto& resource_request_after_redirect =
      loader->GetResourceRequestForTesting();
  ASSERT_TRUE(resource_request_after_redirect.trusted_params);
  EXPECT_TRUE(resource_request_after_redirect.trusted_params
                  ->enabled_client_hints.has_value());
  // The default types are added in addition, and that is why `IsSupersetOf()`
  // is used.
  EXPECT_THAT(resource_request_after_redirect.trusted_params
                  ->enabled_client_hints->hints,
              testing::IsSupersetOf(expected_client_hints));
  EXPECT_EQ(final_origin, resource_request_after_redirect.trusted_params
                              ->enabled_client_hints->origin);
}

TEST_F(NavigationURLLoaderImplTest, Redirect301Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect301-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET", std::string(),
                               true);
}

TEST_F(NavigationURLLoaderImplTest, Redirect302Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect302-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect302-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET", std::string(),
                               true);
}

TEST_F(NavigationURLLoaderImplTest, Redirect303Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect303-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect303-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET", std::string(),
                               true);
}

TEST_F(NavigationURLLoaderImplTest, Redirect307Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect307-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect307-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "POST",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null",
                               true);
}

TEST_F(NavigationURLLoaderImplTest, Redirect308Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect308-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect308-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "POST",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null",
                               true);
}

namespace {

// A `NavigationLoaderInterceptor` to test operations during an interceptor is
// running (between `MaybeCreateLoader()` call and its callback invocation, i.e.
// between `WaitUntilMaybeCreateLoader()` and `Unblock()`).
class TestAsyncInterceptor final : public NavigationLoaderInterceptor {
 public:
  TestAsyncInterceptor() { ResetRunLoop(); }
  ~TestAsyncInterceptor() override = default;

  // Waits until the start of `MaybeCreateLoader()`.
  void WaitUntilMaybeCreateLoader() { run_loop_->Run(); }

  // Finishes `MaybeCreateLoader()` without intercepting the request.
  void Unblock() {
    // Allow `WaitUntilMaybeCreateLoader()` for the next redirect request.
    ResetRunLoop();
    std::move(loader_callback_).Run(std::nullopt);
  }

 private:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback loader_callback,
      FallbackCallback fallback_callback) override {
    run_loop_->Quit();
    loader_callback_ = std::move(loader_callback);
  }

  bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors) override {
    return false;
  }

  void ResetRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  std::unique_ptr<base::RunLoop> run_loop_;
  LoaderCallback loader_callback_;
};

// A `NavigationLoaderInterceptor` to test `MaybeCreateLoaderForResponse()`
// triggering redirects.
class TestResponseInterceptor final : public NavigationLoaderInterceptor {
 public:
  explicit TestResponseInterceptor(const GURL& redirect_url)
      : redirect_url_(redirect_url) {}
  ~TestResponseInterceptor() override = default;

  int response_count() const { return response_count_; }
  void set_should_redirect(bool should_redirect) {
    should_redirect_ = should_redirect;
  }

 private:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    std::move(callback).Run(std::nullopt);
  }

  bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors) override {
    if (!should_redirect_) {
      return false;
    }

    ++response_count_;

    mojo::Remote<network::mojom::URLLoaderClient> client;
    *client_receiver = client.BindNewPipeAndPassReceiver();

    // Create an artificial redirect back to the fallback URL.
    auto new_response_head = network::mojom::URLResponseHead::New();
    net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        request.method, request.url, request.site_for_cookies,
        request.update_first_party_url_on_redirect
            ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
            : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
        request.referrer_policy, request.referrer.spec(),
        request.request_initiator, net::HTTP_TEMPORARY_REDIRECT, redirect_url_,
        /*referrer_policy_header=*/std::nullopt,
        /*insecure_scheme_was_upgraded=*/false);
    client->OnReceiveRedirect(redirect_info, std::move(new_response_head));
    return true;
  }

  const GURL redirect_url_;
  int response_count_ = 0;
  bool should_redirect_ = true;
};

// This sets the timeout timer but doesn't expect the timer is fired
// automatically. If needed, the timer should be fired explicitly e.g. via
// `TriggerTimeoutForTesting()`.
void SetLargeNavigationTimeout(NavigationURLLoaderImpl& loader) {
  loader.SetNavigationTimeout(base::Seconds(1000));
}

}  // namespace

// Timeout case (failure) while waiting for the response from URLLoader.
TEST_F(NavigationURLLoaderImplTest, TimeoutDuringURLLoader) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL final_url = http_test_server_.GetURL("/echo");
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(final_url, std::string(), "GET", &delegate);
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the failure due to the timeout is notified.
  //
  // Note [*1]:
  // In the expected non-test scenario, the operations below should occur
  // between `TriggerTimeoutForTesting()` and `WaitForRequestFailed()` (which
  // should be quite rare though), because `NavigationRequest` destroys
  // `NavigationURLLoaderImpl` upon `OnRequestFailed()`.
  // In tests, `WaitForRequestFailed()` is called before the operations below to
  // avoid race conditions, but we can consider as if the `OnRequestFailed()`
  // waited here is received after the operations below, because
  // `OnRequestFailed()` can delay as a pending posted task without interfering
  // with other parts of `NavigationURLLoaderImpl`.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout + MaybeCreateLoaderForResponse() + redirect case (failure) while
// waiting for the response from URLLoader.
TEST_F(NavigationURLLoaderImplTest, RedirectDuringURLLoader) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL final_url = http_test_server_.GetURL("/echo");
  const GURL interceptor_url = http_test_server_.GetURL("/foo");
  TestNavigationURLLoaderDelegate delegate;
  auto response_interceptor =
      std::make_unique<TestResponseInterceptor>(interceptor_url);
  auto* response_interceptor_ptr = response_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(response_interceptor));
  auto loader =
      CreateTestLoader(final_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Interceptor's MaybeCreateLoaderForResponse() is processed synchronously,
  // which triggers `OnReceiveRedirect()` asynchronously.
  ASSERT_EQ(response_interceptor_ptr->response_count(), 1);

  // Note [*2]:
  // Avoid `MaybeCreateLoaderForResponse()` intercepting the response below, to
  // simplify the expectation a bit. The main thing to test is possibly
  // triggering `MaybeCreateLoaderForResponse()` from `OnComplete()` (i.e. from
  // `TriggerTimeoutForTesting()` above), not the response possibly received
  // (unexpectedly) below after the timeout.
  response_interceptor_ptr->set_should_redirect(false);

  // Wait for the redirect due to the timeout + interceptor is notified.
  delegate.WaitForRequestRedirected();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);
}

// Timeout case (failure) with async `OnRequestRedirected()` ->
// `FollowRedirect()`.
TEST_F(NavigationURLLoaderImplTest, TimeoutDuringFollowRedirect) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(redirect_url, std::string(), "GET", &delegate);
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Wait for the async operation starts.
  delegate.WaitForRequestRedirected();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation.
  loader->FollowRedirect({}, {}, {});

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout + MaybeCreateLoaderForResponse() + redirect case (failure) with async
// `OnRequestRedirected()` -> `FollowRedirect()`.
TEST_F(NavigationURLLoaderImplTest, RedirectDuringFollowRedirect) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL interceptor_url = http_test_server_.GetURL("/foo");
  TestNavigationURLLoaderDelegate delegate;
  auto response_interceptor =
      std::make_unique<TestResponseInterceptor>(interceptor_url);
  auto* response_interceptor_ptr = response_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(response_interceptor));
  auto loader =
      CreateTestLoader(redirect_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Wait for the async operation starts.
  delegate.WaitForRequestRedirected();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // `MaybeCreateLoaderForResponse()` shouldn't be called during an exclusive
  // task.
  ASSERT_EQ(response_interceptor_ptr->response_count(), 0);

  // See Note [*2] above.
  response_interceptor_ptr->set_should_redirect(false);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation.
  loader->FollowRedirect({}, {}, {});

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Successful case with async `ParseHeaders()`.
TEST_F(NavigationURLLoaderImplTest, ForceAsyncParseHeaders) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(redirect_url, std::string(), "GET", &delegate);
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Force the async ParseHeaders() path for redirects.
  delegate.set_clear_parsed_headers_on_redirect(true);

  // Wait for the async operation starts.
  delegate.WaitForOnReceiveRedirect();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Finish the async operation (`ParseHeaders()` automatically completes while
  // running the task queue) and wait for redirect/response received after the
  // async `ParseHeaders()`.
  delegate.WaitForRequestRedirected();
  EXPECT_EQ(delegate.redirect_info().new_url, final_url);
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  loader->FollowRedirect({}, {}, {});
  delegate.WaitForResponseStarted();
  EXPECT_EQ(net::OK, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timouet case (failure) with async `ParseHeaders()`.
TEST_F(NavigationURLLoaderImplTest, TimeoutDuringParseHeaders) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(redirect_url, std::string(), "GET", &delegate);
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Force the async ParseHeaders() path for redirects.
  delegate.set_clear_parsed_headers_on_redirect(true);

  // Wait for the async operation starts.
  delegate.WaitForOnReceiveRedirect();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Simulate timeout during the async operation.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation (`ParseHeaders()` automatically completes while
  // running the task queue). No further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout + MaybeCreateLoaderForResponse() + redirect case (failure) with async
// `ParseHeaders()`.
TEST_F(NavigationURLLoaderImplTest, RedirectDuringParseHeaders) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL interceptor_url = http_test_server_.GetURL("/foo");
  TestNavigationURLLoaderDelegate delegate;
  auto response_interceptor =
      std::make_unique<TestResponseInterceptor>(interceptor_url);
  auto* response_interceptor_ptr = response_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(response_interceptor));
  auto loader =
      CreateTestLoader(redirect_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Force the async ParseHeaders() path for redirects.
  delegate.set_clear_parsed_headers_on_redirect(true);

  // Wait for the async operation starts.
  delegate.WaitForOnReceiveRedirect();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);
  EXPECT_EQ(response_interceptor_ptr->response_count(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // `MaybeCreateLoaderForResponse()` shouldn't be called during an exclusive
  // task.
  ASSERT_EQ(response_interceptor_ptr->response_count(), 0);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation (`ParseHeaders()` automatically completes while
  // running the task queue). No further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Successful case with an async interceptor (initial request).
TEST_F(NavigationURLLoaderImplTest, ForceAsyncInterceptor) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL final_url = http_test_server_.GetURL("/echo");
  TestNavigationURLLoaderDelegate delegate;
  auto async_interceptor = std::make_unique<TestAsyncInterceptor>();
  auto* async_interceptor_ptr = async_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(async_interceptor));
  auto loader =
      CreateTestLoader(final_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Wait for the async operation starts.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Finish the async operation.
  async_interceptor_ptr->Unblock();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for response received after the interceptor.
  delegate.WaitForResponseStarted();
  EXPECT_EQ(net::OK, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout case (failure) with an async interceptor (initial request).
TEST_F(NavigationURLLoaderImplTest, TimeoutDuringAsyncInterceptor) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL final_url = http_test_server_.GetURL("/echo");
  TestNavigationURLLoaderDelegate delegate;
  auto async_interceptor = std::make_unique<TestAsyncInterceptor>();
  auto* async_interceptor_ptr = async_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(async_interceptor));
  auto loader =
      CreateTestLoader(final_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Wait for the async operation starts.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation.
  async_interceptor_ptr->Unblock();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout + MaybeCreateLoaderForResponse() + redirect case (failure) with an
// async interceptor (initial request).
TEST_F(NavigationURLLoaderImplTest, RedirectDuringAsyncInterceptor) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL final_url = http_test_server_.GetURL("/echo");
  const GURL interceptor_url = http_test_server_.GetURL("/foo");
  TestNavigationURLLoaderDelegate delegate;
  auto response_interceptor =
      std::make_unique<TestResponseInterceptor>(interceptor_url);
  auto* response_interceptor_ptr = response_interceptor.get();
  auto async_interceptor = std::make_unique<TestAsyncInterceptor>();
  auto* async_interceptor_ptr = async_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(response_interceptor));
  interceptors.push_back(std::move(async_interceptor));
  auto loader =
      CreateTestLoader(final_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Wait for the async operation starts.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Interceptor's MaybeCreateLoaderForResponse() isn't processed here, because
  // `default_loader_used_` is false. Therefore falling back to the same
  // scenario as the `TimeoutDuringAsyncInterceptor` test above.
  // Anyway, `MaybeCreateLoaderForResponse()` shouldn't be called during an
  // exclusive task.
  ASSERT_EQ(response_interceptor_ptr->response_count(), 0);

  // See Note [*2] above.
  response_interceptor_ptr->set_should_redirect(false);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation.
  async_interceptor_ptr->Unblock();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 0);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Successful case with an async interceptor (redirected request).
TEST_F(NavigationURLLoaderImplTest, ForceAsyncInterceptorForRedirect) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  TestNavigationURLLoaderDelegate delegate;
  auto async_interceptor = std::make_unique<TestAsyncInterceptor>();
  auto* async_interceptor_ptr = async_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(async_interceptor));
  auto loader =
      CreateTestLoader(redirect_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Process the initial request.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  async_interceptor_ptr->Unblock();
  delegate.WaitForRequestRedirected();
  loader->FollowRedirect({}, {}, {});
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the async operation starts.
  // This processes the redirected request until the async interceptor starts.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Finish the async operation.
  async_interceptor_ptr->Unblock();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for response received after the interceptor.
  delegate.WaitForResponseStarted();
  EXPECT_EQ(net::OK, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout case (failure) with an async interceptor (redirect request).
TEST_F(NavigationURLLoaderImplTest, TimeoutDuringAsyncInterceptorForRedirect) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  TestNavigationURLLoaderDelegate delegate;
  auto async_interceptor = std::make_unique<TestAsyncInterceptor>();
  auto* async_interceptor_ptr = async_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(async_interceptor));
  auto loader =
      CreateTestLoader(redirect_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Process the initial request.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  async_interceptor_ptr->Unblock();
  delegate.WaitForRequestRedirected();
  loader->FollowRedirect({}, {}, {});
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the async operation starts.
  // This processes the redirected request until the async interceptor starts.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation.
  async_interceptor_ptr->Unblock();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

// Timeout + MaybeCreateLoaderForResponse() + redirect case (failure) with an
// async interceptor (redirect request).
TEST_F(NavigationURLLoaderImplTest, RedirectDuringAsyncInterceptorForRedirect) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  const GURL interceptor_url = http_test_server_.GetURL("/foo");
  TestNavigationURLLoaderDelegate delegate;
  auto response_interceptor =
      std::make_unique<TestResponseInterceptor>(interceptor_url);
  auto* response_interceptor_ptr = response_interceptor.get();
  auto async_interceptor = std::make_unique<TestAsyncInterceptor>();
  auto* async_interceptor_ptr = async_interceptor.get();
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
  interceptors.push_back(std::move(response_interceptor));
  interceptors.push_back(std::move(async_interceptor));
  auto loader =
      CreateTestLoader(redirect_url, std::string(), "GET", &delegate,
                       blink::NavigationDownloadPolicy(),
                       /*is_main_frame=*/true,
                       /*upgrade_if_insecure=*/false,
                       /*is_ad_tagged=*/false, std::move(interceptors));
  loader->Start();
  SetLargeNavigationTimeout(*loader);

  // Process the initial request.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  async_interceptor_ptr->Unblock();
  delegate.WaitForRequestRedirected();
  loader->FollowRedirect({}, {}, {});
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // Wait for the async operation starts.
  // This processes the redirected request until the async interceptor starts.
  async_interceptor_ptr->WaitUntilMaybeCreateLoader();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);
  EXPECT_EQ(response_interceptor_ptr->response_count(), 0);

  // Simulate timeout during the async operation.
  // `delegate` shouldn't be notified synchronously.
  static_cast<NavigationURLLoaderImpl*>(loader.get())
      ->TriggerTimeoutForTesting();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 0);

  // `MaybeCreateLoaderForResponse()` shouldn't be called during an exclusive
  // task.
  ASSERT_EQ(response_interceptor_ptr->response_count(), 0);

  // See Note [*2] above.
  response_interceptor_ptr->set_should_redirect(false);

  // Wait for the failure due to the timeout is notified (See Note [*1] above).
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Finish the async operation.
  async_interceptor_ptr->Unblock();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);

  // Check that no further loading should occur.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(delegate.on_redirect_handled_counter(), 1);
  EXPECT_EQ(delegate.on_request_handled_counter(), 1);
}

TEST_F(NavigationURLLoaderImplTest, RedirectModifiedHeaders) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");

  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(
      redirect_url, "Header1: Value1\r\nHeader2: Value2", "GET", &delegate);
  loader->Start();
  delegate.WaitForRequestRedirected();

  // Initial request should only have initial headers.
  EXPECT_THAT(
      loader->GetResourceRequestForTesting().headers.GetHeader("Header1"),
      Optional(std::string("Value1")));
  EXPECT_THAT(
      loader->GetResourceRequestForTesting().headers.GetHeader("Header2"),
      Optional(std::string("Value2")));
  EXPECT_FALSE(
      loader->GetResourceRequestForTesting().headers.HasHeader("Header3"));

  // Overwrite Header2 and add Header3.
  net::HttpRequestHeaders redirect_headers;
  redirect_headers.SetHeader("Header2", "");
  redirect_headers.SetHeader("Header3", "Value3");
  loader->FollowRedirect({}, redirect_headers, {});
  delegate.WaitForResponseStarted();

  // Redirected request should also have modified headers.
  EXPECT_THAT(
      loader->GetResourceRequestForTesting().headers.GetHeader("Header1"),
      Optional(std::string("Value1")));
  EXPECT_THAT(
      loader->GetResourceRequestForTesting().headers.GetHeader("Header2"),
      Optional(std::string("")));
  EXPECT_THAT(
      loader->GetResourceRequestForTesting().headers.GetHeader("Header3"),
      Optional(std::string("Value3")));
}

// Tests that the Upgrade If Insecure flag is obeyed.
TEST_F(NavigationURLLoaderImplTest, UpgradeIfInsecureTest) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL url = http_test_server_.GetURL("/redirect301-to-http");
  GURL expected_url = GURL("http://test.test/test");
  // We expect the request to fail since there is no server listening at
  // test.test, but for the purpose of this test we only need to validate the
  // redirect URL was not changed.
  net::RedirectInfo redirect_info = NavigateAndReturnRedirectInfo(
      url, false /* upgrade_if_insecure */, true /* expect_request_fail */);
  EXPECT_FALSE(redirect_info.insecure_scheme_was_upgraded);
  EXPECT_EQ(expected_url, redirect_info.new_url);
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  expected_url = expected_url.ReplaceComponents(replacements);
  redirect_info = NavigateAndReturnRedirectInfo(
      url, true /* upgrade_if_insecure */, true /* expect_request_fail */);
  // Same as above, but validating the URL is upgraded to https.
  EXPECT_TRUE(redirect_info.insecure_scheme_was_upgraded);
  EXPECT_EQ(expected_url, redirect_info.new_url);
}

// Tests that when a navigation timeout is set and the navigation takes longer
// than that timeout, then the navigation load fails with ERR_TIMED_OUT.
TEST_F(NavigationURLLoaderImplTest, NavigationTimeoutTest) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL url = http_test_server_.GetURL("/hung");
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(url, std::string(), "GET", &delegate);
  loader->Start();
  loader->SetNavigationTimeout(base::Seconds(3));
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
}

// Like NavigationTimeoutTest but the navigation initially results in a redirect
// before hanging, to test a slightly more complicated navigation.
// TODO(crbug.com/40805451): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NavigationTimeoutRedirectTest \
  DISABLED_NavigationTimeoutRedirectTest
#else
#define MAYBE_NavigationTimeoutRedirectTest NavigationTimeoutRedirectTest
#endif
TEST_F(NavigationURLLoaderImplTest, MAYBE_NavigationTimeoutRedirectTest) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL hang_url = http_test_server_.GetURL("/hung");
  const GURL redirect_url =
      http_test_server_.GetURL("/server-redirect?" + hang_url.spec());
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(redirect_url, std::string(), "GET", &delegate);
  loader->Start();
  loader->SetNavigationTimeout(base::Seconds(3));
  delegate.WaitForRequestRedirected();
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
}

// Verify that UKMs are recorded when OnAcceptCHFrameReceived is called.
TEST_F(NavigationURLLoaderImplTest, OnAcceptCHFrameReceivedUKM) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);
  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(
      url,
      base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                         url.DeprecatedGetOriginAsURL().spec().c_str()),
      "GET", &delegate, blink::NavigationDownloadPolicy(),
      true /*is_main_frame*/, false /*upgrade_if_insecure*/);
  loader->Start();

  // Try recording no hints.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    static_cast<NavigationURLLoaderImpl*>(loader.get())
        ->OnAcceptCHFrameReceived(origin, {},
                                  base::BindOnce([](int32_t) { return; }));
    auto ukm_entries = ukm_recorder.GetEntriesByName(
        ukm::builders::ClientHints_AcceptCHFrameUsage::kEntryName);
    ASSERT_EQ(ukm_entries.size(), 0u);
  }

  // Try recording one hint.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    static_cast<NavigationURLLoaderImpl*>(loader.get())
        ->OnAcceptCHFrameReceived(origin,
                                  {network::mojom::WebClientHintsType::kDpr},
                                  base::BindOnce([](int32_t) { return; }));
    auto ukm_entries = ukm_recorder.GetEntriesByName(
        ukm::builders::ClientHints_AcceptCHFrameUsage::kEntryName);
    ASSERT_EQ(ukm_entries.size(), 1u);
    EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                  ukm_entries[0],
                  ukm::builders::ClientHints_AcceptCHFrameUsage::kTypeName),
              static_cast<int64_t>(network::mojom::WebClientHintsType::kDpr));
  }

  // Try recording all hints.
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::vector<network::mojom::WebClientHintsType> accept_ch_frame;
    for (int64_t i = 0; i <= static_cast<int64_t>(
                                 network::mojom::WebClientHintsType::kMaxValue);
         ++i) {
      accept_ch_frame.push_back(
          static_cast<network::mojom::WebClientHintsType>(i));
    }
    static_cast<NavigationURLLoaderImpl*>(loader.get())
        ->OnAcceptCHFrameReceived(origin, accept_ch_frame,
                                  base::BindOnce([](int32_t) { return; }));
    auto ukm_entries = ukm_recorder.GetEntriesByName(
        ukm::builders::ClientHints_AcceptCHFrameUsage::kEntryName);
    // If you're here because the test is failing when you added a new client
    // hint be sure to increment the number below and add your new hint to the
    // enum WebClientHintsType in tools/metrics/histograms/enums.xml.
    ASSERT_EQ(ukm_entries.size(), 31u);
    for (int64_t i = 0; i <= static_cast<int64_t>(
                                 network::mojom::WebClientHintsType::kMaxValue);
         ++i) {
      EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                    ukm_entries[i],
                    ukm::builders::ClientHints_AcceptCHFrameUsage::kTypeName),
                i);
    }
  }
}

TEST_F(NavigationURLLoaderImplTest, AdTaggedNavigation) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL redirect_url = http_test_server_.GetURL("/foo");

  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(redirect_url, "", "GET", &delegate,
                                 blink::NavigationDownloadPolicy(),
                                 /*is_main_frame=*/true,
                                 /*upgrade_if_insecure=*/false,
                                 /*is_ad_tagged=*/true);
  loader->Start();
  delegate.WaitForResponseStarted();

  EXPECT_TRUE(loader->GetResourceRequestForTesting().is_ad_tagged);
}

TEST_F(NavigationURLLoaderImplTest, PopulatePermissionsPolicyOnRequest) {
  // TODO(crbug.com/382291442): Remove `scoped_feature_list` once launched.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPopulatePermissionsPolicyOnRequest);

  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(
      url,
      base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                         url.DeprecatedGetOriginAsURL().spec().c_str()),
      "GET", &delegate, blink::NavigationDownloadPolicy(),
      /*is_main_frame=*/true, /*upgrade_if_insecure=*/false);
  loader->Start();
  delegate.WaitForResponseStarted();

  EXPECT_EQ(loader->GetResourceRequestForTesting().permissions_policy,
            std::make_optional(
                *web_contents_->GetPrimaryMainFrame()->GetPermissionsPolicy()));
}

// TODO(crbug.com/382291442): Remove test once feature is launched.
TEST_F(NavigationURLLoaderImplTest,
       PopulatePermissionsPolicyOnRequest_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      network::features::kPopulatePermissionsPolicyOnRequest);

  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  TestNavigationURLLoaderDelegate delegate;
  auto loader = CreateTestLoader(
      url,
      base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                         url.DeprecatedGetOriginAsURL().spec().c_str()),
      "GET", &delegate, blink::NavigationDownloadPolicy(),
      /*is_main_frame=*/true, /*upgrade_if_insecure=*/false);
  loader->Start();
  delegate.WaitForResponseStarted();

  EXPECT_FALSE(loader->GetResourceRequestForTesting().permissions_policy);
}

}  // namespace content
