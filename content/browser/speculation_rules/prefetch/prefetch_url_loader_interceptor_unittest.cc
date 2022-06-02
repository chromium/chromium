// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_url_loader_interceptor.h"

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/browser/speculation_rules/prefetch/prefetch_params.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/browser/speculation_rules/prefetch/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace content {
namespace {

// These tests leak mojo objects (like the PrefetchFromStringURLLoader) because
// they do not have valid mojo channels, which would normally delete the bound
// objects on destruction. This is expected and cannot be easily fixed without
// rewriting these as browsertests. The trade off for the speed and flexibility
// of unittests is an intentional decision.
#if defined(LEAK_SANITIZER)
#define DISABLE_ASAN(x) DISABLED_##x
#else
#define DISABLE_ASAN(x) x
#endif

class TestPrefetchURLLoaderInterceptor : public PrefetchURLLoaderInterceptor {
 public:
  explicit TestPrefetchURLLoaderInterceptor(int frame_tree_node_id)
      : PrefetchURLLoaderInterceptor(frame_tree_node_id) {}
  ~TestPrefetchURLLoaderInterceptor() override = default;

  void AddPrefetch(base::WeakPtr<PrefetchContainer> prefetch_container) {
    prefetches_[prefetch_container->GetURL()] = prefetch_container;
  }

 private:
  base::WeakPtr<PrefetchContainer> GetPrefetch(const GURL& url) const override {
    const auto& iter = prefetches_.find(url);
    if (iter == prefetches_.end())
      return nullptr;
    return iter->second;
  }

  std::map<GURL, base::WeakPtr<PrefetchContainer>> prefetches_;
};

class PrefetchURLLoaderInterceptorTest : public RenderViewHostTestHarness {
 public:
  PrefetchURLLoaderInterceptorTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());

    interceptor_ = std::make_unique<TestPrefetchURLLoaderInterceptor>(
        web_contents()->GetMainFrame()->GetFrameTreeNodeId());
  }

  void TearDown() override {
    interceptor_.release();

    RenderViewHostTestHarness::TearDown();
  }

  TestPrefetchURLLoaderInterceptor* interceptor() { return interceptor_.get(); }

  void WaitForCallback() {
    if (was_intercepted_.has_value())
      return;

    base::RunLoop run_loop;
    on_loader_callback_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void LoaderCallback(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    was_intercepted_ = url_loader_factory != nullptr;
    if (on_loader_callback_closure_) {
      std::move(on_loader_callback_closure_).Run();
    }
  }

  absl::optional<bool> was_intercepted() { return was_intercepted_; }

  bool SetCookie(const GURL& url, const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;

    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsHostCookie());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        *cookie.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));

    // This will run until the cookie is set.
    run_loop.Run();

    // This will run until the cookie listener gets the cookie change.
    base::RunLoop().RunUntilIdle();

    return result;
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<TestPrefetchURLLoaderInterceptor> interceptor_;

  base::HistogramTester histogram_tester_;

  absl::optional<bool> was_intercepted_;
  base::OnceClosure on_loader_callback_closure_;

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationCookieCopyCompleted)) {
  const GURL kTestUrl("https://example.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), kTestUrl,
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true),
          nullptr);

  prefetch_container->TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("test body")));

  // Simulate the cookie copy process starting and finishing before
  // |MaybeCreateLoader| is called.
  prefetch_container->OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));
  prefetch_container->OnIsolatedCookieCopyComplete();

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_TRUE(was_intercepted().value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);
}

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationCookieCopyInProgress)) {
  const GURL kTestUrl("https://example.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), kTestUrl,
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true),
          nullptr);

  prefetch_container->TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("test body")));

  // Simulate the cookie copy process starting, but not finishing until after
  // |MaybeCreateLoader| is called.
  prefetch_container->OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until after the cookie copy process is completed.
  EXPECT_FALSE(was_intercepted().has_value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  prefetch_container->OnIsolatedCookieCopyComplete();
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_TRUE(was_intercepted().value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);
}

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationNoCookieCopyNeeded)) {
  const GURL kTestUrl("https://example.com");

  // No cookies are copied for prefetches where |use_isolated_network_context|
  // is false (i.e. same origin prefetches).
  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), kTestUrl,
          PrefetchType(/*use_isolated_network_context=*/false,
                       /*use_prefetch_proxy=*/false),
          nullptr);

  prefetch_container->TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("test body")));

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_TRUE(was_intercepted().value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);
}

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoPrefetch)) {
  const GURL kTestUrl("https://example.com");

  // With no prefetch set, the navigation shouldn't be intercepted.

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);
}

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoPrefetchedResponse)) {
  const GURL kTestUrl("https://example.com");

  // Without a prefetched response, the navigation shouldn't be intercepted.
  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), kTestUrl,
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true),
          nullptr);

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  // Set up ResourceRequest
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  // Try to create loader
  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);
}

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationStalePrefetchedResponse)) {
  const GURL kTestUrl("https://example.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), kTestUrl,
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true),
          nullptr);

  prefetch_container->TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("test body")));

  // Advance time enough so that the response is considered stale.
  task_environment()->FastForwardBy(2 * PrefetchCacheableDuration());

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);
}

TEST_F(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationCookiesChanged)) {
  const GURL kTestUrl("https://example.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), kTestUrl,
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true),
          nullptr);

  prefetch_container->TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("test body")));

  // Since the cookies associated with |kTestUrl| have changed, the prefetch can
  // no longer be served.
  prefetch_container->RegisterCookieListener(cookie_manager());
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this)),
      base::BindOnce([](bool) { NOTREACHED(); }));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);
}

}  // namespace
}  // namespace content
