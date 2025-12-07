// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class FramebustingBrowserTest : public ContentBrowserTest {
 public:
  FramebustingBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

// Verifies that cross-origin iframes cannot navigate the top frame to a
// different origin (sometimes called "framebusting") without user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest, FailsWithoutUserActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents(), "child",
      embedded_test_server()->GetURL("other.test", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  EXPECT_FALSE(
      ExecJs(child, "top.location = 'foo'", EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());
}

// Verifies that cross-origin iframes can navigate the top frame to a different
// origin (sometimes called "framebusting") with user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest, SucceedsWithUserActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  GURL other_url =
      embedded_test_server()->GetURL("other.test", "/defaultresponse");
  RenderFrameHost* child = CreateSubframe(web_contents(), "child", other_url,
                                          /*wait_for_navigation=*/true);

  TestNavigationObserver observer(web_contents());

  // By default `ExecJs()` executes the provided script with user activation.
  EXPECT_TRUE(ExecJs(child, "top.location = '/defaultresponse'"));

  // The top frame is indeed navigated successfully.
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), other_url);
}

// Verifies that cross-origin iframes can navigate the top frame to a different
// origin (sometimes called "framebusting") with user activation, even after
// a couple `setTimeout()` calls.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       SucceedsWithAsyncUserActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  GURL other_url =
      embedded_test_server()->GetURL("other.test", "/defaultresponse");
  RenderFrameHost* child = CreateSubframe(web_contents(), "child", other_url,
                                          /*wait_for_navigation=*/true);

  TestNavigationObserver observer(web_contents());

  // By default `ExecJs()` executes the provided script with a user activation.
  //
  // With user activation, the navigation should succeed even through nested
  // `setTimeout()` calls.
  EXPECT_TRUE(ExecJs(child, R"(
    setTimeout(() => {
      setTimeout(() => {
        top.location = '/defaultresponse';
      }, 0);
    }, 0);
  )"));

  // The top frame is indeed navigated successfully.
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), other_url);
}

// Verifies that cross-origin unsandboxed iframes cannot escalate the
// allow-top-navigation sandbox privilege in a child iframe, which would allow
// it to navigate the top frame to a different origin (sometimes called
// "framebusting") without user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       FailsFromGrandchildPrivilegeEscalationInSandboxFlags) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents()->GetPrimaryMainFrame(), "child",
      embedded_test_server()->GetURL("other.test", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  RenderFrameHost* grandchild = CreateSubframe(
      child, "grandchild",
      embedded_test_server()->GetURL("other.test", "/defaultresponse"),
      /*wait_for_navigation=*/true,
      {.sandbox_flags = "allow-scripts allow-top-navigation"});

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  EXPECT_FALSE(ExecJs(grandchild, "window.top.location = 'foo'",
                      EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());
}

// Verifies that a grandchild cross-origin unsandboxed iframe cannot give itself
// allow-top-navigation sandbox privileges via its delivered sandbox flags in
// the HTTP response header, which would allow it to navigate the top frame to a
// different origin (sometimes called "framebusting") without user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       FailsFromGrandchildPrivilegeEscalationInDeliveredFlags) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents()->GetPrimaryMainFrame(), "child",
      embedded_test_server()->GetURL("other.test", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  RenderFrameHost* grandchild =
      CreateSubframe(child, "grandchild",
                     embedded_test_server()->GetURL(
                         "other.test",
                         "/set-header?Content-Security-Policy: sandbox "
                         "allow-scripts allow-top-navigation"),
                     /*wait_for_navigation=*/true);

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  EXPECT_FALSE(ExecJs(grandchild, "window.top.location = 'foo'",
                      EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());
}

// Verifies that a child cross-origin unsandboxed iframe document cannot give
// itself allow-top-navigation sandbox privileges via its delivered sandbox
// flags in the HTTP response header, which would allow it to navigate the top
// frame to a different origin (sometimes called "framebusting") without user
// activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       FailsFromChildPrivilegeEscalationInDeliveredFlags) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child =
      CreateSubframe(web_contents()->GetPrimaryMainFrame(), "child",
                     embedded_test_server()->GetURL(
                         "other.test",
                         "/set-header?Content-Security-Policy: sandbox "
                         "allow-scripts allow-top-navigation"),
                     /*wait_for_navigation=*/true);

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  EXPECT_FALSE(ExecJs(child, "window.top.location = 'foo'",
                      EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());
}

// Verifies that a navigation to a cross-site document consumes sticky user
// activation, preventing the new document from navigating the top frame to a
// different origin (sometimes called "framebusting") without user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest, FailsAfterCrossSiteNavigation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents()->GetPrimaryMainFrame(), "child",
      embedded_test_server()->GetURL("foo.com", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, ""));

  // Perform a cross-site navigation. This should clear the sticky user
  // activation state.
  GURL navigate_url =
      embedded_test_server()->GetURL("other.test", "/defaultresponse");
  EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1", navigate_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  child =
      web_contents()->GetPrimaryMainFrame()->child_at(0)->current_frame_host();
  EXPECT_EQ(child->GetLastCommittedURL(), navigate_url);

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  EXPECT_FALSE(ExecJs(child, "window.top.location = 'foo'",
                      EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());
}

// Verifies that a navigation to a same-site document maintains sticky user
// activation, allow the new document to navigate the top frame to a
// different origin (sometimes called "framebusting") without transient user
// activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       SucceedsAfterSameSiteNavigation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents()->GetPrimaryMainFrame(), "child",
      embedded_test_server()->GetURL("foo.com", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, ""));

  // Perform a same-site but cross-origin navigation. This should keep the
  // sticky user activation state.
  GURL navigate_url =
      embedded_test_server()->GetURL("subdomain.foo.com", "/defaultresponse");
  EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1", navigate_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  child =
      web_contents()->GetPrimaryMainFrame()->child_at(0)->current_frame_host();
  EXPECT_EQ(child->GetLastCommittedURL(), navigate_url);

  EXPECT_TRUE(ExecJs(child, "window.top.location = 'foo'",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Verifies that a navigation to a same-site document without sticky user
// activation keeps the unset activation state, preventing the new document from
// navigating the top frame to a different origin (sometimes called
// "framebusting") without transient user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       FailsAfterSameSiteNavigationWithoutUserActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents()->GetPrimaryMainFrame(), "child",
      embedded_test_server()->GetURL("foo.com", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  // Perform a same-site but cross-origin navigation. There is no sticky user
  // activation state, so the newly navigated page should not have sticky user
  // activation either.
  GURL navigate_url =
      embedded_test_server()->GetURL("subdomain.foo.com", "/defaultresponse");
  EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1", navigate_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  child =
      web_contents()->GetPrimaryMainFrame()->child_at(0)->current_frame_host();
  EXPECT_EQ(child->GetLastCommittedURL(), navigate_url);

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  EXPECT_FALSE(ExecJs(child, "window.top.location = 'foo'",
                      EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());
}

// Verifies that cross-origin iframes sandboxed with
// "allow-top-navigation-by-user-activation" can only navigate the top frame to
// a different origin (sometimes called "framebusting") when they have user
// activation.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       AllowTopNavigationByUserActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents()->GetPrimaryMainFrame(), "child",
      embedded_test_server()->GetURL("other.test", "/defaultresponse"),
      /*wait_for_navigation=*/true,
      {.sandbox_flags =
           "allow-scripts allow-top-navigation-by-user-activation"});

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*permission to navigate the target frame*");

  // The initial top-level navigation should fail without user activation.
  EXPECT_FALSE(ExecJs(child, "window.top.location = 'foo'",
                      EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(console_observer.Wait());

  // Once the frame has user activation, the top-level navigation should
  // succeed.
  EXPECT_TRUE(ExecJs(child, ""));
  EXPECT_TRUE(ExecJs(child, "window.top.location = 'foo'",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Verifies that cross-origin iframes can navigate the top frame to another URL
// belonging to the top frame's origin without user activation.
//
// This is non-standard, unspecified behavior.
// See also https://www.chromestatus.com/features/5851021045661696.
IN_PROC_BROWSER_TEST_F(FramebustingBrowserTest,
                       SucceedsInSameOriginWithoutUserActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/defaultresponse")));

  RenderFrameHost* child = CreateSubframe(
      web_contents(), "child",
      embedded_test_server()->GetURL("other.test", "/defaultresponse"),
      /*wait_for_navigation=*/true);

  TestNavigationObserver observer(web_contents());

  GURL destination = embedded_test_server()->GetURL("/echo");
  EXPECT_TRUE(ExecJs(child, JsReplace("top.location = $1", destination),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  // The top frame is indeed navigated successfully.
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), destination);
}

}  // namespace content
