// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "url/gurl.h"

namespace content {

// WebContentsDelegate that fails to open a URL when there's a request that
// needs to be transferred between renderers.
class NoTransferRequestDelegate : public WebContentsDelegate {
 public:
  NoTransferRequestDelegate() {}

  bool ShouldTransferNavigation(bool is_main_frame_navigation) override {
    // Intentionally cancel the transfer.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoTransferRequestDelegate);
};

class CrossSiteTransferTest : public ContentBrowserTest {
 public:
  CrossSiteTransferTest() {}

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void NavigateToURLContentInitiated(Shell* window,
                                     const GURL& url,
                                     bool should_replace_current_entry,
                                     bool should_wait_for_navigation) {
    std::unique_ptr<TestNavigationManager> navigation_manager =
        should_wait_for_navigation
            ? std::unique_ptr<TestNavigationManager>(
                  new TestNavigationManager(window->web_contents(), url))
            : nullptr;
    std::string script;
    if (should_replace_current_entry)
      script = base::StringPrintf("location.replace('%s')", url.spec().c_str());
    else
      script = base::StringPrintf("location.href = '%s'", url.spec().c_str());
    bool result = ExecuteScript(window, script);
    EXPECT_TRUE(result);
    if (should_wait_for_navigation) {
      EXPECT_TRUE(navigation_manager->WaitForRequestStart());
      EXPECT_TRUE(navigation_manager->WaitForResponse());
      navigation_manager->WaitForNavigationFinished();
      EXPECT_TRUE(navigation_manager->was_successful());
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }
};

// The following tests crash in the ThreadSanitizer runtime,
// http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_ReplaceEntryCrossProcessThenTransfer \
    DISABLED_ReplaceEntryCrossProcessThenTransfer
#define MAYBE_ReplaceEntryCrossProcessTwice \
    DISABLED_ReplaceEntryCrossProcessTwice
#else
#define MAYBE_ReplaceEntryCrossProcessThenTransfer \
    ReplaceEntryCrossProcessThenTransfer
#define MAYBE_ReplaceEntryCrossProcessTwice ReplaceEntryCrossProcessTwice
#endif
// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a cross-process navigation.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest,
                       MAYBE_ReplaceEntryCrossProcessThenTransfer) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Navigate to a page on A.com with entry replacement. This navigation is
  // cross-site, so the renderer will send it to the browser via OpenURL to give
  // to a new process. It will then be transferred into yet another process due
  // to the call above.
  GURL url2 =
      embedded_test_server()->GetURL("A.com", "/site_isolation/blank.html?2");
  NavigateToURLContentInitiated(shell(), url2, true, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());

  // Now navigate as before to a page on B.com, but normally (without
  // replacement). This will still perform a double process-swap as above, via
  // OpenURL and then transfer.
  GURL url3 =
      embedded_test_server()->GetURL("B.com", "/site_isolation/blank.html?3");
  NavigateToURLContentInitiated(shell(), url3, false, true);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a content-initiated in-process
// navigation. This test is the same as the test above, except transfering from
// in-process.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest,
                       ReplaceEntryInProcessThenTransfer) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Navigate in-process with entry replacement. It will then be transferred
  // into a new one due to the call above.
  GURL url2 = embedded_test_server()->GetURL("/site_isolation/blank.html?2");
  NavigateToURLContentInitiated(shell(), url2, true, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());

  // Now navigate as before, but without replacement.
  GURL url3 = embedded_test_server()->GetURL("/site_isolation/blank.html?3");
  NavigateToURLContentInitiated(shell(), url3, false, true);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that cross processes twice from renderer policy.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest,
                       MAYBE_ReplaceEntryCrossProcessTwice) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Navigate to a page on A.com which redirects to B.com with entry
  // replacement. This will switch processes via OpenURL twice. First to A.com,
  // and second in response to the server redirect to B.com. The second swap is
  // also renderer-initiated via OpenURL because decidePolicyForNavigation is
  // currently applied on redirects.
  GURL::Replacements replace_host;
  GURL url2b =
      embedded_test_server()->GetURL("B.com", "/site_isolation/blank.html?2");
  GURL url2a = embedded_test_server()->GetURL(
      "A.com", "/cross-site/" + url2b.host() + url2b.PathForRequest());
  NavigateToURLContentInitiated(shell(), url2a, true, true);

  // There should be one history entry. url2b should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());

  // Now repeat without replacement.
  GURL url3b =
      embedded_test_server()->GetURL("B.com", "/site_isolation/blank.html?3");
  GURL url3a = embedded_test_server()->GetURL(
      "A.com", "/cross-site/" + url3b.host() + url3b.PathForRequest());
  NavigateToURLContentInitiated(shell(), url3a, false, true);

  // There should be two history entries. url2b should have replaced url1. url3b
  // should not have replaced url2b.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3b, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the request is destroyed when a cross process navigation is
// cancelled.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest, NoLeakOnCrossSiteCancel) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  NoTransferRequestDelegate no_transfer_request_delegate;
  WebContentsDelegate* old_delegate = shell()->web_contents()->GetDelegate();
  shell()->web_contents()->SetDelegate(&no_transfer_request_delegate);

  // Navigate to a page on A.com with entry replacement. This navigation is
  // cross-site, so the renderer will send it to the browser via OpenURL to give
  // to a new process. It will then be transferred into yet another process due
  // to the call above.
  GURL url2 =
      embedded_test_server()->GetURL("A.com", "/site_isolation/blank.html?2");
  TestNavigationManager navigation_manager(shell()->web_contents(), url2);

  NavigationHandleObserver handle_observer(shell()->web_contents(), url2);
  // Don't wait for the navigation to complete, since that never happens in
  // this case.
  NavigateToURLContentInitiated(shell(), url2, false, false);

  // Make sure the request for url2 did not complete.
  EXPECT_FALSE(navigation_manager.WaitForResponse());

  // There should be one history entry, with url1.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());

  EXPECT_EQ(net::ERR_ABORTED, handle_observer.net_error_code());
  shell()->web_contents()->SetDelegate(old_delegate);
}

// Test that verifies that a cross-process transfer retains ability to read
// files encapsulated by HTTP POST body that is forwarded to the new renderer.
// Invalid handling of this scenario has been suspected as the cause of at least
// some of the renderer kills tracked in https://crbug.com/613260.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest, PostWithFileData) {
  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).  Using 307 (rather than
  // 302) redirection is important to preserve the HTTP method and POST body.
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Prepare a file to upload.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("test-file-content");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  base::RunLoop run_loop;
  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path, run_loop.QuitClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('file').click();"));
  run_loop.Run();

  // Remember the old process id for a sanity check below.
  int old_process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('file-form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());

  // Verify that the test really verifies access of a *new* renderer process.
  int new_process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  ASSERT_NE(new_process_id, old_process_id);

  // MAIN VERIFICATION: Check if the new renderer process is able to read the
  // file.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      new_process_id, file_path));

  // Verify that POST body got preserved by 307 redirect.  This expectation
  // comes from: https://tools.ietf.org/html/rfc7231#section-6.4.7
  std::string actual_page_body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send("
      "document.getElementsByTagName('pre')[0].innerText);",
      &actual_page_body));
  EXPECT_THAT(actual_page_body, ::testing::HasSubstr(file_content));
  EXPECT_THAT(actual_page_body,
              ::testing::HasSubstr(file_path.BaseName().AsUTF8Unsafe()));
  EXPECT_THAT(actual_page_body,
              ::testing::HasSubstr("form-data; name=\"file\""));
}

// Test that verifies that if navigation originator doesn't have access to a
// file, then no access is granted after a cross-process transfer of POST data.
// This is a regression test for https://crbug.com/726067.
//
// This test is somewhat similar to
// http/tests/navigation/form-targets-cross-site-frame-post.html web test
// except that it 1) tests with files, 2) simulates a malicious scenario and 3)
// verifies file access (all of these 3 things are not possible with web
// tests).
//
// This test is very similar to CrossSiteTransferTest.PostWithFileData above,
// except that it simulates a malicious form / POST originator.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest, MaliciousPostWithFileData) {
  // The initial test window is a named form target.
  GURL initial_target_url(
      embedded_test_server()->GetURL("initial-target.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_target_url));
  WebContents* target_contents = shell()->web_contents();
  EXPECT_TRUE(ExecuteScript(target_contents, "window.name = 'form-target';"));

  // Create a new window containing a form targeting |target_contents|.
  GURL form_url(embedded_test_server()->GetURL(
      "main.com", "/form_that_posts_cross_site.html"));
  Shell* other_window = OpenPopup(target_contents, form_url, "form-window");
  WebContents* form_contents = other_window->web_contents();
  EXPECT_TRUE(ExecuteScript(
      form_contents,
      "document.getElementById('file-form').target = 'form-target';"));

  // Verify the current locations and process placement of |target_contents|
  // and |form_contents|.
  EXPECT_EQ(initial_target_url, target_contents->GetLastCommittedURL());
  EXPECT_EQ(form_url, form_contents->GetLastCommittedURL());
  EXPECT_NE(target_contents->GetMainFrame()->GetProcess()->GetID(),
            form_contents->GetMainFrame()->GetProcess()->GetID());

  // Prepare a file to upload.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("test-file-content");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  base::RunLoop run_loop;
  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path, run_loop.QuitClosure()));
  form_contents->Focus();
  form_contents->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(form_contents, "document.getElementById('file').click();"));
  run_loop.Run();
  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(security_policy->CanReadFile(
      form_contents->GetMainFrame()->GetProcess()->GetID(), file_path));

  // Simulate a malicious situation, where the renderer doesn't really have
  // access to the file.
  security_policy->RevokeAllPermissionsForFile(
      form_contents->GetMainFrame()->GetProcess()->GetID(), file_path);
  EXPECT_FALSE(security_policy->CanReadFile(
      form_contents->GetMainFrame()->GetProcess()->GetID(), file_path));
  EXPECT_FALSE(security_policy->CanReadFile(
      target_contents->GetMainFrame()->GetProcess()->GetID(), file_path));

  // Submit the form and wait until the malicious renderer gets killed.
  RenderProcessHostKillWaiter kill_waiter(
      form_contents->GetMainFrame()->GetProcess());
  EXPECT_TRUE(ExecuteScript(
      form_contents,
      "setTimeout(\n"
      "  function() { document.getElementById('file-form').submit(); },\n"
      "  0);"));
  EXPECT_EQ(bad_message::ILLEGAL_UPLOAD_PARAMS, kill_waiter.Wait());

  // The target frame should still be at the original location - the malicious
  // navigation should have been stopped.
  EXPECT_EQ(initial_target_url, target_contents->GetLastCommittedURL());

  // Both processes still shouldn't have access.
  EXPECT_FALSE(security_policy->CanReadFile(
      form_contents->GetMainFrame()->GetProcess()->GetID(), file_path));
  EXPECT_FALSE(security_policy->CanReadFile(
      target_contents->GetMainFrame()->GetProcess()->GetID(), file_path));
}

// Regression test for https://crbug.com/538784 -- ensures that one can't
// sidestep cross-process navigation by detaching a frame mid-request.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest, NoDeliveryToDetachedFrame) {
  GURL attacker_page = embedded_test_server()->GetURL(
      "evil.com", "/cross_site_iframe_factory.html?evil(evil)");
  EXPECT_TRUE(NavigateToURL(shell(), attacker_page));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  RenderFrameHost* child_frame = root->child_at(0)->current_frame_host();

  // Attacker initiates a navigation to a cross-site document. Under --site-per-
  // process, these bytes must not be sent to the attacker process.
  GURL target_resource =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  TestNavigationManager target_navigation(shell()->web_contents(),
                                          target_resource);
  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents()->GetMainFrame(),
      base::StringPrintf("document.getElementById('child-0').src='%s'",
                         target_resource.spec().c_str())));

  // Wait for the navigation to start.
  EXPECT_TRUE(target_navigation.WaitForRequestStart());
  target_navigation.ResumeNavigation();

  // Inject a frame detach message. An attacker-controlled renderer could do
  // this without also cancelling the pending navigation (as blink would, if you
  // removed the iframe from the document via js).
  child_frame->OnMessageReceived(
      FrameHostMsg_Detach(child_frame->GetRoutingID()));

  // This should cancel the navigation.
  EXPECT_FALSE(target_navigation.WaitForResponse())
      << "Request should have been cancelled before reaching the renderer.";
}

}  // namespace content
