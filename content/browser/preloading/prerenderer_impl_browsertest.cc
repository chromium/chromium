// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerenderer_impl.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
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

class PrerendererImplBrowserTestBase : public ContentBrowserTest {
 public:
  PrerendererImplBrowserTestBase() = default;
  ~PrerendererImplBrowserTestBase() override = default;

  void SetUp() override {
    prerender_helper_ =
        std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
            [](PrerendererImplBrowserTestBase* that) {
              return &that->web_contents();
            },
            base::Unretained(this)));

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
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
  }

  GURL GetUrl(const std::string& path) {
    return https_server_->GetURL("a.test", path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return https_server_->GetURL("b.test", path);
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

    // So, we guard the field with lock.
    base::AutoLock auto_lock(lock_);

    requests_.push_back(request);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;

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
    feature_list_.InitWithFeatures(
        {features::kPrefetchReusable,
         features::kPrerender2FallbackPrefetchSpecRules},
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

  std::vector<RequestPathAndSecPurposeHeader> expected{
      {.path = "/empty.html", .sec_purpose_header_value = ""},
      {.path = "/title1.html",
       .sec_purpose_header_value = "prefetch;prerender"},
  };
  ASSERT_EQ(expected, GetObservedRequests());
}

}  // namespace
}  // namespace content
