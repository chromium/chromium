// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/find_request_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

namespace {

const int kInvalidId = -1;

}  // namespace

class FindRequestManagerTest : public ContentBrowserTest,
                               public testing::WithParamInterface<bool> {
 public:
  FindRequestManagerTest()
      : normal_delegate_(nullptr),
        last_request_id_(0) {}
  ~FindRequestManagerTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Swap the WebContents's delegate for our test delegate.
    normal_delegate_ = contents()->GetDelegate();
    contents()->SetDelegate(&test_delegate_);
  }

  void TearDownOnMainThread() override {
    // Swap the WebContents's delegate back to its usual delegate.
    contents()->SetDelegate(normal_delegate_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 protected:
  // Navigates to |url| and waits for it to finish loading.
  void LoadAndWait(const std::string& url) {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("a.com", url)));
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Loads a multi-frame page. The page will have a full binary frame tree of
  // height |height|. If |cross_process| is true, child frames will be loaded
  // cross-process.
  void LoadMultiFramePage(int height, bool cross_process) {
    LoadAndWait("/find_in_page_multi_frame.html");
    FrameTreeNode* root = contents()->GetFrameTree()->root();
    LoadMultiFramePageChildFrames(height, cross_process, root);
  }

  // Reloads the child frame cross-process.
  void MakeChildFrameCrossProcess() {
    FrameTreeNode* root = contents()->GetFrameTree()->root();
    FrameTreeNode* child = root->child_at(0);
    GURL url(embedded_test_server()->GetURL(
        "b.com", child->current_url().path()));

    TestNavigationObserver observer(shell()->web_contents());
    NavigateFrameToURL(child, url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  void Find(const std::string& search_text,
            blink::mojom::FindOptionsPtr options) {
    delegate()->UpdateLastRequest(++last_request_id_);
    contents()->Find(last_request_id_, base::UTF8ToUTF16(search_text),
                     std::move(options));
  }

  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  FindTestWebContentsDelegate* delegate() const {
    return static_cast<FindTestWebContentsDelegate*>(contents()->GetDelegate());
  }

  int last_request_id() const {
    return last_request_id_;
  }

 private:
  // Helper function for LoadMultiFramePage. Loads child frames until the frame
  // tree rooted at |root| is a full binary tree of height |height|.
  void LoadMultiFramePageChildFrames(int height,
                                     bool cross_process,
                                     FrameTreeNode* root) {
    if (height == 0)
      return;

    std::string hostname = root->current_origin().host();
    if (cross_process)
      hostname.insert(0, 1, 'a');
    GURL url(embedded_test_server()->GetURL(hostname,
                                            "/find_in_page_multi_frame.html"));

    TestNavigationObserver observer(shell()->web_contents());

    FrameTreeNode* child = root->child_at(0);
    NavigateFrameToURL(child, url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    LoadMultiFramePageChildFrames(height - 1, cross_process, child);

    child = root->child_at(1);
    NavigateFrameToURL(child, url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    LoadMultiFramePageChildFrames(height - 1, cross_process, child);
  }

  FindTestWebContentsDelegate test_delegate_;
  WebContentsDelegate* normal_delegate_;

  // The ID of the last find request requested.
  int last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(FindRequestManagerTest);
};

INSTANTIATE_TEST_SUITE_P(FindRequestManagerTests,
                         FindRequestManagerTest,
                         testing::Bool());

// TODO(crbug.com/615291): These tests frequently fail on Android.
#if defined(OS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif


// Tests basic find-in-page functionality (such as searching forward and
// backward) and check for correct results at each step.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(Basic)) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  options->find_next = true;
  for (int i = 2; i <= 10; ++i) {
    Find("result", options->Clone());
    delegate()->WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }

  options->forward = false;
  for (int i = 9; i >= 5; --i) {
    Find("result", options->Clone());
    delegate()->WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }
}

bool ExecuteScriptAndExtractRect(FrameTreeNode* frame,
                                 const std::string& script,
                                 gfx::Rect* out) {
  std::string result;
  std::string script_and_extract =
      script +
      "window.domAutomationController.send(rect.x + ',' + rect.y + ','" +
      "+ rect.width + ',' + rect.height);";
  if (!ExecuteScriptAndExtractString(frame, script_and_extract, &result))
    return false;

  std::vector<std::string> tokens = base::SplitString(
      result, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() != 4U)
    return false;

  double x, y, width, height;
  if (!base::StringToDouble(tokens[0], &x) ||
      !base::StringToDouble(tokens[1], &y) ||
      !base::StringToDouble(tokens[2], &width) ||
      !base::StringToDouble(tokens[3], &height))
    return false;

  *out = gfx::Rect(static_cast<int>(x), static_cast<int>(y),
                   static_cast<int>(width), static_cast<int>(height));
  return true;
}

// Basic test that a search result is actually brought into view.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, ScrollAndZoomIntoView) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  WebPreferences prefs =
      web_contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.smooth_scroll_for_find_enabled = false;
  web_contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  LoadAndWait("/find_in_page_desktop.html");
  // Note: for now, don't run this test on Android in OOPIF mode.
  if (GetParam())
#if defined(OS_ANDROID)
    return;
#else
    MakeChildFrameCrossProcess();
#endif  // defined(OS_ANDROID)

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child = root->child_at(0);

  // Start off at a non-origin scroll offset to ensure coordinate conversisons
  // work correctly.
  ASSERT_TRUE(ExecuteScript(root, "window.scrollTo(3500, 1500);"));

  // Search for a result further down in the iframe.
  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result 17", options->Clone());
  delegate()->WaitForFinalReply();

  // gBCR of result box in iframe.
  gfx::Rect target_in_iframe;

  // gBCR of iframe in main document.
  gfx::Rect iframe_rect;

  // Window size with location at origin (for comparison with gBCR).
  gfx::Rect root_rect;

  // Visual viewport rect relative to root_rect.
  gfx::Rect visual_rect;

  ASSERT_TRUE(ExecuteScriptAndExtractRect(
      child,
      "var result = document.querySelector('.margin-overflow');"
      "var rect = result.getBoundingClientRect();",
      &target_in_iframe));
  ASSERT_TRUE(ExecuteScriptAndExtractRect(
      root,
      "var rect = document.querySelector('#frame').getBoundingClientRect();",
      &iframe_rect));
  ASSERT_TRUE(ExecuteScriptAndExtractRect(
      root,
      "var rect = new DOMRect(0, 0, window.innerWidth, window.innerHeight);",
      &root_rect));
  ASSERT_TRUE(ExecuteScriptAndExtractRect(
      root,
      "var rect = new DOMRect(visualViewport.offsetLeft, "
      "                       visualViewport.offsetTop,"
      "                       visualViewport.width,"
      "                       visualViewport.height);",
      &visual_rect));

  gfx::Rect result_in_root = target_in_iframe + iframe_rect.OffsetFromOrigin();

  EXPECT_TRUE(gfx::Rect(iframe_rect.size()).Contains(target_in_iframe))
      << "Result rect[ " << target_in_iframe.ToString()
      << " ] not visible in iframe [ 0,0 " << iframe_rect.size().ToString()
      << " ].";

  EXPECT_TRUE(root_rect.Contains(result_in_root))
      << "Result rect[ " << result_in_root.ToString()
      << " ] not visible in root frame [ " << root_rect.ToString() << " ].";

  EXPECT_TRUE(visual_rect.Contains(result_in_root))
      << "Result rect[ " << result_in_root.ToString()
      << " ] not visible in visual viewport [ " << visual_rect.ToString()
      << " ].";
}

// Tests searching for a word character-by-character, as would typically be done
// by a user typing into the find bar.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(CharacterByCharacter)) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("r", default_options->Clone());
  Find("re", default_options->Clone());
  Find("res", default_options->Clone());
  Find("resu", default_options->Clone());
  Find("resul", default_options->Clone());
  Find("result", default_options->Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// TODO(crbug.com/615291): This test frequently fails on Android.
// TODO(crbug.com/674742): This test is flaky on Win
// TODO(crbug.com/850286): Flaky on CrOS MSan
// Tests sending a large number of find requests subsequently.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_RapidFire) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());

  options->find_next = true;
  for (int i = 2; i <= 1000; ++i)
    Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(last_request_id() % results.number_of_matches,
            results.active_match_ordinal);
}

// Tests removing a frame during a find session.
// TODO(crbug.com/657331): Test is flaky on all platforms.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_RemoveFrame) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();
  options->find_next = true;
  options->forward = false;
  Find("result", options->Clone());
  Find("result", options->Clone());
  Find("result", options->Clone());
  Find("result", options->Clone());
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(17, results.active_match_ordinal);

  // Remove a frame.
  FrameTreeNode* root = contents()->GetFrameTree()->root();
  root->current_frame_host()->RemoveChild(root->child_at(0));

  // The number of matches and active match ordinal should update automatically
  // to exclude the matches from the removed frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(12, results.number_of_matches);
  EXPECT_EQ(8, results.active_match_ordinal);
}

// Tests adding a frame during a find session.
// TODO(crbug.com/657331): Test is flaky on all platforms.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_AddFrame) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  options->find_next = true;
  Find("result", options.Clone());
  Find("result", options.Clone());
  Find("result", options.Clone());
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(5, results.active_match_ordinal);

  // Add a frame. It contains 5 new matches.
  std::string url = embedded_test_server()->GetURL(
      GetParam() ? "b.com" : "a.com", "/find_in_simple_page.html").spec();
  std::string script = std::string() +
      "var frame = document.createElement('iframe');" +
      "frame.src = '" + url + "';" +
      "document.body.appendChild(frame);";
  delegate()->MarkNextReply();
  ASSERT_TRUE(ExecuteScript(shell(), script));
  delegate()->WaitForNextReply();

  // The number of matches should update automatically to include the matches
  // from the newly added frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(26, results.number_of_matches);
  EXPECT_EQ(5, results.active_match_ordinal);
}

// Tests adding a frame during a find session where there were previously no
// matches.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(AddFrameAfterNoMatches)) {
  TestNavigationObserver navigation_observer(contents());
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("result", default_options.Clone());
  delegate()->WaitForFinalReply();

  // Initially, there are no matches on the page.
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(0, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);

  // Add a frame. It contains 5 new matches.
  std::string url =
      embedded_test_server()->GetURL("/find_in_simple_page.html").spec();
  std::string script = std::string() +
      "var frame = document.createElement('iframe');" +
      "frame.src = '" + url + "';" +
      "document.body.appendChild(frame);";
  delegate()->MarkNextReply();
  ASSERT_TRUE(ExecuteScript(shell(), script));
  delegate()->WaitForNextReply();

  // The matches from the new frame should be found automatically, and the first
  // match in the frame should be activated.
  results = delegate()->GetFindResults();
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests a frame navigating to a different page during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(NavigateFrame)) {
  LoadMultiFramePage(2 /* height */, GetParam() /* cross_process */);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  options->find_next = true;
  options->forward = false;
  Find("result", options.Clone());
  Find("result", options.Clone());
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(21, results.number_of_matches);
  EXPECT_EQ(19, results.active_match_ordinal);

  // Navigate one of the empty frames to a page with 5 matches.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  GURL url(embedded_test_server()->GetURL(
      GetParam() ? "b.com" : "a.com", "/find_in_simple_page.html"));
  delegate()->MarkNextReply();
  TestNavigationObserver navigation_observer(contents());
  NavigateFrameToURL(root->child_at(0)->child_at(1)->child_at(0), url);
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  delegate()->WaitForNextReply();

  // The navigation results in an extra reply before the one we care about. This
  // extra reply happens because the RenderFrameHost changes before it navigates
  // (because the navigation is cross-origin). The first reply will not change
  // the number of matches because the frame that is navigating was empty
  // before.
  if (delegate()->GetFindResults().number_of_matches == 21) {
    delegate()->MarkNextReply();
    delegate()->WaitForNextReply();
  }

  // The number of matches and the active match ordinal should update
  // automatically to include the new matches.
  results = delegate()->GetFindResults();
  EXPECT_EQ(26, results.number_of_matches);
  EXPECT_EQ(24, results.active_match_ordinal);
}

// Tests Searching in a hidden frame. Matches in the hidden frame should be
// ignored.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(HiddenFrame)) {
  LoadAndWait("/find_in_hidden_frame.html");

  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("hello", default_options.Clone());
  delegate()->WaitForFinalReply();
  FindResults results = delegate()->GetFindResults();

  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(1, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests that new matches can be found in dynamically added text.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(FindNewMatches)) {
  LoadAndWait("/find_in_dynamic_page.html");

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  options->find_next = true;
  Find("result", options.Clone());
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(3, results.number_of_matches);
  EXPECT_EQ(3, results.active_match_ordinal);

  // Dynamically add new text to the page. This text contains 5 new matches for
  // "result".
  ASSERT_TRUE(ExecuteScript(contents()->GetMainFrame(), "addNewText()"));

  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(8, results.number_of_matches);
  EXPECT_EQ(4, results.active_match_ordinal);
}

// TODO(crbug.com/615291): These tests frequently fail on Android.
// TODO(crbug.com/779912): Flaky timeout on Win7 (dbg).
// TODO(crbug.com/875306): Flaky on Win10.
#if defined(OS_ANDROID) || defined(OS_WIN)
#define MAYBE_FindInPage_Issue627799 DISABLED_FindInPage_Issue627799
#else
#define MAYBE_FindInPage_Issue627799 FindInPage_Issue627799
#endif

IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE_FindInPage_Issue627799) {
  LoadAndWait("/find_in_long_page.html");

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("42", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(970, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  delegate()->StartReplyRecord();
  options->find_next = true;
  options->forward = false;
  Find("42", options.Clone());
  delegate()->WaitForFinalReply();

  // This is the crux of the issue that this test guards against. Searching
  // across the frame boundary should not cause the frame to be re-scoped. If
  // the re-scope occurs, then we will see the number of matches change in one
  // of the recorded find replies.
  for (auto& reply : delegate()->GetReplyRecord()) {
    EXPECT_EQ(last_request_id(), reply.request_id);
    EXPECT_TRUE(reply.number_of_matches == kInvalidId ||
                reply.number_of_matches == results.number_of_matches);
  }
}

IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, DetachFrameWithMatch) {
  // Detaching an iframe with matches when the main document doesn't
  // have matches should work and just remove the matches from the
  // removed frame.
  LoadAndWait("/find_in_page_two_frames.html");
  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;

  Find("result", options.Clone());
  delegate()->WaitForFinalReply();
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(6, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.body.removeChild("
                            "document.querySelectorAll('iframe')[0])"));

  Find("result", options.Clone());
  delegate()->WaitForFinalReply();
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(3, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(FindInPage_Issue644448)) {
  TestNavigationObserver navigation_observer(contents());
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("result", default_options.Clone());
  delegate()->WaitForFinalReply();

  // Initially, there are no matches on the page.
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(0, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);

  // Load a page with matches.
  LoadAndWait("/find_in_simple_page.html");

  Find("result", default_options.Clone());
  delegate()->WaitForFinalReply();

  // There should now be matches found. When the bug was present, there were
  // still no matches found.
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(5, results.number_of_matches);
}

#if defined(OS_ANDROID)
// TODO(wjmaclean): This test, if re-enabled, may require work to make it
// OOPIF-compatible.
// Tests requesting find match rects.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(FindMatchRects)) {
  LoadAndWait("/find_in_page.html");
  if (GetParam())
    MakeChildFrameCrossProcess();

  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("result", default_options.Clone());
  delegate()->WaitForFinalReply();
  EXPECT_EQ(19, delegate()->GetFindResults().number_of_matches);

  // Request the find match rects.
  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();
  const std::vector<gfx::RectF>& rects = delegate()->find_match_rects();

  // The first match should be active.
  EXPECT_EQ(rects[0], delegate()->active_match_rect());

  // All results after the first two should be between them in find-in-page
  // coordinates. This is because results 2 to 19 are inside an iframe located
  // between results 0 and 1. This applies to the fixed div too.
  EXPECT_LT(rects[0].y(), rects[1].y());
  for (int i = 2; i < 19; ++i) {
    EXPECT_LT(rects[0].y(), rects[i].y());
    EXPECT_GT(rects[1].y(), rects[i].y());
  }

  // Result 3 should be below results 2 and 4. This is caused by the CSS
  // transform in the containing div. If the transform doesn't work then result
  // 3 will be between results 2 and 4.
  EXPECT_GT(rects[3].y(), rects[2].y());
  EXPECT_GT(rects[3].y(), rects[4].y());

  // Results 6, 7, 8 and 9 should be one below the other in that same order. If
  // overflow:scroll is not properly handled then result 8 would be below result
  // 9 or result 7 above result 6 depending on the scroll.
  EXPECT_LT(rects[6].y(), rects[7].y());
  EXPECT_LT(rects[7].y(), rects[8].y());
  EXPECT_LT(rects[8].y(), rects[9].y());

  // Results 11, 12, 13 and 14 should be between results 10 and 15, as they are
  // inside the table.
  EXPECT_GT(rects[11].y(), rects[10].y());
  EXPECT_GT(rects[12].y(), rects[10].y());
  EXPECT_GT(rects[13].y(), rects[10].y());
  EXPECT_GT(rects[14].y(), rects[10].y());
  EXPECT_LT(rects[11].y(), rects[15].y());
  EXPECT_LT(rects[12].y(), rects[15].y());
  EXPECT_LT(rects[13].y(), rects[15].y());
  EXPECT_LT(rects[14].y(), rects[15].y());

  // Result 11 should be above results 12, 13 and 14 as it's in the table
  // header.
  EXPECT_LT(rects[11].y(), rects[12].y());
  EXPECT_LT(rects[11].y(), rects[13].y());
  EXPECT_LT(rects[11].y(), rects[14].y());

  // Result 11 should also be right of results 12, 13 and 14 because of the
  // colspan.
  EXPECT_GT(rects[11].x(), rects[12].x());
  EXPECT_GT(rects[11].x(), rects[13].x());
  EXPECT_GT(rects[11].x(), rects[14].x());

  // Result 12 should be left of results 11, 13 and 14 in the table layout.
  EXPECT_LT(rects[12].x(), rects[11].x());
  EXPECT_LT(rects[12].x(), rects[13].x());
  EXPECT_LT(rects[12].x(), rects[14].x());

  // Results 13, 12 and 14 should be one above the other in that order because
  // of the rowspan and vertical-align: middle by default.
  EXPECT_LT(rects[13].y(), rects[12].y());
  EXPECT_LT(rects[12].y(), rects[14].y());

  // Result 16 should be below result 15.
  EXPECT_GT(rects[15].y(), rects[14].y());

  // Result 18 should be normalized with respect to the position:relative div,
  // and not it's immediate containing div. Consequently, result 18 should be
  // above result 17.
  EXPECT_GT(rects[17].y(), rects[18].y());
}

namespace {

class ZoomToFindInPageRectMessageFilter : public content::BrowserMessageFilter {
 public:
  ZoomToFindInPageRectMessageFilter()
      : content::BrowserMessageFilter(WidgetMsgStart),
        widget_message_seen_(false) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(ZoomToFindInPageRectMessageFilter, message)
      IPC_MESSAGE_HANDLER(WidgetHostMsg_ZoomToFindInPageRectInMainFrame,
                          OnWidgetHostMessage)
    IPC_END_MESSAGE_MAP()
    return false;
  }

  void Reset() {
    widget_rect_seen_ = gfx::Rect();
    widget_message_seen_ = false;
  }

  void WaitForWidgetHostMessage() {
    if (widget_message_seen_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  gfx::Rect& widget_message_rect() { return widget_rect_seen_; }

 private:
  ~ZoomToFindInPageRectMessageFilter() override {}

  void OnWidgetHostMessage(const gfx::Rect& rect_to_zoom) {
    widget_rect_seen_ = rect_to_zoom;
    widget_message_seen_ = true;
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  gfx::Rect widget_rect_seen_;
  bool widget_message_seen_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ZoomToFindInPageRectMessageFilter);
};

}  // namespace

// Tests activating the find match nearest to a given point.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, ActivateNearestFindMatch) {
  LoadAndWait("/find_in_page.html");
  bool test_with_oopif = GetParam();
  if (test_with_oopif)
    MakeChildFrameCrossProcess();

  scoped_refptr<ZoomToFindInPageRectMessageFilter> message_filter_root =
      new ZoomToFindInPageRectMessageFilter();
  scoped_refptr<ZoomToFindInPageRectMessageFilter> message_filter_child =
      new ZoomToFindInPageRectMessageFilter();

  if (test_with_oopif) {
    FrameTreeNode* root = contents()->GetFrameTree()->root();
    FrameTreeNode* child = root->child_at(0);
    child->current_frame_host()->GetProcess()->AddFilter(
        message_filter_child.get());
  }

  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("result", default_options.Clone());
  delegate()->WaitForFinalReply();
  EXPECT_EQ(19, delegate()->GetFindResults().number_of_matches);

  auto* find_request_manager = contents()->GetFindRequestManagerForTesting();

  // Get the find match rects.
  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();
  const std::vector<gfx::RectF>& rects = delegate()->find_match_rects();

  // Activate matches via points inside each of the find match rects, in an
  // arbitrary order. Check that the correct match becomes active after each
  // activation.
  int order[19] =
      {11, 13, 2, 0, 16, 5, 7, 10, 6, 1, 15, 14, 9, 17, 18, 3, 8, 12, 4};
  for (int i = 0; i < 19; ++i) {
    delegate()->MarkNextReply();
    contents()->ActivateNearestFindResult(
        rects[order[i]].CenterPoint().x(), rects[order[i]].CenterPoint().y());
    delegate()->WaitForNextReply();

    bool is_match_in_oopif = order[i] > 1 && test_with_oopif;
    // Check widget message rect to make sure it matches.
    if (is_match_in_oopif) {
      message_filter_child->WaitForWidgetHostMessage();
      EXPECT_EQ(find_request_manager->GetSelectionRectForTesting(),
                message_filter_child->widget_message_rect());
      message_filter_child->Reset();
    }

    EXPECT_EQ(order[i] + 1, delegate()->GetFindResults().active_match_ordinal);
  }
}
#endif  // defined(OS_ANDROID)

// Test basic find-in-page functionality after going back and forth to the same
// page. In particular, find-in-page should continue to work after going back to
// a page using the back-forward cache.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, HistoryBackAndForth) {
  GURL url_a = embedded_test_server()->GetURL("a.com", "/find_in_page.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/find_in_page.html");

  auto test_page = [&] {
    if (GetParam())
      MakeChildFrameCrossProcess();

    auto options = blink::mojom::FindOptions::New();

    // The initial find-in-page request.
    Find("result", options->Clone());
    delegate()->WaitForFinalReply();

    FindResults results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);

    // Iterate forward/backward over a few elements.
    int match_index = results.active_match_ordinal;
    for (int delta : {-1, -1, +1, +1, +1, +1, -1, +1, +1}) {
      options->find_next = true;
      options->forward = delta > 0;
      // |active_match_ordinal| uses 1-based index. It belongs to [1, 19].
      match_index += delta;
      match_index = (match_index + 18) % 19 + 1;

      Find("result", options->Clone());
      delegate()->WaitForFinalReply();
      results = delegate()->GetFindResults();

      EXPECT_EQ(last_request_id(), results.request_id);
      EXPECT_EQ(19, results.number_of_matches);
      EXPECT_EQ(match_index, results.active_match_ordinal);
    }
  };

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  test_page();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  test_page();

  // 3) Go back to A.
  contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  test_page();

  // 4) Go forward to B.
  contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  test_page();
}

}  // namespace content
