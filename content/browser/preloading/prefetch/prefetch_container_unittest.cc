// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_test_utils.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

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

  void UpdatePrefetchRequestMetrics(
      PrefetchContainer* prefetch_container,
      const absl::optional<network::URLLoaderCompletionStatus>&
          completion_status,
      const network::mojom::URLResponseHead* head) {
    prefetch_container->UpdatePrefetchRequestMetrics(completion_status, head);
  }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

TEST_F(PrefetchContainerTest, CreatePrefetchContainer) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), nullptr);

  EXPECT_EQ(prefetch_container.GetReferringRenderFrameHostId(),
            GlobalRenderFrameHostId(1234, 5678));
  EXPECT_EQ(prefetch_container.GetURL(), GURL("https://test.com"));
  EXPECT_EQ(prefetch_container.GetPrefetchType(),
            PrefetchType(/*use_isolated_network_context=*/true,
                         /*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kEager));

  EXPECT_EQ(prefetch_container.GetPrefetchContainerKey(),
            std::make_pair(GlobalRenderFrameHostId(1234, 5678),
                           GURL("https://test.com")));
  EXPECT_FALSE(prefetch_container.GetHead());
}

TEST_F(PrefetchContainerTest, PrefetchStatus) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), nullptr);

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
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), nullptr);

  EXPECT_FALSE(prefetch_container.IsDecoy());

  prefetch_container.SetIsDecoy(true);
  EXPECT_TRUE(prefetch_container.IsDecoy());
}

TEST_F(PrefetchContainerTest, Servable) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), nullptr);

  prefetch_container.TakeStreamingURLLoader(
      MakeServableStreamingURLLoaderForTest(
          network::mojom::URLResponseHead::New(), "test body"));

  task_environment()->FastForwardBy(base::Minutes(2));

  EXPECT_FALSE(prefetch_container.IsPrefetchServable(base::Minutes(1)));
  EXPECT_TRUE(prefetch_container.IsPrefetchServable(base::Minutes(3)));
  EXPECT_TRUE(prefetch_container.GetHead());
}

TEST_F(PrefetchContainerTest, CookieListener) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), nullptr);

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
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), nullptr);
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

  // The URL interceptor checks on the cookie copy status when trying to serve a
  // prefetch. If its still in progress, it registers a callback to be called
  // once the copy is complete.
  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());
  prefetch_container.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));
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
      base::Milliseconds(60), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::Milliseconds(30), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      base::Milliseconds(70), 1);
}

TEST_F(PrefetchContainerTest, PrefetchProxyPrefetchedResourceUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(), nullptr);

  network::URLLoaderCompletionStatus completion_status;
  completion_status.encoded_data_length = 100;
  completion_status.completion_time =
      base::TimeTicks() + base::Milliseconds(200);

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->load_timing.request_start = base::TimeTicks();

  UpdatePrefetchRequestMetrics(prefetch_container.get(), completion_status,
                               head.get());

  prefetch_container->TakeStreamingURLLoader(
      MakeServableStreamingURLLoaderForTest(
          network::mojom::URLResponseHead::New(), "test body"));

  // Simulates the URL of the prefetch being navigated to and the prefetch being
  // considered for serving.
  prefetch_container->OnNavigationToPrefetch();

  // Simulate a successful DNS probe for this prefetch. Not this will also
  // update the status of the prefetch to
  // |PrefetchStatus::kPrefetchUsedProbeSuccess|.
  prefetch_container->OnPrefetchProbeResult(
      PrefetchProbeResult::kDNSProbeSuccess);

  // Deleting the prefetch container will trigger the recording of the
  // PrefetchProxy_PrefetchedResource UKM event.
  prefetch_container.reset();

  auto ukm_entries = ukm_recorder.GetEntries(
      ukm::builders::PrefetchProxy_PrefetchedResource::kEntryName,
      {
          ukm::builders::PrefetchProxy_PrefetchedResource::kResourceTypeName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kStatusName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkClickedName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kDataLengthName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kFetchDurationMSName,
          ukm::builders::PrefetchProxy_PrefetchedResource::
              kISPFilteringStatusName,
          ukm::builders::PrefetchProxy_PrefetchedResource::
              kNavigationStartToFetchStartMSName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkPositionName,
      });

  ASSERT_EQ(ukm_entries.size(), 1U);
  EXPECT_EQ(ukm_entries[0].source_id, ukm::kInvalidSourceId);

  const auto& ukm_metrics = ukm_entries[0].metrics;

  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kResourceTypeName) !=
      ukm_metrics.end());
  EXPECT_EQ(
      ukm_metrics.at(
          ukm::builders::PrefetchProxy_PrefetchedResource::kResourceTypeName),
      /*mainfrmae*/ 1);

  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kStatusName) !=
      ukm_metrics.end());
  EXPECT_EQ(ukm_metrics.at(
                ukm::builders::PrefetchProxy_PrefetchedResource::kStatusName),
            static_cast<int>(PrefetchStatus::kPrefetchResponseUsed));

  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkClickedName) !=
      ukm_metrics.end());
  EXPECT_EQ(
      ukm_metrics.at(
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkClickedName),
      1);

  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kDataLengthName) !=
      ukm_metrics.end());
  EXPECT_EQ(
      ukm_metrics.at(
          ukm::builders::PrefetchProxy_PrefetchedResource::kDataLengthName),
      ukm::GetExponentialBucketMinForBytes(100));

  ASSERT_TRUE(ukm_metrics.find(ukm::builders::PrefetchProxy_PrefetchedResource::
                                   kFetchDurationMSName) != ukm_metrics.end());
  EXPECT_EQ(ukm_metrics.at(ukm::builders::PrefetchProxy_PrefetchedResource::
                               kFetchDurationMSName),
            200);

  ASSERT_TRUE(ukm_metrics.find(ukm::builders::PrefetchProxy_PrefetchedResource::
                                   kISPFilteringStatusName) !=
              ukm_metrics.end());
  EXPECT_EQ(ukm_metrics.at(ukm::builders::PrefetchProxy_PrefetchedResource::
                               kISPFilteringStatusName),
            static_cast<int>(PrefetchProbeResult::kDNSProbeSuccess));

  // These fields are not set and should not be in the UKM event.
  EXPECT_TRUE(ukm_metrics.find(ukm::builders::PrefetchProxy_PrefetchedResource::
                                   kNavigationStartToFetchStartMSName) ==
              ukm_metrics.end());
  EXPECT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkPositionName) ==
      ukm_metrics.end());
}

TEST_F(PrefetchContainerTest, PrefetchProxyPrefetchedResourceUkm_NothingSet) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(), nullptr);
  prefetch_container.reset();

  auto ukm_entries = ukm_recorder.GetEntries(
      ukm::builders::PrefetchProxy_PrefetchedResource::kEntryName,
      {
          ukm::builders::PrefetchProxy_PrefetchedResource::kResourceTypeName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kStatusName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkClickedName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kDataLengthName,
          ukm::builders::PrefetchProxy_PrefetchedResource::kFetchDurationMSName,
          ukm::builders::PrefetchProxy_PrefetchedResource::
              kISPFilteringStatusName,
      });

  ASSERT_EQ(ukm_entries.size(), 1U);
  EXPECT_EQ(ukm_entries[0].source_id, ukm::kInvalidSourceId);

  const auto& ukm_metrics = ukm_entries[0].metrics;
  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kResourceTypeName) !=
      ukm_metrics.end());
  EXPECT_EQ(
      ukm_metrics.at(
          ukm::builders::PrefetchProxy_PrefetchedResource::kResourceTypeName),
      /*mainfrmae*/ 1);

  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kStatusName) !=
      ukm_metrics.end());
  EXPECT_EQ(ukm_metrics.at(
                ukm::builders::PrefetchProxy_PrefetchedResource::kStatusName),
            static_cast<int>(PrefetchStatus::kPrefetchNotStarted));

  ASSERT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkClickedName) !=
      ukm_metrics.end());
  EXPECT_EQ(
      ukm_metrics.at(
          ukm::builders::PrefetchProxy_PrefetchedResource::kLinkClickedName),
      0);

  EXPECT_TRUE(
      ukm_metrics.find(
          ukm::builders::PrefetchProxy_PrefetchedResource::kDataLengthName) ==
      ukm_metrics.end());
  EXPECT_TRUE(ukm_metrics.find(ukm::builders::PrefetchProxy_PrefetchedResource::
                                   kFetchDurationMSName) == ukm_metrics.end());
  EXPECT_TRUE(ukm_metrics.find(ukm::builders::PrefetchProxy_PrefetchedResource::
                                   kISPFilteringStatusName) ==
              ukm_metrics.end());
}

}  // namespace content
