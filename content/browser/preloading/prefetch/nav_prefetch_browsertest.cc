// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

using net::test_server::ControllableHttpResponse;

class NavPrefetchBrowserTest : public ContentBrowserTest {
 protected:
  void StartPrefetch(const GURL& url) {
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
  }
};

IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest,
                       DoesNotHangIfCancelledWhileWaitingForHead) {
  ControllableHttpResponse response1(embedded_test_server(), "/next");
  ControllableHttpResponse response2(embedded_test_server(), "/next");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL referrer_url = embedded_test_server()->GetURL("/empty.html");
  GURL next_url = embedded_test_server()->GetURL("/next");
  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));

  // Prefetch the next page and wait for that request to arrive.
  StartPrefetch(next_url);
  response1.WaitForRequest();

  // Start a navigation which may block on head (since we haven't sent it).
  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  TestFrameNavigationObserver nav_observer(rfh);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(rfh, next_url));
  ASSERT_TRUE(nav_observer.navigation_started());

  // Cancel the prefetch.
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(rfh);
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);

  // Wait for a new request, and respond to it.
  response2.WaitForRequest();
  response2.Send(net::HTTP_OK);
  response2.Done();

  // The navigation should now succeed.
  nav_observer.Wait();
  EXPECT_EQ(nav_observer.last_committed_url(), next_url);
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
}

}  // namespace
}  // namespace content
