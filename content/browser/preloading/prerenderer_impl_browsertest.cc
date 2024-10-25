// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerenderer_impl.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

struct RequestPathAndSecPurposeHeader {
  std::string path;
  std::string sec_purpose_header_value;

  bool operator==(const RequestPathAndSecPurposeHeader& other) const = default;
};

std::ostream& operator<<(std::ostream& ostream,
                         const RequestPathAndSecPurposeHeader& x) {
  return ostream << "RequestPathAndSecPurposeHeader {.path = \"" << x.path
                 << "\", .sec_purpose_header_value = \""
                 << x.sec_purpose_header_value << "\"}";
}

class PrerendererImplBrowserTestBase : public ContentBrowserTest {
 public:
  PrerendererImplBrowserTestBase() = default;
  ~PrerendererImplBrowserTestBase() override = default;

  void SetUp() override {
    prerender_helper_ = std::make_unique<test::PrerenderTestHelper>(
        base::BindRepeating(
            [](PrerendererImplBrowserTestBase* that) {
              return &that->web_contents();
            },
            base::Unretained(this)),
        /*force_disable_prerender2fallback=*/false);

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_->SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->RegisterRequestMonitor(
        base::BindRepeating(&PrerendererImplBrowserTestBase::OnResourceRequest,
                            base::Unretained(this)));
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_->Start());

    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&PrerendererImplBrowserTestBase::OnResourceRequest,
                            base::Unretained(this)));
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  GURL GetUrl(const std::string& path) {
    return https_server_->GetURL("a.test", path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return https_server_->GetURL("b.test", path);
  }

  GURL GetUrlHttp(const std::string& path) {
    return embedded_test_server()->GetURL("a.test", path);
  }

  Prefetcher& GetPrefetcher() {
    return PreloadingDecider::GetOrCreateForCurrentDocument(
               web_contents_impl().GetPrimaryMainFrame())
        ->GetPrefetcherForTesting();
  }

  PrerendererImpl& GetPrerendererImpl() {
    return static_cast<PrerendererImpl&>(
        PreloadingDecider::GetOrCreateForCurrentDocument(
            web_contents_impl().GetPrimaryMainFrame())
            ->GetPrerendererForTesting());
  }

  blink::mojom::SpeculationCandidatePtr CreateSpeculationCandidate(
      const GURL& url) {
    return blink::mojom::SpeculationCandidate::New(
        /*url=*/url,
        /*action=*/blink::mojom::SpeculationAction::kPrerender,
        /*referrer=*/blink::mojom::Referrer::New(),
        /*requires_anonymous_client_ip_when_cross_origin=*/false,
        /*target_browsing_context_name_hint=*/
        blink::mojom::SpeculationTargetHint::kNoHint,
        /*eagerness=*/blink::mojom::SpeculationEagerness::kEager,
        /*no_vary_search_hint=*/nullptr,
        /*injection_type=*/blink::mojom::SpeculationInjectionType::kNone);
  }

  std::vector<RequestPathAndSecPurposeHeader> GetObservedRequests() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    base::AutoLock auto_lock(lock_);

    std::vector<RequestPathAndSecPurposeHeader> ret;
    for (auto request : requests_) {
      ret.push_back(RequestPathAndSecPurposeHeader{
          .path = request.GetURL().PathForRequest(),
          .sec_purpose_header_value = request.headers["Sec-Purpose"],
      });
    }
    return ret;
  }

  void SetResponseDelay(base::TimeDelta duration) {
    response_delay_ = duration;
  }

  net::EmbeddedTestServer& https_server() { return *https_server_.get(); }
  base::HistogramTester& histogram_tester() { return *histogram_tester_.get(); }
  test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_.get();
  }
  WebContents& web_contents() { return *shell()->web_contents(); }
  WebContentsImpl& web_contents_impl() {
    return static_cast<WebContentsImpl&>(web_contents());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  void OnResourceRequest(const net::test_server::HttpRequest& request) {
    // Called from a thread for EmbeddedTestServer.
    CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI) &&
          !BrowserThread::CurrentlyOn(BrowserThread::IO));

    base::PlatformThread::Sleep(response_delay_);

    // So, we guard the field with lock.
    base::AutoLock auto_lock(lock_);

    requests_.push_back(request);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;

  base::TimeDelta response_delay_ = base::Seconds(0);
  base::Lock lock_;
  std::vector<net::test_server::HttpRequest> requests_ GUARDED_BY(lock_);
};

class PrerendererImplBrowserTestNoPrefetchAhead
    : public PrerendererImplBrowserTestBase {
 public:
  PrerendererImplBrowserTestNoPrefetchAhead() {
    feature_list_.InitWithFeatures(
        {features::kPrefetchReusable},
        {features::kPrerender2FallbackPrefetchSpecRules,
         blink::features::kLCPTimingPredictorPrerender2});
  }
};

class PrerendererImplBrowserTestPrefetchAhead
    : public PrerendererImplBrowserTestBase {
 public:
  PrerendererImplBrowserTestPrefetchAhead() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchReusable, {}},
         {features::kPrerender2FallbackPrefetchSpecRules, {}},
         {features::kPrefetchUseContentRefactor,
          {
              {"prefetch_timeout_ms", "1500"},
              {"block_until_head_timeout_moderate_prefetch", "500"},
          }}},
        {blink::features::kLCPTimingPredictorPrerender2});
  }
};

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestNoPrefetchAhead,
                       PrefetchNotTriggeredPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectTotalCount(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome", 0);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"}};
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestNoPrefetchAhead,
                       PrefetchNotTriggeredPrerenderFailure) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  // Make prerender failure by calling a forbidden API.
  test::PrerenderHostObserver observer(web_contents(), prerender_url);
  RenderFrameHost* rfh =
      prerender_helper().GetPrerenderedMainFrameHost(prerender_url);
  ASSERT_TRUE(rfh);
  const char* script = "navigator.getGamepads();";
  rfh->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script),
                                 base::NullCallback(),
                                 ISOLATED_WORLD_ID_GLOBAL);
  observer.WaitForDestroyed();

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectTotalCount(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome", 0);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
      {.path = "/title1.html", .sec_purpose_header_value = ""},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchSuccessPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  // TODO(https://crbug.com/342089066): Record prefetch as
  // kTriggeredButUpgradedToPrerender.
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"}};
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchSuccessPrerenderNotEligible) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  test::TestPrefetchWatcher watcher;
  const GURL prerender_url = GetCrossSiteUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  watcher.WaitUntilPrefetchResponseCompleted(
      web_contents_impl().GetPrimaryMainFrame()->GetDocumentToken(),
      prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchSuccessPrerenderFailure) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  // Make prerender failure by calling a forbidden API.
  test::PrerenderHostObserver observer(web_contents(), prerender_url);
  RenderFrameHost* rfh =
      prerender_helper().GetPrerenderedMainFrameHost(prerender_url);
  ASSERT_TRUE(rfh);
  const char* script = "navigator.getGamepads();";
  rfh->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script),
                                 base::NullCallback(),
                                 ISOLATED_WORLD_ID_GLOBAL);
  observer.WaitForDestroyed();

  prerender_helper().NavigatePrimaryPage(prerender_url);

  // TODO(https://crbug.com/342089066): Record prefetch as
  // kTriggeredButUpgradedToPrerender.
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kMojoBinderPolicy, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchNotEligiblePrerenderFailure) {
  PrefetchService::SetForceIneligibilityForTesting(
      PreloadingEligibility::kHostIsNonUnique);

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});

  // Here we shouldn't call
  // `prerender_helper().WaitForPrerenderLoadCompletion(prerender_url)` since
  // this eligibility check of prefetch synchronously fails and the call first
  // tries to get `PrerenderHost`, which has been already destructed.

  ASSERT_TRUE(NavigateToURL(shell(), prerender_url));

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kPrerenderFailedDuringPrefetch, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "SpeculationRule",
      PrefetchStatus::kPrefetchIneligibleHostIsNonUnique, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html", .sec_purpose_header_value = ""},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchNotEligibleNonHttpsPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrlHttp("/empty.html")));

  const GURL prerender_url = GetUrlHttp("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchNotEligibleNonHttpsPrerenderSuccessWithDelay) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrlHttp("/empty.html")));

  base::test::TestFuture<base::OnceClosure> eligibility_check_callback_future;
  auto& prefetch_service = *PrefetchService::GetFromFrameTreeNodeId(
      web_contents().GetPrimaryMainFrame()->GetFrameTreeNodeId());
  prefetch_service.SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             eligibility_check_callback_future,
         base::OnceClosure callback) {
        eligibility_check_callback_future->SetValue(std::move(callback));
      },
      base::Unretained(&eligibility_check_callback_future)));

  const GURL prerender_url = GetUrlHttp("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});

  base::PlatformThread::Sleep(base::Milliseconds(101));

  // Proceed to the eligibility check.
  eligibility_check_callback_future.Take().Run();

  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

// Scenario:
//
// - URL U has a service worker.
// - Trigger prefetch ahead of prerender A for URL U.
// - A failed in eligibility check due to SW.
// - Trigger prerender A' for URL U.
//   - A' falls back to non-prefetch navigation.
// - Navigation is started. A' is used.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchNotEligibleServiceWorkerPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/prerender/empty.html")));

  std::string script = R"(
    (async () => {
      await navigator.serviceWorker.register('./sw_fallback.js');
      await navigator.serviceWorker.ready;
      return true;
    })();
  )";
  EXPECT_TRUE(ExecJs(web_contents().GetPrimaryMainFrame(), script));

  const GURL prerender_url = GetUrl("/prerender/empty.html?2");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/prerender/empty.html", .sec_purpose_header_value = ""},
      {.path = "/prerender/sw_fallback.js", .sec_purpose_header_value = ""},
      {.path = "/prerender/empty.html?2",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

// Variant of PrefetchNotEligibleServiceWorkerPrerenderSuccess.
// Eligibility check with delay.
//
// Scenario:
//
// - URL U has a service worker.
// - Trigger prefetch ahead of prerender A for URL U.
// - Trigger prerender A' for URL U.
//   - A blocks A'.
// - A failed in eligibility check due to SW.
//   - A' falls back to non-prefetch navigation.
// - Navigation is started. A' is used.
IN_PROC_BROWSER_TEST_F(
    PrerendererImplBrowserTestPrefetchAhead,
    PrefetchNotEligibleServiceWorkerPrerenderSuccessWithDelay) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/prerender/empty.html")));

  std::string script = R"(
    (async () => {
      await navigator.serviceWorker.register('./sw_fallback.js');
      await navigator.serviceWorker.ready;
      return true;
    })();
  )";
  EXPECT_TRUE(ExecJs(web_contents().GetPrimaryMainFrame(), script));

  base::test::TestFuture<base::OnceClosure> eligibility_check_callback_future;
  auto& prefetch_service = *PrefetchService::GetFromFrameTreeNodeId(
      web_contents().GetPrimaryMainFrame()->GetFrameTreeNodeId());
  prefetch_service.SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             eligibility_check_callback_future,
         base::OnceClosure callback) {
        eligibility_check_callback_future->SetValue(std::move(callback));
      },
      base::Unretained(&eligibility_check_callback_future)));

  const GURL prerender_url = GetUrl("/prerender/empty.html?2");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});

  base::PlatformThread::Sleep(base::Milliseconds(101));

  // Proceed to the eligibility check.
  eligibility_check_callback_future.Take().Run();

  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/prerender/empty.html", .sec_purpose_header_value = ""},
      {.path = "/prerender/sw_fallback.js", .sec_purpose_header_value = ""},
      {.path = "/prerender/empty.html?2",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

// Scenario:
//
// - Trigger prefetch ahead of prerender A for URL U.
// - Trigger prerender A' for URL U.
//   - A blocks A'.
// - A failed due to timeout.
//   - The failure is propagated to A'.
// - Navigation is started. No preloads are used.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchTimeoutPrerenderFailure) {
  // Prefetch will fail as
  // `prefetch_timeout_ms = 1500 < response_delay_ = 1500 + 1000`.
  SetResponseDelay(base::Milliseconds(1500 + 1000));

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  blink::mojom::SpeculationCandidatePtr candidate =
      CreateSpeculationCandidate(prerender_url);
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                      PreloadingConfidence{100});
  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  ASSERT_TRUE(NavigateToURL(shell(), prerender_url));

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kPrerenderFailedDuringPrefetch, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "SpeculationRule",
      PrefetchStatus::kPrefetchNotFinishedInTime, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      // Prefetch and prerender, timed out and aborted.
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
      // Normal navigation.
      {.path = "/title1.html", .sec_purpose_header_value = ""}};
  ASSERT_EQ(expected, GetObservedRequests());
}

// Consider a case that a site uses a SpecRules containing prefetch and
// prerender for a URL U.
//
// Scenario:
//
// - Trigger prefetch A a URL U.
// - Trigger prefetch ahead of prerender B for URL U.
//   - B is migrated into A. A inherits `PreloadPipelineInfo` and is considered
//     as `IsLikelyAheadOfPrerender`.
// - Trigger prerender B' for URL U.
//   - A blocks B'.
// - Navigation is started. B' is used.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchMigratedPrefetchSuccessPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    GetPrefetcher().MaybePrefetch(std::move(candidate), enacting_predictor);
  }
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
  }

  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome"),
      testing::ElementsAre(
          base::Bucket(PreloadingTriggeringOutcome::kUnspecified, 1),
          base::Bucket(PreloadingTriggeringOutcome::kSuccess, 1)));
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html", .sec_purpose_header_value = "prefetch"}};
  ASSERT_EQ(expected, GetObservedRequests());
}

// Variant of PrefetchMigratedPrefetchSuccessPrerenderSuccess.
// The order of migration and prefetch completion is reversed.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchSuccessPrefetchMigratedPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  {
    test::TestPrefetchWatcher watcher;
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    GetPrefetcher().MaybePrefetch(std::move(candidate), enacting_predictor);
    watcher.WaitUntilPrefetchResponseCompleted(
        web_contents_impl().GetPrimaryMainFrame()->GetDocumentToken(),
        prerender_url);
  }
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
  }

  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  prerender_helper().NavigatePrimaryPage(prerender_url);

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome"),
      testing::ElementsAre(
          base::Bucket(PreloadingTriggeringOutcome::kUnspecified, 1),
          base::Bucket(PreloadingTriggeringOutcome::kSuccess, 1)));
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kActivated, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html", .sec_purpose_header_value = "prefetch"}};
  ASSERT_EQ(expected, GetObservedRequests());
}

// Scenario:
//
// - Trigger prefetch A a URL U.
// - Eligibility check for A is done.
// - Trigger prefetch ahead of prerender B for URL U.
//   - B is migrated into A. A inherits `PreloadPipelineInfo` and is considered
//     as `IsLikelyAheadOfPrerender`. Eligibility of A is propagated to the
//     `PreloadPipelineInfo`.
// - Trigger prerender B' for URL U.
//   - A blocks B'.
// - A fails by timeout.
// - Navigation is started. No preloads are used.
//
// This shows the necessity of eligibility propagation in
// `PrefetchContainer::MigrateNewlyAdded()`.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchMigratedPrefetchFailurePrerenderFailure) {
  // Prefetch will fail as
  // `prefetch_timeout_ms = 1500 < response_delay_ = 1500 + 1000`.
  SetResponseDelay(base::Milliseconds(1500 + 1000));

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    GetPrefetcher().MaybePrefetch(std::move(candidate), enacting_predictor);
  }

  // Ensure that eligibility check is done.
  base::PlatformThread::Sleep(base::Milliseconds(101));

  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
  }

  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  ASSERT_TRUE(NavigateToURL(shell(), prerender_url));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome"),
      testing::ElementsAre(
          base::Bucket(PreloadingTriggeringOutcome::kUnspecified, 1),
          base::Bucket(PreloadingTriggeringOutcome::kFailure, 1)));
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kPrerenderFailedDuringPrefetch, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "SpeculationRule",
      PrefetchStatus::kPrefetchNotFinishedInTime, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html", .sec_purpose_header_value = "prefetch"},
      {.path = "/title1.html", .sec_purpose_header_value = ""}};
  ASSERT_EQ(expected, GetObservedRequests());
}

// Scenario:
//
// - Trigger prefetch A.
// - Trigger prefetch ahead of prerender B for URL U.
//   - B is migrated into A. A inherits `PreloadPipelineInfo` and is considered
//     as `IsLikelyAheadOfPrerender`.
// - Trigger prerender B' for URL U.
// - A fails in eligibility check.
//   - B' fails as the ineligibility is not admissible.
// - Navigation is started. No preloads are used.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchMigratedPrefetchNotEligiblePrerenderFailure) {
  PrefetchService::SetForceIneligibilityForTesting(
      PreloadingEligibility::kHostIsNonUnique);

  base::test::TestFuture<base::OnceClosure> eligibility_check_callback_future;
  auto& prefetch_service = *PrefetchService::GetFromFrameTreeNodeId(
      web_contents().GetPrimaryMainFrame()->GetFrameTreeNodeId());
  prefetch_service.SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             eligibility_check_callback_future,
         base::OnceClosure callback) {
        eligibility_check_callback_future->SetValue(std::move(callback));
      },
      base::Unretained(&eligibility_check_callback_future)));

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    GetPrefetcher().MaybePrefetch(std::move(candidate), enacting_predictor);
  }
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
  }

  // Proceed to the eligibility check of the first prefetch.
  eligibility_check_callback_future.Take().Run();

  // Here we shouldn't call
  // `prerender_helper().WaitForPrerenderLoadCompletion(prerender_url)` since
  // the call first tries to get `PrerenderHost`, which has been already
  // destructed.

  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kUnspecified, 2);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kPrerenderFailedDuringPrefetch, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "SpeculationRule",
      PrefetchStatus::kPrefetchIneligibleHostIsNonUnique, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html", .sec_purpose_header_value = ""},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

// Consider a case that a site uses a SpecRules containing prefetch and
// prerender for a URL U.
//
// Scenario:
//
// - Trigger prefetch A a URL U.
// - Trigger prefetch ahead of prerender B for URL U.
//   - B is migrated into A. A inherits `PreloadPipelineInfo` and is considered
//     as `IsLikelyAheadOfPrerender`.
// - Trigger prerender B' for URL U.
//   - A blocks B'.
// - A failed due to timeout.
//   - The failure is propagated to B'.
// - Navigation is started. No preloads are used.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrefetchMigratedPrefetchTimeoutPrerenderFailure) {
  // Prefetch will fail as
  // `prefetch_timeout_ms = 1500 < response_delay_ = 1500 + 1000`.
  SetResponseDelay(base::Milliseconds(1500 + 1000));

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    GetPrefetcher().MaybePrefetch(std::move(candidate), enacting_predictor);
  }
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
  }

  prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

  ASSERT_TRUE(NavigateToURL(shell(), prerender_url));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome"),
      testing::ElementsAre(
          base::Bucket(PreloadingTriggeringOutcome::kUnspecified, 1),
          base::Bucket(PreloadingTriggeringOutcome::kFailure, 1)));
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kPrerenderFailedDuringPrefetch, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "SpeculationRule",
      PrefetchStatus::kPrefetchNotFinishedInTime, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      // Prefetch and prerender, timed out and aborted.
      {.path = "/title1.html", .sec_purpose_header_value = "prefetch"},
      // Normal navigation.
      {.path = "/title1.html", .sec_purpose_header_value = ""}};
  ASSERT_EQ(expected, GetObservedRequests());
}

// Scenario:
//
// - Trigger prefetch ahead of prerender A for URL U.
// - Trigger prerender A' for URL U.
//   - A blocks A'.
// - Prefetch matching process ended due to timeout. A' is aborted.
// - A received response and succeeded.
// - Navigation is started. A is used.
IN_PROC_BROWSER_TEST_F(
    PrerendererImplBrowserTestPrefetchAhead,
    PrefetchSuccessPrefetchMatchResolverTimeoutPrerenderFailure) {
  SetResponseDelay(base::Milliseconds(1000));

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  {
    test::TestPrefetchWatcher watcher;

    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    // Use `kModerate` to trigger `PrefetchMatchResolver2::OnTimeout()`.
    // Note that `block_until_head_timeout_moderate_prefetch = 500 <
    // response_delay_ = 1000 < prefetch_timeout_ms = 1500`.
    candidate->eagerness = blink::mojom::SpeculationEagerness::kModerate;
    PreloadingPredictor enacting_predictor =
        GetPredictorForPreloadingTriggerType(
            PreloadingTriggerType::kSpeculationRule);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});

    prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

    watcher.WaitUntilPrefetchResponseCompleted(
        web_contents_impl().GetPrimaryMainFrame()->GetDocumentToken(),
        prerender_url);
  }

  ASSERT_TRUE(NavigateToURL(shell(), prerender_url));

  // TODO(crbug.com/372851198): Investigate why
  // `PrefetchContainer::Reader::OnPrefetchProbeResult()` is not called.
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kReady, 1);
  histogram_tester().ExpectUniqueSample(
      "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome",
      PreloadingTriggeringOutcome::kFailure, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kPrerenderFailedDuringPrefetch, 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "SpeculationRule",
      PrefetchStatus::kPrefetchNotFinishedInTime, 1);

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      // Prerender is aborted, but prefetch is success and used.
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"}};
  ASSERT_EQ(expected, GetObservedRequests());
}

// Consider a case that a site inserted a SpecRules for prerender, removed it,
// and inserted again.
//
// Scenario:
//
// - Trigger prefetch ahead of prerender A for URL U.
// - Trigger prerender A' for URL U.
//   - A' uses A.
// - A' is cancelled.
// - Trigger prefetch ahead of prerender B for URL U.
//   - B is migrated into A. A inherits `PreloadPipelineInfo`.
// - Trigger prerender B' for URL U.
//   - B' uses A.
// - Navigation is started. B' is used.
IN_PROC_BROWSER_TEST_F(PrerendererImplBrowserTestPrefetchAhead,
                       PrerenderSuccessCancelledAnotherPrerenderSuccess) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("/empty.html")));

  const GURL prerender_url = GetUrl("/title1.html");
  PreloadingPredictor enacting_predictor = GetPredictorForPreloadingTriggerType(
      PreloadingTriggerType::kSpeculationRule);
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
    prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);

    GetPrerendererImpl().CancelStartedPrerendersForTesting();
  }
  {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreateSpeculationCandidate(prerender_url);
    GetPrerendererImpl().MaybePrerender(candidate, enacting_predictor,
                                        PreloadingConfidence{100});
    prerender_helper().WaitForPrerenderLoadCompletion(prerender_url);
  }

  prerender_helper().NavigatePrimaryPage(prerender_url);

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Preloading.Prefetch.Attempt.SpeculationRules.TriggeringOutcome"),
      testing::ElementsAre(
          base::Bucket(PreloadingTriggeringOutcome::kSuccess, 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Preloading.Prerender.Attempt.SpeculationRules.TriggeringOutcome"),
      testing::ElementsAre(
          base::Bucket(PreloadingTriggeringOutcome::kReady, 1),
          base::Bucket(PreloadingTriggeringOutcome::kSuccess, 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule"),
      testing::ElementsAre(
          base::Bucket(PrerenderFinalStatus::kActivated, 1),
          base::Bucket(PrerenderFinalStatus::kTriggerDestroyed, 1)));

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"}};
  ASSERT_EQ(expected, GetObservedRequests());
}

}  // namespace
}  // namespace content
