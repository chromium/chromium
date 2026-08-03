// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/render_frame_test_helper.mojom.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

// The general structure of tests is to navigate and check that browser and
// renderer initiator state tokens match.
class InitiatorStateTokenBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  base::UnguessableToken GetBrowserSideToken(ToRenderFrameHost adapter) {
    return static_cast<RenderFrameHostImpl*>(adapter.render_frame_host())
        ->current_initiator_state_token();
  }

  // Verifies that the browser-side `initiator_state_token` and the
  // renderer-side `initiator_state_token` have matching values.
  [[nodiscard]] ::testing::AssertionResult VerifyMatchingTokens(
      ToRenderFrameHost adapter) {
    base::UnguessableToken token_from_browser = GetBrowserSideToken(adapter);

    mojo::Remote<mojom::RenderFrameTestHelper> remote;
    adapter.render_frame_host()->GetRemoteInterfaces()->GetInterface(
        remote.BindNewPipeAndPassReceiver());
    base::UnguessableToken token_from_renderer;
    base::RunLoop run_loop;
    remote->GetInitiatorStateToken(
        base::BindLambdaForTesting([&](const base::UnguessableToken& token) {
          token_from_renderer = token;
          run_loop.Quit();
        }));
    run_loop.Run();

    if (token_from_browser == token_from_renderer) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << "browser token was " << token_from_browser
           << " but renderer token was " << token_from_renderer;
  }

  // Whether or not `NavigateAndGetNewToken()` should wait for the response and
  // validate initiator state token state immediately afterwards. Most tests
  // should expect and wait for a response; however, tests that are exercising
  // `CommitFailedNavigation()` will probably want to specify `kNo`.
  enum class ExpectedResponse {
    kYes,
    kNo,
  };

  // Navigate `adapter.render_frame_host()` to `target_url`. Verifies that the
  // browser and renderer state are in sync, and that the initiator state token
  // is not updated until the navigation actually commits.
  base::UnguessableToken NavigateAndGetNewToken(
      ToRenderFrameHost adapter,
      const GURL& target_url,
      ExpectedResponse expect_response = ExpectedResponse::kYes) {
    SCOPED_TRACE(target_url.spec());
    // Capture the FrameTreeNode now; when a navigation commits, the current
    // RenderFrameHost may change.
    RenderFrameHostImpl* const old_render_frame_host =
        static_cast<RenderFrameHostImpl*>(adapter.render_frame_host());
    FrameTreeNode* const frame_tree_node =
        old_render_frame_host->frame_tree_node();
    const base::UnguessableToken old_token =
        GetBrowserSideToken(old_render_frame_host);

    // Start a new navigation in the main frame. The navigation is still
    // ongoing, so `InitiatorStateToken` should not be updated yet.
    TestNavigationManager nav_manager(
        WebContents::FromRenderFrameHost(old_render_frame_host), target_url);
    EXPECT_TRUE(BeginNavigateToURLFromRenderer(adapter, target_url));
    EXPECT_TRUE(VerifyMatchingTokens(old_render_frame_host));
    EXPECT_EQ(old_token, GetBrowserSideToken(old_render_frame_host));

    // Just before the request is actually issued, the navigation is still
    // ongoing, so `InitiatorStateToken` should not be updated yet.
    EXPECT_TRUE(nav_manager.WaitForRequestStart());
    EXPECT_TRUE(VerifyMatchingTokens(old_render_frame_host));
    EXPECT_EQ(old_token, GetBrowserSideToken(old_render_frame_host));

    if (ExpectedResponse::kYes == expect_response) {
      // Just before reading the response, the navigation is still ongoing, so
      // `InitiatorStateToken` should not be updated yet.
      EXPECT_TRUE(nav_manager.WaitForResponse());
      EXPECT_TRUE(VerifyMatchingTokens(old_render_frame_host));
      EXPECT_EQ(old_token, GetBrowserSideToken(old_render_frame_host));
    }

    // Once a cross-document navigation completes, the initiator state token
    // should be updated though.
    EXPECT_TRUE(nav_manager.WaitForNavigationFinished());
    // The RenderFrameHost may have changed; use the FrameTreeNode captured
    // above instead.
    RenderFrameHostImpl* const new_render_frame_host =
        frame_tree_node->current_frame_host();
    EXPECT_EQ(target_url, new_render_frame_host->GetLastCommittedURL());
    EXPECT_TRUE(VerifyMatchingTokens(new_render_frame_host));
    const base::UnguessableToken new_token =
        GetBrowserSideToken(new_render_frame_host);
    EXPECT_NE(new_token, old_token);
    return new_token;
  }
};

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, MainFrameBasic) {
  std::vector<base::UnguessableToken> seen_tokens;

  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  seen_tokens.push_back(GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  seen_tokens.push_back(NavigateAndGetNewToken(
      web_contents(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  std::set unique_tokens(seen_tokens.begin(), seen_tokens.end());
  EXPECT_EQ(unique_tokens.size(), seen_tokens.size());
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, FailedNavigation) {
  std::vector<base::UnguessableToken> seen_tokens;

  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  seen_tokens.push_back(GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      web_contents(), embedded_test_server()->GetURL("a.com", "/close-socket"),
      ExpectedResponse::kNo));

  seen_tokens.push_back(NavigateAndGetNewToken(
      web_contents(), embedded_test_server()->GetURL("a.com", "/close-socket"),
      ExpectedResponse::kNo));

  seen_tokens.push_back(NavigateAndGetNewToken(
      web_contents(), embedded_test_server()->GetURL("b.com", "/close-socket"),
      ExpectedResponse::kNo));

  // Test that a regular successful navigation still updates the initiator state
  // token.
  seen_tokens.push_back(NavigateAndGetNewToken(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  std::set unique_tokens(seen_tokens.begin(), seen_tokens.end());
  EXPECT_EQ(unique_tokens.size(), seen_tokens.size());
}

}  // namespace

}  // namespace content
