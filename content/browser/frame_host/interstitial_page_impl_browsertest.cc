// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_impl.h"

#include <tuple>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ipc/message_filter.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace content {

namespace {

class TestInterstitialPageDelegate : public InterstitialPageDelegate {
 private:
  // InterstitialPageDelegate:
  std::string GetHTMLContents() override {
    return "<html>"
           "<head>"
           "<script>"
           "function create_input_and_set_text(text) {"
           "  var input = document.createElement('input');"
           "  input.id = 'input';"
           "  document.body.appendChild(input);"
           "  document.getElementById('input').value = text;"
           "  input.addEventListener('input',"
           "      function() { document.title='TEXT_CHANGED'; });"
           "}"
           "function focus_select_input() {"
           "  document.getElementById('input').select();"
           "}"
           "function get_input_text() {"
           "  window.domAutomationController.send("
           "      document.getElementById('input').value);"
           "}"
           "function get_selection() {"
           "  window.domAutomationController.send("
           "      window.getSelection().toString());"
           "}"
           "function set_selection_change_listener() {"
           "  document.addEventListener('selectionchange',"
           "    function() { document.title='SELECTION_CHANGED'; })"
           "}"
           "</script>"
           "</head>"
           "<body>original body text</body>"
           "</html>";
  }
};

class ClipboardChangedObserver : ui::ClipboardObserver {
 public:
  ClipboardChangedObserver() {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }

  ~ClipboardChangedObserver() override {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void OnClipboardDataChanged() override {
    DCHECK(!quit_closure_.is_null());
    std::move(quit_closure_).Run();
  }

  void WaitForWriteCommit() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class InterstitialPageImplTest : public ContentBrowserTest {
 public:
  InterstitialPageImplTest() {}

  ~InterstitialPageImplTest() override {}

 protected:
  void SetUpInterstitialPage() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());

    // Create the interstitial page.
    TestInterstitialPageDelegate* interstitial_delegate =
        new TestInterstitialPageDelegate;
    GURL url("http://interstitial");
    interstitial_.reset(new InterstitialPageImpl(
        web_contents, static_cast<RenderWidgetHostDelegate*>(web_contents),
        true, url, interstitial_delegate));
    interstitial_->Show();
    WaitForInterstitialAttach(web_contents);

    // Focus the interstitial frame
    FrameTree* frame_tree =
        static_cast<RenderViewHostDelegate*>(interstitial_.get())
            ->GetFrameTree();
    static_cast<RenderFrameHostDelegate*>(interstitial_.get())
        ->SetFocusedFrame(frame_tree->root(),
                          frame_tree->GetMainFrame()->GetSiteInstance());

    // Wait until page loads completely.
    ASSERT_TRUE(WaitForRenderFrameReady(interstitial_->GetMainFrame()));
  }

  void TearDownInterstitialPage() {
    // Close the interstitial.
    interstitial_->DontProceed();
    WaitForInterstitialDetach(shell()->web_contents());
    interstitial_.reset();
  }

  InterstitialPageImpl* interstitial() { return interstitial_.get(); }

  bool FocusInputAndSelectText() {
    return ExecuteScript(interstitial_->GetMainFrame(), "focus_select_input()");
  }

  bool GetInputText(std::string* input_text) {
    return ExecuteScriptAndExtractString(interstitial_->GetMainFrame(),
                                         "get_input_text()", input_text);
  }

  bool GetSelection(std::string* input_text) {
    return ExecuteScriptAndExtractString(interstitial_->GetMainFrame(),
                                         "get_selection()", input_text);
  }

  bool CreateInputAndSetText(const std::string& text) {
    return ExecuteScript(interstitial_->GetMainFrame(),
                         "create_input_and_set_text('" + text + "')");
  }

  bool SetSelectionChangeListener() {
    return ExecuteScript(interstitial_->GetMainFrame(),
                         "set_selection_change_listener()");
  }

  void PerformCut() {
    ClipboardChangedObserver clipboard_observer;
    const base::string16 expected_title = base::UTF8ToUTF16("TEXT_CHANGED");
    content::TitleWatcher title_watcher(shell()->web_contents(),
                                        expected_title);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Cut();
    clipboard_observer.WaitForWriteCommit();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void PerformCopy() {
    ClipboardChangedObserver clipboard_observer;
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Copy();
    clipboard_observer.WaitForWriteCommit();
  }

  void PerformPaste() {
    const base::string16 expected_title = base::UTF8ToUTF16("TEXT_CHANGED");
    content::TitleWatcher title_watcher(shell()->web_contents(),
                                        expected_title);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->Paste();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void PerformSelectAll() {
    const base::string16 expected_title =
        base::UTF8ToUTF16("SELECTION_CHANGED");
    content::TitleWatcher title_watcher(shell()->web_contents(),
                                        expected_title);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->delegate()->SelectAll();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void PerformBack() {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(interstitial_->GetMainFrame());
    rfh->GetRenderWidgetHost()->ForwardMouseEvent(blink::WebMouseEvent(
        blink::WebInputEvent::Type::kMouseUp, blink::WebFloatPoint(),
        blink::WebFloatPoint(), blink::WebPointerProperties::Button::kBack, 0,
        0, base::TimeTicks::Now()));
  }

 private:
  std::unique_ptr<InterstitialPageImpl> interstitial_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageImplTest);
};

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, Cut) {
  BrowserTestClipboardScope clipboard;
  SetUpInterstitialPage();

  ASSERT_TRUE(CreateInputAndSetText("text-to-cut"));
  ASSERT_TRUE(FocusInputAndSelectText());

  PerformCut();
  std::string clipboard_text;
  clipboard.GetText(&clipboard_text);
  EXPECT_EQ("text-to-cut", clipboard_text);

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ(std::string(), input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, Copy) {
  BrowserTestClipboardScope clipboard;
  SetUpInterstitialPage();

  ASSERT_TRUE(CreateInputAndSetText("text-to-copy"));
  ASSERT_TRUE(FocusInputAndSelectText());

  PerformCopy();
  std::string clipboard_text;
  clipboard.GetText(&clipboard_text);
  EXPECT_EQ("text-to-copy", clipboard_text);

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ("text-to-copy", input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, Paste) {
  BrowserTestClipboardScope clipboard;
  SetUpInterstitialPage();

  clipboard.SetText("text-to-paste");

  ASSERT_TRUE(CreateInputAndSetText(std::string()));
  ASSERT_TRUE(FocusInputAndSelectText());

  PerformPaste();

  std::string input_text;
  ASSERT_TRUE(GetInputText(&input_text));
  EXPECT_EQ("text-to-paste", input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, SelectAll) {
  SetUpInterstitialPage();
  ASSERT_TRUE(SetSelectionChangeListener());

  std::string input_text;
  ASSERT_TRUE(GetSelection(&input_text));
  EXPECT_EQ(std::string(), input_text);

  PerformSelectAll();

  ASSERT_TRUE(GetSelection(&input_text));
  EXPECT_EQ("original body text", input_text);

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, FocusAfterDetaching) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Load something into the WebContents.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Blur the main frame.
  web_contents->GetMainFrame()->GetRenderWidgetHost()->Blur();
  EXPECT_FALSE(
      web_contents->GetMainFrame()->GetRenderWidgetHost()->is_focused());

  // Setup the interstitial and focus it.
  SetUpInterstitialPage();
  interstitial()->GetView()->GetRenderWidgetHost()->Focus();
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  EXPECT_TRUE(static_cast<RenderWidgetHostImpl*>(
                  interstitial()->GetView()->GetRenderWidgetHost())
                  ->is_focused());

  // Tear down interstitial.
  TearDownInterstitialPage();

  // Since the interstitial was focused, the main frame should be now focused
  // after the interstitial teardown.
  EXPECT_TRUE(web_contents->GetRenderViewHost()->GetWidget()->is_focused());
}

// Ensure that we don't show the underlying RenderWidgetHostView if a subframe
// commits in the original page while an interstitial is showing.
// See https://crbug.com/729105.
IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, UnderlyingSubframeCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an initial page and inject an iframe that won't commit yet.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  GURL slow_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  TestNavigationManager subframe_delayer(web_contents, slow_url);
  {
    std::string script =
        "var iframe = document.createElement('iframe');"
        "iframe.src = '" +
        slow_url.spec() +
        "';"
        "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), script));
  }
  EXPECT_TRUE(subframe_delayer.WaitForRequestStart());

  // Show an interstitial. The underlying RenderWidgetHostView should not be
  // showing.
  SetUpInterstitialPage();
  EXPECT_FALSE(web_contents->GetMainFrame()->GetView()->IsShowing());
  EXPECT_TRUE(web_contents->GetMainFrame()->GetRenderWidgetHost()->is_hidden());

  // Allow the subframe to commit.
  subframe_delayer.WaitForNavigationFinished();

  // The underlying RenderWidgetHostView should still not be showing.
  EXPECT_FALSE(web_contents->GetMainFrame()->GetView()->IsShowing());
  EXPECT_TRUE(web_contents->GetMainFrame()->GetRenderWidgetHost()->is_hidden());

  TearDownInterstitialPage();
}

IN_PROC_BROWSER_TEST_F(InterstitialPageImplTest, BackMouseButton) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load something into the WebContents.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL(embedded_test_server()->GetURL("/title1.html"))));
  SetUpInterstitialPage();

  EXPECT_TRUE(shell()->web_contents()->ShowingInterstitialPage());
  PerformBack();
  EXPECT_FALSE(shell()->web_contents()->ShowingInterstitialPage());

  TearDownInterstitialPage();
}

}  // namespace content
