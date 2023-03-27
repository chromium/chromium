// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace content {

namespace {

// A dummy WebContentsDelegate which tracks whether CloseContents() has been
// called. It refuses the actual close but keeps track of whether the renderer
// requested it.
class CloseTrackingDelegate : public WebContentsDelegate {
 public:
  CloseTrackingDelegate() = default;

  CloseTrackingDelegate(const CloseTrackingDelegate&) = delete;
  CloseTrackingDelegate& operator=(const CloseTrackingDelegate&) = delete;

  bool close_contents_called() const { return close_contents_called_; }

  void CloseContents(WebContents* source) override {
    close_contents_called_ = true;
  }

 private:
  bool close_contents_called_ = false;
};

}  // namespace

class OpenedByDOMTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Use --site-per-process to force process swaps on cross-site navigations.
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool AttemptCloseFromJavaScript(WebContents* web_contents) {
    CloseTrackingDelegate close_tracking_delegate;
    WebContentsDelegate* old_delegate = web_contents->GetDelegate();
    web_contents->SetDelegate(&close_tracking_delegate);

    const char kCloseWindowScript[] =
        // Close the window.
        "window.close();"
        // Report back after an event loop iteration; the close IPC isn't sent
        // immediately.
        "new Promise(resolve => setTimeout(() => {"
        "  resolve(0);"
        "}));";
    CHECK_EQ(0, EvalJs(web_contents, kCloseWindowScript).ExtractInt());

    web_contents->SetDelegate(old_delegate);
    return close_tracking_delegate.close_contents_called();
  }

  Shell* OpenWindowFromJavaScript(Shell* shell, const GURL& url) {
    // Wait for the popup to be created and for it to have navigated.
    ShellAddedObserver new_shell_observer;
    TestNavigationObserver nav_observer(nullptr);
    nav_observer.StartWatchingNewWebContents();
    CHECK(ExecJs(shell, JsReplace("window.open($1)", url)));
    nav_observer.Wait();
    return new_shell_observer.GetShell();
  }
};

// Tests that window.close() does not work on a normal window that has navigated
// a few times.
IN_PROC_BROWSER_TEST_F(OpenedByDOMTest, NormalWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // window.close is allowed if the window was opened by DOM OR the back/forward
  // list has only one element. Navigate a bit so the second condition is false.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  GURL url2 = embedded_test_server()->GetURL("/site_isolation/blank.html?2");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // This window was not opened by DOM, so close does not reach the browser
  // process.
  EXPECT_FALSE(AttemptCloseFromJavaScript(shell()->web_contents()));
}

// Tests that window.close() works in a popup window that has navigated a few
// times.
IN_PROC_BROWSER_TEST_F(OpenedByDOMTest, Popup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  GURL url2 = embedded_test_server()->GetURL("/site_isolation/blank.html?2");
  GURL url3 = embedded_test_server()->GetURL("/site_isolation/blank.html?3");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  Shell* popup = OpenWindowFromJavaScript(shell(), url2);
  EXPECT_TRUE(NavigateToURL(popup, url3));
  EXPECT_TRUE(AttemptCloseFromJavaScript(popup->web_contents()));
}

// Tests that window.close() works in a popup window that's opened with noopener
// that has navigated a few times.
IN_PROC_BROWSER_TEST_F(OpenedByDOMTest, NoOpenerPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  GURL url2 = embedded_test_server()->GetURL("/site_isolation/blank.html?2");
  GURL url3 = embedded_test_server()->GetURL("/site_isolation/blank.html?3");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Create a popup through window.open() and 'noopener'.
  ShellAddedObserver new_shell_observer;
  TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  CHECK(
      ExecJs(shell(), JsReplace("window.open($1, '_blank','noopener')", url2)));
  nav_observer.Wait();
  Shell* popup = new_shell_observer.GetShell();

  // Navigate the popup.
  EXPECT_TRUE(NavigateToURL(popup, url3));
  // Closing the popup should still work.
  EXPECT_TRUE(AttemptCloseFromJavaScript(popup->web_contents()));
}

// Tests that window.close() works in a popup window that has navigated a few
// times and swapped processes.
IN_PROC_BROWSER_TEST_F(OpenedByDOMTest, CrossProcessPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");

  GURL url2 = embedded_test_server()->GetURL("/site_isolation/blank.html?2");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("foo.com");
  url2 = url2.ReplaceComponents(replace_host);

  GURL url3 = embedded_test_server()->GetURL("/site_isolation/blank.html?3");
  url3 = url3.ReplaceComponents(replace_host);

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  Shell* popup = OpenWindowFromJavaScript(shell(), url2);
  EXPECT_TRUE(NavigateToURL(popup, url3));
  EXPECT_TRUE(AttemptCloseFromJavaScript(popup->web_contents()));
}

}  // namespace content
