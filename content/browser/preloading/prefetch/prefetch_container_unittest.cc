// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/loading_params.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace content {

class PrefetchContainerTestBase : public RenderViewHostTestHarness,
                                  public WithPrefetchRearchParam {
 public:
  explicit PrefetchContainerTestBase(PrefetchRearchParam param)
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        WithPrefetchRearchParam(param) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    InitRearchFeatures();

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    RenderViewHostTestHarness::TearDown();
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

  RenderFrameHostImpl* main_rfhi() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

  struct SpeculationRulesPrefetchContainerOptions {
    blink::mojom::SpeculationEagerness eagerness;
    SpeculationRulesTags speculation_rules_tags;
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager;
    blink::DocumentToken document_token;
  };

  std::unique_ptr<PrefetchContainer> CreateSpeculationRulesPrefetchContainer(
      const GURL& prefetch_url,
      SpeculationRulesPrefetchContainerOptions options = {}) {
    return PrefetchContainer::CreateForTesting(
        PrefetchRequest::CreateRendererInitiated(
            *main_rfhi(), options.document_token, prefetch_url,
            PrefetchType(PreloadingTriggerType::kSpeculationRule,
                         /*use_prefetch_proxy=*/true, options.eagerness),
            blink::mojom::Referrer(),
            std::make_optional(std::move(options.speculation_rules_tags)),
            /*no_vary_search_hint=*/std::nullopt, /*priority=*/std::nullopt,
            options.prefetch_document_manager,
            PreloadPipelineInfo::Create(
                /*planned_max_preloading_type=*/PreloadingType::kPrefetch)));
  }

  std::unique_ptr<PrefetchContainer> CreateEmbedderPrefetchContainer(
      const GURL& prefetch_url,
      const std::optional<url::Origin> referring_origin = std::nullopt,
      bool use_prefetch_proxy = true) {
    return PrefetchContainer::CreateForTesting(
        PrefetchRequest::CreateBrowserInitiated(
            *web_contents(), prefetch_url,
            PrefetchType(PreloadingTriggerType::kEmbedder, use_prefetch_proxy),
            test::kPreloadingEmbedderHistgramSuffixForTesting,
            blink::mojom::Referrer(), std::move(referring_origin),
            /*no_vary_search_hint=*/std::nullopt, /*priority=*/std::nullopt,
            PreloadPipelineInfo::Create(
                /*planned_max_preloading_type=*/PreloadingType::kPrefetch),
            /*attempt=*/nullptr));
  }

  std::unique_ptr<PrefetchContainer> CreateBrowserContextPrefetchContainer(
      const GURL& prefetch_url,
      const net::HttpRequestHeaders& additional_headers = {},
      bool should_append_additional_headers = true) {
    return PrefetchContainer::CreateForTesting(
        PrefetchRequest::CreateBrowserInitiatedWithoutWebContents(
            browser_context(), prefetch_url,
            PrefetchType(PreloadingTriggerType::kEmbedder,
                         /*use_prefetch_proxy=*/true),
            test::kPreloadingEmbedderHistgramSuffixForTesting,
            blink::mojom::Referrer(),
            /*javascript_enabled=*/true,
            /*referring_origin=*/std::nullopt,
            /*no_vary_search_hint=*/std::nullopt,
            /*priority=*/PrefetchPriority::kHighest,
            /*attempt=*/nullptr, additional_headers,
            /*request_status_listener=*/nullptr, base::Minutes(10),
            should_append_additional_headers,
            /*should_disable_block_until_head_timeout=*/false,
            /*should_bypass_http_cache=*/false));
  }
  bool SetCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateForTesting(url, value, base::Time::Now()));

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
    task_environment()->RunUntilIdle();

    return result;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

namespace {

// Add a redirect hop with dummy redirect info that should be good enough in
// most cases.
void AddRedirectHop(PrefetchContainer* container, const GURL& url) {
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = url;
  redirect_info.new_site_for_cookies = net::SiteForCookies::FromUrl(url);
  container->AddRedirectHop(redirect_info);
}

}  // namespace

class PrefetchContainerTest
    : public PrefetchContainerTestBase,
      public ::testing::WithParamInterface<PrefetchRearchParam> {
 public:
  PrefetchContainerTest() : PrefetchContainerTestBase(GetParam()) {}

 private:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kIgnoreSignedInState};
};

class PrefetchContainerXClientDataHeaderTest
    : public PrefetchContainerTestBase,
      // bool: In incognito or not.
      public ::testing::WithParamInterface<
          std::tuple<PrefetchRearchParam, bool>> {
 public:
  PrefetchContainerXClientDataHeaderTest()
      : PrefetchContainerTestBase(std::get<0>(GetParam())) {}

 private:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kIgnoreSignedInState};

 protected:
  bool IsIncognito() const { return std::get<1>(GetParam()); }
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto browser_context = std::make_unique<TestBrowserContext>();
    browser_context->set_is_off_the_record(IsIncognito());
    return browser_context;
  }
};

TEST_P(PrefetchContainerXClientDataHeaderTest,
       AddHeaderForEligibleUrlOnlyWhenNotInIncognito) {
  const GURL kTestEligibleUrl = GURL("https://google.com");

  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(kTestEligibleUrl);
  variations::VariationsIdsProvider::GetInstance()->ForceVariationIdsForTesting(
      {"1"}, {"2"});

  prefetch_container->MakeResourceRequest();
  auto* request = prefetch_container->GetResourceRequest();
  // Don't add the header when in incognito mode.
  EXPECT_EQ(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader),
      !IsIncognito());
}

TEST_P(PrefetchContainerXClientDataHeaderTest,
       NeverAddHeaderForNonEligibleUrl) {
  const GURL kTestNonEligibleUrl = GURL("https://non-eligible.com");

  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(kTestNonEligibleUrl);
  variations::VariationsIdsProvider::GetInstance()->ForceVariationIdsForTesting(
      {"1"}, {"2"});

  prefetch_container->MakeResourceRequest();
  auto* request = prefetch_container->GetResourceRequest();
  // Don't ever add the header.
  EXPECT_FALSE(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader));
}

TEST_P(PrefetchContainerXClientDataHeaderTest,
       AddHeaderForEligibleRedirectUrlOnlyWhenNotInIncognito) {
  const GURL kTestNonEligibleUrl = GURL("https://non-eligible.com");
  const GURL kTestEligibleUrl = GURL("https://google.com");

  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(kTestNonEligibleUrl);
  variations::VariationsIdsProvider::GetInstance()->ForceVariationIdsForTesting(
      {"1"}, {"2"});

  prefetch_container->MakeResourceRequest();
  auto* request = prefetch_container->GetResourceRequest();
  // Don't ever add the header.
  EXPECT_FALSE(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader));

  AddRedirectHop(prefetch_container.get(), kTestEligibleUrl);
  EXPECT_EQ(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader),
      !IsIncognito());
}

TEST_P(PrefetchContainerXClientDataHeaderTest,
       NeverAddHeaderForNonEligibleRedirectUrl) {
  const GURL kTestNonEligibleUrl1 = GURL("https://non-eligible1.com");
  const GURL kTestNonEligibleUrl2 = GURL("https://non-eligible2.com");

  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(kTestNonEligibleUrl1);
  variations::VariationsIdsProvider::GetInstance()->ForceVariationIdsForTesting(
      {"1"}, {"2"});

  prefetch_container->MakeResourceRequest();
  auto* request = prefetch_container->GetResourceRequest();
  // Don't ever add the header.
  EXPECT_FALSE(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader));

  AddRedirectHop(prefetch_container.get(), kTestNonEligibleUrl2);
  EXPECT_FALSE(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader));
}

TEST_P(PrefetchContainerXClientDataHeaderTest,
       NeverAddHeaderIfBrowserContextSettingIsOff) {
  const GURL kTestEligibleUrl = GURL("https://google.com");
  net::HttpRequestHeaders additional_headers;
  additional_headers.SetHeader(variations::kClientDataHeader,
                               "test_client_data");

  auto prefetch_container = CreateBrowserContextPrefetchContainer(
      kTestEligibleUrl, additional_headers, false);
  variations::VariationsIdsProvider::GetInstance()->ForceVariationIdsForTesting(
      {"1"}, {"2"});

  prefetch_container->MakeResourceRequest();
  auto* request = prefetch_container->GetResourceRequest();

  EXPECT_TRUE(request->headers.HasHeader(variations::kClientDataHeader));
  // It should be treated as non-trusted.
  EXPECT_FALSE(
      request->cors_exempt_headers.HasHeader(variations::kClientDataHeader));
  // Make sure it doesn't get overridden.
  EXPECT_EQ(request->headers.GetHeader(variations::kClientDataHeader).value(),
            "test_client_data");
}

INSTANTIATE_TEST_SUITE_P(
    PrefetchContainerXClientDataTests,
    PrefetchContainerXClientDataHeaderTest,
    testing::Combine(testing::ValuesIn(PrefetchRearchParam::Params()),
                     ::testing::Bool()));

TEST_P(PrefetchContainerTest, CreatePrefetchContainer) {
  blink::DocumentToken document_token;
  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(
      GURL("https://test.com"),
      {.eagerness = blink::mojom::SpeculationEagerness::kImmediate,
       .speculation_rules_tags = SpeculationRulesTags({"example"}),
       .document_token = document_token});

  ASSERT_TRUE(prefetch_container->request().GetRendererInitiatorInfo());
  EXPECT_EQ(prefetch_container->request()
                .GetRendererInitiatorInfo()
                ->GetRenderFrameHostId(),
            main_rfh()->GetGlobalId());
  EXPECT_EQ(prefetch_container->GetURL(), GURL("https://test.com"));
  EXPECT_EQ(prefetch_container->request().prefetch_type(),
            PrefetchType(PreloadingTriggerType::kSpeculationRule,
                         /*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kImmediate));
  EXPECT_TRUE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  EXPECT_EQ(prefetch_container->key(),
            PrefetchKey(document_token, GURL("https://test.com")));
  EXPECT_FALSE(prefetch_container->GetNonRedirectHead());
}

TEST_P(PrefetchContainerTest, CreatePrefetchContainer_Embedder) {
  auto prefetch_container = CreateEmbedderPrefetchContainer(
      GURL("https://test.com"), /*referring_origin=*/std::nullopt,
      /*use_prefetch_proxy=*/false);

  ASSERT_FALSE(prefetch_container->request().GetRendererInitiatorInfo());
  EXPECT_EQ(prefetch_container->GetURL(), GURL("https://test.com"));
  EXPECT_EQ(prefetch_container->request().prefetch_type(),
            PrefetchType(PreloadingTriggerType::kEmbedder,
                         /*use_prefetch_proxy=*/false));
  EXPECT_FALSE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  EXPECT_EQ(prefetch_container->key(),
            PrefetchKey(std::nullopt, GURL("https://test.com")));
  EXPECT_FALSE(prefetch_container->GetNonRedirectHead());
  // Embedder-initiated prefetch shouldn't include any tag.
  EXPECT_FALSE(prefetch_container->request().speculation_rules_tags());
}

TEST_P(PrefetchContainerTest, PrefetchStatus) {
  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));

  EXPECT_FALSE(prefetch_container->HasPrefetchStatus());

  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);

  EXPECT_TRUE(prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

TEST_P(PrefetchContainerTest, IsDecoy) {
  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));

  EXPECT_FALSE(prefetch_container->IsDecoy());

  prefetch_container->SetIsDecoy(true);
  EXPECT_TRUE(prefetch_container->IsDecoy());
}

TEST_P(PrefetchContainerTest, Servable) {
  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));

  prefetch_container->SimulatePrefetchEligibleForTest();
  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  task_environment()->FastForwardBy(base::Minutes(2));

  EXPECT_NE(prefetch_container->GetServableState(base::Minutes(1)),
            PrefetchServableState::kServable);
  EXPECT_EQ(prefetch_container->GetServableState(base::Minutes(3)),
            PrefetchServableState::kServable);
  EXPECT_TRUE(prefetch_container->GetNonRedirectHead());
}

TEST_P(PrefetchContainerTest, CookieListener) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");
  const GURL kTestUrl3 = GURL("https://test3.com");

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(kTestUrl1);

  prefetch_container->MakeResourceRequest();
  prefetch_container->RegisterCookieListener(cookie_manager());

  // Add redirect hops, and register its own cookie listener for each hop.
  AddRedirectHop(prefetch_container.get(), kTestUrl2);
  prefetch_container->RegisterCookieListener(cookie_manager());
  AddRedirectHop(prefetch_container.get(), kTestUrl3);
  prefetch_container->RegisterCookieListener(cookie_manager());

  // Check the cookies for `kTestUrl1`, `kTestUrl2` and `kTestUrl3`,
  // respectively. AdvanceCurrentURLToServe() is used to set the current hop to
  // check the cookies.
  {
    auto serving_handle = prefetch_container->CreateServingHandle();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
  }

  {
    auto serving_handle = prefetch_container->CreateServingHandle();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
  }

  prefetch_container->PauseAllCookieListeners();
  ASSERT_TRUE(SetCookie(kTestUrl1, "test-cookie0"));
  {
    auto serving_handle = prefetch_container->CreateServingHandle();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
  }

  prefetch_container->ResumeAllCookieListeners();
  ASSERT_TRUE(SetCookie(kTestUrl1, "test-cookie1"));

  {
    auto serving_handle = prefetch_container->CreateServingHandle();
    EXPECT_TRUE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
  }

  ASSERT_TRUE(SetCookie(kTestUrl2, "test-cookie2"));

  {
    auto serving_handle = prefetch_container->CreateServingHandle();
    EXPECT_TRUE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_TRUE(serving_handle.HaveDefaultContextCookiesChanged());
    serving_handle.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());
  }
}

TEST_P(PrefetchContainerTest, CookieCopy) {
  const GURL kTestUrl = GURL("https://test.com");
  base::HistogramTester histogram_tester;
  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(kTestUrl);

  prefetch_container->RegisterCookieListener(cookie_manager());

  auto serving_handle = prefetch_container->CreateServingHandle();

  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());

  serving_handle.OnIsolatedCookieCopyStart();

  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());

  // Once the cookie copy process has started, we should stop the cookie
  // listener.
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));
  EXPECT_FALSE(serving_handle.HaveDefaultContextCookiesChanged());

  task_environment()->FastForwardBy(base::Milliseconds(10));
  serving_handle.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));

  // The URL interceptor checks on the cookie copy status when trying to serve a
  // prefetch. If its still in progress, it registers a callback to be called
  // once the copy is complete.
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());
  serving_handle.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));
  bool callback_called = false;
  serving_handle.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  serving_handle.OnIsolatedCookieCopyComplete();

  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());
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

TEST_P(PrefetchContainerTest, CookieCopyWithRedirects) {
  const GURL kTestUrl = GURL("https://test.com");
  const GURL kRedirectUrl1 = GURL("https://redirect1.com");
  const GURL kRedirectUrl2 = GURL("https://redirect2.com");
  base::HistogramTester histogram_tester;
  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(kTestUrl);
  prefetch_container->MakeResourceRequest();
  prefetch_container->RegisterCookieListener(cookie_manager());

  AddRedirectHop(prefetch_container.get(), kRedirectUrl1);
  prefetch_container->RegisterCookieListener(cookie_manager());

  AddRedirectHop(prefetch_container.get(), kRedirectUrl2);
  prefetch_container->RegisterCookieListener(cookie_manager());

  auto serving_handle = prefetch_container->CreateServingHandle();

  EXPECT_EQ(serving_handle.GetCurrentURLToServe(), kTestUrl);

  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());
  serving_handle.OnIsolatedCookieCopyStart();
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());

  // Once the cookie copy process has started, all cookie listeners are stopped.
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));
  ASSERT_TRUE(SetCookie(kRedirectUrl1, "test-cookie"));
  ASSERT_TRUE(SetCookie(kRedirectUrl2, "test-cookie"));

  // Check the cookies for `kTestUrl`, `kRedirectUrl1` and `kRedirectUrl2`,
  // respectively. AdvanceCurrentURLToServe() is used to set the current
  // hop to check the cookies.
  {
    auto serving_handle1 = prefetch_container->CreateServingHandle();
    EXPECT_FALSE(serving_handle1.HaveDefaultContextCookiesChanged());
    serving_handle1.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle1.HaveDefaultContextCookiesChanged());
    serving_handle1.AdvanceCurrentURLToServe();
    EXPECT_FALSE(serving_handle1.HaveDefaultContextCookiesChanged());
  }

  task_environment()->FastForwardBy(base::Milliseconds(10));
  serving_handle.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));

  // The URL interceptor checks on the cookie copy status when trying to serve a
  // prefetch. If its still in progress, it registers a callback to be called
  // once the copy is complete.
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());
  serving_handle.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));
  bool callback_called = false;
  serving_handle.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  serving_handle.OnIsolatedCookieCopyComplete();

  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());
  EXPECT_TRUE(callback_called);

  // Simulate copying cookies for the next redirect hop.
  serving_handle.AdvanceCurrentURLToServe();
  EXPECT_EQ(serving_handle.GetCurrentURLToServe(), kRedirectUrl1);
  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());

  serving_handle.OnIsolatedCookieCopyStart();
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());
  task_environment()->FastForwardBy(base::Milliseconds(10));

  serving_handle.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());

  serving_handle.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));

  callback_called = false;
  serving_handle.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  serving_handle.OnIsolatedCookieCopyComplete();
  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());
  EXPECT_TRUE(callback_called);

  // Simulate copying cookies for the last redirect hop.
  serving_handle.AdvanceCurrentURLToServe();
  EXPECT_EQ(serving_handle.GetCurrentURLToServe(), kRedirectUrl2);
  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());

  serving_handle.OnIsolatedCookieCopyStart();
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());
  task_environment()->FastForwardBy(base::Milliseconds(10));

  serving_handle.OnIsolatedCookiesReadCompleteAndWriteStart();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(serving_handle.IsIsolatedCookieCopyInProgress());

  serving_handle.OnInterceptorCheckCookieCopy();
  task_environment()->FastForwardBy(base::Milliseconds(40));

  callback_called = false;
  serving_handle.SetOnCookieCopyCompleteCallback(
      base::BindOnce([](bool* callback_called) { *callback_called = true; },
                     &callback_called));

  serving_handle.OnIsolatedCookieCopyComplete();
  EXPECT_FALSE(serving_handle.IsIsolatedCookieCopyInProgress());
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

TEST_P(PrefetchContainerTest, PrefetchProxyPrefetchedResourceUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));

  prefetch_container->SimulatePrefetchEligibleForTest();

  network::URLLoaderCompletionStatus completion_status;
  completion_status.encoded_data_length = 100;
  completion_status.completion_time =
      base::TimeTicks() + base::Milliseconds(200);

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->load_timing.request_start = base::TimeTicks();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        std::move(head), "test body",
                                        std::move(completion_status));

  // Simulates the URL of the prefetch being navigated to and the prefetch being
  // considered for serving.
  prefetch_container->OnUnregisterCandidate(
      GURL("https://test.com"),
      /*is_served=*/true, PrefetchPotentialCandidateServingResult::kServed,
      /*is_nav_prerender=*/false,
      /*blocked_duration=*/std::nullopt);

  // Simulate a successful DNS probe for this prefetch. Not this will also
  // update the status of the prefetch to
  // |PrefetchStatus::kPrefetchUsedProbeSuccess|.
  prefetch_container->CreateServingHandle().OnPrefetchProbeResult(
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

TEST_P(PrefetchContainerTest, PrefetchProxyPrefetchedResourceUkm_NothingSet) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));
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

TEST_P(PrefetchContainerTest, EligibilityCheck) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");

  base::HistogramTester histogram_tester;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &web_contents()->GetPrimaryPage().GetMainDocument());

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(
      kTestUrl1,
      {.prefetch_document_manager = prefetch_document_manager->GetWeakPtr()});

  prefetch_container->MakeResourceRequest();

  // Mark initial prefetch as eligible
  prefetch_container->OnEligibilityCheckComplete(
      PreloadingEligibility::kEligible);

  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);

  // Add a redirect, register a callback for it, and then mark it as eligible.
  AddRedirectHop(prefetch_container.get(), kTestUrl2);
  prefetch_container->OnEligibilityCheckComplete(
      PreloadingEligibility::kEligible);

  // Referring page metrics is only incremented for the original prefetch URL
  // and not any redirects.
  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);
}

TEST_P(PrefetchContainerTest, IneligibleRedirect) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");

  base::HistogramTester histogram_tester;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &web_contents()->GetPrimaryPage().GetMainDocument());

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(
      kTestUrl1,
      {.prefetch_document_manager = prefetch_document_manager->GetWeakPtr()});

  prefetch_container->MakeResourceRequest();

  // Mark initial prefetch as eligible
  prefetch_container->OnEligibilityCheckComplete(
      PreloadingEligibility::kEligible);

  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);

  // Add a redirect, register a callback for it, and then mark it as ineligible.
  AddRedirectHop(prefetch_container.get(), kTestUrl2);
  prefetch_container->OnEligibilityCheckComplete(
      PreloadingEligibility::kUserHasCookies);

  // Ineligible redirects are treated as failed prefetches, and not ineligible
  // prefetches.
  EXPECT_EQ(prefetch_document_manager->GetReferringPageMetrics()
                .prefetch_eligible_count,
            1);
  EXPECT_EQ(prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchFailedIneligibleRedirect);
}

TEST_P(PrefetchContainerTest, BlockUntilHeadHistograms) {
  struct TestCase {
    PrefetchType prefetch_type;
    bool is_served;
    bool is_nav_prerender;
    std::optional<base::TimeDelta> blocked_duration;
  };

  std::vector<TestCase> test_cases{
      {
          .prefetch_type =
              PrefetchType(PreloadingTriggerType::kSpeculationRule,
                           /*use_prefetch_proxy=*/true,
                           blink::mojom::SpeculationEagerness::kImmediate),
          .is_served = true,
          .is_nav_prerender = false,
          .blocked_duration = std::nullopt,
      },
      {
          .prefetch_type =
              PrefetchType(PreloadingTriggerType::kSpeculationRule,
                           /*use_prefetch_proxy=*/true,
                           blink::mojom::SpeculationEagerness::kEager),
          .is_served = true,
          .is_nav_prerender = false,
          .blocked_duration = base::Milliseconds(10),
      },
      {
          .prefetch_type =
              PrefetchType(PreloadingTriggerType::kSpeculationRule,
                           /*use_prefetch_proxy=*/true,
                           blink::mojom::SpeculationEagerness::kModerate),
          .is_served = true,
          .is_nav_prerender = false,
          .blocked_duration = base::Milliseconds(10),
      },
      {
          .prefetch_type =
              PrefetchType(PreloadingTriggerType::kSpeculationRule,
                           /*use_prefetch_proxy=*/true,
                           blink::mojom::SpeculationEagerness::kConservative),
          .is_served = false,
          .is_nav_prerender = false,
          .blocked_duration = std::nullopt,
      },
      {.prefetch_type = PrefetchType(PreloadingTriggerType::kEmbedder,
                                     /*use_prefetch_proxy=*/true),
       .is_served = false,
       .is_nav_prerender = false,
       .blocked_duration = base::Milliseconds(20)},
  };

  const GURL prefetch_url = GURL("https://test.example/?nvsparam=1");
  const GURL navigated_url = GURL("https://test.example/");

  base::HistogramTester histogram_tester;
  for (const auto& test_case : test_cases) {
    auto prefetch_container = [&] {
      if (test_case.prefetch_type.IsRendererInitiated()) {
        return CreateSpeculationRulesPrefetchContainer(
            prefetch_url,
            {.eagerness = test_case.prefetch_type.GetEagerness()});
      } else {
        return CreateEmbedderPrefetchContainer(prefetch_url);
      }
    }();

    // For `PrefetchPotentialCandidateServingResult`, provides placeholder value
    // which is consistent with `is_served`.
    prefetch_container->OnUnregisterCandidate(
        navigated_url, test_case.is_served,
        test_case.is_served ? PrefetchPotentialCandidateServingResult::kServed
                            : PrefetchPotentialCandidateServingResult::
                                  kNotServedBlockUntilHeadTimeout,
        test_case.is_nav_prerender, test_case.blocked_duration);
  }

  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_Immediate2",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "NonPrerender.SpeculationRule_Immediate2",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "Prerender.SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served."
      "SpeculationRule_Immediate2",
      base::Milliseconds(0), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed."
      "SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "Served.SpeculationRule_Immediate2",
      base::Milliseconds(0), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "NotServed.SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender.Served."
      "SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender."
      "NotServed.SpeculationRule_Immediate2",
      0);

  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_Eager2",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_Moderate",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "NonPrerender.SpeculationRule_Immediate2",
      false, 1);

  histogram_tester.ExpectTotalCount(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "Prerender.SpeculationRule_Eager2",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served."
      "SpeculationRule_Eager2",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed."
      "SpeculationRule_Eager2",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "Served.SpeculationRule_Eager2",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "NotServed.SpeculationRule_Eager2",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender.Served."
      "SpeculationRule_Eager2",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender."
      "NotServed.SpeculationRule_Eager2",
      0);

  histogram_tester.ExpectTotalCount(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "Prerender.SpeculationRule_Moderate",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served."
      "SpeculationRule_Moderate",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed."
      "SpeculationRule_Moderate",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "Served.SpeculationRule_Moderate",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "NotServed.SpeculationRule_Moderate",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender.Served."
      "SpeculationRule_Moderate",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender."
      "NotServed.SpeculationRule_Moderate",
      0);

  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_Conservative",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "NonPrerender.SpeculationRule_Conservative",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "Prerender.SpeculationRule_Conservative",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served."
      "SpeculationRule_Conservative",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed."
      "SpeculationRule_Conservative",
      base::Milliseconds(0), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "Served.SpeculationRule_Conservative",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "NotServed.SpeculationRule_Conservative",
      base::Milliseconds(0), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender.Served."
      "SpeculationRule_Conservative",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender."
      "NotServed.SpeculationRule_Conservative",
      0);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Prefetch.PrefetchMatchingBlockedNavigation."
                    "PerMatchingCandidate.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      true, 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.PrefetchMatchingBlockedNavigation."
                    "PerMatchingCandidate.Prerender.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate."
                    "Served.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate."
                    "NotServed.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      base::Milliseconds(20), 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate."
                    "NonPrerender.Served.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate."
                    "NonPrerender.NotServed.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      base::Milliseconds(20), 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate."
                    "Prerender.Served.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate."
                    "Prerender.NotServed.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
}

TEST_P(PrefetchContainerTest, BlockUntilHeadHistograms_Prerender) {
  struct TestCase {
    PrefetchType prefetch_type;
    bool is_served;
    bool is_nav_prerender;
    std::optional<base::TimeDelta> blocked_duration;
  };

  TestCase test_case = {
      .prefetch_type =
          PrefetchType(PreloadingTriggerType::kSpeculationRule,
                       /*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kImmediate),
      .is_served = true,
      .is_nav_prerender = true,
      .blocked_duration = base::Milliseconds(100),
  };

  base::HistogramTester histogram_tester;
  const GURL prefetch_url = GURL("https://test.com/?nvsparam=1");
  const GURL navigated_url = GURL("https://test.com/");

  auto prefetch_container = [&] {
    if (test_case.prefetch_type.IsRendererInitiated()) {
      return CreateSpeculationRulesPrefetchContainer(
          prefetch_url, {.eagerness = test_case.prefetch_type.GetEagerness()});
    } else {
      return CreateEmbedderPrefetchContainer(prefetch_url);
    }
  }();

  // For `PrefetchPotentialCandidateServingResult`, provides placeholder value
  // which is consistent with `is_served`.
  prefetch_container->OnUnregisterCandidate(
      navigated_url, test_case.is_served,
      test_case.is_served ? PrefetchPotentialCandidateServingResult::kServed
                          : PrefetchPotentialCandidateServingResult::
                                kNotServedBlockUntilHeadTimeout,
      test_case.is_nav_prerender, test_case.blocked_duration);

  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_Immediate2",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "NonPrerender.SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "Prerender.SpeculationRule_Immediate2",
      true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served."
      "SpeculationRule_Immediate2",
      test_case.blocked_duration.value_or(base::Seconds(0)), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed."
      "SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "Served.SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NonPrerender."
      "NotServed.SpeculationRule_Immediate2",
      0);
  histogram_tester.ExpectUniqueTimeSample(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender.Served."
      "SpeculationRule_Immediate2",
      test_case.blocked_duration.value_or(base::Seconds(0)), 1);
  histogram_tester.ExpectTotalCount(
      "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Prerender."
      "NotServed.SpeculationRule_Immediate2",
      0);
}

TEST_P(PrefetchContainerTest, RecordRedirectChainSize) {
  base::HistogramTester histogram_tester;

  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));
  prefetch_container->MakeResourceRequest();

  prefetch_container->SimulatePrefetchEligibleForTest();
  prefetch_container->SimulatePrefetchStartedForTest();

  AddRedirectHop(prefetch_container.get(), GURL("https://redirect1.com"));
  AddRedirectHop(prefetch_container.get(), GURL("https://redirect2.com"));
  prefetch_container->OnDeterminedHead(/*is_successful_determined_head=*/true);
  prefetch_container->OnPrefetchComplete(/*is_success=*/true,
                                         network::URLLoaderCompletionStatus());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.RedirectChainSize", 3, 1);
}

TEST_P(PrefetchContainerTest, IsIsolatedNetworkRequired) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://test.com/referrer"));
  auto prefetch_container_same_origin = CreateSpeculationRulesPrefetchContainer(
      GURL("https://test.com/prefetch"));
  prefetch_container_same_origin->MakeResourceRequest();
  EXPECT_FALSE(prefetch_container_same_origin
                   ->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://other.com/referrer"));
  auto prefetch_container_cross_origin =
      CreateSpeculationRulesPrefetchContainer(
          GURL("https://test.com/prefetch"));
  prefetch_container_cross_origin->MakeResourceRequest();
  EXPECT_TRUE(prefetch_container_cross_origin
                  ->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
}

TEST_P(PrefetchContainerTest, IsIsolatedNetworkRequired_Embedder) {
  auto prefetch_container_default = CreateEmbedderPrefetchContainer(
      GURL("https://test.com/prefetch"), std::nullopt);
  prefetch_container_default->MakeResourceRequest();
  EXPECT_FALSE(prefetch_container_default
                   ->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  auto prefetch_container_same_origin = CreateEmbedderPrefetchContainer(
      GURL("https://test.com/prefetch"),
      url::Origin::Create(GURL("https://test.com/referrer")));
  prefetch_container_same_origin->MakeResourceRequest();
  EXPECT_FALSE(prefetch_container_same_origin
                   ->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  auto prefetch_container_cross_origin = CreateEmbedderPrefetchContainer(
      GURL("https://test.com/prefetch"),
      url::Origin::Create(GURL("https://other.com/referrer")));
  prefetch_container_cross_origin->MakeResourceRequest();
  EXPECT_TRUE(prefetch_container_cross_origin
                  ->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
}

TEST_P(PrefetchContainerTest, IsIsolatedNetworkRequiredWithRedirect) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://test.com/referrer"));

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(
      GURL("https://test.com/prefetch"));

  prefetch_container->MakeResourceRequest();

  EXPECT_FALSE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  AddRedirectHop(prefetch_container.get(), GURL("https://test.com/redirect"));

  EXPECT_FALSE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_FALSE(prefetch_container
                   ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop());

  AddRedirectHop(prefetch_container.get(), GURL("https://m.test.com/redirect"));

  EXPECT_FALSE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_FALSE(prefetch_container
                   ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop());

  AddRedirectHop(prefetch_container.get(), GURL("https://other.com/redirect1"));

  EXPECT_TRUE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_FALSE(prefetch_container
                   ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop());

  AddRedirectHop(prefetch_container.get(), GURL("https://other.com/redirect2"));

  EXPECT_TRUE(
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_TRUE(prefetch_container
                  ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop());
}

TEST_P(PrefetchContainerTest, MultipleStreamingURLLoaders) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");

  base::HistogramTester histogram_tester;

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(kTestUrl1);

  prefetch_container->MakeResourceRequest();

  EXPECT_FALSE(prefetch_container->GetStreamingURLLoader());

  EXPECT_NE(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);
  EXPECT_FALSE(prefetch_container->GetNonRedirectHead());

  prefetch_container->SimulatePrefetchEligibleForTest();
  MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
      prefetch_container.get(), kTestUrl1, kTestUrl2);
  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);
  EXPECT_TRUE(prefetch_container->GetNonRedirectHead());

  // As the prefetch is already completed, the streaming loader is deleted
  // asynchronously.
  EXPECT_TRUE(
      prefetch_container->IsStreamingURLLoaderDeletionScheduledForTesting());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(prefetch_container->GetStreamingURLLoader());

  PrefetchServingHandle serving_handle =
      prefetch_container->CreateServingHandle();

  base::WeakPtr<PrefetchResponseReader> weak_first_response_reader =
      serving_handle.GetCurrentResponseReaderToServeForTesting();
  PrefetchRequestHandler first_request_handler =
      serving_handle.CreateRequestHandler().first;

  base::WeakPtr<PrefetchResponseReader> weak_second_response_reader =
      serving_handle.GetCurrentResponseReaderToServeForTesting();
  PrefetchRequestHandler second_request_handler =
      serving_handle.CreateRequestHandler().first;

  // `CreateRequestHandler()` itself doesn't make the PrefetchContainer
  // non-servable.
  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);
  EXPECT_TRUE(prefetch_container->GetNonRedirectHead());

  std::unique_ptr<PrefetchTestURLLoaderClient> first_serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();
  network::ResourceRequest first_serving_request;
  first_serving_request.url = kTestUrl1;
  first_serving_request.method = "GET";

  std::move(first_request_handler)
      .Run(first_serving_request,
           first_serving_url_loader_client->BindURLloaderAndGetReceiver(),
           first_serving_url_loader_client->BindURLLoaderClientAndGetRemote());

  std::unique_ptr<PrefetchTestURLLoaderClient>
      second_serving_url_loader_client =
          std::make_unique<PrefetchTestURLLoaderClient>();
  network::ResourceRequest second_serving_request;
  second_serving_request.url = kTestUrl2;
  second_serving_request.method = "GET";

  std::move(second_request_handler)
      .Run(second_serving_request,
           second_serving_url_loader_client->BindURLloaderAndGetReceiver(),
           second_serving_url_loader_client->BindURLLoaderClientAndGetRemote());

  prefetch_container.reset();

  task_environment()->RunUntilIdle();

  EXPECT_EQ(first_serving_url_loader_client->received_redirects().size(), 1u);

  EXPECT_EQ(second_serving_url_loader_client->body_content(), "test body");
  EXPECT_TRUE(
      second_serving_url_loader_client->completion_status().has_value());

  EXPECT_TRUE(weak_first_response_reader);
  EXPECT_TRUE(weak_second_response_reader);

  first_serving_url_loader_client->DisconnectMojoPipes();
  second_serving_url_loader_client->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(weak_first_response_reader);
  EXPECT_FALSE(weak_second_response_reader);
}

TEST_P(PrefetchContainerTest, CancelAndClearStreamingLoader) {
  const GURL kTestUrl1 = GURL("https://test1.com");
  const GURL kTestUrl2 = GURL("https://test2.com");

  base::HistogramTester histogram_tester;

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(kTestUrl1);

  prefetch_container->MakeResourceRequest();

  prefetch_container->SimulatePrefetchEligibleForTest();
  auto pending_request =
      MakeManuallyServableStreamingURLLoaderForTest(prefetch_container.get());

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  CHECK_EQ(mojo::CreateDataPipe(1024, producer_handle, consumer_handle),
           MOJO_RESULT_OK);
  pending_request.client->OnReceiveResponse(
      network::mojom::URLResponseHead::New(), std::move(consumer_handle),
      std::nullopt);
  task_environment()->RunUntilIdle();

  // Prefetching is ongoing.
  ASSERT_TRUE(prefetch_container->GetStreamingURLLoader());
  base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader =
      prefetch_container->GetStreamingURLLoader();
  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);

  prefetch_container->CancelStreamingURLLoaderIfNotServing();

  // `streaming_loader` is still alive and working.
  EXPECT_FALSE(prefetch_container->GetStreamingURLLoader());
  EXPECT_TRUE(streaming_loader);
  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);

  task_environment()->RunUntilIdle();

  // `streaming_loader` is deleted asynchronously and its prefetching URL loader
  // is canceled. This itself doesn't make PrefetchContainer non-servable.
  EXPECT_FALSE(streaming_loader);
  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);
}

// To test lifetime and ownership issues, all possible event orderings for
// successful prefetching and serving are tested.
enum class Event {
  // Call OnComplete().
  kPrefetchOnComplete,

  // Call CreateRequestHandler().
  kCreateRequestHandler,

  // Call the PrefetchRequestHandler returned by CreateRequestHandler().
  kRequestHandler,

  // Disconnect `serving_url_loader_client`.
  kDisconnectServingClient,

  // Completely read the body mojo pipe.
  kCompleteBody,

  // Destruct PrefetchContainer.
  kDestructPrefetchContainer,

  // Serve for the second serving client.
  // All steps (corresponding to `kCreateRequestHandler`, `kRequestHandler`,
  // `kDisconnectServingClient` and `kCompleteBody`) are merged in order to
  // reduce the number of tests.
  kSecondClient,
};

std::ostream& operator<<(std::ostream& ostream, Event event) {
  switch (event) {
    case Event::kPrefetchOnComplete:
      return ostream << "kPrefetchOnComplete";
    case Event::kCreateRequestHandler:
      return ostream << "kCreateRequestHandler";
    case Event::kRequestHandler:
      return ostream << "kRequestHandler";
    case Event::kDisconnectServingClient:
      return ostream << "kDisconnectServingClient";
    case Event::kCompleteBody:
      return ostream << "kCompleteBody";
    case Event::kDestructPrefetchContainer:
      return ostream << "kDestructPrefetchContainer";
    case Event::kSecondClient:
      return ostream << "kSecondClient";
  }
}

enum class BodySize { kSmall, kLarge };
std::ostream& operator<<(std::ostream& ostream, BodySize body_size) {
  switch (body_size) {
    case BodySize::kSmall:
      return ostream << "Small";
    case BodySize::kLarge:
      return ostream << "Large";
  }
}

// To detect corner cases around lifetime and ownership, test all possible
// permutations of the order of events.
class PrefetchContainerLifetimeTest
    : public PrefetchContainerTestBase,
      public ::testing::WithParamInterface<
          std::tuple<PrefetchRearchParam, std::vector<Event>, BodySize>> {
 public:
  PrefetchContainerLifetimeTest()
      : PrefetchContainerTestBase(std::get<0>(GetParam())) {}
};

TEST_P(PrefetchContainerLifetimeTest, Lifetime) {
  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"));

  prefetch_container->SimulatePrefetchEligibleForTest();

  auto pending_request =
      MakeManuallyServableStreamingURLLoaderForTest(prefetch_container.get());

  const auto producer_pipe_capacity = network::GetDataPipeDefaultAllocationSize(
      network::DataPipeAllocationSize::kLargerSizeIfPossible);

  const BodySize body_size = std::get<2>(GetParam());
  std::string content;
  switch (body_size) {
    case BodySize::kSmall:
      content = "Body";
      break;
    case BodySize::kLarge:
      // Set the minimum data size that makes write to data pipe fail
      // incomplete.
      //
      // - `features::kPrefetchReusableBodySizeLimit` (multiple of
      //   `producer_pipe_capacity`) is used to fill buffer of
      //   `PrefetchDataPipeTee`. `PrefetchDataPipeTee` doesn't close the
      //   producer pipe at this stage and tries to read one more time (but it
      //   will discard the next one as the limit exceeded).
      // - `producer_pipe_capacity` is used to fill data pipe after the above
      //   buffer becomes full. The data is sent to `PrefetchDataPipeTee` (*)
      //   and `PrefetchDataPipeTee` tries to read it, but discards it. At this
      //   timing, `PrefetchDataPipeTee` closes the producer pipe (**).
      // - 1 is used to make the write incomplete as the producer tries to send
      //   one more byte after (**).
      size_t limit =
          static_cast<size_t>(features::kPrefetchReusableBodySizeLimit.Get()) +
          producer_pipe_capacity;
      content = std::string(limit + 1, '-');
      break;
  }

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  bool producer_completed = false;

  {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(producer_pipe_capacity, producer_handle,
                                   consumer_handle),
              MOJO_RESULT_OK);
    auto producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    mojo::DataPipeProducer* raw_producer = producer.get();
    raw_producer->Write(std::make_unique<mojo::StringDataSource>(
                            content, mojo::StringDataSource::AsyncWritingMode::
                                         STRING_STAYS_VALID_UNTIL_COMPLETION),
                        base::BindOnce(
                            [](std::unique_ptr<mojo::DataPipeProducer> producer,
                               bool* producer_completed, MojoResult result) {
                              *producer_completed = true;
                              CHECK_EQ(result, MOJO_RESULT_OK);
                              // `producer` is deleted here.
                            },
                            std::move(producer), &producer_completed));
  }

  EXPECT_NE(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);
  EXPECT_FALSE(prefetch_container->GetNonRedirectHead());

  pending_request.client->OnReceiveResponse(
      network::mojom::URLResponseHead::New(), std::move(consumer_handle),
      std::nullopt);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchServableState::kServable);
  EXPECT_TRUE(prefetch_container->GetNonRedirectHead());

  PrefetchServingHandle serving_handle =
      prefetch_container->CreateServingHandle();

  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      serving_handle.GetCurrentResponseReaderToServeForTesting();
  ASSERT_TRUE(prefetch_container->GetStreamingURLLoader());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      prefetch_container->GetStreamingURLLoader();

  PrefetchRequestHandler request_handler;
  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client;

  PrefetchServingHandle serving_handle2 =
      prefetch_container->CreateServingHandle();
  ASSERT_EQ(weak_response_reader.get(),
            serving_handle2.GetCurrentResponseReaderToServeForTesting().get());

  network::ResourceRequest serving_request;
  serving_request.url = GURL("https://test.com");
  serving_request.method = "GET";

  // `PrefetchStreamingURLLoader` and `PrefetchResponseReader` are initially
  // both expected alive, because they are needed for serving `request_handler`.

  std::set<Event> done;

  for (const Event event : std::get<1>(GetParam())) {
    switch (event) {
      case Event::kPrefetchOnComplete:
        pending_request.client->OnComplete(
            network::URLLoaderCompletionStatus(net::OK));
        break;

      case Event::kCreateRequestHandler:
        ASSERT_FALSE(request_handler);
        ASSERT_TRUE(prefetch_container);
        EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
                  PrefetchServableState::kServable);
        request_handler = serving_handle.CreateRequestHandler().first;
        ASSERT_TRUE(request_handler);
        break;

      // Call the PrefetchRequestHandler returned by CreateRequestHandler().
      case Event::kRequestHandler: {
        ASSERT_TRUE(request_handler);  // NOLINT(bugprone-use-after-move)
        ASSERT_FALSE(serving_url_loader_client);

        serving_url_loader_client =
            std::make_unique<PrefetchTestURLLoaderClient>();
        serving_url_loader_client->SetAutoDraining(false);

        // NOLINT(bugprone-use-after-move)
        std::move(request_handler)
            .Run(serving_request,
                 serving_url_loader_client->BindURLloaderAndGetReceiver(),
                 serving_url_loader_client->BindURLLoaderClientAndGetRemote());
        break;
      }

      // Disconnect `serving_url_loader_client`.
      case Event::kDisconnectServingClient:
        ASSERT_TRUE(serving_url_loader_client);
        serving_url_loader_client->DisconnectMojoPipes();
        break;

      // Completely read the body mojo pipe.
      case Event::kCompleteBody: {
        if (body_size == BodySize::kLarge) {
          // The body is sufficiently large to fill the data pipes and thus the
          // producer should still have pending data to write before
          // `StartDraining()`.
          EXPECT_FALSE(producer_completed);
        }
        // Wait until the URLLoaderClient completion.
        // `base::RunLoop().RunUntilIdle()` is not sufficient here, because
        // `mojo::DataPipeProducer` uses thread pool.
        serving_url_loader_client->StartDraining();
        task_environment()->RunUntilIdle();
        EXPECT_TRUE(producer_completed);
        break;
      }

      case Event::kSecondClient:
        ASSERT_TRUE(prefetch_container);
        EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
                  PrefetchServableState::kServable);

        // The second request is servable if the body data pipe is finished and
        // the whole body fits within the data pipe tee size limit.
        if (!done.count(Event::kPrefetchOnComplete) ||
            body_size == BodySize::kLarge) {
          // Not servable.
          ASSERT_FALSE(serving_handle2.CreateRequestHandler().first);
        } else {
          // As the first client is already served, the body pipe producer
          // should be also completed.
          EXPECT_TRUE(producer_completed);

          auto request_handler2 = serving_handle2.CreateRequestHandler().first;
          ASSERT_TRUE(request_handler2);

          auto serving_url_loader_client2 =
              std::make_unique<PrefetchTestURLLoaderClient>();

          std::move(request_handler2)
              .Run(serving_request,
                   serving_url_loader_client2->BindURLloaderAndGetReceiver(),
                   serving_url_loader_client2
                       ->BindURLLoaderClientAndGetRemote());

          task_environment()->RunUntilIdle();
          serving_url_loader_client2->DisconnectMojoPipes();

          EXPECT_TRUE(
              serving_url_loader_client2->completion_status().has_value());
          EXPECT_EQ(serving_url_loader_client2->body_content().size(),
                    content.size());
          EXPECT_EQ(serving_url_loader_client2->body_content(), content);
        }
        break;

      case Event::kDestructPrefetchContainer:
        ASSERT_TRUE(prefetch_container);
        prefetch_container.reset();
        break;
    }
    done.insert(event);

    task_environment()->RunUntilIdle();

    // `PrefetchResponseReader` should be kept alive as long as
    // `PrefetchContainer` is alive or serving URLLoaderClients are not
    // finished.
    // The second client is not alive here because it is created and finished
    // within `kSecondClient`.
    EXPECT_EQ(!!weak_response_reader,
              !done.count(Event::kDisconnectServingClient) ||
                  !done.count(Event::kDestructPrefetchContainer));

    // `PrefetchStreamingURLLoader` is kept alive until prefetching is
    // completed.
    EXPECT_EQ(!!weak_streaming_loader, !done.count(Event::kPrefetchOnComplete));

    if (done.count(Event::kPrefetchOnComplete) &&
        done.count(Event::kCompleteBody)) {
      EXPECT_TRUE(producer_completed);
    }
    if (done.count(Event::kRequestHandler)) {
      EXPECT_EQ(serving_url_loader_client->completion_status().has_value(),
                done.count(Event::kPrefetchOnComplete));
    }
    if (done.count(Event::kCompleteBody)) {
      EXPECT_EQ(serving_url_loader_client->body_content().size(),
                content.size());
      EXPECT_EQ(serving_url_loader_client->body_content(), content);
    }
  }
}

TEST_P(PrefetchContainerTest, SpeculationRulesTagsAddedToRequestHeader) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://test.com/referrer"));

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(
      GURL("https://test.com"),
      {.speculation_rules_tags = SpeculationRulesTags({"tag1", "tag2"})});
  prefetch_container->MakeResourceRequest();
  EXPECT_TRUE(prefetch_container->GetResourceRequest()
                  ->headers.GetHeader(blink::kSecSpeculationTagsHeaderName)
                  .has_value());
  EXPECT_EQ(prefetch_container->GetResourceRequest()
                ->headers.GetHeader(blink::kSecSpeculationTagsHeaderName)
                .value(),
            "\"tag1\", \"tag2\"");
}

TEST_P(PrefetchContainerTest, CrossSitePrefetchContainerNoSpeculationTag) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://other.com/referrer"));

  auto prefetch_container = CreateSpeculationRulesPrefetchContainer(
      GURL("https://test.com"),
      {.speculation_rules_tags = SpeculationRulesTags({"tag1", "tag2"})});

  EXPECT_TRUE(prefetch_container->IsCrossOriginRequest(
      url::Origin::Create(prefetch_container->GetURL())));
  // Cross-site Speculation rules prefetch should not contain tag in the header.
  prefetch_container->MakeResourceRequest();
  EXPECT_FALSE(prefetch_container->GetResourceRequest()
                   ->headers.GetHeader(blink::kSecSpeculationTagsHeaderName)
                   .has_value());
}

TEST_P(PrefetchContainerTest, SpeculationRulesNoTagAddedToRequestHeader) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://test.com/referrer"));
  auto prefetch_container =
      CreateSpeculationRulesPrefetchContainer(GURL("https://test.com"), {});
  prefetch_container->MakeResourceRequest();
  EXPECT_TRUE(prefetch_container->GetResourceRequest()
                  ->headers.GetHeader(blink::kSecSpeculationTagsHeaderName)
                  .has_value());
  EXPECT_EQ(prefetch_container->GetResourceRequest()
                ->headers.GetHeader(blink::kSecSpeculationTagsHeaderName)
                .value(),
            "null");
}

INSTANTIATE_TEST_SUITE_P(,
                         PrefetchContainerTest,
                         testing::ValuesIn(PrefetchRearchParam::Params()));

std::vector<std::vector<Event>> ValidEventPermutations(bool has_second_client) {
  std::vector<Event> events({
      Event::kPrefetchOnComplete,
      Event::kCreateRequestHandler,
      Event::kRequestHandler,
      Event::kDisconnectServingClient,
      Event::kCompleteBody,
      Event::kDestructPrefetchContainer,
  });
  if (has_second_client) {
    events.push_back(Event::kSecondClient);
  }

  std::vector<std::vector<Event>> params;
  do {
    const auto it_prefetch_on_complete =
        std::find(events.begin(), events.end(), Event::kPrefetchOnComplete);
    const auto it_create_request_handler =
        std::find(events.begin(), events.end(), Event::kCreateRequestHandler);
    const auto it_request_handler =
        std::find(events.begin(), events.end(), Event::kRequestHandler);
    const auto it_disconnect_serving_client = std::find(
        events.begin(), events.end(), Event::kDisconnectServingClient);
    const auto it_complete_body =
        std::find(events.begin(), events.end(), Event::kCompleteBody);
    const auto it_destruct_prefetch_container = std::find(
        events.begin(), events.end(), Event::kDestructPrefetchContainer);

    // Ordering requirements due to direct data dependencies:

    // `kCreateRequestHandler` -> `kRequestHandler` (`request_handler`)
    if (it_create_request_handler > it_request_handler) {
      continue;
    }
    // `kRequestHandler` -> `kDisconnectServingClient`
    // (`serving_url_loader_client`)
    if (it_request_handler > it_disconnect_serving_client) {
      continue;
    }
    // `kCreateRequestHandler` -> `kDestructPrefetchContainer`
    // (`prefetch_container`)
    if (it_create_request_handler > it_destruct_prefetch_container) {
      continue;
    }
    // `kRequestHandler` -> `kCompleteBody` (body data pipe)
    if (it_request_handler > it_complete_body) {
      continue;
    }

    // `kPrefetchOnComplete` -> `kCompleteBody` and successful
    // `kDisconnectServingClient` (prefetch completion)
    if (it_prefetch_on_complete > it_complete_body) {
      continue;
    }
    if (it_prefetch_on_complete > it_disconnect_serving_client) {
      continue;
    }

    if (has_second_client) {
      const auto it_second_client =
          std::find(events.begin(), events.end(), Event::kSecondClient);

      // `kPrefetchOnComplete` -> `kSecondClient` ->
      // `kDestructPrefetchContainer`
      if (it_prefetch_on_complete > it_second_client ||
          it_second_client > it_destruct_prefetch_container) {
        continue;
      }

      // `kCreateRequestHandler` -> `kSecondClient` (the second request
      // starts after the first request, but doesn't necessarily complete
      // subsequent steps after those of the first request).
      if (it_create_request_handler > it_second_client) {
        continue;
      }
    }

    params.push_back(events);
  } while (std::next_permutation(events.begin(), events.end()));

  // Make sure some particular sequences are tested, where:

  if (!has_second_client) {
    // - `PrefetchContainer` is destructed before prefetch is completed:
    CHECK(base::Contains(
        params,
        std::vector<Event>{Event::kCreateRequestHandler, Event::kRequestHandler,
                           Event::kDestructPrefetchContainer,
                           Event::kPrefetchOnComplete, Event::kCompleteBody,
                           Event::kDisconnectServingClient}));

    // - `PrefetchContainer` is destructed before PrefetchRequestHandler is
    // invoked and prefetch is completed:
    CHECK(base::Contains(
        params,
        std::vector<Event>{
            Event::kCreateRequestHandler, Event::kDestructPrefetchContainer,
            Event::kRequestHandler, Event::kPrefetchOnComplete,
            Event::kCompleteBody, Event::kDisconnectServingClient}));

    // - `PrefetchContainer` is destructed before PrefetchRequestHandler is
    // invoked but after prefetch is completed:
    CHECK(base::Contains(
        params, std::vector<Event>{
                    Event::kPrefetchOnComplete, Event::kCreateRequestHandler,
                    Event::kDestructPrefetchContainer, Event::kRequestHandler,
                    Event::kCompleteBody, Event::kDisconnectServingClient}));
  }

  return params;
}

INSTANTIATE_TEST_SUITE_P(
    SingleClient,
    PrefetchContainerLifetimeTest,
    testing::Combine(testing::ValuesIn(PrefetchRearchParam::Params()),
                     testing::ValuesIn(ValidEventPermutations(false)),
                     testing::Values(BodySize::kSmall, BodySize::kLarge)));

INSTANTIATE_TEST_SUITE_P(
    TwoClients,
    PrefetchContainerLifetimeTest,
    testing::Combine(testing::ValuesIn(PrefetchRearchParam::Params()),
                     testing::ValuesIn(ValidEventPermutations(true)),
                     testing::Values(BodySize::kSmall)));

}  // namespace content
