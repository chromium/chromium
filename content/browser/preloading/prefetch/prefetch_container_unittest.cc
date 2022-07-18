// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchContainerTest : public RenderViewHostTestHarness {
 public:
  PrefetchContainerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));

    EXPECT_TRUE(cookie.get());

    bool result = false;
    base::RunLoop run_loop;

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

    // This will run until the cookie listener is updated.
    base::RunLoop().RunUntilIdle();

    return result;
  }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

TEST_F(PrefetchContainerTest, CreatePrefetchContainer) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true),
      nullptr);

  EXPECT_EQ(prefetch_container.GetReferringRenderFrameHostId(),
            GlobalRenderFrameHostId(1234, 5678));
  EXPECT_EQ(prefetch_container.GetURL(), GURL("https://test.com"));
  EXPECT_EQ(prefetch_container.GetPrefetchType(),
            PrefetchType(/*use_isolated_network_context=*/true,
                         /*use_prefetch_proxy=*/true));

  EXPECT_EQ(prefetch_container.GetPrefetchContainerKey(),
            std::make_pair(GlobalRenderFrameHostId(1234, 5678),
                           GURL("https://test.com")));
}

TEST_F(PrefetchContainerTest, PrefetchStatus) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true),
      nullptr);

  EXPECT_FALSE(prefetch_container.HasPrefetchStatus());

  prefetch_container.SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

TEST_F(PrefetchContainerTest, IsDecoy) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true),
      nullptr);

  EXPECT_FALSE(prefetch_container.IsDecoy());

  prefetch_container.SetIsDecoy(true);
  EXPECT_TRUE(prefetch_container.IsDecoy());
}

TEST_F(PrefetchContainerTest, ValidResponse) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true),
      nullptr);

  prefetch_container.TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("test body")));

  task_environment()->FastForwardBy(base::Minutes(2));

  EXPECT_FALSE(prefetch_container.HasValidPrefetchedResponse(base::Minutes(1)));
  EXPECT_TRUE(prefetch_container.HasValidPrefetchedResponse(base::Minutes(3)));
}

TEST_F(PrefetchContainerTest, CookieListener) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true),
      nullptr);

  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged());

  prefetch_container.RegisterCookieListener(cookie_manager());

  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged());

  ASSERT_TRUE(SetCookie(GURL("https://test.com"), "test-cookie"));

  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged());
}

TEST_F(PrefetchContainerTest, CookieCopy) {
  base::HistogramTester histogram_tester;

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true),
      nullptr);
  prefetch_container.RegisterCookieListener(cookie_manager());

  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());

  prefetch_container.OnIsolatedCookieCopyStart();

  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());

  // Once the cookie copy process has started, we should stop the cookie
  // listener.
  ASSERT_TRUE(SetCookie(GURL("https://test.com"), "test-cookie"));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged());

  task_environment()->FastForwardBy(base::Milliseconds(10));
  prefetch_container.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));

  bool callback_called = false;
  prefetch_container.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  prefetch_container.OnIsolatedCookieCopyComplete();

  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());
  EXPECT_TRUE(callback_called);

  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieReadTime",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWriteTime",
      base::Milliseconds(20), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      base::Milliseconds(30), 1);
}

}  // namespace
}  // namespace content
