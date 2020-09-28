// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "ipc/ipc_message.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_owner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestNavigationLoaderInterceptor : public NavigationLoaderInterceptor {
 public:
  explicit TestNavigationLoaderInterceptor(
      base::Optional<network::ResourceRequest>* most_recent_resource_request)
      : most_recent_resource_request_(most_recent_resource_request) {
    net::URLRequestContextBuilder context_builder;
    context_builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());
    context_ = context_builder.Build();
    constexpr int child_id = 4;
    constexpr int route_id = 8;
    resource_scheduler_client_ =
        base::MakeRefCounted<network::ResourceSchedulerClient>(
            child_id, route_id, &resource_scheduler_,
            context_->network_quality_estimator());
  }

  ~TestNavigationLoaderInterceptor() override {
    url_loader_ = nullptr;
    resource_scheduler_client_ = nullptr;
  }

  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    std::move(callback).Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(
        base::BindOnce(&TestNavigationLoaderInterceptor::StartLoader,
                       base::Unretained(this))));
  }

  void StartLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    *most_recent_resource_request_ = resource_request;
    static network::mojom::URLLoaderFactoryParams params;
    params.process_id = network::mojom::kBrowserProcessId;
    params.is_corb_enabled = false;
    url_loader_ = std::make_unique<network::URLLoader>(
        context_.get(), nullptr /* network_service_client */,
        nullptr /* network_context_client */,
        base::BindOnce(&TestNavigationLoaderInterceptor::DeleteURLLoader,
                       base::Unretained(this)),
        std::move(receiver), 0 /* options */, resource_request,
        std::move(client), /*reponse_body_use_tracker=*/base::nullopt,
        TRAFFIC_ANNOTATION_FOR_TESTS, &params,
        /*coep_reporter=*/nullptr, 0, /* request_id */
        0 /* keepalive_request_size */, resource_scheduler_client_,
        nullptr /* keepalive_statistics_recorder */,
        nullptr /* network_usage_accumulator */, nullptr /* header_client */,
        nullptr /* origin_policy_manager */, nullptr /* trust_token_helper */,
        mojo::NullRemote() /* cookie_observer */);
  }

  bool MaybeCreateLoaderForResponse(
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
  void DeleteURLLoader(network::mojom::URLLoader* url_loader) {
    DCHECK_EQ(url_loader_.get(), url_loader);
    url_loader_.reset();
  }

  base::Optional<network::ResourceRequest>*
      most_recent_resource_request_;  // NOT OWNED.
  network::ResourceScheduler resource_scheduler_;
  std::unique_ptr<net::URLRequestContext> context_;
  scoped_refptr<network::ResourceSchedulerClient> resource_scheduler_client_;
  std::unique_ptr<network::URLLoader> url_loader_;
};

}  // namespace

class NavigationURLLoaderImplTest : public testing::Test {
 public:
  NavigationURLLoaderImplTest()
      : task_environment_(std::make_unique<BrowserTaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::IO)),
        network_change_notifier_(
            net::test::MockNetworkChangeNotifier::Create()) {
    // Because the network service is enabled we need a system Connector or
    // BrowserContext::GetDefaultStoragePartition will segfault when
    // ContentBrowserClient::CreateNetworkContext tries to call
    // GetNetworkService.
    mojo::PendingReceiver<service_manager::mojom::Connector> connector_receiver;
    SetSystemConnectorForTesting(
        service_manager::Connector::Create(&connector_receiver));

    browser_context_.reset(new TestBrowserContext);
    http_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));

#if BUILDFLAG(ENABLE_PLUGINS)
    PluginService::GetInstance()->Init();
#endif
  }

  ~NavigationURLLoaderImplTest() override {
    browser_context_.reset();
    SetSystemConnectorForTesting(nullptr);
    // Reset the BrowserTaskEnvironment to force destruction of the local
    // NetworkService, which is held in SequenceLocalStorage. This must happen
    // before destruction of |network_change_notifier_|, to allow observers to
    // be unregistered.
    task_environment_.reset();
  }

  std::unique_ptr<NavigationURLLoader> CreateTestLoader(
      const GURL& url,
      const std::string& headers,
      const std::string& method,
      NavigationURLLoaderDelegate* delegate,
      NavigationDownloadPolicy download_policy = NavigationDownloadPolicy(),
      bool is_main_frame = true,
      bool upgrade_if_insecure = false) {
    mojom::BeginNavigationParamsPtr begin_params =
        mojom::BeginNavigationParams::New(
            MSG_ROUTING_NONE /* initiator_routing_id */, headers,
            net::LOAD_NORMAL, false /* skip_service_worker */,
            blink::mojom::RequestContextType::LOCATION,
            network::mojom::RequestDestination::kDocument,
            blink::WebMixedContentContextType::kBlockable,
            false /* is_form_submission */,
            false /* was_initiated_by_link_click */,
            GURL() /* searchable_form_url */,
            std::string() /* searchable_form_encoding */,
            GURL() /* client_side_redirect_url */,
            base::nullopt /* devtools_initiator_info */,
            false /* attach_same_site_cookie */,
            nullptr /* trust_token_params */, base::nullopt /* impression */,
            base::TimeTicks() /* renderer_before_unload_start */,
            base::TimeTicks() /* renderer_before_unload_end */);

    auto common_params = CreateCommonNavigationParams();
    common_params->url = url;
    common_params->initiator_origin = url::Origin::Create(url);
    common_params->method = method;
    common_params->download_policy = download_policy;
    url::Origin origin = url::Origin::Create(url);

    std::unique_ptr<NavigationRequestInfo> request_info(
        new NavigationRequestInfo(
            std::move(common_params), std::move(begin_params),
            net::IsolationInfo::Create(
                net::IsolationInfo::RedirectMode::kUpdateTopFrame, origin,
                origin, net::SiteForCookies::FromUrl(url)),
            is_main_frame, false /* parent_is_main_frame */,
            false /* are_ancestors_secure */, -1 /* frame_tree_node_id */,
            false /* is_for_guests_only */, false /* report_raw_headers */,
            false /* is_prerenering */,
            upgrade_if_insecure /* upgrade_if_insecure */,
            nullptr /* blob_url_loader_factory */,
            base::UnguessableToken::Create() /* devtools_navigation_token */,
            base::UnguessableToken::Create() /* devtools_frame_token */,
            false /* obey_origin_policy */, {} /* cors_exempt_headers */,
            nullptr /* client_security_state */));
    std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors;
    most_recent_resource_request_ = base::nullopt;
    interceptors.push_back(std::make_unique<TestNavigationLoaderInterceptor>(
        &most_recent_resource_request_));

    return std::make_unique<NavigationURLLoaderImpl>(
        browser_context_.get(),
        BrowserContext::GetDefaultStoragePartition(browser_context_.get()),
        std::move(request_info), nullptr /* navigation_ui_data */,
        nullptr /* service_worker_handle */, nullptr /* appcache_handle */,
        nullptr /* prefetched_signed_exchange_cache */, delegate,
        mojo::NullRemote() /* cookie_access_obsever */,
        std::move(interceptors));
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
        base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                           redirect_url.GetOrigin().spec().c_str()),
        request_method, &delegate);
    delegate.WaitForRequestRedirected();
    loader->FollowRedirect({}, {}, {}, blink::PreviewsTypes::PREVIEWS_OFF);

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

  net::RequestPriority NavigateAndReturnRequestPriority(const GURL& url,
                                                        bool is_main_frame) {
    TestNavigationURLLoaderDelegate delegate;
    base::test::ScopedFeatureList scoped_feature_list_;

    scoped_feature_list_.InitAndEnableFeature(features::kLowPriorityIframes);

    std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
        url,
        base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                           url.GetOrigin().spec().c_str()),
        "GET", &delegate, NavigationDownloadPolicy(), is_main_frame);
    delegate.WaitForRequestRedirected();
    loader->FollowRedirect({}, {}, {}, blink::PreviewsTypes::PREVIEWS_OFF);
    delegate.WaitForResponseStarted();

    return most_recent_resource_request_.value().priority;
  }

  net::RedirectInfo NavigateAndReturnRedirectInfo(const GURL& url,
                                                  bool upgrade_if_insecure,
                                                  bool expect_request_fail) {
    TestNavigationURLLoaderDelegate delegate;
    std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
        url,
        base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                           url.GetOrigin().spec().c_str()),
        "GET", &delegate, NavigationDownloadPolicy(), true /*is_main_frame*/,
        upgrade_if_insecure);
    delegate.WaitForRequestRedirected();
    loader->FollowRedirect({}, {}, {}, blink::PreviewsTypes::PREVIEWS_OFF);
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
  base::Optional<network::ResourceRequest> most_recent_resource_request_;
};

TEST_F(NavigationURLLoaderImplTest, RequestPriority) {
  ASSERT_TRUE(http_test_server_.Start());
  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");

  EXPECT_EQ(net::HIGHEST,
            NavigateAndReturnRequestPriority(url, true /* is_main_frame */));
  EXPECT_EQ(net::LOWEST,
            NavigateAndReturnRequestPriority(url, false /* is_main_frame */));
}

TEST_F(NavigationURLLoaderImplTest, IsolationInfoOfMainFrameNavigation) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/foo");
  const url::Origin origin = url::Origin::Create(url);

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
      url,
      base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                         url.GetOrigin().spec().c_str()),
      "GET", &delegate, NavigationDownloadPolicy(), true /*is_main_frame*/,
      false /*upgrade_if_insecure*/);
  delegate.WaitForRequestStarted();

  ASSERT_TRUE(most_recent_resource_request_);
  ASSERT_TRUE(most_recent_resource_request_->trusted_params);
  EXPECT_TRUE(
      net::IsolationInfo::Create(
          net::IsolationInfo::RedirectMode::kUpdateTopFrame, origin, origin,
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());

  ASSERT_TRUE(most_recent_resource_request_->trusted_params);
  EXPECT_TRUE(
      net::IsolationInfo::Create(
          net::IsolationInfo::RedirectMode::kUpdateTopFrame, origin, origin,
          net::SiteForCookies::FromOrigin(origin))
          .IsEqualForTesting(
              most_recent_resource_request_->trusted_params->isolation_info));
}

TEST_F(NavigationURLLoaderImplTest, Redirect301Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect301-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "POST", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null",
                               true);
}

TEST_F(NavigationURLLoaderImplTest, Redirect308Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect308-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect308-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "POST", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null",
                               true);
}

TEST_F(NavigationURLLoaderImplTest, RedirectModifiedHeaders) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL redirect_url = http_test_server_.GetURL("/redirect301-to-echo");

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
      redirect_url, "Header1: Value1\r\nHeader2: Value2", "GET", &delegate);
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
  loader->FollowRedirect({}, redirect_headers, {},
                         blink::PreviewsTypes::PREVIEWS_OFF);
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

}  // namespace content
