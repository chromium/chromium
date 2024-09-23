// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/find_in_page_client.h"
#include "content/browser/find_request_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/view_android.h"
#endif

namespace content {

namespace {

const int kInvalidId = -1;

const url::Origin& GetOriginForFrameTreeNode(FrameTreeNode* node) {
  return node->current_frame_host()->GetLastCommittedOrigin();
}

#if BUILDFLAG(IS_ANDROID)
double GetFrameDeviceScaleFactor(const ToRenderFrameHost& adapter) {
  return EvalJs(adapter, "window.devicePixelRatio;").ExtractDouble();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class FindRequestManagerTestBase : public ContentBrowserTest {
 public:
  FindRequestManagerTestBase()
      : normal_delegate_(nullptr), last_request_id_(0) {}

  FindRequestManagerTestBase(const FindRequestManagerTestBase&) = delete;
  FindRequestManagerTestBase& operator=(const FindRequestManagerTestBase&) =
      delete;

  ~FindRequestManagerTestBase() override = default;

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
    normal_delegate_ = nullptr;
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

    // crbug.com/330147459: Ensure a frame has been produced in the renderer so
    // the active match is set correctly.
    ASSERT_TRUE(
        EvalJsAfterLifecycleUpdate(contents()->GetPrimaryMainFrame(), "", "")
            .error.empty());
  }

  // Loads a multi-frame page. The page will have a full binary frame tree of
  // height |height|. If |cross_process| is true, child frames will be loaded
  // cross-process.
  void LoadMultiFramePage(int height, bool cross_process) {
    LoadAndWait("/find_in_page_multi_frame.html");
    LoadMultiFramePageChildFrames(height, cross_process, root());
  }

  // Reloads the child frame cross-process.
  void MakeChildFrameCrossProcess() {
    FrameTreeNode* child = first_child();
    GURL url =
        embedded_test_server()->GetURL("b.com", child->current_url().path());
    EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
  }

  void Find(const std::string& search_text,
            blink::mojom::FindOptionsPtr options) {
    delegate()->UpdateLastRequest(++last_request_id_);
    contents()->Find(last_request_id_, base::UTF8ToUTF16(search_text),
                     std::move(options), /*skip_delay=*/false);
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

  FrameTreeNode* root() { return contents()->GetPrimaryFrameTree().root(); }

  FrameTreeNode* first_child() { return root()->child_at(0); }

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
  raw_ptr<WebContentsDelegate> normal_delegate_;

  // The ID of the last find request requested.
  int last_request_id_;
};

class FindRequestManagerTest : public FindRequestManagerTestBase,
                               public testing::WithParamInterface<bool> {
 protected:
  bool test_with_oopif() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(FindRequestManagerTests,
                         FindRequestManagerTest,
                         testing::Bool());

// TODO(crbug.com/40470937): These tests frequently fail on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif


// Tests basic find-in-page functionality (such as searching forward and
// backward) and check for correct results at each step.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(Basic)) {
  LoadAndWait("/find_in_page.html");
  if (test_with_oopif())
    MakeChildFrameCrossProcess();

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  options->new_session = false;
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

IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, FindInPage_Issue615291) {
  LoadAndWait("/find_in_simple_page.html");

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  options->find_match = false;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);

  options->new_session = false;
  Find("result", options->Clone());
  // With the issue being tested, this would loop forever and cause the
  // test to timeout.
  delegate()->WaitForFinalReply();
  results = delegate()->GetFindResults();
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(0, results.active_match_ordinal);
}

bool ExecuteScriptAndExtractRect(FrameTreeNode* frame,
                                 const std::string& script,
                                 gfx::Rect* out) {
  std::string script_and_extract =
      script + "rect.x + ',' + rect.y + ',' + rect.width + ',' + rect.height;";
  std::string result = EvalJs(frame, script_and_extract).ExtractString();

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
  blink::web_pref::WebPreferences prefs =
      web_contents->GetOrCreateWebPreferences();
  prefs.smooth_scroll_for_find_enabled = false;
  web_contents->SetWebPreferences(prefs);

  LoadAndWait("/find_in_page_desktop.html");
  // Note: for now, don't run this test on Android in OOPIF mode.
  if (test_with_oopif())
#if BUILDFLAG(IS_ANDROID)
    return;
#else
    MakeChildFrameCrossProcess();
#endif  // BUILDFLAG(IS_ANDROID)

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);

  // Start off at a non-origin scroll offset to ensure coordinate conversisons
  // work correctly.
  ASSERT_TRUE(ExecJs(root, "window.scrollTo(3500, 1500);"));

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
  if (test_with_oopif())
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

// TODO(crbug.com/40470937): This test frequently fails on Android.
// TODO(crbug.com/41291496): This test is flaky on Win
// TODO(crbug.com/41393143): Flaky on CrOS MSan
// Tests sending a large number of find requests subsequently.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_RapidFire) {
  LoadAndWait("/find_in_page.html");
  if (test_with_oopif())
    MakeChildFrameCrossProcess();

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());

  options->new_session = false;
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
// TODO(crbug.com/40489609): Test is flaky on all platforms.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_RemoveFrame) {
  LoadMultiFramePage(2 /* height */, test_with_oopif() /* cross_process */);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();
  options->new_session = false;
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
  root()->current_frame_host()->RemoveChild(first_child());

  // The number of matches and active match ordinal should update automatically
  // to exclude the matches from the removed frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(12, results.number_of_matches);
  EXPECT_EQ(8, results.active_match_ordinal);
}

IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, RemoveMainFrame) {
  LoadAndWait("/find_in_page.html");

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();
  options->new_session = false;
  options->forward = false;
  Find("result", options->Clone());
  Find("result", options->Clone());
  Find("result", options->Clone());
  Find("result", options->Clone());
  Find("result", options->Clone());

  // Don't wait for the reply, and end the test. This will remove the main
  // frame, which should not crash.
}

// Tests adding a frame during a find session.
// TODO(crbug.com/40489609): Test is flaky on all platforms.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_AddFrame) {
  LoadMultiFramePage(2 /* height */, test_with_oopif() /* cross_process */);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  options->new_session = false;
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
  std::string url = embedded_test_server()
                        ->GetURL(test_with_oopif() ? "b.com" : "a.com",
                                 "/find_in_simple_page.html")
                        .spec();
  std::string script = std::string() +
      "var frame = document.createElement('iframe');" +
      "frame.src = '" + url + "';" +
      "document.body.appendChild(frame);";
  delegate()->MarkNextReply();
  ASSERT_TRUE(ExecJs(shell(), script));
  delegate()->WaitForNextReply();

  // The number of matches should update automatically to include the matches
  // from the newly added frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(26, results.number_of_matches);
  EXPECT_EQ(5, results.active_match_ordinal);
}

// Tests adding an in-process hidden iframe during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest,
                       AddInprocessHiddenFrameDuringFind) {
  LoadAndWait("/find_in_page.html");

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(19, results.number_of_matches);

  // Add a frame. It contains 5 new matches.
  std::string url = embedded_test_server()
                        ->GetURL("a.com", "/find_in_simple_page.html")
                        .spec();
  std::string script = JsReplace(R"JS(
      var frame = document.createElement('iframe');
      frame.src = '$1';
      frame.style.visibility = 'hidden';
      document.body.appendChild(frame);
      )JS",
                                 url);

  delegate()->MarkNextReply();
  ASSERT_TRUE(ExecJs(shell(), script));
  delegate()->WaitForNextReply();

  // The number of matches should not be effected by the
  // the newly added hidden frame.
  results = delegate()->GetFindResults();
  EXPECT_EQ(19, results.number_of_matches);
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
  ASSERT_TRUE(ExecJs(shell(), script));
  delegate()->WaitForNextReply();

  // The matches from the new frame should be found automatically, and the first
  // match in the frame should be activated.
  results = delegate()->GetFindResults();
  EXPECT_EQ(5, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Tests a frame navigating to a different page during a find session.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE(NavigateFrame)) {
  LoadMultiFramePage(2 /* height */, test_with_oopif() /* cross_process */);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  options->new_session = false;
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
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  GURL url(embedded_test_server()->GetURL(test_with_oopif() ? "b.com" : "a.com",
                                          "/find_in_simple_page.html"));
  delegate()->MarkNextReply();
  TestNavigationObserver navigation_observer(contents());
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0)->child_at(1)->child_at(0), url));
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
// TODO(crbug.com/330194342): Deflake and re-enable.
#if BUILDFLAG(IS_ANDROID) || \
    (BUILDFLAG(IS_LINUX) && !defined(UNDEFINED_SANITIZER))
#define MAYBE_FindNewMatches DISABLED_FindNewMatches
#else
#define MAYBE_FindNewMatches FindNewMatches
#endif
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, MAYBE_FindNewMatches) {
  LoadAndWait("/find_in_dynamic_page.html");

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  options->new_session = false;
  Find("result", options.Clone());
  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(3, results.number_of_matches);
  EXPECT_EQ(3, results.active_match_ordinal);

  // Dynamically add new text to the page. This text contains 5 new matches for
  // "result".
  ASSERT_TRUE(ExecJs(contents()->GetPrimaryMainFrame(), "addNewText()"));

  Find("result", options.Clone());
  delegate()->WaitForFinalReply();

  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(8, results.number_of_matches);
  EXPECT_EQ(4, results.active_match_ordinal);
}

// TODO(crbug.com/40470937): These tests frequently fail on Android.
// TODO(crbug.com/41352658): Flaky timeout on Win7 (dbg).
// TODO(crbug.com/41408666): Flaky on Win10.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
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
  options->new_session = false;
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
  EXPECT_TRUE(ExecJs(shell(),
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

#if BUILDFLAG(IS_ANDROID)
// Tests empty active match rect when kWrapAround is false.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, EmptyActiveMatchRect) {
  LoadAndWait("/find_in_page.html");

  // kWrapAround is false by default.
  auto default_options = blink::mojom::FindOptions::New();
  default_options->run_synchronously_for_testing = true;
  Find("result 01", default_options.Clone());
  delegate()->WaitForFinalReply();
  EXPECT_EQ(1, delegate()->GetFindResults().number_of_matches);

  // Request the find match rects.
  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();
  const std::vector<gfx::RectF>& rects = delegate()->find_match_rects();

  // The first match should be active.
  EXPECT_EQ(rects[0], delegate()->active_match_rect());

  Find("result 00", default_options.Clone());
  delegate()->WaitForFinalReply();
  EXPECT_EQ(1, delegate()->GetFindResults().number_of_matches);

  // Request the find match rects.
  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();

  // The active match rect should be empty.
  EXPECT_EQ(gfx::RectF(), delegate()->active_match_rect());
}

class MainFrameSizeChangedWaiter : public WebContentsObserver {
 public:
  MainFrameSizeChangedWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  void Wait() { run_loop_.Run(); }

 private:
  void FrameSizeChanged(RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override {
    if (render_frame_host->IsInPrimaryMainFrame())
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

// Tests match rects in the iframe are updated with the size of the main frame,
// and the active match rect should be in it.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest,
                       RectsUpdateWhenMainFrameSizeChanged) {
  LoadAndWait("/find_in_page.html");

  // Make a initial size for native view.
  const int kWidth = 1080;
  const int kHeight = 1286;
  gfx::Size size(kWidth, kHeight);
  contents()->GetNativeView()->OnSizeChanged(kWidth, kHeight);
  contents()->GetNativeView()->OnPhysicalBackingSizeChanged(size);

  // Make a FindRequest for "result".
  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();
  EXPECT_EQ(19, delegate()->GetFindResults().number_of_matches);

  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();

  // Change the size of native view.
  const int kNewHeight = 2121;
  size = gfx::Size(kWidth, kNewHeight);
  contents()->GetNativeView()->OnSizeChanged(kWidth, kNewHeight);
  contents()->GetNativeView()->OnPhysicalBackingSizeChanged(size);

  // Wait for the size of the mainframe to change, and then the position
  // of match rects should change as expected.
  MainFrameSizeChangedWaiter(contents()).Wait();

  contents()->RequestFindMatchRects(-1);
  delegate()->WaitForMatchRects();
  std::vector<gfx::RectF> new_rects = delegate()->find_match_rects();

  // The first match should be active.
  EXPECT_EQ(new_rects[0], delegate()->active_match_rect());

  // Check that all active rects (including iframe) matches with corresponding
  // match rect.
  for (int i = 1; i < 19; i++) {
    options->new_session = false;
    options->forward = true;
    Find("result", options->Clone());
    delegate()->WaitForFinalReply();

    EXPECT_EQ(19, delegate()->GetFindResults().number_of_matches);

    // Request the find match rects.
    contents()->RequestFindMatchRects(-1);
    delegate()->WaitForMatchRects();
    new_rects = delegate()->find_match_rects();

    // The active rect should be equal to the corresponding match rect.
    EXPECT_EQ(new_rects[i], delegate()->active_match_rect());
  }
}

// TODO(wjmaclean): This test, if re-enabled, may require work to make it
// OOPIF-compatible.
// Tests requesting find match rects.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, MAYBE(FindMatchRects)) {
  LoadAndWait("/find_in_page.html");
  if (test_with_oopif())
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

class ZoomToFindInPageRectMessageFilter
    : public blink::mojom::FrameWidgetHostInterceptorForTesting {
 public:
  ZoomToFindInPageRectMessageFilter(RenderWidgetHostImpl* rwhi)
      : impl_(rwhi->frame_widget_host_receiver_for_testing().SwapImplForTesting(
            this)),
        widget_message_seen_(false) {}

  ZoomToFindInPageRectMessageFilter(const ZoomToFindInPageRectMessageFilter&) =
      delete;
  ZoomToFindInPageRectMessageFilter& operator=(
      const ZoomToFindInPageRectMessageFilter&) = delete;

  ~ZoomToFindInPageRectMessageFilter() override {}

  blink::mojom::FrameWidgetHost* GetForwardingInterface() override {
    return impl_;
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
  void ZoomToFindInPageRectInMainFrame(const gfx::Rect& rect_to_zoom) override {
    widget_rect_seen_ = rect_to_zoom;
    widget_message_seen_ = true;
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  raw_ptr<blink::mojom::FrameWidgetHost> impl_;
  gfx::Rect widget_rect_seen_;
  bool widget_message_seen_;
  base::OnceClosure quit_closure_;
};

}  // namespace

// Tests activating the find match nearest to a given point.
// TODO(crbug.com/40864045): Fix flaky failures.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest,
                       DISABLED_ActivateNearestFindMatch) {
  LoadAndWait("/find_in_page.html");
  if (test_with_oopif())
    MakeChildFrameCrossProcess();

  std::unique_ptr<ZoomToFindInPageRectMessageFilter> message_interceptor_child;

  if (test_with_oopif()) {
    message_interceptor_child =
        std::make_unique<ZoomToFindInPageRectMessageFilter>(
            first_child()->current_frame_host()->GetRenderWidgetHost());
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

  double device_scale_factor = GetFrameDeviceScaleFactor(contents());

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

    bool is_match_in_oopif = order[i] > 1 && test_with_oopif();
    // Check widget message rect to make sure it matches.
    if (is_match_in_oopif) {
      message_interceptor_child->WaitForWidgetHostMessage();
      auto expected_rect = gfx::ScaleToEnclosingRect(
          message_interceptor_child->widget_message_rect(),
          1.f / device_scale_factor);
      EXPECT_EQ(find_request_manager->GetSelectionRectForTesting(),
                expected_rect);
      message_interceptor_child->Reset();
    }

    EXPECT_EQ(order[i] + 1, delegate()->GetFindResults().active_match_ordinal);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test basic find-in-page functionality after going back and forth to the same
// page. In particular, find-in-page should continue to work after going back to
// a page using the back-forward cache.
// Flaky everywhere: https://crbug.com/1115102
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DISABLED_HistoryBackAndForth) {
  GURL url_a = embedded_test_server()->GetURL("a.com", "/find_in_page.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/find_in_page.html");

  auto test_page = [&] {
    if (test_with_oopif())
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
      options->new_session = false;
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

class FindInPageDisabledForOriginBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  // ContentBrowserClient:
  bool IsFindInPageDisabledForOrigin(const url::Origin& origin) override {
    return origin.host() == "b.com";
  }
};

// Tests that find-in-page won't show results for origins that disabled
// find-in-page.
IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, FindInPageDisabledForOrigin) {
  FindInPageDisabledForOriginBrowserClient browser_client;

  // Start with a basic case to set a baseline.
  LoadAndWait("/find_in_page.html");
  url::Origin root_origin = GetOriginForFrameTreeNode(root());
  url::Origin child_origin = GetOriginForFrameTreeNode(first_child());
  EXPECT_EQ("a.com", root_origin.host());
  EXPECT_EQ("a.com", child_origin.host());
  EXPECT_FALSE(browser_client.IsFindInPageDisabledForOrigin(root_origin));
  EXPECT_FALSE(browser_client.IsFindInPageDisabledForOrigin(child_origin));

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);

  // Navigate child frame to b.com.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      first_child(), embedded_test_server()->GetURL(
                         "b.com", first_child()->current_url().path())));
  root_origin = GetOriginForFrameTreeNode(root());
  child_origin = GetOriginForFrameTreeNode(first_child());
  EXPECT_EQ("a.com", root_origin.host());
  EXPECT_EQ("b.com", child_origin.host());
  EXPECT_FALSE(browser_client.IsFindInPageDisabledForOrigin(root_origin));
  EXPECT_TRUE(browser_client.IsFindInPageDisabledForOrigin(child_origin));

  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  // Given the custom `browser_client` disabled find-in-page for b.com, only the
  // results from the root node should show up now.
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(2, results.number_of_matches);

  // Navigate child frame, but remain on b.com.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      first_child(),
      embedded_test_server()->GetURL("b.com", "/find_in_simple_page.html")));
  root_origin = GetOriginForFrameTreeNode(root());
  child_origin = GetOriginForFrameTreeNode(first_child());
  EXPECT_EQ("a.com", root_origin.host());
  EXPECT_EQ("b.com", child_origin.host());
  EXPECT_FALSE(browser_client.IsFindInPageDisabledForOrigin(root_origin));
  EXPECT_TRUE(browser_client.IsFindInPageDisabledForOrigin(child_origin));

  // Results from the child frame on b.com still do not show up.
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(2, results.number_of_matches);

  // Navigate child frame to a.com again.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      first_child(),
      embedded_test_server()->GetURL("a.com", "/find_in_simple_page.html")));
  root_origin = GetOriginForFrameTreeNode(root());
  child_origin = GetOriginForFrameTreeNode(first_child());
  EXPECT_EQ("a.com", root_origin.host());
  EXPECT_EQ("a.com", child_origin.host());
  EXPECT_FALSE(browser_client.IsFindInPageDisabledForOrigin(root_origin));
  EXPECT_FALSE(browser_client.IsFindInPageDisabledForOrigin(child_origin));

  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  // Since the child frame is now on a.com, find-in-page is enabled, so its
  // results show up again.
  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(7, results.number_of_matches);
}

class FindTestWebContentsPrerenderingDelegate
    : public FindTestWebContentsDelegate {
 public:
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override {
    return PreloadingEligibility::kEligible;
  }
};

class FindRequestManagerPrerenderingTest : public FindRequestManagerTest {
 public:
  FindRequestManagerPrerenderingTest()
      : prerender_helper_(base::BindRepeating(
            &FindRequestManagerPrerenderingTest::web_contents,
            base::Unretained(this))) {}
  ~FindRequestManagerPrerenderingTest() override = default;

  void SetUpOnMainThread() override {
    FindRequestManagerTest::SetUpOnMainThread();
    contents()->SetDelegate(&delegate_);
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  FindTestWebContentsPrerenderingDelegate delegate_;
};

// Tests that find-in-page won't show results inside a prerendering page.
IN_PROC_BROWSER_TEST_F(FindRequestManagerPrerenderingTest, Basic) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  // Do a find-in-page on an empty page.
  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(0, results.number_of_matches);

  // Load a page that has 5 matches for "result" in the prerender.
  auto prerender_url =
      embedded_test_server()->GetURL("/find_in_simple_page.html?prerendering");
  prerender_helper()->AddPrerender(prerender_url);

  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  // The prerendering page shouldn't affect the results of a find-in-page .
  EXPECT_EQ(0, results.number_of_matches);

  // Activate the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  Find("result", options->Clone());
  delegate()->WaitForFinalReply();

  results = delegate()->GetFindResults();
  // The results from the prerendered page getting activated should be 5 as the
  // mainframe(5 results) and no subframe.
  EXPECT_EQ(5, results.number_of_matches);
}

class FindRequestManagerTestWithBFCache : public FindRequestManagerTest {
 public:
  FindRequestManagerTestWithBFCache() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~FindRequestManagerTestWithBFCache() override = default;

  content::RenderFrameHost* render_frame_host() {
    return contents()->GetPrimaryMainFrame();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test basic find-in-page functionality when a page gets into and out of
// BFCache.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTestWithBFCache, Basic) {
  GURL url_a = embedded_test_server()->GetURL("a.com", "/find_in_page.html");
  GURL url_b =
      embedded_test_server()->GetURL("b.com", "/find_in_simple_page.html");

  auto options = blink::mojom::FindOptions::New();
  auto expect_match_results = [&](int expected_number_of_matches) {
    // The initial find-in-page request.
    Find("result", options->Clone());
    delegate()->WaitForFinalReply();

    FindResults results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(expected_number_of_matches, results.number_of_matches);
  };

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  content::RenderFrameHostWrapper rfh_a(render_frame_host());
  // The results from the page A should be 19 as the mainframe(2 results) and
  // the new subframe (17 results).
  expect_match_results(19);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  content::RenderFrameHostWrapper rfh_b(render_frame_host());
  // The results from the page B should be 5 as the mainframe(5 results) and no
  // subframe.
  expect_match_results(5);

  // Ensure A is cached.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Go back to A.
  contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // |rfh_a| should become the active frame.
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  // The results from the page A should be 19 as the mainframe(2 results) and
  // the new subframe (17 results).
  expect_match_results(19);

  // Ensure B is cached.
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Go forward to B.
  contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // |rfh_b| should become the active frame.
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());
  // The results from the page B should be 5 as the mainframe(5 results) and no
  // subframe.
  expect_match_results(5);
}

class WaitForFindTestWebContentsDelegate : public FindTestWebContentsDelegate {
 public:
  void WaitForFramesReply(int wait_count) {
    wait_count_ = wait_count;
    EXPECT_GT(wait_count_, 0);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  void TryToStopWaiting() {
    if (run_loop_ && !--wait_count_)
      run_loop_->Quit();
  }

  bool ShouldWait() { return wait_count_ > 0; }

 private:
  int wait_count_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class FindRequestManagerFencedFrameTest : public FindRequestManagerTest {
 public:
  FindRequestManagerFencedFrameTest() = default;
  ~FindRequestManagerFencedFrameTest() override = default;
  FindRequestManagerFencedFrameTest(const FindRequestManagerFencedFrameTest&) =
      delete;

  FindRequestManagerFencedFrameTest& operator=(
      const FindRequestManagerFencedFrameTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  content::WebContents* GetWebContents() { return shell()->web_contents(); }

  int find_request_queue_size() {
    return contents()
        ->GetFindRequestManagerForTesting()
        ->find_request_queue_.size();
  }

  bool CheckFrame(RenderFrameHost* render_frame_host) const {
    return contents()->GetFindRequestManagerForTesting()->CheckFrame(
        render_frame_host);
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// This find-in-page client will make the find-request-queue not empty so that
// we can test a fenced frame doesn't clear the find-request-queue when it's
// deleted. To keep the find-request-queue not empty, this class
// intercepts the Mojo methods calls, and changes the FindMatchUpdateType to
// kMoreUpdatesComing (including those that were marked as kFinalUpdate), so
// that the find-request-queue won't get popped and will stay non-empty.
class NeverFinishFencedFrameFindInPageClient : public FindInPageClient {
 public:
  NeverFinishFencedFrameFindInPageClient(
      FindRequestManager* find_request_manager,
      RenderFrameHostImpl* rfh)
      : FindInPageClient(find_request_manager, rfh) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    delegate_ = static_cast<WaitForFindTestWebContentsDelegate*>(
        web_contents->GetDelegate());
  }
  ~NeverFinishFencedFrameFindInPageClient() override = default;

  // blink::mojom::FindInPageClient overrides
  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      blink::mojom::FindMatchUpdateType update_type) override {
    update_type = blink::mojom::FindMatchUpdateType::kMoreUpdatesComing;
    FindInPageClient::SetNumberOfMatches(request_id, current_number_of_matches,
                                         update_type);
  }

  // Do nothing on SetActiveMatch() calls, since this can potentially trigger
  // FindRequestManager::AdvanceQueue() and pop an item from the
  // find-request-queue.
  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      blink::mojom::FindMatchUpdateType update_type) override {}

 private:
  raw_ptr<WaitForFindTestWebContentsDelegate> delegate_;
};

static std::unique_ptr<FindInPageClient> CreateFencedFrameFindInPageClient(
    FindRequestManager* find_request_manager,
    RenderFrameHostImpl* rfh) {
  return std::make_unique<NeverFinishFencedFrameFindInPageClient>(
      find_request_manager, rfh);
}

// Tests that a main frame, a sub frame, and a fenced frame clear the
// find-request-queue when the fenced frame is deleted.
IN_PROC_BROWSER_TEST_F(FindRequestManagerFencedFrameTest,
                       OnlyPrimaryMainFrameClearsFindRequestQueue) {
  WaitForFindTestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  // Override the FindInPageClient class so that we can intercept the Mojo
  // methods calls to keep its find request queue non-empty.
  contents()
      ->GetFindRequestManagerForTesting()
      ->SetCreateFindInPageClientFunctionForTesting(
          &CreateFencedFrameFindInPageClient);

  LoadAndWait("/find_in_page.html");
  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;
  Find("result", options.Clone());
  // Initial find request is pop from the queue immediately so we make a second
  // find request.
  options->new_session = false;
  Find("result", options.Clone());

  // Create a fenced frame.
  GURL find_test_url =
      embedded_test_server()->GetURL("/fenced_frames/find_in_page.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), find_test_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  EXPECT_TRUE(CheckFrame(fenced_frame_host));
  EXPECT_EQ(find_request_queue_size(), 1);
  EXPECT_EQ(last_request_id(), delegate.GetFindResults().request_id);

  // Navigate the fenced frame, this won't cause the find request queue to be
  // cleared, since it's not a primary main frame.
  fenced_frame_host = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_host, find_test_url);
  EXPECT_TRUE(CheckFrame(fenced_frame_host));
  EXPECT_EQ(find_request_queue_size(), 1);
  EXPECT_EQ(last_request_id(), delegate.GetFindResults().request_id);

  // Navigate the non-fenced frame subframe, this also won't cause the find
  // request queue to be cleared, since it's not a primary main frame.
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), find_test_url));
  EXPECT_TRUE(CheckFrame(root->child_at(0)->current_frame_host()));
  EXPECT_EQ(find_request_queue_size(), 1);
  EXPECT_EQ(last_request_id(), delegate.GetFindResults().request_id);

  // Navigate the main frame, this causes the find request queue to be cleared,
  // since it's the primary main frame.
  EXPECT_TRUE(NavigateToURL(shell(), find_test_url));
  EXPECT_TRUE(CheckFrame(GetWebContents()->GetPrimaryMainFrame()));
  EXPECT_EQ(find_request_queue_size(), 0);
}

// This find-in-page client will make it so that we never stop listening for
// find-in-page updates only for subframes, through modifying final updates to
// be marked as non-final updates. It helps us to simulate various things that
// can happen before a find-in-page session finishes (e.g. navigation,
// lifecycle state change) without finishing the find session.
class NeverFinishSubframeFindInPageClient : public FindInPageClient {
 public:
  NeverFinishSubframeFindInPageClient(FindRequestManager* find_request_manager,
                                      RenderFrameHostImpl* rfh)
      : FindInPageClient(find_request_manager, rfh), rfh_(rfh) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    delegate_ = static_cast<WaitForFindTestWebContentsDelegate*>(
        web_contents->GetDelegate());
  }
  ~NeverFinishSubframeFindInPageClient() override = default;

  // blink::mojom::FindInPageClient overrides
  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      blink::mojom::FindMatchUpdateType update_type) override {
    bool should_wait = delegate_->ShouldWait();
    if (update_type == blink::mojom::FindMatchUpdateType::kFinalUpdate)
      delegate_->TryToStopWaiting();

    // Make sure subframe's reply is not marked as the final update.
    if (!rfh_->is_main_frame() && should_wait)
      update_type = blink::mojom::FindMatchUpdateType::kMoreUpdatesComing;

    FindInPageClient::SetNumberOfMatches(request_id, current_number_of_matches,
                                         update_type);
  }

  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      blink::mojom::FindMatchUpdateType update_type) override {
    if (update_type == blink::mojom::FindMatchUpdateType::kFinalUpdate)
      delegate_->TryToStopWaiting();

    // Make sure subframe's reply is not marked as the final update.
    if (!rfh_->is_main_frame())
      update_type = blink::mojom::FindMatchUpdateType::kMoreUpdatesComing;

    FindInPageClient::SetActiveMatch(request_id, active_match_rect,
                                     active_match_ordinal, update_type);
  }

 private:
  raw_ptr<RenderFrameHostImpl> rfh_;
  raw_ptr<WaitForFindTestWebContentsDelegate> delegate_;
};

class FindRequestManagerTestObserver : public WebContentsObserver {
 public:
  explicit FindRequestManagerTestObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    auto* delegate = static_cast<FindTestWebContentsDelegate*>(
        web_contents()->GetDelegate());
    delegate->MarkNextReply();
  }
};

static std::unique_ptr<FindInPageClient> CreateFindInPageClient(
    FindRequestManager* find_request_manager,
    RenderFrameHostImpl* rfh) {
  return std::make_unique<NeverFinishSubframeFindInPageClient>(
      find_request_manager, rfh);
}

enum class FrameSiteType {
  kSameOrigin,
  kCrossOrigin,
};

enum class FrameTestType {
  kIFrame,
  kFencedFrame,
};

class FindRequestManagerTestWithTestConfig
    : public FindRequestManagerTestBase,
      public testing::WithParamInterface<
          ::testing::tuple<FrameSiteType, FrameTestType>> {
 public:
  FrameSiteType GetFrameSiteType() const { return std::get<0>(GetParam()); }

  FrameTestType GetFrameTestType() const { return std::get<1>(GetParam()); }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    FindRequestManagers,
    FindRequestManagerTestWithTestConfig,
    ::testing::Combine(::testing::Values(FrameSiteType::kSameOrigin,
                                         FrameSiteType::kCrossOrigin),
                       ::testing::Values(FrameTestType::kIFrame,
                                         FrameTestType::kFencedFrame)));

// Tests that the previous results from old document are removed and we get the
// new results from the new document when we navigate the subframe that
// hasn't finished the find-in-page session to the new document.
// TODO(crbug.com/40220234): Fix flakiness and reenable the test.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
#define MAYBE_NavigateFrameDuringFind DISABLED_NavigateFrameDuringFind
#else
#define MAYBE_NavigateFrameDuringFind NavigateFrameDuringFind
#endif
IN_PROC_BROWSER_TEST_P(FindRequestManagerTestWithTestConfig,
                       MAYBE_NavigateFrameDuringFind) {
  WaitForFindTestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  // 1) Load a main frame with 5 matches.
  LoadAndWait("/find_in_simple_page.html");

  GURL frame_url =
      embedded_test_server()->GetURL("a.com", "/find_in_page_frame.html");
  content::RenderFrameHost* fenced_frame_host = nullptr;

  // 2) Load a subframe with 17 matches.
  if (GetFrameTestType() == FrameTestType::kIFrame) {
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
        var frame = document.createElement('iframe');
        frame.src = $1;
        document.body.appendChild(frame);
      )",
                                          frame_url)));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  } else {
    fenced_frame_host = fenced_frame_test_helper().CreateFencedFrame(
        shell()->web_contents()->GetPrimaryMainFrame(), frame_url);
    EXPECT_NE(nullptr, fenced_frame_host);
  }

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;

  // 2) First try a normal find-in-page session that finishes completely.
  Find("result", options.Clone());
  delegate.WaitForFinalReply();

  FindResults results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(22, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  // 3) Override the FindInPageClient class so that we can simulate a subframe
  // change that happens in the middle of a find-in-page session.
  contents()
      ->GetFindRequestManagerForTesting()
      ->SetCreateFindInPageClientFunctionForTesting(&CreateFindInPageClient);

  // 4) Try to find-in-page again, but this time the subframe won't be marked as
  // finished before it got navigated.
  Find("result", options.Clone());

  // 5) Wait for the find request of the main frame's reply.
  delegate.WaitForFramesReply(2);
  results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(22, results.number_of_matches);
  EXPECT_EQ(2, results.active_match_ordinal);

  // 6) Navigate the subframe that hasn't finished the find-in-page session to a
  // document with 5 matches. This will trigger a find-in-page request on the
  // new document on the unfinished subframe, and removes the result from the
  // old document.
  FindRequestManagerTestObserver observer(contents());
  GURL url(embedded_test_server()->GetURL(
      GetFrameSiteType() == FrameSiteType::kSameOrigin ? "a.com" : "b.com",
      "/find_in_simple_page.html"));
  if (GetFrameTestType() == FrameTestType::kIFrame) {
    FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  } else {
    fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                              url);
  }

  delegate.WaitForNextReply();

  results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  // The results from the old subframe (17 results) is removed entirely even
  // when it hasn't finished, and we added the next reply from the new subframe
  // (5 results). So, the final results should be 10 as the mainframe(5 results)
  // and the new subframe (5 results).
  EXPECT_EQ(10, results.number_of_matches);
  EXPECT_EQ(2, results.active_match_ordinal);
}

// Tests that the previous results from the old documents are removed and we
// get the new results from the new document when we go back to the page in
// BFCache from the page that hasn't finished the find-in-page session.
// This TC does not intentionally check the |active_match_ordinal| value,
// because the main frame is not focused on Android, so it has a different
// result on Android.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTestWithBFCache,
                       NavigateFrameDuringFind) {
  WaitForFindTestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  GURL url_a = embedded_test_server()->GetURL("a.com", "/find_in_page.html");
  GURL url_b =
      embedded_test_server()->GetURL("b.com", "/find_in_page_two_frames.html");

  // 1) Load A that is a main frame with 2 matches and a subframe with 17
  // matches.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  content::RenderFrameHostWrapper rfh_a(render_frame_host());

  // 2) Load B that is a main frame with no match and two subframes with each 3
  // matches.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // Ensure A is cached.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::RenderFrameHostWrapper rfh_b(render_frame_host());

  // 3) Override the FindInPageClient class so that we can simulate a subframe
  // change that happens in the middle of a find-in-page session.
  contents()
      ->GetFindRequestManagerForTesting()
      ->SetCreateFindInPageClientFunctionForTesting(&CreateFindInPageClient);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;

  // 4) Try to find-in-page again, but this time the subframe won't be marked as
  // finished before it goes back in the BF cache.
  Find("result", options.Clone());

  // 5) Wait for replies from the main frame and the subframes.
  delegate.WaitForFramesReply(3);
  FindResults results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(6, results.number_of_matches);

  // 6) Go back to A which has a main frame with 2 matches and the subframe with
  // 17 matches.
  FindRequestManagerTestObserver observer1(contents());
  contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // |rfh_a| should become the active frame.
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  // Ensure B is cached.
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 7) Wait for replies from the main frame and the subframes.
  delegate.WaitForFramesReply(2);
  results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  // The results from the old page (6 results) is removed entirely even when
  // it hasn't finished, and we added the next reply from the new page (19
  // results). So, the final results should be 19.
  EXPECT_EQ(19, results.number_of_matches);

  // 8) Go forward to B which has a main frame with no match and two subframes
  // with each 3 matches.
  contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // |rfh_b| should become the active frame.
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());

  // 9) Wait for replies from the main frame and the subframes.
  delegate.WaitForFinalReply();
  results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  // The results from the old page (19 results) is removed entirely even when
  // it hasn't finished, and we added the next reply from the new page (6
  // results). So, the final results should be 6.
  EXPECT_EQ(6, results.number_of_matches);
}

IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, CrashDuringFind) {
  WaitForFindTestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  // 1) Load a main frame with 2 matches and a subframe with 17 matches.
  LoadAndWait("/find_in_page.html");
  MakeChildFrameCrossProcess();

  // 2) Override the FindInPageClient class so that we can simulate a subframe
  // change that happens in the middle of a find-in-page session.
  contents()
      ->GetFindRequestManagerForTesting()
      ->SetCreateFindInPageClientFunctionForTesting(&CreateFindInPageClient);

  auto options = blink::mojom::FindOptions::New();
  options->run_synchronously_for_testing = true;

  // 3) Try to find-in-page again, but this time the subframe won't be marked as
  // finished before it crashed.
  Find("result", options.Clone());

  // 4) Wait for the find request of the main frame's reply.
  delegate.WaitForFramesReply(2);
  FindResults results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  // 5) Crash the subframe that hasn't finished the find-in-page
  // session. This will remove the result from the crashed document.
  {
    FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::RenderFrameDeletedObserver crash_observer(
        root->child_at(0)->current_frame_host());
    root->child_at(0)->current_frame_host()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }

  // 6) Wait for the crashed frame to be deleted.
  delegate.WaitForFinalReply();
  results = delegate.GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  // The results from the crashed subframe (17 results) is removed entirely and
  // only have 2 results from the main frame.
  EXPECT_EQ(2, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

IN_PROC_BROWSER_TEST_P(FindRequestManagerTest, DelayThenStop) {
  LoadAndWait("/find_in_page.html");
  if (test_with_oopif())
    MakeChildFrameCrossProcess();

  auto default_options = blink::mojom::FindOptions::New();
  Find("r", default_options->Clone());
  contents()->StopFinding(STOP_FIND_ACTION_CLEAR_SELECTION);

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(0, results.number_of_matches);

  EXPECT_FALSE(contents()
                   ->GetFindRequestManagerForTesting()
                   ->RunDelayedFindTaskForTesting());
}

}  // namespace content
