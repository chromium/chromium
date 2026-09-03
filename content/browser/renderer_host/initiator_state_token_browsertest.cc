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
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/tokens/tokens.h"
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

  blink::InitiatorStateToken GetBrowserSideToken(ToRenderFrameHost adapter) {
    return static_cast<RenderFrameHostImpl*>(adapter.render_frame_host())
        ->current_initiator_state_token();
  }

  // Verifies that the browser-side `initiator_state_token` and the
  // renderer-side `initiator_state_token` have matching values.
  [[nodiscard]] ::testing::AssertionResult VerifyMatchingTokens(
      ToRenderFrameHost adapter) {
    blink::InitiatorStateToken token_from_browser =
        GetBrowserSideToken(adapter);

    mojo::Remote<mojom::RenderFrameTestHelper> remote;
    adapter.render_frame_host()->GetRemoteInterfaces()->GetInterface(
        remote.BindNewPipeAndPassReceiver());
    blink::InitiatorStateToken token_from_renderer;
    base::RunLoop run_loop;
    remote->GetInitiatorStateToken(base::BindLambdaForTesting(
        [&](const blink::InitiatorStateToken& token) {
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
  blink::InitiatorStateToken NavigateAndGetNewToken(
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
    const blink::InitiatorStateToken old_token =
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
    const blink::InitiatorStateToken new_token =
        GetBrowserSideToken(new_render_frame_host);
    EXPECT_NE(new_token, old_token);
    return new_token;
  }
};

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, MainFrameBasic) {
  std::vector<blink::InitiatorStateToken> seen_tokens;

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

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, SubFrameBasic) {
  std::vector<blink::InitiatorStateToken> seen_tokens;

  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "a.com", "/cross_site_iframe_factory.html?a(a)")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  EXPECT_TRUE(VerifyMatchingTokens(ChildFrameAt(web_contents(), 0)));
  seen_tokens.push_back(GetBrowserSideToken(web_contents()));
  seen_tokens.push_back(GetBrowserSideToken(ChildFrameAt(web_contents(), 0)));

  seen_tokens.push_back(NavigateAndGetNewToken(
      ChildFrameAt(web_contents(), 0),
      embedded_test_server()->GetURL("a.com", "/title1.html")));
  // Main document did not navigate so the token should be the same.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      ChildFrameAt(web_contents(), 0),
      embedded_test_server()->GetURL("b.com", "/title1.html")));
  // Main document did not navigate so the token should be the same.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  std::set unique_tokens(seen_tokens.begin(), seen_tokens.end());
  EXPECT_EQ(unique_tokens.size(), seen_tokens.size());
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, NewWindowBasic) {
  std::vector<blink::InitiatorStateToken> seen_tokens;

  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_EQ(1u, Shell::windows().size());
  seen_tokens.push_back(GetBrowserSideToken(web_contents()));

  WebContents* new_contents = nullptr;
  {
    // This block is largely derived from `NavigateAndGetNewToken()`. This test
    // cannot easily reuse that helper because:
    //
    // - it is important to specify an actual target URL other than about:blank
    //   for `window.open()`. Specifying no target URL and then later navigating
    //   the window has subtly different behavior (e.g. the
    //   `NewWindowSyncCommit` test below).
    // - the helper expects the `WebContents` to already exist in order to
    // install
    //   `TestNavigationManager`. However, in this test, a new `WebContents` is
    //   created in the process of running the test.
    ShellAddedObserver wait_for_new_shell;
    ExecuteScriptAsync(web_contents(), JsReplace("window.open($1)",
                                                 embedded_test_server()->GetURL(
                                                     "a.com", "/title1.html")));
    new_contents = wait_for_new_shell.GetShell()->web_contents();
    DCHECK_EQ(2u, Shell::windows().size());
    DCHECK_EQ(new_contents, Shell::windows()[1]->web_contents());
    DCHECK_NE(new_contents, web_contents());
    seen_tokens.push_back(GetBrowserSideToken(new_contents));
    TestNavigationManager nav_manager(
        new_contents, embedded_test_server()->GetURL("a.com", "/title1.html"));

    // Capture the FrameTreeNode now; when a navigation commits, the current
    // RenderFrameHost may change.
    RenderFrameHostImpl* const old_render_frame_host =
        static_cast<RenderFrameHostImpl*>(new_contents->GetPrimaryMainFrame());
    FrameTreeNode* const frame_tree_node =
        old_render_frame_host->frame_tree_node();
    const blink::InitiatorStateToken old_token =
        GetBrowserSideToken(new_contents);

    EXPECT_TRUE(VerifyMatchingTokens(new_contents));
    EXPECT_EQ(old_token, GetBrowserSideToken(new_contents));
    EXPECT_NE(old_token, GetBrowserSideToken(web_contents()));
    // Even after creating a new window, the original `WebContents` should still
    // have the same initiator state token.
    EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

    // Just before the request is actually issued, the navigation is still
    // ongoing, so initiator state token should not be updated yet.
    EXPECT_TRUE(nav_manager.WaitForRequestStart());
    EXPECT_TRUE(VerifyMatchingTokens(new_contents));
    EXPECT_EQ(old_token, GetBrowserSideToken(new_contents));
    // The original `WebContents` should still have the same initiator state
    // token.
    EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

    // Just before reading the response, the navigation is still ongoing, so
    // initiator state token should not be updated yet.
    EXPECT_TRUE(nav_manager.WaitForResponse());
    EXPECT_TRUE(VerifyMatchingTokens(new_contents));
    EXPECT_EQ(old_token, GetBrowserSideToken(new_contents));
    // The original `WebContents` should still have the same initiator state
    // token.
    EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

    // Once a cross-document navigation completes, the initiator state token
    // should be updated though.
    ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
    // The RenderFrameHost may have changed; use the FrameTreeNode captured
    // above instead.
    RenderFrameHostImpl* const new_render_frame_host =
        frame_tree_node->current_frame_host();
    EXPECT_EQ(embedded_test_server()->GetURL("a.com", "/title1.html"),
              new_render_frame_host->GetLastCommittedURL());
    EXPECT_TRUE(VerifyMatchingTokens(new_render_frame_host));
    const blink::InitiatorStateToken new_token =
        GetBrowserSideToken(new_render_frame_host);
    EXPECT_NE(new_token, old_token);
    seen_tokens.push_back(new_token);
    // The original `WebContents` should still have the same initiator state
    // token.
    EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));
  }

  seen_tokens.push_back(NavigateAndGetNewToken(
      new_contents, embedded_test_server()->GetURL("a.com", "/title1.html")));
  // The original `WebContents` should still have the same initiator state
  // token.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      new_contents, embedded_test_server()->GetURL("b.com", "/title1.html")));
  // The original `WebContents` should still have the same initiator state
  // token.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  std::set unique_tokens(seen_tokens.begin(), seen_tokens.end());
  EXPECT_EQ(unique_tokens.size(), seen_tokens.size());
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, SubFrameSyncCommit) {
  std::vector<blink::InitiatorStateToken> seen_tokens;

  // This is a basic test that the synchronous commit of about:blank properly
  // propagates the initiator state token in both the renderer and the browser
  // process. Note that unlike the DocumentToken, the synchronous commit
  // generates a new initiator state token.
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("a.com", "/page_with_blank_iframe.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));

  // Even if the iframe committed its initial empty document synchronously, it
  // should be created with matching tokens in the browser and the renderer
  // process.
  EXPECT_TRUE(VerifyMatchingTokens(ChildFrameAt(web_contents(), 0)));
  seen_tokens.push_back(GetBrowserSideToken(web_contents()));
  seen_tokens.push_back(GetBrowserSideToken(ChildFrameAt(web_contents(), 0)));

  seen_tokens.push_back(NavigateAndGetNewToken(
      ChildFrameAt(web_contents(), 0),
      embedded_test_server()->GetURL("a.com", "/title1.html")));
  // Main document did not navigate so the token should be the same.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      ChildFrameAt(web_contents(), 0),
      embedded_test_server()->GetURL("b.com", "/title1.html")));
  // Main document did not navigate so the token should be the same.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  std::set unique_tokens(seen_tokens.begin(), seen_tokens.end());
  EXPECT_EQ(unique_tokens.size(), seen_tokens.size());
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, NewWindowSyncCommit) {
  std::vector<blink::InitiatorStateToken> seen_tokens;

  ASSERT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  seen_tokens.push_back(GetBrowserSideToken(web_contents()));

  // This is a basic test that the synchronous commit of about:blank reuses the
  // same initiator state token.
  ASSERT_TRUE(ExecJs(web_contents(), "window.open()"));
  ASSERT_EQ(2u, Shell::windows().size());
  WebContents* new_contents = Shell::windows()[1]->web_contents();
  DCHECK_NE(new_contents, web_contents());
  // Even if the newly created window committed its initial empty document
  // synchronously, it should be created with matching tokens in the browser and
  // the renderer process.
  EXPECT_TRUE(VerifyMatchingTokens(new_contents));
  // The original `WebContents` should still have the same initiator state
  // token.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      new_contents, embedded_test_server()->GetURL("a.com", "/title1.html")));
  // The original `WebContents` should still have the same initiator state
  // token.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      new_contents, embedded_test_server()->GetURL("a.com", "/title1.html")));
  // The original `WebContents` should still have the same initiator state
  // token.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  seen_tokens.push_back(NavigateAndGetNewToken(
      new_contents, embedded_test_server()->GetURL("b.com", "/title1.html")));
  // The original `WebContents` should still have the same initiator state
  // token.
  EXPECT_EQ(seen_tokens[0], GetBrowserSideToken(web_contents()));

  std::set unique_tokens(seen_tokens.begin(), seen_tokens.end());
  EXPECT_EQ(unique_tokens.size(), seen_tokens.size());
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, JavascriptURL) {
  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken token = GetBrowserSideToken(web_contents());

  // A javascript: navigation that replaces the document should not change the
  // initiator state token. This does not use the normal Navigate*() helpers
  // since it does not commit a normal cross-document navigation.
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("location = $1", "javascript:'Hello world!'")));
  EXPECT_EQ("Hello world!", EvalJs(web_contents(), "document.body.innerText"));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  EXPECT_EQ(token, GetBrowserSideToken(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, FailedNavigation) {
  std::vector<blink::InitiatorStateToken> seen_tokens;

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

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, CrashThenReload) {
  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken old_token =
      GetBrowserSideToken(web_contents());

  // Cause the renderer to crash.
  RenderProcessHostWatcher crash_observer(
      web_contents(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_FALSE(NavigateToURL(shell(), GURL(blink::kChromeUICrashURL)));
  // Wait for browser to notice the renderer crash.
  crash_observer.Wait();

  // After a crash, the initiator state token should still be the same even
  // though the renderer process is gone.
  EXPECT_EQ(old_token, GetBrowserSideToken(web_contents()));

  // But when a live RenderFrame is needed again, RenderDocument should force a
  // new RenderFrameHost, and thus, a new initiator state token. The remainder
  // of this test does not use `NavigateAndGetNewToken()`, which tries to use a
  // renderer-initiated navigation (which is not possible when the renderer is
  // not live).
  TestNavigationManager nav_manager(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html"));
  shell()->LoadURL(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken token_after_navigation_started =
      GetBrowserSideToken(web_contents());
  EXPECT_NE(token_after_navigation_started, old_token);

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken token_after_navigation_finished =
      GetBrowserSideToken(web_contents());
  EXPECT_NE(token_after_navigation_finished, old_token);
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest,
                       CrashThenImmediateReinitialize) {
  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  const blink::LocalFrameToken frame_token = main_frame->GetFrameToken();
  const blink::InitiatorStateToken old_token = GetBrowserSideToken(main_frame);

  // Cause the renderer to crash.
  RenderProcessHostWatcher crash_observer(
      web_contents(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_FALSE(NavigateToURL(shell(), GURL(blink::kChromeUICrashURL)));
  // Wait for browser to notice the renderer crash.
  crash_observer.Wait();

  // If the main render frame is re-initialized, it also gets a new
  // initiator state token. Validate that the new initiator state token is
  // created before the renderer is re-created; a typical failure in this path
  // will manifest as a mismatch between the browser and renderer-side initiator
  // state tokens.
  main_frame->frame_tree_node()
      ->render_manager()
      ->InitializeMainRenderFrameForImmediateUse();
  // The RenderFrameHost should be reused.
  ASSERT_EQ(frame_token,
            web_contents()->GetPrimaryMainFrame()->GetFrameToken());
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  // The re-created RenderFrame should have a distinct initiator state token.
  const blink::InitiatorStateToken new_token =
      GetBrowserSideToken(web_contents());
  EXPECT_NE(new_token, old_token);
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest, UpdateReferrerPolicy) {
  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken initial_token =
      GetBrowserSideToken(web_contents());

  ASSERT_TRUE(ExecJs(web_contents(),
                     "let meta = document.createElement('meta');"
                     "meta.name = 'referrer';"
                     "meta.content = 'no-referrer';"
                     "document.head.appendChild(meta);"));

  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken updated_token =
      GetBrowserSideToken(web_contents());
  EXPECT_NE(initial_token, updated_token);
}

IN_PROC_BROWSER_TEST_F(InitiatorStateTokenBrowserTest,
                       UpdateContentSecurityPolicy) {
  ASSERT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken initial_token =
      GetBrowserSideToken(web_contents());

  ASSERT_TRUE(ExecJs(web_contents(),
                     "let meta = document.createElement('meta');"
                     "meta.httpEquiv = 'content-security-policy';"
                     "meta.content = \"script-src 'self'\";"
                     "document.head.appendChild(meta);"));

  EXPECT_TRUE(VerifyMatchingTokens(web_contents()));
  const blink::InitiatorStateToken updated_token =
      GetBrowserSideToken(web_contents());
  EXPECT_NE(initial_token, updated_token);
}

}  // namespace

}  // namespace content
