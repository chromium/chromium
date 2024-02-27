// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/preloading/prefetch/mock_prefetch_service_delegate.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class ContaminationDelayBrowserTest : public ContentBrowserTest {
 protected:
  ContaminationDelayBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchStateContaminationMitigation, {}},
         {features::kPrefetchRedirects, {}},
         // This is needed specifically for CrOS MSAN, where we apply a 10x
         // multiplier to all test timeouts, which happens to be enough to push
         // the response delay in this test (which is scaled in that way to
         // match the slowdown of everything else) over the default prefetch
         // timeout. To be resilient also to changes in that value, it is
         // expressly overridden here to be a timeout that is much longer and
         // scales with the timeout multiplier.
         {features::kPrefetchUseContentRefactor,
          {{"prefetch_timeout_ms",
            base::NumberToString(
                TestTimeouts::action_max_timeout().InMilliseconds())}}}},
        {});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ContaminationDelayBrowserTest::MaybeServeRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  base::TimeDelta response_delay() const { return response_delay_; }
  void set_response_delay(base::TimeDelta delay) { response_delay_ = delay; }

  void Prefetch(const GURL& url) {
    auto* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            shell()->web_contents()->GetPrimaryMainFrame());
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->url = url;
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
    candidate->referrer = Referrer::SanitizeForRequest(
        url, blink::mojom::Referrer(
                 shell()->web_contents()->GetURL(),
                 network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
    candidates.push_back(std::move(candidate));
    prefetch_document_manager->ProcessCandidates(candidates,
                                                 /*devtools_observer=*/nullptr);
    ASSERT_TRUE(base::test::RunUntil([&] {
      return prefetch_document_manager->GetReferringPageMetrics()
                 .prefetch_successful_count >= 1;
    })) << "timed out waiting for prefetch to complete ("
        << prefetch_document_manager->GetReferringPageMetrics()
               .prefetch_attempted_count
        << " attempted)";
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> MaybeServeRequest(
      const net::test_server::HttpRequest& request) {
    GURL url = request.GetURL();
    if (url.path_piece() == "/delayed") {
      return std::make_unique<net::test_server::DelayedHttpResponse>(
          response_delay());
    }
    if (url.path_piece() == "/redirect-cross-site") {
      auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
          response_delay());
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader("Location",
                                embedded_test_server()
                                    ->GetURL("prefetch.localhost", "/delayed")
                                    .spec());
      return response;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::TimeDelta response_delay_ = TestTimeouts::tiny_timeout() * 12;
};

IN_PROC_BROWSER_TEST_F(ContaminationDelayBrowserTest, CrossSite) {
  set_response_delay(TestTimeouts::tiny_timeout() * 4);

  GURL referrer_url =
      embedded_test_server()->GetURL("referrer.localhost", "/title1.html");
  GURL prefetch_url =
      embedded_test_server()->GetURL("prefetch.localhost", "/delayed");
  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));
  Prefetch(prefetch_url);

  base::ElapsedTimer timer;
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), prefetch_url));
  EXPECT_GE(timer.Elapsed(), response_delay());
}

IN_PROC_BROWSER_TEST_F(ContaminationDelayBrowserTest, IgnoresSameOrigin) {
  GURL referrer_url =
      embedded_test_server()->GetURL("referrer.localhost", "/title1.html");
  GURL prefetch_url =
      embedded_test_server()->GetURL("referrer.localhost", "/delayed");
  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));
  Prefetch(prefetch_url);

  base::ElapsedTimer timer;
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), prefetch_url));
  EXPECT_LT(timer.Elapsed(), response_delay());
}

IN_PROC_BROWSER_TEST_F(ContaminationDelayBrowserTest, IgnoresSameSite) {
  GURL referrer_url =
      embedded_test_server()->GetURL("referrer.localhost", "/title1.html");
  GURL prefetch_url =
      embedded_test_server()->GetURL("sub.referrer.localhost", "/delayed");
  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));
  Prefetch(prefetch_url);

  base::ElapsedTimer timer;
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), prefetch_url));
  EXPECT_LT(timer.Elapsed(), response_delay());
}

IN_PROC_BROWSER_TEST_F(ContaminationDelayBrowserTest, IgnoresIfExempt) {
  GURL referrer_url =
      embedded_test_server()->GetURL("referrer.localhost", "/title1.html");
  GURL prefetch_url =
      embedded_test_server()->GetURL("prefetch.localhost", "/delayed");

  auto* prefetch_service = PrefetchService::GetFromFrameTreeNodeId(
      shell()->web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId());
  auto owned_delegate = std::make_unique<MockPrefetchServiceDelegate>();
  EXPECT_CALL(*owned_delegate, IsContaminationExempt(referrer_url))
      .WillRepeatedly(testing::Return(true));
  prefetch_service->SetPrefetchServiceDelegateForTesting(
      std::move(owned_delegate));

  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));
  Prefetch(prefetch_url);

  base::ElapsedTimer timer;
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), prefetch_url));
  EXPECT_LT(timer.Elapsed(), response_delay());
}

IN_PROC_BROWSER_TEST_F(ContaminationDelayBrowserTest, DelayAfterRedirect) {
  set_response_delay(TestTimeouts::tiny_timeout() * 8);

  GURL referrer_url =
      embedded_test_server()->GetURL("referrer.localhost", "/title1.html");
  GURL prefetch_url = embedded_test_server()->GetURL("referrer.localhost",
                                                     "/redirect-cross-site");
  GURL commit_url =
      embedded_test_server()->GetURL("prefetch.localhost", "/delayed");

  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));
  Prefetch(prefetch_url);

  base::ElapsedTimer timer;
  ASSERT_TRUE(NavigateToURLFromRenderer(shell(), prefetch_url, commit_url));
  EXPECT_LT(timer.Elapsed(), response_delay() * 2);
  EXPECT_GE(timer.Elapsed(), response_delay());
}

}  // namespace
}  // namespace content
