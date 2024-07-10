// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class EmbeddingTokenBrowserTest : public ContentBrowserTest {
 public:
  EmbeddingTokenBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());

    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  RenderFrameHostImpl* top_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
  }

  EmbeddingTokenBrowserTest(const EmbeddingTokenBrowserTest&) = delete;
  EmbeddingTokenBrowserTest& operator=(const EmbeddingTokenBrowserTest&) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest, EmbeddingTokenOnMainFrame) {
  GURL a_url = embedded_test_server()->GetURL("a.com", "/site_isolation/");
  GURL b_url = embedded_test_server()->GetURL("b.com", "/site_isolation/");
  // Starts without an embedding token.
  EXPECT_FALSE(top_frame_host()->GetEmbeddingToken().has_value());

  // Embedding tokens should get added to the main frame.
  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html")));
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto first_token = top_frame_host()->GetEmbeddingToken().value();

  EXPECT_TRUE(NavigateToURL(shell(), b_url.Resolve("blank.html")));
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  EXPECT_NE(top_frame_host()->GetEmbeddingToken().value(), first_token);
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokensAddedToCrossDocumentIFrames) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b(a),c,a)")));

  ASSERT_EQ(3U, top_frame_host()->child_count());
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token = top_frame_host()->GetEmbeddingToken().value();

  // Child 0 (b) should have an embedding token.
  auto child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);
  EXPECT_NE(top_token, child_0_token);

  // Child 0 (a) of Child 0 (b) should have an embedding token.
  ASSERT_EQ(1U, top_frame_host()->child_at(0)->child_count());
  auto child_0_0_token = top_frame_host()
                             ->child_at(0)
                             ->child_at(0)
                             ->current_frame_host()
                             ->GetEmbeddingToken();
  ASSERT_TRUE(child_0_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_0_token);
  EXPECT_NE(top_token, child_0_0_token);
  EXPECT_NE(child_0_token, child_0_0_token);

  // Child 1 (c) should have an embedding token.
  auto child_1_token =
      top_frame_host()->child_at(1)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_1_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_1_token);
  EXPECT_NE(top_token, child_1_token);
  EXPECT_NE(child_0_token, child_1_token);
  EXPECT_NE(child_0_0_token, child_1_token);

  // Child 2 (a) should have an embedding token.
  auto child_2_token =
      top_frame_host()->child_at(2)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_2_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_2_token);
  EXPECT_NE(top_token, child_2_token);
  EXPECT_NE(child_0_token, child_2_token);
  EXPECT_NE(child_0_0_token, child_2_token);

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokenSwapsOnCrossDocumentNavigation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token = top_frame_host()->GetEmbeddingToken().value();

  // Child 0 (b) should have an embedding token.
  RenderFrameHost* target = top_frame_host()->child_at(0)->current_frame_host();
  auto child_0_token = target->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);
  EXPECT_NE(top_token, child_0_token);

  // Navigate child 0 (b) to same-site the token should swap.
  NavigateIframeToURL(shell()->web_contents(), "child-0",
                      embedded_test_server()
                          ->GetURL("b.com", "/site_isolation/")
                          .Resolve("blank.html"));
  auto same_site_new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(same_site_new_child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), same_site_new_child_0_token);
  EXPECT_NE(top_token, same_site_new_child_0_token);
  EXPECT_NE(child_0_token, same_site_new_child_0_token);

  // Navigate child 0 (b) to another site (cross-process) the token should swap.
  {
    // The RenderFrameHost might have been replaced when the frame navigated.
    target = top_frame_host()->child_at(0)->current_frame_host();
    RenderFrameDeletedObserver deleted_observer(target);
    NavigateIframeToURL(shell()->web_contents(), "child-0",
                        embedded_test_server()
                            ->GetURL("c.com", "/site_isolation/")
                            .Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }
  auto new_site_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(same_site_new_child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), new_site_child_0_token);
  EXPECT_NE(top_token, new_site_child_0_token);
  EXPECT_NE(child_0_token, new_site_child_0_token);
  EXPECT_NE(same_site_new_child_0_token, new_site_child_0_token);

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(
    EmbeddingTokenBrowserTest,
    EmbeddingTokenNotChangedOnSubframeSameDocumentNavigation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token = top_frame_host()->GetEmbeddingToken().value();

  // Child 0 (a) should have an embedding token.
  RenderFrameHost* target = top_frame_host()->child_at(0)->current_frame_host();
  auto child_0_token = target->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);
  EXPECT_NE(top_token, child_0_token);

  auto b_url = embedded_test_server()->GetURL("b.com", "/site_isolation/");
  // Navigate child 0 to another site (cross-process) a token should be created.
  {
    RenderFrameDeletedObserver deleted_observer(
        top_frame_host()->child_at(0)->current_frame_host());
    NavigateIframeToURL(web_contents(), "child-0", b_url.Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }

  // Child 0 (b) should have a new embedding token.
  auto new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), new_child_0_token);
  EXPECT_NE(top_token, new_child_0_token);
  EXPECT_NE(child_0_token, new_child_0_token);

  // Navigate child 0 (b) to same document the token should not swap.
  NavigateIframeToURL(web_contents(), "child-0",
                      b_url.Resolve("blank.html#foo"));
  auto same_document_new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(same_document_new_child_0_token.has_value());
  EXPECT_EQ(new_child_0_token, same_document_new_child_0_token);

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       EmbeddingTokenChangedOnSubframeNavigationToNewDocument) {
  auto a_url = embedded_test_server()->GetURL("a.com", "/");
  EXPECT_TRUE(NavigateToURL(
      shell(), a_url.Resolve("cross_site_iframe_factory.html?a(b)")));

  ASSERT_EQ(1U, top_frame_host()->child_count());
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token = top_frame_host()->GetEmbeddingToken().value();

  // Child 0 (b) should have an embedding token.
  RenderFrameHost* target = top_frame_host()->child_at(0)->current_frame_host();
  auto child_0_token = target->GetEmbeddingToken();
  ASSERT_TRUE(child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), child_0_token);
  EXPECT_NE(top_token, child_0_token);

  // Navigate child 0 (b) to the same site as the main frame. This should create
  // an embedding token.
  {
    RenderFrameDeletedObserver deleted_observer(target);
    NavigateIframeToURL(web_contents(), "child-0",
                        a_url.Resolve("site_isolation/").Resolve("blank.html"));
    deleted_observer.WaitUntilDeleted();
  }

  auto new_child_0_token =
      top_frame_host()->child_at(0)->current_frame_host()->GetEmbeddingToken();
  ASSERT_TRUE(new_child_0_token.has_value());
  EXPECT_NE(base::UnguessableToken::Null(), new_child_0_token);
  EXPECT_NE(top_token, new_child_0_token);
  EXPECT_NE(child_0_token, new_child_0_token);

  // TODO(ckitagawa): Somehow assert that the parent and child have matching
  // embedding tokens in parent HTMLOwnerElement and child LocalFrame.
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       BackForwardCacheCrossDocument) {
  auto a_url = embedded_test_server()->GetURL("a.com", "/site_isolation/");
  auto b_url = embedded_test_server()->GetURL("b.com", "/site_isolation/");
  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html")));

  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a = top_frame_host()->GetEmbeddingToken().value();

  EXPECT_TRUE(NavigateToURL(shell(), b_url.Resolve("blank.html")));
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_b = top_frame_host()->GetEmbeddingToken().value();
  EXPECT_NE(top_token_a, top_token_b);

  // Navigate back to the first origin. The back forward cache should keep
  // the embedding token.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a_prime = top_frame_host()->GetEmbeddingToken().value();
  EXPECT_EQ(top_token_a, top_token_a_prime);
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       BackForwardCacheCrossDocumentAfterSameDocument) {
  auto a_url = embedded_test_server()->GetURL("a.com", "/site_isolation/");
  auto b_url = embedded_test_server()->GetURL("b.com", "/site_isolation/");
  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html")));

  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a = top_frame_host()->GetEmbeddingToken().value();

  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html#foo")));
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  EXPECT_EQ(top_frame_host()->GetEmbeddingToken().value(), top_token_a);

  EXPECT_TRUE(NavigateToURL(shell(), b_url.Resolve("blank.html")));
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_b = top_frame_host()->GetEmbeddingToken().value();
  EXPECT_NE(top_token_a, top_token_b);

  // Navigate back to the first origin. The back forward cache should keep
  // the embedding token even when the embedding token is not present in the
  // most recent navigation.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a_prime = top_frame_host()->GetEmbeddingToken().value();
  EXPECT_EQ(top_token_a, top_token_a_prime);
}

IN_PROC_BROWSER_TEST_F(EmbeddingTokenBrowserTest,
                       SameDocumentHistoryPreservesTokens) {
  auto a_url = embedded_test_server()->GetURL("a.com", "/site_isolation/");
  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html")));

  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a = top_frame_host()->GetEmbeddingToken().value();

  EXPECT_TRUE(NavigateToURL(shell(), a_url.Resolve("blank.html#foo")));
  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a_prime = top_frame_host()->GetEmbeddingToken().value();
  EXPECT_EQ(top_token_a, top_token_a_prime);

  // Navigate back to before the fragment was added. This should preserve the
  // embedding token.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_TRUE(top_frame_host()->GetEmbeddingToken().has_value());
  auto top_token_a_prime_prime = top_frame_host()->GetEmbeddingToken().value();
  EXPECT_EQ(top_token_a, top_token_a_prime_prime);
}

}  // namespace content
