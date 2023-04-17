// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
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
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/test/url_loader_context_for_tests.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_owner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace content {

namespace {

class TestNavigationLoaderInterceptor : public NavigationLoaderInterceptor {
 public:
  explicit TestNavigationLoaderInterceptor(
      absl::optional<network::ResourceRequest>* most_recent_resource_request)
      : most_recent_resource_request_(most_recent_resource_request) {
    net::URLRequestContextBuilder url_request_context_builder;
    url_request_context_builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());
    url_request_context_ = url_request_context_builder.Build();
    url_loader_context_.set_url_request_context(url_request_context_.get());

    constexpr network::ResourceScheduler::ClientId kClientId(3);
    resource_scheduler_client_ =
        base::MakeRefCounted<network::ResourceSchedulerClient>(
            kClientId, network::IsBrowserInitiated(false), &resource_scheduler_,
            url_request_context_->network_quality_estimator());
    url_loader_context_.set_resource_scheduler_client(
        resource_scheduler_client_);

    url_loader_context_.mutable_factory_params().process_id =
        network::mojom::kBrowserProcessId;
    url_loader_context_.mutable_factory_params().is_corb_enabled = false;
  }

  ~TestNavigationLoaderInterceptor() override {
    url_loader_ = nullptr;
    resource_scheduler_client_ = nullptr;
  }

  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    std::move(callback).Run(
        base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
            base::BindOnce(&TestNavigationLoaderInterceptor::StartLoader,
                           base::Unretained(this))));
  }

  void StartLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    *most_recent_resource_request_ = resource_request;
    url_loader_ = std::make_unique<network::URLLoader>(
        url_loader_context_,
        base::BindOnce(&TestNavigationLoaderInterceptor::DeleteURLLoader,
                       base::Unretained(this)),
        std::move(receiver), 0 /* options */, resource_request,
        std::move(client), nullptr /* sync_url_loader_client */,
        TRAFFIC_ANNOTATION_FOR_TESTS, 0, /* request_id */
        0 /* keepalive_request_size */,
        nullptr /* keepalive_statistics_recorder */,
        nullptr /* trust_token_helper */,
        mojo::NullRemote() /* cookie_observer */,
        mojo::NullRemote() /* trust_token_observer */,
        mojo::NullRemote() /* url_loader_network_observer */,
        /*devtools_observer=*/mojo::NullRemote(),
        /*accept_ch_frame_observer=*/mojo::NullRemote(),
        /*third_party_cookies_enabled=*/true, net::CookieSettingOverrides(),
        /*cache_transparency_settings=*/nullptr,
        /*attribution_request_helper=*/nullptr);
  }

  bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors,
      bool* will_return_unsafe_redirect) override {
    return false;
  }

 private:
  void DeleteURLLoader(network::URLLoader* url_loader) {
    DCHECK_EQ(url_loader_.get(), url_loader);
    url_loader_.reset();
  }

  raw_ptr<absl::optional<network::ResourceRequest>>
      most_recent_resource_request_;  // NOT OWNED.
  network::ResourceScheduler resource_scheduler_;
  network::URLLoaderContextForTests url_loader_context_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<network::ResourceSchedulerClient> resource_scheduler_client_;
  std::unique_ptr<network::URLLoader> url_loader_;

  const network::cors::OriginAccessList kEmptyOriginAccessList;
};

}  // namespace

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
    rvh_test_enabler_.reset();
  }

  std::unique_ptr<NavigationURLLoader> CreateTestLoader(
      const GURL& url,
      const std::string& headers,
      const std::string& method,
      NavigationURLLoaderDelegate* delegate,
      blink::NavigationDownloadPolicy download_policy =
          blink::NavigationDownloadPolicy(),
      bool is_main_frame = true,
      bool upgrade_if_insecure = false) {
    // NavigationURLLoader assumes that the corresponding FrameTreeNode has an
    // associated NavigationRequest.
    pending_navigation_ = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://example.com"), web_contents_.get());
    pending_navigation_->Start();

    blink::mojom::BeginNavigationParamsPtr begin_params =
        blink::mojom::BeginNavigationParams::New(
            absl::nullopt /* initiator_frame_token */, headers,
            net::LOAD_NORMAL, false /* skip_service_worker */,
            blink::mojom::RequestContextType::LOCATION,
            blink::mojom::MixedContentContextType::kBlockable,
            false /* is_form_submission */,
            false /* was_initiated_by_link_click */,
            blink::mojom::ForceHistoryPush::kNo,
            GURL() /* searchable_form_url */,
            std::string() /* searchable_form_encoding */,
            GURL() /* client_side_redirect_url */,
            absl::nullopt /* devtools_initiator_info */,
            nullptr /* trust_token_params */, absl::nullopt /* impression */,
            base::TimeTicks() /* renderer_before_unload_start */,
            base::TimeTicks() /* renderer_before_unload_end */,
            absl::nullopt /* web_bundle_token */,
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kDidNotStartWithTransientActivation,
            false /* is_container_initiated */,
            false /* is_fullscreen_requested */,
            false /* has_storage_access */);

    auto common_params = blink::CreateCommonNavigationParams();
    common_params->url = url;
    common_params->initiator_origin = url::Origin::Create(url);
    common_params->method = method;
    common_params->download_policy = download_policy;
    common_params->request_destination =
        network::mojom::RequestDestination::kDocument;
    url::Origin origin = url::Origin::Create(url);

    uint32_t frame_tree_node_id =
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
            net::HttpRequestHeaders() /* cors_exempt_headers */,
            nullptr /* client_security_state */,
            absl::nullopt /* devtools_accepted_stream_types */,
            false /* is_pdf */,
            content::WeakDocumentPtr() /* initiator_document */,
            GlobalRenderFrameHostId() /* previous_render_frame_host_id */,
            false /* allow_cookies_from_browser */));
    std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
    most_recent_resource_request_ = absl::nullopt;
    interceptors.push_back(std::make_unique<TestNavigationLoaderInterceptor>(
        &most_recent_resource_request_));

    return std::make_unique<NavigationURLLoaderImpl>(
        browser_context_.get(), browser_context_->GetDefaultStoragePartition(),
        std::move(request_info), nullptr /* navigation_ui_data */,
        nullptr /* service_worker_handle */,
        nullptr /* prefetched_signed_exchange_cache */, delegate,
        mojo::NullRemote() /* cookie_access_obsever */,
        mojo::NullRemote() /* trust_token_observer */,
        mojo::NullRemote() /* url_loader_network_observer */,
        /*devtools_observer=*/mojo::NullRemote(), std::move(interceptors));
  }

  // Requests |redirect_url|, which must return a HTTP 3xx redirect. It's also
  // used as the initial origin.
  // |request_method| is the method to use for the initial request.
  // |expected_redirect_method| is the method that is expected to be used for
  // the second request, after redirection.
  // |expected_origin_value| is the expected value for the Origin header after
  // redirection. If empty, expects that there will be no Origin header.
  void HTTPRedirectOriginHeaderTest(const GURL& redirect_url,
                                    const std::string& request_method,
                                    const std::string& expected_redirect_method,
                                    const std::string& expected_origin_value,
                                    bool expect_request_fail = false) {
    TestNavigationURLLoaderDelegate delegate;
    std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
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
    ASSERT_TRUE(most_recent_resource_request_);

    // Note that there is no check for request success here because, for
    // purposes of testing, the request very well may fail. For example, if the
    // test redirects to an HTTPS server from an HTTP origin, thus it is cross
    // origin, there is not an HTTPS server in this unit test framework, so the
    // request would fail. However, that's fine, as long as the request headers
    // are in order and pass the checks below.
    if (expected_origin_value.empty()) {
      EXPECT_FALSE(most_recent_resource_request_->headers.HasHeader(
          net::HttpRequestHeaders::kOrigin));
    } else {
      std::string origin_header;
      EXPECT_TRUE(most_recent_resource_request_->headers.GetHeader(
          net::HttpRequestHeaders::kOrigin, &origin_header));
      EXPECT_EQ(expected_origin_value, origin_header);
    }
  }

  net::RedirectInfo NavigateAndReturnRedirectInfo(const GURL& url,
                                                  bool upgrade_if_insecure,
                                                  bool expect_request_fail) {
    TestNavigationURLLoaderDelegate delegate;
    std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
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
  std::unique_ptr<BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  net::EmbeddedTestServer http_test_server_;
  absl::optional<network::ResourceRequest> most_recent_resource_request_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<TestWebContents> web_contents_;
  // NavigationURLLoaderImpl relies on the existence of the
  // |frame_tree_node->navigation_request()|.
  std::unique_ptr<NavigationSimulator> pending_navigation_;
};

TEST_F(NavigationURLLoaderImplTest, IsolationInfoOfMainFrameNavigation) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
      url,
      base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                         url.DeprecatedGetOriginAsURL().spec().c_str()),
      "GET", &delegate, blink::NavigationDownloadPolicy(),
      true /*is_main_frame*/, false /*upgrade_if_insecure*/);
  loader->Start();
  delegate.WaitForResponseStarted();

  ASSERT_TRUE(most_recent_resource_request_);
  ASSERT_TRUE(most_recent_resource_request_->trusted_params);
  EXPECT_TRUE(
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 origin, origin,
                                 net::SiteForCookies::FromOrigin(origin))
          .IsEqualForTesting(
              most_recent_resource_request_->trusted_params->isolation_info));
}

TEST_F(NavigationURLLoaderImplTest,
       IsolationInfoOfRedirectedMainFrameNavigation) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL final_url = http_test_server_.GetURL("/echo");
  const url::Origin origin = url::Origin::Create(url);

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());

  ASSERT_TRUE(most_recent_resource_request_->trusted_params);
  EXPECT_TRUE(
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 origin, origin,
                                 net::SiteForCookies::FromOrigin(origin))
          .IsEqualForTesting(
              most_recent_resource_request_->trusted_params->isolation_info));
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

TEST_F(NavigationURLLoaderImplTest, RedirectModifiedHeaders) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
      redirect_url, "Header1: Value1\r\nHeader2: Value2", "GET", &delegate);
  loader->Start();
  delegate.WaitForRequestRedirected();

  ASSERT_TRUE(most_recent_resource_request_);

  // Initial request should only have initial headers.
  std::string header1, header2;
  EXPECT_TRUE(
      most_recent_resource_request_->headers.GetHeader("Header1", &header1));
  EXPECT_EQ("Value1", header1);
  EXPECT_TRUE(
      most_recent_resource_request_->headers.GetHeader("Header2", &header2));
  EXPECT_EQ("Value2", header2);
  EXPECT_FALSE(most_recent_resource_request_->headers.HasHeader("Header3"));

  // Overwrite Header2 and add Header3.
  net::HttpRequestHeaders redirect_headers;
  redirect_headers.SetHeader("Header2", "");
  redirect_headers.SetHeader("Header3", "Value3");
  loader->FollowRedirect({}, redirect_headers, {});
  delegate.WaitForResponseStarted();

  // Redirected request should also have modified headers.
  EXPECT_TRUE(
      most_recent_resource_request_->headers.GetHeader("Header1", &header1));
  EXPECT_EQ("Value1", header1);
  EXPECT_TRUE(
      most_recent_resource_request_->headers.GetHeader("Header2", &header2));
  EXPECT_EQ("", header2);
  std::string header3;
  EXPECT_TRUE(
      most_recent_resource_request_->headers.GetHeader("Header3", &header3));
  EXPECT_EQ("Value3", header3);
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
  std::unique_ptr<NavigationURLLoader> loader =
      CreateTestLoader(url, std::string(), "GET", &delegate);
  loader->Start();
  loader->SetNavigationTimeout(base::Seconds(3));
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_TIMED_OUT, delegate.net_error());
}

// Like NavigationTimeoutTest but the navigation initially results in a redirect
// before hanging, to test a slightly more complicated navigation.
// TODO(crbug.com/1271228): Flaky on Linux.
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
  std::unique_ptr<NavigationURLLoader> loader =
      CreateTestLoader(redirect_url, std::string(), "GET", &delegate);
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
  std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
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
    ASSERT_EQ(ukm_entries.size(), 29u);
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

}  // namespace content
