// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/loader/prefetch_browsertest_base.h"
#include "content/browser/loader/prefetch_url_loader_service_context.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
#include "components/unexportable_keys/features.h"
#endif

namespace content {

namespace {

enum class SplitCacheTestCase {
  kDisabled,
  kEnabledTripleKeyed,
  kEnabledTriplePlusCredsBool,
};

const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCredsBool,
     net::features::kSplitCacheByIncludeCredentials}};

}  // namespace

class PrefetchBrowserTest
    : public PrefetchBrowserTestBase,
      public testing::WithParamInterface<SplitCacheTestCase> {
 public:
  PrefetchBrowserTest()
      : cross_origin_server_(std::make_unique<net::EmbeddedTestServer>()),
        split_cache_test_case_(GetParam()),
        split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsSplitCacheEnabled()) {
      enabled_features.emplace_back(
          net::features::kSplitCacheByNetworkIsolationKey,
          base::FieldTrialParams());
    } else {
      disabled_features.emplace_back(
          net::features::kSplitCacheByNetworkIsolationKey);
    }
    enabled_features.emplace_back(net::features::kHttpCacheNoVarySearch,
                                  base::FieldTrialParams());
    enabled_features.emplace_back(
        blink::features::kRestrictLinkHeaderOnSubresource,
        base::FieldTrialParams{{"disable_resource_load", "false"}});
    split_cache_enabled_feature_list_.InitWithFeaturesAndParameters(
        enabled_features, disabled_features);
  }

  PrefetchBrowserTest(const PrefetchBrowserTest&) = delete;
  PrefetchBrowserTest& operator=(const PrefetchBrowserTest&) = delete;

  ~PrefetchBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PrefetchBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsSplitCacheEnabled() const {
    return split_cache_test_case_ != SplitCacheTestCase::kDisabled;
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;
  const SplitCacheTestCase split_cache_test_case_;

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList split_cache_enabled_feature_list_;
};

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginDocumentHasNoSameSiteCookies) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  URLLoaderMonitor monitor({cross_origin_target_url});

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(cross_origin_target_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->site_for_cookies.IsNull());
  ASSERT_TRUE(request->trusted_params);
  url::Origin cross_origin = url::Origin::Create(cross_origin_target_url);
  EXPECT_TRUE(net::IsolationInfo::Create(
                  net::IsolationInfo::RequestType::kMainFrame, cross_origin,
                  cross_origin, net::SiteForCookies())
                  .IsEqualForTesting(request->trusted_params->isolation_info));
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginDocumentReusedAsNavigation) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(target_path,
                   ResponseEntry("<head><title>Prefetch Target</title></head>",
                                 // The empty content type prevents this
                                 // response from being blocked by ORB.
                                 /*content_types=*/""));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the cross-origin target URL shouldn't hit the
  // network, and should be loaded from cache.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "Prefetch Target");
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginDocumentFromOpaqueOrigin) {
  // Prefetching as=document from a data: URL does not crash the renderer.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("data:text/html,<title>Data URL Prefetch Target</title><link "
           "rel=prefetch as=document href=https://google.com>")));
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(
    PrefetchBrowserTest,
    DISABLED_CrossOriginDocumentNotReusedAsNestedFrameNavigation) {
  // TODO(crbug.com/40093267): Remove this early-return when SplitCache is
  // enabled by default.
  if (!IsSplitCacheEnabled()) {
    GTEST_SKIP() << "This test is relevant only with SplitCache.";
  }
  const char* prefetch_path = "/prefetch.html";
  const char* host_path = "/host.html";
  const char* iframe_path = "/iframe.html";
  RegisterResponse(
      host_path,
      ResponseEntry(base::StringPrintf(
          "<head><title>Cross-Origin Host</title></head><body><iframe "
          "onload='document.title=\"Host Loaded\"' src='%s'></iframe></body>",
          iframe_path)));
  RegisterResponse(iframe_path, ResponseEntry("<h1>I am an iframe</h1>"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_iframe_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), iframe_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_host_url =
      cross_origin_server_->GetURL("3p.example", host_path);
  const GURL cross_origin_iframe_url =
      cross_origin_server_->GetURL("3p.example", iframe_path);
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          cross_origin_iframe_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_iframe_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin iframe URL increments its
  // counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_iframe_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the cross-origin host site will trigger an iframe
  // load which will not reuse the iframe that was prefetched from
  // |prefetch_path|. This is because cross-origin document prefetches must
  // only be reused for top-level navigations, and cannot be reused as
  // cross-origin iframes.
  NavigateToURLAndWaitTitle(cross_origin_host_url, "Host Loaded");
  EXPECT_EQ(2, cross_origin_iframe_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSubresourceNotReused) {
  // TODO(crbug.com/40093267): Remove this early-return when SplitCache is
  // enabled by default.
  if (!IsSplitCacheEnabled()) {
    GTEST_SKIP() << "This test is relevant only with SplitCache.";
  }
  const char* prefetch_path = "/prefetch.html";
  const char* host_path = "/host.html";
  const char* subresource_path = "/subresource.js";
  RegisterResponse(
      host_path,
      ResponseEntry(base::StringPrintf(
          "<head><title>Cross-Origin Host</title></head><body><script src='%s' "
          "onload='document.title=\"Host Loaded\"'></script></body>",
          subresource_path)));
  RegisterResponse(subresource_path, ResponseEntry("console.log('I loaded')"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_subresource_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), subresource_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_host_url =
      cross_origin_server_->GetURL("3p.example", host_path);
  const GURL cross_origin_subresource_url =
      cross_origin_server_->GetURL("3p.example", subresource_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin subresource URL
  // increments its counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the cross-origin host attempting to reuse the
  // resource that was prefetched results in the request hitting the network.
  // This is because cross-origin subresources must only be reused within the
  // frame they were fetched from.
  NavigateToURLAndWaitTitle(cross_origin_host_url, "Host Loaded");
  EXPECT_EQ(2, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSubresourceReusedByCurrentFrame) {
  const char* prefetch_path = "/prefetch.html";
  const char* use_prefetch_path = "/use-prefetch.html";
  const char* subresource_path = "/subresource.js";
  RegisterResponse(subresource_path, ResponseEntry("console.log('I loaded')"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_subresource_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), subresource_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_subresource_url =
      cross_origin_server_->GetURL("3p.example", subresource_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterResponse(use_prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><script src='%s' onload='document.title=\"Use "
                       "Prefetch Loaded\"'></script></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin subresource URL
  // increments its counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shut down the cross-origin server.
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the same-origin document that attempts to reuse
  // the cross-origin prefetch is able to reuse the resource from the cache.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(use_prefetch_path),
                            "Use Prefetch Loaded");

  // Shutdown the same-origin server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

// This tests more of an implementation detail than anything. A single resource
// must be committed to the cache partition corresponding to a single
// NetworkAnonymizationKey. This means that even though it is considered "safe"
// to reused cross-origin subresource prefetches for top-level navigations, we
// can't actually do this, because the subresource is only reusable from the
// frame that fetched it.
// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSubresourceNotReusedAsNavigation) {
  // TODO(crbug.com/40093267): Remove this early-return when SplitCache is
  // enabled by default.
  if (!IsSplitCacheEnabled()) {
    GTEST_SKIP() << "This test is relevant only with SplitCache.";
  }
  const char* prefetch_path = "/prefetch.html";
  const char* subresource_path = "/subresource.js";
  RegisterResponse(subresource_path, ResponseEntry("console.log('I loaded');"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_subresource_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), subresource_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_subresource_url =
      cross_origin_server_->GetURL("3p.example", subresource_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin subresource URL
  // increments its counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the same-origin server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  // Subsequent navigation to the cross-origin subresource itself will not be
  // reused from the cache, because the cached resource is not partitioned under
  // the cross-origin it is served from.
  EXPECT_TRUE(NavigateToURL(shell(), cross_origin_subresource_url));
  EXPECT_EQ(2, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the cross-origin server.
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, Simple) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>", "text/html",
                    {{"cache-control", "public, max-age=3600"}}));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, DoublePrefetch) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      target_path, target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>", "text/html",
                    {{"cache-control", "public, max-age=3600"}}));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|, but it should hit only once.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, NoCacheAndNoStore) {
  const char* prefetch_path = "/prefetch.html";
  const char* nocache_path = "/target1.html";
  const char* nostore_path = "/target2.html";

  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body>"
                                      "<link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      nocache_path, nostore_path)));
  RegisterResponse(nocache_path,
                   ResponseEntry("<head><title>NoCache Target</title></head>",
                                 "text/html", {{"cache-control", "no-cache"}}));
  RegisterResponse(nostore_path,
                   ResponseEntry("<head><title>NoStore Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop nocache_waiter;
  base::RunLoop nostore_waiter;
  auto nocache_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), nocache_path, &nocache_waiter);
  auto nostore_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), nostore_path, &nostore_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment the
  // fetch count for the both targets.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  nocache_waiter.Run();
  nostore_waiter.Run();
  EXPECT_EQ(1, nocache_request_counter->GetRequestCount());
  EXPECT_EQ(1, nostore_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the no-cache URL do hit the network, because
  // prefetch respects cache semantics.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nocache_path),
                            "NoCache Target");
  EXPECT_EQ(2, nocache_request_counter->GetRequestCount());

  // Subsequent navigation to the no-store URL hit the network again, for the
  // same reason.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nostore_path),
                            "NoStore Target");
  EXPECT_EQ(2, nostore_request_counter->GetRequestCount());

  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"cache-control", "public, max-age=600"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), preload_path, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(embedded_test_server()->GetURL(preload_path));

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  NavigateToURLAndWaitTitle(target_url, "done");
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithPreloadHasNoSameSiteCookies) {
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='anonymous'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  URLLoaderMonitor monitor({cross_origin_target_url});

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  GURL cross_origin_preload_url =
      cross_origin_server_->GetURL("3p.example", preload_path);
  WaitUntilLoaded(cross_origin_preload_url);

  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(cross_origin_target_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->site_for_cookies.IsNull());
  ASSERT_TRUE(request->trusted_params);
  url::Origin cross_origin = url::Origin::Create(cross_origin_target_url);
  EXPECT_TRUE(net::IsolationInfo::Create(
                  net::IsolationInfo::RequestType::kMainFrame, cross_origin,
                  cross_origin, net::SiteForCookies())
                  .IsEqualForTesting(request->trusted_params->isolation_info));
}

// Variants of this test:
// - PrefetchBrowserTest.CrossOriginWithPreloadAnonymous
// - PrefetchBrowserTest.CrossOriginWithPreloadCredentialled
// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithPreloadAnonymous) {
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  base::RunLoop preload_waiter_second_request;
  auto preload_request_counter_second_request =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), preload_path,
                                       &preload_waiter_second_request);

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='anonymous'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  GURL cross_origin_preload_url =
      cross_origin_server_->GetURL("3p.example", preload_path);
  WaitUntilLoaded(cross_origin_preload_url);

  // When SplitCache is enabled and the prefetch resource and its headers are
  // fetched with a modified IsolationInfo, the preload header resource must
  // not be reusable by any other origin but its parent prefetch's.
  // TODO(crbug.com/40093267): When SplitCache is enabled by default, get rid of
  // the below conditional.
  if (IsSplitCacheEnabled()) {
    // Spin up another server, hosting a page with a preload header identical to
    // the one in |target_path|.
    const char* reuse_preload_attempt_path = "/reuse.html";
    RegisterResponse(
        reuse_preload_attempt_path,
        ResponseEntry(
            base::StringPrintf("<head><title>Other site</title><script "
                               "src='%s'></script></head>",
                               cross_origin_preload_url.spec().c_str()),
            "text/html",
            {{"link",
              base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                 cross_origin_preload_url.spec().c_str())},
             {"access-control-allow-origin", "*"}}));
    std::unique_ptr<net::EmbeddedTestServer> other_cross_origin_server =
        std::make_unique<net::EmbeddedTestServer>();
    RegisterRequestHandler(other_cross_origin_server.get());

    ASSERT_TRUE(other_cross_origin_server->Start());

    // Navigate to a page on the above-created server. A request for the same
    // preload header fetched earlier must not be reusable, and must hit the
    // network.
    EXPECT_TRUE(NavigateToURL(
        shell(), other_cross_origin_server->GetURL(
                     "other3p.example", reuse_preload_attempt_path)));
    preload_waiter_second_request.Run();
    EXPECT_EQ(2, preload_request_counter_second_request->GetRequestCount());

    // We won't need this server again.
    EXPECT_TRUE(other_cross_origin_server->ShutdownAndWaitUntilComplete());
  }

  if (split_cache_test_case_ ==
      SplitCacheTestCase::kEnabledTriplePlusCredsBool) {
    // The navigation is requested with credentials, but the prefetch is
    // requested anonymously. As a result of "SplitCacheByIncludeCredentials",
    // those aren't considered the same for the HTTP cache. Early return.
    // See the variant of this test in:
    // PrefetchBrowserTest.CrossOriginWithPreloadCredentialled
    return;
  }

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  EXPECT_TRUE(ExecJs(shell()->web_contents(), "document.title = 'not done';"));

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
}

// Regression test for crbug.com/357325599 - If a Link header with
// rel="preload" has as="document" (which is invalid), we shouldn't attempt to
// treat this as a rel="prefetch" as="document" and instead should just ignore
// the header.
// TODO(crbug.com/40256279): De-flake and enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithInvalidPreloadAsDocument) {
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"document\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path);
  RegisterRequestHandler(cross_origin_server_.get());
  base::RunLoop preload_waiter_second_request;
  auto preload_request_counter_second_request =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), preload_path,
                                       &preload_waiter_second_request);

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='anonymous'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment
  // `target_request_counter` but not `preload_request_counter` because the
  // preload header should be ignored.
  base::test::TestFuture<void> prefetch_future;
  RegisterPrefetchLoaderCallback(prefetch_future.GetCallback());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  EXPECT_TRUE(prefetch_future.Wait());
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());
  EXPECT_EQ(0, preload_request_counter->GetRequestCount());

  // Subsequent navigation to the target URL should result in the preloaded JS
  // being served from the network.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
}

// Variants of this test:
// - PrefetchBrowserTest.CrossOriginWithPreloadAnonymous
// - PrefetchBrowserTest.CrossOriginWithPreloadCredentialled
// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithPreloadCredentialled) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  const auto port = embedded_test_server()->port();
  const char target_path[] = "/target.html";
  const char preload_path[] = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {
                        {
                            "link",
                            "</preload.js>;rel=\"preload\";as=\"script\"",
                        },
                        {
                            "Access-Control-Allow-Origin",
                            "http://prefetch.com:" + base::NumberToString(port),
                        },
                        {
                            "Access-Control-Allow-Credentials",
                            "true",
                        },
                    }));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  base::RunLoop preload_waiter_second_request;
  auto preload_request_counter_second_request =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), preload_path,
                                       &preload_waiter_second_request);

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='use-credentials'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("prefetch.com", prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  GURL cross_origin_preload_url =
      cross_origin_server_->GetURL("3p.example", preload_path);
  WaitUntilLoaded(cross_origin_preload_url);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, SignedExchangeWithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry(base::StrCat({MockSignedExchangeHandler::kMockSxgPrefix,
                                  "<head><title>Prefetch Target (SXG)</title>"
                                  "<script src=\"./preload.js\"></script>"
                                  "</head>"}),
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  auto target_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_sxg_path, &prefetch_waiter);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), preload_path_in_sxg, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL preload_url_in_sxg =
      embedded_test_server()->GetURL(preload_path_in_sxg);
  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(embedded_test_server()->GetURL(target_path)), "text/html",
      {{"Link", base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                   preload_url_in_sxg.spec().c_str())}},
      net::SHA256HashValue({{0x00}}))});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());

  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

// TODO(crbug.com/40256279): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSignedExchangeWithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry(base::StrCat({MockSignedExchangeHandler::kMockSxgPrefix,
                                  "<head><title>Prefetch Target (SXG)</title>"
                                  "<script src=\"./preload.js\"></script>"
                                  "</head>"}),
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  auto target_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_sxg_path, &prefetch_waiter);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path_in_sxg, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL target_sxg_url =
      cross_origin_server_->GetURL("3p.example", target_sxg_path);
  const GURL preload_url_in_sxg =
      cross_origin_server_->GetURL("3p.example", preload_path_in_sxg);

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          target_sxg_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(cross_origin_server_->GetURL("3p.example", target_path)),
      "text/html",
      {{"Link", base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                   preload_url_in_sxg.spec().c_str())}},
      net::SHA256HashValue({{0x00}}))});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());

  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());

  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(preload_url_in_sxg);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, FileToHttp) {
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>",
                    // The empty content type prevents this
                    // response from being blocked by ORB.
                    /*content_types=*/"",
                    {{"cache-control", "public, max-age=31536000"}}));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.html");
    std::string file_content = base::StringPrintf(
        "<body><link rel='prefetch' as='document' href='%s'></body>",
        target_url.spec().c_str());
    ASSERT_TRUE(base::WriteFile(file_path, file_content));

    // Loading a page that prefetches the target URL would increment the
    // |request_counter|.
    GURL file_url = net::FilePathToFileURL(file_path);
    EXPECT_TRUE(NavigateToURL(shell(), file_url));
    prefetch_waiter.Run();
    EXPECT_EQ(1, request_counter->GetRequestCount());
    EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());
  }

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

class PrefetchLoadFlagsBrowserTest : public PrefetchBrowserTestBase {
 public:
  PrefetchLoadFlagsBrowserTest()
      : cross_origin_server_(std::make_unique<net::EmbeddedTestServer>()) {}

  void SetUpOnMainThread() override {
    PrefetchBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  // Binds a remote to the browser-side `SubresourceProxyingURLLoaderService`
  // for the current main frame, mirroring the binding that the renderer would
  // receive at navigation commit time.
  mojo::Remote<network::mojom::URLLoaderFactory> BindFactoryForMainFrame() {
    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
    StoragePartitionImpl* partition =
        static_cast<StoragePartitionImpl*>(rfh->GetStoragePartition());
    mojo::Remote<network::mojom::URLLoaderFactory> remote;
    partition->GetSubresourceProxyingURLLoaderService()->GetFactory(
        remote.BindNewPipeAndPassReceiver(), rfh->GetFrameTreeNodeId(),
        proxied_factory_.GetSafeWeakWrapper(), rfh->GetWeakPtr(),
        /*prefetched_signed_exchange_cache=*/nullptr);
    return remote;
  }

  network::ResourceRequest CreateCrossOriginPrefetchRequest(
      const GURL& target_url) {
    network::ResourceRequest request;
    request.url = target_url;
    request.load_flags =
        net::LOAD_PREFETCH | net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
    request.request_initiator = shell()
                                    ->web_contents()
                                    ->GetPrimaryMainFrame()
                                    ->GetLastCommittedOrigin();
    return request;
  }

  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;
  network::TestURLLoaderFactory proxied_factory_;
};

// Verifies that a cross-origin prefetch request from the renderer is rejected
// if it carries load flags that are not permitted on requests originating from
// an untrusted process. The request reaches the network service via a trusted
// loader factory, so the browser must validate the flags before forwarding.
IN_PROC_BROWSER_TEST_F(PrefetchLoadFlagsBrowserTest,
                       CrossOriginPrefetchRejectsRestrictedLoadFlags) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterResponse(target_path,
                   ResponseEntry("<head><title>Target</title></head>",
                                 /*content_types=*/""));
  RegisterRequestHandler(embedded_test_server());
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(cross_origin_server_->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  const GURL target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  for (int restricted_flag :
       {net::LOAD_BYPASS_PROXY, net::LOAD_DISABLE_CERT_NETWORK_FETCHES}) {
    SCOPED_TRACE(testing::Message() << "restricted_flag=" << restricted_flag);

    network::ResourceRequest request =
        CreateCrossOriginPrefetchRequest(target_url);
    request.load_flags |= restricted_flag;

    mojo::test::BadMessageObserver bad_message_observer;
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    factory->CreateLoaderAndStart(
        loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
        network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
    EXPECT_EQ("Prefetch/CreatePrefetchLoaderAndStart: restricted load flag",
              bad_message_observer.WaitForBadMessage());

    // The receiver is removed when a bad message is reported, so rebind for
    // the next iteration.
    factory.reset();
    factory = BindFactoryForMainFrame();
  }
}

// Verifies that a cross-origin prefetch request carrying only load flags that
// are permitted on requests from an untrusted process is forwarded to the
// network service.
IN_PROC_BROWSER_TEST_F(PrefetchLoadFlagsBrowserTest,
                       CrossOriginPrefetchAllowsRendererLoadFlags) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterResponse(target_path,
                   ResponseEntry("<head><title>Target</title></head>",
                                 /*content_types=*/""));
  RegisterRequestHandler(embedded_test_server());

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(cross_origin_server_->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  const GURL target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  network::ResourceRequest request =
      CreateCrossOriginPrefetchRequest(target_url);
  request.load_flags |= net::LOAD_SUPPORT_ASYNC_REVALIDATION;

  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
}

class PrefetchRecursiveBrowserTest : public PrefetchBrowserTestBase {
 public:
  PrefetchRecursiveBrowserTest() = default;

  void SetUpOnMainThread() override {
    PrefetchBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  mojo::Remote<network::mojom::URLLoaderFactory> BindFactoryForMainFrame() {
    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
    StoragePartitionImpl* partition =
        static_cast<StoragePartitionImpl*>(rfh->GetStoragePartition());
    mojo::Remote<network::mojom::URLLoaderFactory> remote;
    bind_context_ =
        partition->GetSubresourceProxyingURLLoaderService()->GetFactory(
            remote.BindNewPipeAndPassReceiver(), rfh->GetFrameTreeNodeId(),
            proxied_factory_.GetSafeWeakWrapper(), rfh->GetWeakPtr(),
            /*prefetched_signed_exchange_cache=*/nullptr);
    return remote;
  }

  // Registers a recursive-prefetch token on the current `BindContext` bound to
  // the given origin, and points the context's cross-origin factory at
  // `cross_origin_factory_` so forwarded requests can be observed.
  base::UnguessableToken RegisterRecursivePrefetchToken(
      const url::Origin& origin) {
    CHECK(bind_context_);
    bind_context_->cross_origin_factory =
        cross_origin_factory_.GetSafeWeakWrapper();
    base::UnguessableToken token = base::UnguessableToken::Create();
    bind_context_->total_tokens_generated++;
    bind_context_->prefetch_isolation_infos.Put(
        token,
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                   origin, origin, net::SiteForCookies()));
    return token;
  }

  network::TestURLLoaderFactory proxied_factory_;
  network::TestURLLoaderFactory cross_origin_factory_;
  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context_;
};

// Verifies that a recursive prefetch whose request URL is cross-origin with
// the initiating document is forwarded via the cross-origin factory using the
// `IsolationInfo` associated with its `recursive_prefetch_token`.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RecursivePrefetchAllowsCrossOriginTarget) {
  base::HistogramTester histogram_tester;
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  const url::Origin token_origin =
      url::Origin::Create(GURL("https://3p.example"));
  base::UnguessableToken token = RegisterRecursivePrefetchToken(token_origin);

  const GURL target_url("https://3p.example/subresource.js");
  network::ResourceRequest request;
  request.url = target_url;
  request.load_flags = net::LOAD_PREFETCH;
  request.request_initiator = shell()
                                  ->web_contents()
                                  ->GetPrimaryMainFrame()
                                  ->GetLastCommittedOrigin();
  request.recursive_prefetch_token = token;

  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  factory.FlushForTesting();

  ASSERT_EQ(1, cross_origin_factory_.NumPending());
  const network::ResourceRequest& forwarded =
      cross_origin_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(target_url, forwarded.url);
  ASSERT_TRUE(forwarded.trusted_params);
  EXPECT_EQ(token_origin,
            forwarded.trusted_params->isolation_info.top_frame_origin());
  EXPECT_EQ(token_origin,
            forwarded.trusted_params->isolation_info.frame_origin());

  histogram_tester.ExpectUniqueSample(
      "Prefetch.RecursivePrefetch.TokenLookupResult",
      RecursivePrefetchTokenLookupResult::kSuccess, 1);
}

// Verifies that a recursive prefetch whose request URL is same-origin with the
// initiating document is rejected as a bad message and not forwarded to the
// network.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RecursivePrefetchRejectsInitiatorOriginTarget) {
  base::HistogramTester histogram_tester;
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  const url::Origin token_origin =
      url::Origin::Create(GURL("https://3p.example"));
  base::UnguessableToken token = RegisterRecursivePrefetchToken(token_origin);

  // Use a target URL that matches the initiating document's origin.
  const GURL same_origin_target_url =
      embedded_test_server()->GetURL("/subresource.js");
  network::ResourceRequest request;
  request.url = same_origin_target_url;
  request.load_flags = net::LOAD_PREFETCH;
  request.request_initiator = shell()
                                  ->web_contents()
                                  ->GetPrimaryMainFrame()
                                  ->GetLastCommittedOrigin();
  request.recursive_prefetch_token = token;

  mojo::test::BadMessageObserver bad_message_observer;
  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  EXPECT_EQ("Prefetch/CreatePrefetchLoaderAndStart: initiator origin",
            bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0, cross_origin_factory_.NumPending());

  histogram_tester.ExpectUniqueSample(
      "Prefetch.RecursivePrefetch.TokenLookupResult",
      RecursivePrefetchTokenLookupResult::kRejectedSameOrigin, 1);
}

// Verifies that presenting an invalid or evicted recursive prefetch token fails
// gracefully with ERR_INVALID_ARGUMENT and logs kTokenNotFound without crashing
// or reporting a bad message.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RecursivePrefetchRejectsInvalidOrEvictedToken) {
  base::HistogramTester histogram_tester;
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();
  bind_context_->cross_origin_factory =
      cross_origin_factory_.GetSafeWeakWrapper();

  const GURL target_url("https://3p.example/subresource.js");
  network::ResourceRequest request;
  request.url = target_url;
  request.load_flags = net::LOAD_PREFETCH;
  request.request_initiator = shell()
                                  ->web_contents()
                                  ->GetPrimaryMainFrame()
                                  ->GetLastCommittedOrigin();
  request.recursive_prefetch_token = base::UnguessableToken::Create();

  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  EXPECT_EQ(0, cross_origin_factory_.NumPending());

  histogram_tester.ExpectUniqueSample(
      "Prefetch.RecursivePrefetch.TokenLookupResult",
      RecursivePrefetchTokenLookupResult::kTokenNotFound, 1);
}

// Verifies that stored isolation infos for recursive prefetch tokens are managed
// by an LRU cache, evicting older tokens when the limit is exceeded.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RecursivePrefetchTokenLRUEviction) {
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();
  ASSERT_TRUE(bind_context_);

  std::vector<base::UnguessableToken> tokens;
  constexpr size_t kNumTokens =
      SubresourceProxyingURLLoaderService::BindContext::
          kMaxPrefetchIsolationInfoEntries +
      5;
  for (size_t i = 0; i < kNumTokens; ++i) {
    tokens.push_back(RegisterRecursivePrefetchToken(
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.example", i)))));
  }

  EXPECT_EQ(
      SubresourceProxyingURLLoaderService::BindContext::
          kMaxPrefetchIsolationInfoEntries,
      bind_context_->prefetch_isolation_infos.size());

  // The earliest 5 tokens should have been evicted.
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(bind_context_->prefetch_isolation_infos.end(),
              bind_context_->prefetch_isolation_infos.Peek(tokens[i]));
  }

  // The latest tokens should still be present.
  for (size_t i = 5; i < kNumTokens; ++i) {
    EXPECT_NE(bind_context_->prefetch_isolation_infos.end(),
              bind_context_->prefetch_isolation_infos.Peek(tokens[i]));
  }
}

// Verifies that a prefetch carrying both LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME
// and a recursive_prefetch_token is rejected with a bad message.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RejectsRestrictedMainFrameWithRecursiveToken) {
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  const url::Origin token_origin =
      url::Origin::Create(GURL("https://3p.example"));
  base::UnguessableToken token = RegisterRecursivePrefetchToken(token_origin);

  network::ResourceRequest request;
  request.url = GURL("https://3p.example/target.html");
  request.load_flags =
      net::LOAD_PREFETCH | net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  request.request_initiator = shell()
                                  ->web_contents()
                                  ->GetPrimaryMainFrame()
                                  ->GetLastCommittedOrigin();
  request.recursive_prefetch_token = token;

  mojo::test::BadMessageObserver bad_message_observer;
  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  EXPECT_EQ("Prefetch/CreatePrefetchLoaderAndStart: recursive token with "
            "restricted main frame prefetch",
            bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0, cross_origin_factory_.NumPending());
}

// Verifies that a recursive prefetch request missing request_initiator is
// rejected with a bad message.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RejectsRecursivePrefetchWithoutRequestInitiator) {
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  const url::Origin token_origin =
      url::Origin::Create(GURL("https://3p.example"));
  base::UnguessableToken token = RegisterRecursivePrefetchToken(token_origin);

  network::ResourceRequest request;
  request.url = GURL("https://3p.example/subresource.js");
  request.load_flags = net::LOAD_PREFETCH;
  request.request_initiator = std::nullopt;
  request.recursive_prefetch_token = token;

  mojo::test::BadMessageObserver bad_message_observer;
  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  EXPECT_EQ("Prefetch/CreatePrefetchLoaderAndStart: no request_initiator",
            bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0, cross_origin_factory_.NumPending());
}

// Verifies that a recursive prefetch request whose request_initiator does not
// match the frame's committed origin is rejected with a bad message.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       RejectsRecursivePrefetchWithFrameOriginMismatch) {
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();

  const url::Origin token_origin =
      url::Origin::Create(GURL("https://3p.example"));
  base::UnguessableToken token = RegisterRecursivePrefetchToken(token_origin);

  network::ResourceRequest request;
  request.url = GURL("https://3p.example/subresource.js");
  request.load_flags = net::LOAD_PREFETCH;
  request.request_initiator =
      url::Origin::Create(GURL("https://attacker.example"));
  request.recursive_prefetch_token = token;

  mojo::test::BadMessageObserver bad_message_observer;
  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  EXPECT_EQ("Prefetch/CreatePrefetchLoaderAndStart: frame origin mismatch",
            bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0, cross_origin_factory_.NumPending());
}

// Verifies that Prefetch.RecursivePrefetch.TokensPerDocument is recorded upon
// document navigation when tokens were generated.
IN_PROC_BROWSER_TEST_F(PrefetchRecursiveBrowserTest,
                       TokensPerDocumentHistogramRecordedOnNavigation) {
  base::HistogramTester histogram_tester;
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path, ResponseEntry("<body></body>"));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      BindFactoryForMainFrame();
  ASSERT_TRUE(bind_context_);

  for (int i = 0; i < 3; ++i) {
    RegisterRecursivePrefetchToken(
        url::Origin::Create(GURL(base::StringPrintf("https://%d.example", i))));
  }

  factory.reset();
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample(
      "Prefetch.RecursivePrefetch.TokensPerDocument", /*sample=*/3,
      /*expected_bucket_count=*/1);
}

class PrefetchPartitionedCookiesBrowserTest : public PrefetchBrowserTestBase {
 public:
  PrefetchPartitionedCookiesBrowserTest() {
    // Enable `kRestrictLinkHeaderOnSubresource` (`disable_resource_load=false`)
    // so recursive prefetch tests exercise `Link: rel=preload` subresource
    // promotion, matching `PrefetchBrowserTest`.
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kRestrictLinkHeaderOnSubresource,
          {{"disable_resource_load", "false"}}}},
        {});
  }

  void SetUpOnMainThread() override {
    PrefetchBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &PrefetchPartitionedCookiesBrowserTest::MonitorRequest,
        base::Unretained(this)));
    RegisterRequestHandler(&https_server_);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &PrefetchPartitionedCookiesBrowserTest::HandleProbeRequest,
        base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  static net::CookiePartitionKey MakePartitionKey(const GURL& url,
                                                  bool cross_site) {
    return net::CookiePartitionKey::FromURLForTesting(
        url, net::CookiePartitionKey::BoolToAncestorChainBit(cross_site));
  }

  void SetBlockThirdPartyCookies(bool block) {
    auto* cookie_manager = browser_context()
                               ->GetDefaultStoragePartition()
                               ->GetCookieManagerForBrowserProcess();
    cookie_manager->BlockThirdPartyCookies(block);
    base::test::TestFuture<const net::CookieList&> flush;
    cookie_manager->GetAllCookies(flush.GetCallback());
    std::ignore = flush.Get();
  }

  // Seeds `cookie_host` with `1p_part=1` (in `cookie_host`'s first-party
  // partition), `3p_part=1` (in `top_frame_host`'s cross-site partition), and
  // `unpart=1` (unpartitioned).
  void SeedCookies(std::string_view cookie_host,
                   std::string_view top_frame_host) {
    const GURL cookie_origin_url = https_server_.GetURL(cookie_host, "/");
    const GURL top_frame_url = https_server_.GetURL(top_frame_host, "/");
    const auto inclusive =
        net::CookieOptions::SameSiteCookieContext::MakeInclusive();
    ASSERT_TRUE(SetCookie(
        browser_context(), cookie_origin_url,
        "1p_part=1; Path=/; Secure; SameSite=None; Partitioned", inclusive,
        MakePartitionKey(cookie_origin_url, /*cross_site=*/false)));
    ASSERT_TRUE(SetCookie(
        browser_context(), cookie_origin_url,
        "3p_part=1; Path=/; Secure; SameSite=None; "
        "Partitioned",
        inclusive, MakePartitionKey(top_frame_url, /*cross_site=*/true)));
    ASSERT_TRUE(SetCookie(browser_context(), cookie_origin_url,
                          "unpart=1; Path=/; Secure; SameSite=None"));
  }

  std::string Prefetch(const GURL& target_url,
                       bool as_document = true,
                       std::string_view cross_origin = "") {
    static constexpr char kScript[] = R"(
      new Promise(resolve => {
        const link = document.createElement('link');
        link.rel = 'prefetch';
        if ($2) link.as = 'document';
        if ($3) link.crossOrigin = $3;
        link.href = $1;
        link.onload = () => resolve('load');
        link.onerror = () => resolve('error');
        document.head.appendChild(link);
      })
    )";
    return EvalJs(shell()->web_contents(),
                  JsReplace(kScript, target_url, as_document, cross_origin))
        .ExtractString();
  }

  std::optional<std::string> TakeObservedCookieHeader(const std::string& path) {
    base::AutoLock auto_lock(lock_);
    auto it = observed_cookies_.find(path);
    if (it == observed_cookies_.end()) {
      return std::nullopt;
    }
    std::string header = std::move(it->second);
    observed_cookies_.erase(it);
    return header;
  }

  void WaitForRequest(const std::string& relative_url) {
    base::RunLoop run_loop;
    {
      base::AutoLock auto_lock(lock_);
      if (request_counts_[relative_url] > 0) {
        return;
      }
      request_waiters_[relative_url] = run_loop.QuitClosure();
    }
    run_loop.Run();
  }

  int GetRequestCount(const std::string& relative_url) {
    base::AutoLock auto_lock(lock_);
    auto it = request_counts_.find(relative_url);
    return it == request_counts_.end() ? 0 : it->second;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    auto it = request.headers.find(net::HttpRequestHeaders::kCookie);
    observed_cookies_[std::string(request.GetURL().path())] =
        it != request.headers.end() ? it->second : std::string();
    ++request_counts_[request.relative_url];
    auto waiter_it = request_waiters_.find(request.relative_url);
    if (waiter_it != request_waiters_.end()) {
      std::move(waiter_it->second).Run();
      request_waiters_.erase(waiter_it);
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleProbeRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != "/probe") {
      return nullptr;
    }
    auto it = request.headers.find(net::HttpRequestHeaders::kCookie);
    const bool has_1p_part = it != request.headers.end() &&
                             it->second.find("1p_part=1") != std::string::npos;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(has_1p_part ? net::HTTP_OK : net::HTTP_NOT_FOUND);
    response->set_content_type("text/html");
    response->AddCustomHeader("Cache-Control", "no-store");
    response->AddCustomHeader(
        "Set-Cookie",
        "probe_set_part=1; Path=/; Secure; SameSite=None; Partitioned");
    return response;
  }

  base::test::ScopedFeatureList feature_list_;
  base::Lock lock_;
  std::map<std::string, std::string> observed_cookies_ GUARDED_BY(lock_);
  std::map<std::string, int> request_counts_ GUARDED_BY(lock_);
  std::map<std::string, base::OnceClosure> request_waiters_ GUARDED_BY(lock_);
};

class PrefetchPartitionedCookiesWith3pcSettingBrowserTest
    : public PrefetchPartitionedCookiesBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool block_third_party_cookies() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrefetchPartitionedCookiesWith3pcSettingBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PrefetchPartitionedCookiesWith3pcSettingBrowserTest,
    CrossOriginDocumentPrefetchDoesNotSendPartitionedCookies) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL destination_origin_url = https_server_.GetURL("c.test", "/");
  const GURL probe_url = https_server_.GetURL("c.test", "/probe");
  SeedCookies("c.test", "a.test");
  SetBlockThirdPartyCookies(block_third_party_cookies());
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  EXPECT_EQ("error", Prefetch(probe_url));
  EXPECT_EQ(block_third_party_cookies() ? "" : "unpart=1",
            TakeObservedCookieHeader("/probe"));
  EXPECT_EQ("1p_part=1; 3p_part=1; unpart=1",
            GetCookies(browser_context(), destination_origin_url));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchPartitionedCookiesBrowserTest,
    CrossOriginDocumentPrefetchRedirectDoesNotSendPartitionedCookies) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL destination_origin_url = https_server_.GetURL("c.test", "/");
  const GURL probe_url = https_server_.GetURL("c.test", "/probe");
  const GURL redirect_to_probe_url =
      https_server_.GetURL("b.test", "/server-redirect?" + probe_url.spec());
  SeedCookies("c.test", "a.test");
  SetBlockThirdPartyCookies(false);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  EXPECT_EQ("error", Prefetch(redirect_to_probe_url));
  EXPECT_EQ("unpart=1", TakeObservedCookieHeader("/probe"));
  EXPECT_EQ("1p_part=1; 3p_part=1; unpart=1",
            GetCookies(browser_context(), destination_origin_url));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchPartitionedCookiesBrowserTest,
    CrossOriginSubresourcePrefetchSendsThirdPartyPartitionedCookies) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL probe_url = https_server_.GetURL("c.test", "/probe");
  SeedCookies("c.test", "a.test");
  SetBlockThirdPartyCookies(true);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  EXPECT_EQ("error", Prefetch(probe_url, /*as_document=*/false));
  EXPECT_EQ("3p_part=1", TakeObservedCookieHeader("/probe"));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchPartitionedCookiesBrowserTest,
    SameOriginDocumentPrefetchSendsAndSetsFirstPartyPartitionedCookies) {
  const GURL destination_page_url =
      https_server_.GetURL("c.test", "/empty.html");
  const GURL destination_origin_url = https_server_.GetURL("c.test", "/");
  const GURL probe_url = https_server_.GetURL("c.test", "/probe");
  SeedCookies("c.test", "a.test");
  SetBlockThirdPartyCookies(true);
  ASSERT_TRUE(NavigateToURL(shell(), destination_page_url));

  EXPECT_EQ("load", Prefetch(probe_url));
  EXPECT_EQ("1p_part=1; unpart=1", TakeObservedCookieHeader("/probe"));
  EXPECT_EQ(
      "1p_part=1; unpart=1; probe_set_part=1",
      GetCookies(browser_context(), destination_origin_url,
                 net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                 net::CookiePartitionKeyCollection(MakePartitionKey(
                     destination_origin_url, /*cross_site=*/false))));
}

IN_PROC_BROWSER_TEST_F(PrefetchPartitionedCookiesBrowserTest,
                       TopLevelNavigationSendsFirstPartyPartitionedCookies) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL probe_url = https_server_.GetURL("c.test", "/probe");
  SeedCookies("c.test", "a.test");
  SetBlockThirdPartyCookies(true);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  ASSERT_TRUE(NavigateToURL(shell(), probe_url));
  EXPECT_EQ("1p_part=1; unpart=1", TakeObservedCookieHeader("/probe"));
}

IN_PROC_BROWSER_TEST_P(
    PrefetchPartitionedCookiesWith3pcSettingBrowserTest,
    CrossOriginDocumentPrefetchReusedFromHttpCacheOnNavigation) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL cacheable_doc_url =
      https_server_.GetURL("c.test", "/cacheable_doc");
  // Empty content-type prevents ORB from blocking the no-cors prefetch body.
  RegisterResponse("/cacheable_doc",
                   ResponseEntry("<head><title>Cached Target</title></head>",
                                 /*content_types=*/"",
                                 {{"Cache-Control", "public, max-age=3600"}}));
  SeedCookies("c.test", "a.test");
  SetBlockThirdPartyCookies(block_third_party_cookies());
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  EXPECT_EQ("load", Prefetch(cacheable_doc_url));
  EXPECT_EQ(1, GetRequestCount("/cacheable_doc"));
  EXPECT_EQ(block_third_party_cookies() ? "" : "unpart=1",
            TakeObservedCookieHeader("/cacheable_doc"));

  NavigateToURLAndWaitTitle(cacheable_doc_url, "Cached Target");
  EXPECT_EQ(1, GetRequestCount("/cacheable_doc"));
  EXPECT_FALSE(TakeObservedCookieHeader("/cacheable_doc"));
}

IN_PROC_BROWSER_TEST_P(PrefetchPartitionedCookiesWith3pcSettingBrowserTest,
                       CrossOriginDocumentPrefetchIgnoresSetPartitionedCookie) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL destination_origin_url = https_server_.GetURL("c.test", "/");
  SetBlockThirdPartyCookies(block_third_party_cookies());
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  // Unpartitioned SameSite=None Set-Cookie follows the 3PC setting while
  // Partitioned Set-Cookie is always ignored (neither stored nor downgraded).
  const GURL set_cookies_url = https_server_.GetURL(
      "c.test",
      "/set-cookie?part=1;Path=/;Secure;SameSite=None;Partitioned&"
      "unpart=1;Path=/;Secure;SameSite=None");
  EXPECT_EQ("load", Prefetch(set_cookies_url));
  const std::string expected = block_third_party_cookies() ? "" : "unpart=1";
  EXPECT_EQ(expected, GetCookies(browser_context(), destination_origin_url));
  EXPECT_EQ(
      expected,
      GetCookies(browser_context(), destination_origin_url,
                 net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                 net::CookiePartitionKeyCollection()));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchPartitionedCookiesBrowserTest,
    CrossOriginDocumentPrefetchClearSiteDataPreservesPartitionedCookies) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL destination_origin_url = https_server_.GetURL("c.test", "/");
  const GURL clear_cookies_url = https_server_.GetURL(
      "c.test",
      "/set-header?Clear-Site-Data: \"cookies\"&Cache-Control: no-store");
  const GURL redirect_to_clear_cookies_url = https_server_.GetURL(
      "b.test", "/server-redirect?" + clear_cookies_url.spec());
  SeedCookies("c.test", "a.test");
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  SetBlockThirdPartyCookies(true);
  EXPECT_EQ("load", Prefetch(clear_cookies_url));
  EXPECT_EQ("1p_part=1; 3p_part=1; unpart=1",
            GetCookies(browser_context(), destination_origin_url));

  // Across a cross-origin redirect, partitioned cookies remain preserved.
  // TODO(crbug.com/581608762): Also preserve `unpart=1` when 3PCs are blocked
  // once redirected cross-origin prefetches no longer populate
  // `isolation_info().site_for_cookies()` in `HandleClearSiteDataHeader()`.
  EXPECT_EQ("load", Prefetch(redirect_to_clear_cookies_url));
  EXPECT_EQ("1p_part=1; 3p_part=1",
            GetCookies(browser_context(), destination_origin_url));

  // Re-seed `unpart`: with 3PC allowed, only unpartitioned cookies are cleared.
  ASSERT_TRUE(SetCookie(browser_context(), destination_origin_url,
                        "unpart=1; Path=/; Secure; SameSite=None"));
  SetBlockThirdPartyCookies(false);
  EXPECT_EQ("load", Prefetch(clear_cookies_url));
  EXPECT_EQ("1p_part=1; 3p_part=1",
            GetCookies(browser_context(), destination_origin_url));
}

class PrefetchRecursivePartitionedCookiesBrowserTest
    : public PrefetchPartitionedCookiesBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  bool block_third_party_cookies() const { return std::get<0>(GetParam()); }
  bool same_origin_subresource() const { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrefetchRecursivePartitionedCookiesBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(PrefetchRecursivePartitionedCookiesBrowserTest,
                       RecursivePrefetchDisallowsPartitionedCookies) {
  const std::string_view subresource_host =
      same_origin_subresource() ? "c.test" : "d.test";
  const std::string_view other_partition_host =
      same_origin_subresource() ? "a.test" : "c.test";
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL prefetched_doc_url =
      https_server_.GetURL("c.test", "/doc_with_preload");
  const GURL subresource_origin_url =
      https_server_.GetURL(subresource_host, "/");
  const GURL preload_script_url =
      https_server_.GetURL(subresource_host, "/preload_sub.js");
  RegisterResponse(
      "/doc_with_preload",
      ResponseEntry(
          "<!doctype html>", "text/html",
          {{"Link", base::StringPrintf("<%s>; rel=\"preload\"; as=\"script\"",
                                       preload_script_url.spec().c_str())},
           {"Access-Control-Allow-Origin",
            url::Origin::Create(initiator_page_url).Serialize()},
           {"Access-Control-Allow-Credentials", "true"}}));
  RegisterResponse("/preload_sub.js",
                   ResponseEntry("// script", "text/javascript",
                                 {{"Set-Cookie",
                                   "set_part=1; Path=/; Secure; SameSite=None; "
                                   "Partitioned"}}));

  SeedCookies(subresource_host, other_partition_host);
  SetBlockThirdPartyCookies(block_third_party_cookies());
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));

  EXPECT_EQ("load", Prefetch(prefetched_doc_url, /*as_document=*/true,
                             "use-credentials"));
  WaitUntilLoaded(preload_script_url);

  EXPECT_EQ(block_third_party_cookies() ? "" : "unpart=1",
            TakeObservedCookieHeader("/preload_sub.js"));
  EXPECT_EQ("1p_part=1; 3p_part=1; unpart=1",
            GetCookies(browser_context(), subresource_origin_url));
  EXPECT_EQ(
      "unpart=1",
      GetCookies(browser_context(), subresource_origin_url,
                 net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                 net::CookiePartitionKeyCollection()));
}

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
class PrefetchDeviceBoundSessionsBrowserTest
    : public PrefetchPartitionedCookiesBrowserTest {
 public:
  PrefetchDeviceBoundSessionsBrowserTest() {
    dbsc_feature_list_.InitWithFeatures(
        {net::features::kDeviceBoundSessions,
         unexportable_keys::
             kEnableBoundSessionCredentialsSoftwareKeysForManualTesting},
        {});
  }

 private:
  base::test::ScopedFeatureList dbsc_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrefetchDeviceBoundSessionsBrowserTest,
                       SuppressesDeviceBoundSessionRegistration) {
  const GURL initiator_page_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL destination_page_url =
      https_server_.GetURL("c.test", "/empty.html");
  const GURL cross_origin_doc_url =
      https_server_.GetURL("c.test", "/dbsc_cross_origin");
  const GURL recursive_preload_url =
      https_server_.GetURL("d.test", "/dbsc_recursive.js");
  const GURL same_origin_doc_url =
      https_server_.GetURL("c.test", "/dbsc_same_origin");

  RegisterResponse(
      "/dbsc_cross_origin",
      ResponseEntry(
          "<!doctype html>", "text/html",
          {{"Secure-Session-Registration",
            "(RS256 ES256);challenge=\"test\";"
            "path=\"/dbsc_register_cross_origin\""},
           {"Link", base::StringPrintf("<%s>; rel=\"preload\"; as=\"script\"",
                                       recursive_preload_url.spec().c_str())},
           {"Access-Control-Allow-Origin",
            url::Origin::Create(initiator_page_url).Serialize()},
           {"Access-Control-Allow-Credentials", "true"}}));
  RegisterResponse("/dbsc_recursive.js",
                   ResponseEntry("// script", "text/javascript",
                                 {{"Secure-Session-Registration",
                                   "(RS256 ES256);challenge=\"test\";"
                                   "path=\"/dbsc_register_recursive\""}}));
  RegisterResponse("/dbsc_same_origin",
                   ResponseEntry("<!doctype html>", "text/html",
                                 {{"Secure-Session-Registration",
                                   "(RS256 ES256);challenge=\"test\";"
                                   "path=\"/dbsc_register_same_origin\""}}));

  // Cross-origin document prefetch and its recursive subresource preload must
  // ignore `Secure-Session-Registration`.
  ASSERT_TRUE(NavigateToURL(shell(), initiator_page_url));
  EXPECT_EQ("load", Prefetch(cross_origin_doc_url, /*as_document=*/true,
                             "use-credentials"));
  WaitUntilLoaded(recursive_preload_url);

  // Control: a same-origin document prefetch honors
  // `Secure-Session-Registration` and triggers the registration fetch.
  ASSERT_TRUE(NavigateToURL(shell(), destination_page_url));
  EXPECT_EQ("load", Prefetch(same_origin_doc_url));
  WaitForRequest("/dbsc_register_same_origin");

  EXPECT_EQ(1, GetRequestCount("/dbsc_register_same_origin"));
  EXPECT_EQ(0, GetRequestCount("/dbsc_register_cross_origin"));
  EXPECT_EQ(0, GetRequestCount("/dbsc_register_recursive"));
}
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

INSTANTIATE_TEST_SUITE_P(
    All,
    PrefetchBrowserTest,
    testing::ValuesIn({SplitCacheTestCase::kDisabled,
                       SplitCacheTestCase::kEnabledTripleKeyed,
                       SplitCacheTestCase::kEnabledTriplePlusCredsBool}),

    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      switch (info.param) {
        case SplitCacheTestCase::kDisabled:
          return "SplitCacheDisabled";
        case SplitCacheTestCase::kEnabledTripleKeyed:
          return "SplitCacheEnabledTripleKeyed";
        case SplitCacheTestCase::kEnabledTriplePlusCredsBool:
          return "SplitCacheEnabledTriplePlusCredsBool";
      }
    });

}  // namespace content
