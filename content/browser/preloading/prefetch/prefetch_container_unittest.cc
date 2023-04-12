// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_test_utils.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
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
      blink::mojom::Referrer(), absl::nullopt, nullptr);

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
      blink::mojom::Referrer(), absl::nullopt, nullptr);

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
      blink::mojom::Referrer(), absl::nullopt, nullptr);

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
      blink::mojom::Referrer(), absl::nullopt, nullptr);

  prefetch_container.TakeStreamingURLLoader(
      MakeServableStreamingURLLoaderForTest(
          network::mojom::URLResponseHead::New(), "test body"));

  task_environment()->FastForwardBy(base::Minutes(2));

  EXPECT_FALSE(prefetch_container.IsPrefetchServable(base::Minutes(1)));
  EXPECT_TRUE(prefetch_container.IsPrefetchServable(base::Minutes(3)));
  EXPECT_TRUE(prefetch_container.GetHead());
}

TEST_F(PrefetchContainerTest, CookieListener) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");
  const GURL kTestUrl3 = GURL("https://test3.com");

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), kTestUrl1,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt, nullptr);

  // Add redirect hops. Each hop will have its own cookie listener.
  prefetch_container.AddRedirectHop(kTestUrl2);
  prefetch_container.AddRedirectHop(kTestUrl3);

  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl1));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl2));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl3));

  prefetch_container.RegisterCookieListener(kTestUrl1, cookie_manager());
  prefetch_container.RegisterCookieListener(kTestUrl2, cookie_manager());
  prefetch_container.RegisterCookieListener(kTestUrl3, cookie_manager());

  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl1));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl2));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl3));

  ASSERT_TRUE(SetCookie(kTestUrl1, "test-cookie1"));

  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl1));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl2));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl3));

  ASSERT_TRUE(SetCookie(kTestUrl2, "test-cookie2"));

  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl1));
  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl2));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl3));

  prefetch_container.StopAllCookieListeners();
  ASSERT_TRUE(SetCookie(kTestUrl2, "test-cookie3"));

  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl1));
  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl2));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl3));
}

TEST_F(PrefetchContainerTest, CookieCopy) {
  const GURL kTestUrl = GURL("https://test.com");
  base::HistogramTester histogram_tester;
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), kTestUrl,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt, nullptr);
  prefetch_container.RegisterCookieListener(kTestUrl, cookie_manager());

  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());

  prefetch_container.OnIsolatedCookieCopyStart();

  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());

  // Once the cookie copy process has started, we should stop the cookie
  // listener.
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl));

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

TEST_F(PrefetchContainerTest, CookieCopyWithRedirects) {
  const GURL kTestUrl = GURL("https://test.com");
  const GURL kRedirectUrl1 = GURL("https://redirect1.com");
  const GURL kRedirectUrl2 = GURL("https://redirect2.com");
  base::HistogramTester histogram_tester;
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), kTestUrl,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt, nullptr);

  prefetch_container.AddRedirectHop(kRedirectUrl1);
  prefetch_container.AddRedirectHop(kRedirectUrl2);

  prefetch_container.RegisterCookieListener(kTestUrl, cookie_manager());
  prefetch_container.RegisterCookieListener(kRedirectUrl1, cookie_manager());
  prefetch_container.RegisterCookieListener(kRedirectUrl2, cookie_manager());

  EXPECT_EQ(prefetch_container.GetCurrentURLToServe(), kTestUrl);

  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());
  prefetch_container.OnIsolatedCookieCopyStart();
  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());

  // Once the cookie copy process has started, all cookie listeners are stopped.
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));
  ASSERT_TRUE(SetCookie(kRedirectUrl1, "test-cookie"));
  ASSERT_TRUE(SetCookie(kRedirectUrl2, "test-cookie"));

  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl));
  EXPECT_FALSE(
      prefetch_container.HaveDefaultContextCookiesChanged(kRedirectUrl1));
  EXPECT_FALSE(
      prefetch_container.HaveDefaultContextCookiesChanged(kRedirectUrl2));

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

  // Simulate copying cookies for the next redirect hop.
  prefetch_container.AdvanceCurrentURLToServe();
  EXPECT_EQ(prefetch_container.GetCurrentURLToServe(), kRedirectUrl1);
  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());

  prefetch_container.OnIsolatedCookieCopyStart();
  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());
  task_environment()->FastForwardBy(base::Milliseconds(10));

  prefetch_container.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());

  prefetch_container.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));

  callback_called = false;
  prefetch_container.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  prefetch_container.OnIsolatedCookieCopyComplete();
  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());
  EXPECT_TRUE(callback_called);

  // Simulate copying cookies for the last redirect hop.
  prefetch_container.AdvanceCurrentURLToServe();
  EXPECT_EQ(prefetch_container.GetCurrentURLToServe(), kRedirectUrl2);
  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());

  prefetch_container.OnIsolatedCookieCopyStart();
  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());
  task_environment()->FastForwardBy(base::Milliseconds(10));

  prefetch_container.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(prefetch_container.IsIsolatedCookieCopyInProgress());

  prefetch_container.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));

  callback_called = false;
  prefetch_container.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  prefetch_container.OnIsolatedCookieCopyComplete();
  EXPECT_FALSE(prefetch_container.IsIsolatedCookieCopyInProgress());
  EXPECT_TRUE(callback_called);

  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieReadTime",
      base::Milliseconds(10), 3);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWriteTime",
      base::Milliseconds(60), 3);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::Milliseconds(30), 3);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      base::Milliseconds(70), 3);
}

TEST_F(PrefetchContainerTest, PrefetchProxyPrefetchedResourceUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(), absl::nullopt, nullptr);

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
  prefetch_container->OnReturnPrefetchToServe(/*served=*/true);

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
          blink::mojom::Referrer(), absl::nullopt, nullptr);
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

TEST_F(PrefetchContainerTest, EligibilityCheck) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");

  base::HistogramTester histogram_tester;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &web_contents()->GetPrimaryPage().GetMainDocument());

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), kTestUrl1,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt,
      prefetch_document_manager->GetWeakPtr());

  // Mark initial prefetch as eligible
  prefetch_container.OnEligibilityCheckComplete(kTestUrl1, true, absl::nullopt);

  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);

  // Add a redirect, register a callback for it, and then mark it as eligible.
  prefetch_container.AddRedirectHop(kTestUrl2);

  base::RunLoop run_loop;
  prefetch_container.SetOnEligibilityCheckCompleteCallback(
      kTestUrl2, base::BindOnce(
                     [](base::RunLoop* run_loop, bool is_eligible) {
                       EXPECT_TRUE(is_eligible);
                       run_loop->Quit();
                     },
                     &run_loop));

  prefetch_container.OnEligibilityCheckComplete(kTestUrl2, true, absl::nullopt);
  run_loop.Run();

  // Referring page metrics is only incremented for the original prefetch URL
  // and not any redirects.
  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);
}

TEST_F(PrefetchContainerTest, IneligibleRedirect) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");

  base::HistogramTester histogram_tester;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &web_contents()->GetPrimaryPage().GetMainDocument());

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), kTestUrl1,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt,
      prefetch_document_manager->GetWeakPtr());

  // Mark initial prefetch as eligible
  prefetch_container.OnEligibilityCheckComplete(kTestUrl1, true, absl::nullopt);

  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);

  // Add a redirect, register a callback for it, and then mark it as ineligible.
  prefetch_container.AddRedirectHop(kTestUrl2);

  base::RunLoop run_loop;
  prefetch_container.SetOnEligibilityCheckCompleteCallback(
      kTestUrl2, base::BindOnce(
                     [](base::RunLoop* run_loop, bool is_eligible) {
                       EXPECT_FALSE(is_eligible);
                       run_loop->Quit();
                     },
                     &run_loop));

  prefetch_container.OnEligibilityCheckComplete(
      kTestUrl2, false, PrefetchStatus::kPrefetchNotEligibleUserHasCookies);
  run_loop.Run();

  // Ineligible redirects are treated as failed prefetches, and not ineligible
  // prefetches.
  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchFailedIneligibleRedirect);
}

TEST_F(PrefetchContainerTest, NoVarySearchHelper) {
  const GURL kTestUrl = GURL("https://test.com?a=2&b=3");

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), kTestUrl,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt, nullptr);

  // Set up NoVarySearchHelper.
  scoped_refptr<NoVarySearchHelper> no_vary_search_helper =
      base::MakeRefCounted<NoVarySearchHelper>();

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->parsed_headers = network::mojom::ParsedHeaders::New();
  head->parsed_headers->no_vary_search_with_parse_error =
      network::mojom::NoVarySearchWithParseError::NewNoVarySearch(
          network::mojom::NoVarySearch::New());
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->vary_on_key_order = true;
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  no_vary_search_helper->AddUrl(kTestUrl, *head);
  prefetch_container.SetNoVarySearchHelper(no_vary_search_helper);

  // Register Cookie listener for the prefetch URL.
  prefetch_container.RegisterCookieListener(kTestUrl, cookie_manager());

  // Can use either the exact URL or a matching URL based on the
  // NoVarySearchHelper.
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl));
  EXPECT_FALSE(prefetch_container.HaveDefaultContextCookiesChanged(
      GURL("https://test.com?a=2")));

  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));

  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(kTestUrl));
  EXPECT_TRUE(prefetch_container.HaveDefaultContextCookiesChanged(
      GURL("https://test.com?a=2")));
}

TEST_F(PrefetchContainerTest, BlockUntilHeadHistograms) {
  struct TestCase {
    blink::mojom::SpeculationEagerness eagerness;
    bool block_until_head;
    base::TimeDelta block_until_head_duration;
    bool served;
  };

  std::vector<TestCase> test_cases{
      {blink::mojom::SpeculationEagerness::kEager, true, base::Milliseconds(10),
       true},
      {blink::mojom::SpeculationEagerness::kModerate, false,
       base::Milliseconds(20), false},
      {blink::mojom::SpeculationEagerness::kConservative, true,
       base::Milliseconds(40), false}};

  base::HistogramTester histogram_tester;
  for (const auto& test_case : test_cases) {
    PrefetchContainer prefetch_container(
        GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
        PrefetchType(/*use_isolated_network_context=*/true,
                     /*use_prefetch_proxy=*/true, test_case.eagerness),
        blink::mojom::Referrer(), absl::nullopt, nullptr);

    prefetch_container.OnGetPrefetchToServe(test_case.block_until_head);
    if (test_case.block_until_head) {
      task_environment()->FastForwardBy(test_case.block_until_head_duration);
    }
    prefetch_container.OnReturnPrefetchToServe(test_case.served);
  }

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.Eager", true, 1);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.Eager", false,
      0);

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.Moderate", true,
      0);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.Moderate", false,
      1);

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.Conservative",
      true, 1);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.Conservative",
      false, 0);

  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.BlockUntilHeadDuration.Served.Eager",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.Eager", 0);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.BlockUntilHeadDuration.Served.Moderate", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.Moderate", 0);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.BlockUntilHeadDuration.Served.Conservative", 0);
  histogram_tester.ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.Conservative",
      base::Milliseconds(40), 1);
}

TEST_F(PrefetchContainerTest, RecordRedirectChainSize) {
  base::HistogramTester histogram_tester;

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), absl::nullopt, nullptr);

  prefetch_container.AddRedirectHop(GURL("https://redirect1.com"));
  prefetch_container.AddRedirectHop(GURL("https://redirect2.com"));
  prefetch_container.OnPrefetchComplete();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.RedirectChainSize", 3, 1);
}

}  // namespace content
