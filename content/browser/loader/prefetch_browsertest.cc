// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/loader/prefetch_browsertest_base.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/isolation_info.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

enum class SplitCacheTestCase {
  kDisabled,
  kEnabledTripleKeyed,
  kEnabledTriplePlusCredsBool,
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator
  // Note: For the purposes of our HTTP Cache initiator experiment we won't
  // assume that SplitCacheByCredentials feature can be enabled while any of the
  // initiator features are, and we won't test these combinations.
};

const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCredsBool,
     net::features::kSplitCacheByIncludeCredentials},
    {SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {SplitCacheTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator}};

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
    if (IsSplitCacheEnabled()) {
      split_cache_enabled_feature_list_.InitAndEnableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    } else {
      split_cache_enabled_feature_list_.InitAndDisableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    }
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

class PrefetchBrowserTestPrivacyChanges
    : public PrefetchBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrefetchBrowserTestPrivacyChanges()
      : privacy_changes_enabled_(GetParam()),
        cross_origin_server_(std::make_unique<net::EmbeddedTestServer>()) {}

  PrefetchBrowserTestPrivacyChanges(const PrefetchBrowserTestPrivacyChanges&) =
      delete;
  PrefetchBrowserTestPrivacyChanges& operator=(
      const PrefetchBrowserTestPrivacyChanges&) = delete;

  ~PrefetchBrowserTestPrivacyChanges() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enable_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (privacy_changes_enabled_) {
      enable_features.push_back(blink::features::kPrefetchPrivacyChanges);
    } else {
      disabled_features.push_back(blink::features::kPrefetchPrivacyChanges);
    }
    feature_list_.InitWithFeatures(enable_features, disabled_features);
    PrefetchBrowserTestBase::SetUp();
  }

 protected:
  const bool privacy_changes_enabled_;
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test flakes.
// TODO(crbug.com/40248957): Resolve flake and reenable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTestPrivacyChanges,
                       DISABLED_RedirectNotFollowed) {
  const char* prefetch_path = "/prefetch.html";
  const char* redirect_path = "/redirect.html";
  const char* destination_path = "/destination.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", redirect_path)));
  RegisterResponse(
      redirect_path,
      ResponseEntry("", "", {{"location", std::string(destination_path)}},
                    net::HTTP_MOVED_PERMANENTLY));
  RegisterResponse(destination_path,
                   ResponseEntry("<head><title>Prefetch Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop prefetch_waiter;
  auto main_page_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), prefetch_path, &prefetch_waiter);
  auto destination_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), destination_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, main_page_counter->GetRequestCount());
  EXPECT_EQ(0, destination_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL destination_url = embedded_test_server()->GetURL(destination_path);
  // Loading a page that prefetches the redirect resource only follows the
  // redirect when the mode is follow.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, main_page_counter->GetRequestCount());

  NavigateToURLAndWaitTitle(destination_url, "Prefetch Target");
  const int expected_request_count = privacy_changes_enabled_ ? 1 : 2;
  EXPECT_EQ(expected_request_count, destination_counter->GetRequestCount());
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

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
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

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
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

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

  // Subsequent navigation to the no-cache URL wouldn't hit the network, because
  // no-cache resource is kept available up to kPrefetchReuseMins.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nocache_path),
                            "NoCache Target");
  EXPECT_EQ(1, nocache_request_counter->GetRequestCount());

  // Subsequent navigation to the no-store URL hit the network again, because
  // no-store resource is not cached even for prefetch.
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
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""}}));
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
      ResponseEntry(MockSignedExchangeHandler::kMockSxgPrefix +
                        "<head><title>Prefetch Target (SXG)</title><script "
                        "src=\"./preload.js\"></script></head>",
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
      ResponseEntry(MockSignedExchangeHandler::kMockSxgPrefix +
                        "<head><title>Prefetch Target (SXG)</title><script "
                        "src=\"./preload.js\"></script></head>",
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
  RegisterResponse(target_path,
                   ResponseEntry("<head><title>Prefetch Target</title></head>",
                                 // The empty content type prevents this
                                 // response from being blocked by ORB.
                                 /*content_types=*/""));

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

  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
    case SplitCacheTestCase::kEnabledTripleKeyed:
    case SplitCacheTestCase::kEnabledTriplePlusCredsBool:
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
      // Shutdown the server.
      EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

      // Subsequent navigation to the target URL wouldn't hit the network for
      // the target URL. The target content should still be read correctly.
      NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
      break;
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      // We don't expect re-use of the prefetched navigations because caching
      // isn't supported when the initiator is opaque (when the initiator is
      // incorporated into the cache key).
      NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
      EXPECT_EQ(2, request_counter->GetRequestCount());
      break;
  }
}

class FencedFramePrefetchTest : public PrefetchBrowserTestBase {
 public:
  FencedFramePrefetchTest()
      : cross_origin_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    PrefetchBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up the embedded https test server for fenced frame which requires a
    // secure context to load.
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(&embedded_https_test_server());
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());

    cross_origin_server()->SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(cross_origin_server());
    net::test_server::RegisterDefaultHandlers(cross_origin_server());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  net::EmbeddedTestServer* cross_origin_server() {
    return &cross_origin_server_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
  net::EmbeddedTestServer cross_origin_server_;
};

// Verify that prefetch works in fenced frame.
IN_PROC_BROWSER_TEST_F(FencedFramePrefetchTest, BasicPrefetch) {
  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      &embedded_https_test_server(), "/image.jpg", &prefetch_waiter);

  RegisterRequestHandler(&embedded_https_test_server());
  ASSERT_TRUE(embedded_https_test_server().Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  GURL prefetch_url =
      embedded_https_test_server().GetURL("a.test", "/image.jpg");
  URLLoaderMonitor monitor({prefetch_url});

  const GURL main_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  const GURL fenced_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  // Loading a page that prefetches the URL would increment the
  // |request_counter|.
  TestFrameNavigationObserver observer(fenced_frame_rfh);
  EXPECT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     JsReplace(
                         R"(document.querySelector('fencedframe').config
                            = new FencedFrameConfig($1);)",
                         embedded_https_test_server().GetURL(
                             "a.test", "/link_rel_prefetch.html"))));
  observer.WaitForCommit();

  // Expect there is a prefetch request.
  prefetch_waiter.Run();
  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(prefetch_url);
  EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);

  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
}

// Test that after fenced frame disables untrusted network access, prefetch
// request is not allowed.
IN_PROC_BROWSER_TEST_F(FencedFramePrefetchTest, NetworkCutoffDisablesPrefetch) {
  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      &embedded_https_test_server(), "/image.jpg", &prefetch_waiter);

  RegisterRequestHandler(&embedded_https_test_server());
  ASSERT_TRUE(embedded_https_test_server().Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  GURL prefetch_url =
      embedded_https_test_server().GetURL("a.test", "/image.jpg");
  URLLoaderMonitor monitor({prefetch_url});

  const GURL main_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  const GURL fenced_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  // Loading a page that immediately disables untrusted network by calling
  // `window.fence.disableUntrustedNetwork()`.
  TestFrameNavigationObserver observer(fenced_frame_rfh);
  EXPECT_TRUE(
      ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
             JsReplace(
                 R"(document.querySelector('fencedframe').config
                            = new FencedFrameConfig($1);)",
                 embedded_https_test_server().GetURL(
                     "a.test", "/link_rel_prefetch_disable_network.html"))));
  observer.WaitForCommit();

  // There should be no prefetch request because the untrusted network has been
  // disabled.
  prefetch_waiter.RunUntilIdle();
  EXPECT_EQ(monitor.WaitForRequestCompletion(prefetch_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(0, request_counter->GetRequestCount());

  // The `PrefetchURLLoader` count is 1 because the request did go through it.
  // It was eventually blocked by the nonce network status check in
  // `CorsURLLoaderFactory::CreateLoaderAndStart`.
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
}

// Similar to "PrefetchBrowserTest.CrossOriginWithPreloadCredentialled" but the
// test procedure takes place within a fenced frame.
// 1. Fenced frame navigates to `prefetch_path`.
// 2. The response to navigation triggers a prefetch request to
// `cross_origin_target_url`.
// 3. The response to prefetch triggers a recursive prefetch request to
// `preload_url`.
IN_PROC_BROWSER_TEST_F(FencedFramePrefetchTest,
                       CrossOriginWithPreloadCredentialled) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
  const auto port = embedded_https_test_server().port();
  const char target_path[] = "/target.html";
  const char preload_path[] = "/preload.js";

  // Register the response to the recursive prefetch request.
  RegisterResponse(preload_path,
                   ResponseEntry(/*content=*/"document.title=\"done\";",
                                 /*content_types=*/"text/javascript",
                                 /*headers=*/
                                 {{"cache-control", "public, max-age=600"},
                                  {"Supports-Loading-Mode", "fenced-frame"}}));

  // Set up request counters.
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server(), target_path);

  base::RunLoop preload_waiter;
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server(), preload_path, &preload_waiter);

  // Start cross origin server.
  RegisterRequestHandler(cross_origin_server());
  ASSERT_TRUE(cross_origin_server()->Start());

  // Register the response to the navigation request.
  const GURL cross_origin_target_url =
      cross_origin_server()->GetURL("b.test", target_path);
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(/*content=*/JsReplace(
                        R"(
                        <body>
                          <link rel='prefetch' href=$1 as='document'
                            crossorigin='use-credentials'>
                        </body>
                      )",
                        cross_origin_target_url),
                    /*content_types=*/"text/html",
                    /*headers=*/{{"Supports-Loading-Mode", "fenced-frame"}}));

  RegisterRequestHandler(&embedded_https_test_server());
  embedded_https_test_server().StartAcceptingConnections();
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Register the response to the initial prefetch request.
  const GURL preload_url =
      cross_origin_server()->GetURL("c.test", preload_path);
  RegisterResponse(
      target_path,
      ResponseEntry(
          /*content=*/JsReplace(R"(
                      <head>
                        <title>
                          Prefetch Target
                        </title>
                        <script src=$1></script>
                      </head>
                    )",
                                preload_url),
          /*content_types=*/"text/html",
          /*headers=*/
          {{
               "link",
               base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                  preload_url.spec().c_str()),
           },
           {
               "Access-Control-Allow-Origin",
               "https://a.test:" + base::NumberToString(port),
           },
           {
               "Access-Control-Allow-Credentials",
               "true",
           },
           {"Supports-Loading-Mode", "fenced-frame"}}));

  // Create the fenced frame.
  const GURL main_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  const GURL fenced_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  TestFrameNavigationObserver observer(fenced_frame_rfh);
  EXPECT_TRUE(ExecJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          R"(document.querySelector('fencedframe').config
                            = new FencedFrameConfig($1);)",
          embedded_https_test_server().GetURL("a.test", prefetch_path))));
  observer.WaitForCommit();

  // Expect there are two prefetch requests:
  // 1. Navigation to `prefetch_path` which responses with a `link` element with
  // `prefetch` attribute. This triggers a prefetch request.
  // 2. The prefetch request from 1 to `cross_origin_target_url` gets a response
  // with a `link` header with `preload` attribute. This is turned into a
  // prefetch request because of the recursive prefetch token.
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server()->ShutdownAndWaitUntilComplete());
}

// Similar to FencedFramePrefetchTest.CrossOriginWithPreloadCredentialled except
// the fenced frame disables its network with exemption of the first prefetch
// request url. This allows the first prefetch request to go through. However,
// the second prefetch request, which is changed from a preload request because
// of the recursive prefetch token, is blocked.
// TODO(crbug.com/336778624): This test is based on
// PrefetchBrowserTest.CrossOriginWithPreloadCredentialled, which is flaky. Once
// the flakiness is addressed, re-enable this test as well.
IN_PROC_BROWSER_TEST_F(FencedFramePrefetchTest,
                       DISABLED_NetworkCutoffDisablesRecursivePrefetch) {
  ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
  const auto port = embedded_https_test_server().port();
  const char target_path[] = "/target.html";
  const char preload_path[] = "/preload.js";

  // Register the response to the recursive prefetch request.
  RegisterResponse(preload_path,
                   ResponseEntry(/*content=*/"document.title=\"done\";",
                                 /*content_types=*/"text/javascript",
                                 /*headers=*/
                                 {{"cache-control", "public, max-age=600"},
                                  {"Supports-Loading-Mode", "fenced-frame"}}));

  // Set up request counters.
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server(), target_path);
  auto preload_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server(), preload_path);

  // Start cross origin server.
  RegisterRequestHandler(cross_origin_server());
  ASSERT_TRUE(cross_origin_server()->Start());

  // Register the response to the navigation request.
  const GURL cross_origin_target_url =
      cross_origin_server()->GetURL("b.test", target_path);
  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(/*content=*/JsReplace(
                        R"(
                        <body>
                          <link rel='prefetch' href=$1 as='document'
                            crossorigin='use-credentials'>
                        </body>
                      )",
                        cross_origin_target_url),
                    /*content_types=*/"text/html",
                    /*headers=*/
                    {{
                         "Access-Control-Allow-Origin",
                         "https://a.test:" + base::NumberToString(port),
                     },
                     {"Supports-Loading-Mode", "fenced-frame"}}));

  RegisterRequestHandler(&embedded_https_test_server());
  embedded_https_test_server().StartAcceptingConnections();
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Register the response to the initial prefetch request.
  const GURL preload_url =
      cross_origin_server()->GetURL("c.test", preload_path);
  RegisterResponse(
      target_path,
      ResponseEntry(
          /*content=*/JsReplace(R"(
                      <head>
                        <title>
                          Prefetch Target
                        </title>
                        <script src=$1></script>
                      </head>
                    )",
                                preload_url),
          /*content_types=*/"text/html",
          /*headers=*/
          {{
               "link",
               base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                  preload_url.spec().c_str()),
           },
           {
               "Access-Control-Allow-Origin",
               "https://a.test:" + base::NumberToString(port),
           },
           {
               "Access-Control-Allow-Credentials",
               "true",
           },
           {"Supports-Loading-Mode", "fenced-frame"}}));

  // Create the fenced frame.
  const GURL main_url =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  const GURL fenced_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  // This callback is invoked when the first `PrefetchURLLoader` is created.
  // This is needed because once fenced frame commits the navigation, it gets
  // a new nonce. The network revocation call needs to take place after the
  // navigation but before the prefetch request is sent.
  RegisterPrefetchLoaderCallback(base::BindLambdaForTesting([&]() {
    // Disable fenced frame untrusted network but exempt
    // `cross_origin_target_url`. This allows the prefetch request to this url.
    // Note the exemption must be done first, otherwise the in-progress prefetch
    // request to `cross_origin_target_url` will be blocked.
    RenderFrameHost* rfh =
        test::FencedFrameTestHelper::GetMostRecentlyAddedFencedFrame(
            shell()->web_contents()->GetPrimaryMainFrame());

    test::ExemptUrlsFromFencedFrameNetworkRevocation(rfh,
                                                     {cross_origin_target_url});
    EXPECT_TRUE(ExecJs(rfh, R"(
      (async () => {
        return window.fence.disableUntrustedNetwork();
      })();
    )"));
  }));

  // Monitor requests to `preload_url`.
  URLLoaderMonitor monitor({preload_url});

  // Navigate the fenced frame.
  TestFrameNavigationObserver observer(fenced_frame_rfh);
  EXPECT_TRUE(ExecJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          R"(document.querySelector('fencedframe').config
                            = new FencedFrameConfig($1);)",
          embedded_https_test_server().GetURL("a.test", prefetch_path))));
  observer.WaitForCommit();

  // There should only be one prefetch request to `cross_origin_target_url`.
  // The recursive prefetch request is blocked because the fenced frame has
  // disabled its network and the request destination `preload_url` is not
  // exempted.
  EXPECT_EQ(monitor.WaitForRequestCompletion(preload_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(0, preload_request_counter->GetRequestCount());

  // The `PrefetchURLLoader` is still called twice because the request did go
  // through it. The recursive prefetch request was eventually blocked by the
  // nonce network status check in `CorsURLLoaderFactory::CreateLoaderAndStart`.
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server()->ShutdownAndWaitUntilComplete());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrefetchBrowserTest,
    testing::ValuesIn(
        {SplitCacheTestCase::kDisabled, SplitCacheTestCase::kEnabledTripleKeyed,
         SplitCacheTestCase::kEnabledTriplePlusCredsBool,
         SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
         SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
         SplitCacheTestCase::kEnabledTriplePlusNavInitiator}),
    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      switch (info.param) {
        case SplitCacheTestCase::kDisabled:
          return "SplitCacheDisabled";
        case SplitCacheTestCase::kEnabledTripleKeyed:
          return "SplitCacheEnabledTripleKeyed";
        case SplitCacheTestCase::kEnabledTriplePlusCredsBool:
          return "SplitCacheEnabledTriplePlusCredsBool";
        case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
          return "SplitCacheEnabledTriplePlusCrossSiteMainFrameNavigationBool";
        case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
          return "SplitCacheEnabledTriplePlusMainFrameNavigationInitiator";
        case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
          return "SplitCacheEnabledTriplePlusNavigationInitiator";
      }
    });

INSTANTIATE_TEST_SUITE_P(PrefetchBrowserTestPrivacyChanges,
                         PrefetchBrowserTestPrivacyChanges,
                         testing::Bool());

}  // namespace content
