// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using base::ASCIIToUTF16;

namespace content {

namespace {

const char kOpenUrlViaClickTargetFunc[] =
    "(function(url) {\n"
    "  var lnk = document.createElement(\"a\");\n"
    "  lnk.href = url;\n"
    "  lnk.target = \"_blank\";\n"
    "  document.body.appendChild(lnk);\n"
    "  lnk.click();\n"
    "})";

// Adds a link with given url and target=_blank, and clicks on it.
void OpenUrlViaClickTarget(const ToRenderFrameHost& adapter, const GURL& url) {
  EXPECT_TRUE(ExecuteScript(adapter, std::string(kOpenUrlViaClickTargetFunc) +
                                         "(\"" + url.spec() + "\");"));
}

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  using WebUIMessageHandler::AllowJavascript;
  using WebUIMessageHandler::IsJavascriptAllowed;

 protected:
  void RegisterMessages() override {}
};

// This class implements waiting for RenderFrameHost destruction. It relies on
// the fact that RenderFrameDeleted event is fired when RenderFrameHost is
// destroyed.
// Note: RenderFrameDeleted is also fired when the process associated with the
// RenderFrameHost crashes, so this cannot be used in cases where process dying
// is expected.
class RenderFrameHostDestructionObserver : public WebContentsObserver {
 public:
  explicit RenderFrameHostDestructionObserver(RenderFrameHost* rfh)
      : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
        message_loop_runner_(new MessageLoopRunner),
        deleted_(false),
        render_frame_host_(rfh) {}
  ~RenderFrameHostDestructionObserver() override {}

  bool deleted() const { return deleted_; }

  void Wait() {
    if (deleted_)
      return;

    message_loop_runner_->Run();
  }

  // WebContentsObserver implementation:
  void RenderFrameDeleted(RenderFrameHost* rfh) override {
    if (rfh == render_frame_host_) {
      CHECK(!deleted_);
      deleted_ = true;
    }

    if (deleted_ && message_loop_runner_->loop_running()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, message_loop_runner_->QuitClosure());
    }
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  bool deleted_;
  RenderFrameHost* render_frame_host_;
};

// A NavigationThrottle implementation that blocks all outgoing navigation
// requests for a specific WebContents. It is used to block navigations to
// WebUI URLs in the following test.
class RequestBlockingNavigationThrottle : public NavigationThrottle {
 public:
  explicit RequestBlockingNavigationThrottle(NavigationHandle* handle)
      : NavigationThrottle(handle) {}

  static std::unique_ptr<NavigationThrottle> Create(NavigationHandle* handle) {
    return std::make_unique<RequestBlockingNavigationThrottle>(handle);
  }

 private:
  ThrottleCheckResult WillStartRequest() override {
    return NavigationThrottle::BLOCK_REQUEST;
  }

  const char* GetNameForLogging() override {
    return "RequestBlockingNavigationThrottle";
  }

  DISALLOW_COPY_AND_ASSIGN(RequestBlockingNavigationThrottle);
};

// Helper function for error page navigations that makes sure that the last
// committed origin on |node| is an opaque origin with a precursor that matches
// |url|'s origin.
// Returns true if the frame has an opaque origin with the expected precursor
// information. Otherwise returns false.
bool IsOriginOpaqueAndCompatibleWithURL(FrameTreeNode* node, const GURL& url) {
  url::Origin frame_origin =
      node->current_frame_host()->GetLastCommittedOrigin();

  if (!frame_origin.opaque()) {
    LOG(ERROR) << "Frame origin was not opaque. " << frame_origin;
    return false;
  }

  const GURL url_origin = url.GetOrigin();
  const GURL precursor_origin =
      frame_origin.GetTupleOrPrecursorTupleIfOpaque().GetURL();
  if (url_origin != precursor_origin) {
    LOG(ERROR) << "url_origin '" << url_origin << "' !=  precursor_origin '"
               << precursor_origin << "'";
    return false;
  }
  return true;
}

bool IsMainFrameOriginOpaqueAndCompatibleWithURL(Shell* shell,
                                                 const GURL& url) {
  return IsOriginOpaqueAndCompatibleWithURL(
      static_cast<WebContentsImpl*>(shell->web_contents())
          ->GetFrameTree()
          ->root(),
      url);
}

}  // anonymous namespace

class RenderFrameHostManagerTest : public ContentBrowserTest {
 public:
  RenderFrameHostManagerTest() : foo_com_("foo.com") {
    replace_host_.SetHostStr(foo_com_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const char kBlinkPageLifecycleFeature[] = "PageLifecycle";
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    kBlinkPageLifecycleFeature);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void DisableBackForwardCache(
      BackForwardCacheImpl::DisableForTestingReason reason) const {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetController()
        .GetBackForwardCache()
        .DisableForTesting(reason);
  }

  void StartServer() {
    ASSERT_TRUE(embedded_test_server()->Start());

    foo_host_port_ = embedded_test_server()->host_port_pair();
    foo_host_port_.set_host(foo_com_);
  }

  void StartEmbeddedServer() {
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<content::URLLoaderInterceptor> SetupRequestFailForURL(
      const GURL& url) {
    return URLLoaderInterceptor::SetupRequestFailForURL(url,
                                                        net::ERR_DNS_TIMED_OUT);
  }

  // Returns a URL on foo.com with the given path.
  GURL GetCrossSiteURL(const std::string& path) {
    GURL cross_site_url(embedded_test_server()->GetURL(path));
    return cross_site_url.ReplaceComponents(replace_host_);
  }

  void NavigateToPageWithLinks(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(
        shell, embedded_test_server()->GetURL("/click-noreferrer-links.html")));

    // Rewrite selected links on the page to be actual cross-site (bar.com)
    // URLs. This does not use the /cross-site/ redirector, since that creates
    // links that initially look same-site.
    std::string script = "setOriginForLinks('http://bar.com:" +
                         embedded_test_server()->base_url().port() + "/');";
    EXPECT_TRUE(ExecuteScript(shell, script));
  }

 protected:
  std::string foo_com_;
  GURL::Replacements replace_host_;
  net::HostPortPair foo_host_port_;
};

// Web pages should not have script access to the swapped out page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, NoScriptAccessAfterSwapOut) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Open a same-site link in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      new_shell, embedded_test_server()->GetURL("foo.com", "/title1.html")));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_EQ(orig_site_instance, new_site_instance);
  } else {
    EXPECT_NE(orig_site_instance, new_site_instance);
  }

  // We should no longer have script access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_FALSE(success);

  // We now navigate the window to an about:blank page.
  TestNavigationObserver navigation_observer(new_shell->web_contents());
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickBlankTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish.
  navigation_observer.Wait();

  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(blank_url, new_shell->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithRelNoreferrerAndTargetBlank) {
  StartEmbeddedServer();

  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Same as above, but for 'noopener'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithRelNoopenerAndTargetBlank) {
  StartEmbeddedServer();

  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickNoOpenerTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());

  // Check that the referrer is set correctly.
  std::string expected_referrer =
      embedded_test_server()->GetURL("/click-noreferrer-links.html").spec();
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell,
      "window.domAutomationController.send(document.referrer == '" +
          expected_referrer + "');",
      &success));
  EXPECT_TRUE(success);

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noopener_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noopener_blank_site_instance);
}

// 'noopener' also works from 'window.open'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithWindowOpenAndNoopener) {
  StartEmbeddedServer();

  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get());

  // Test opening a window with the 'noopener' feature.
  ShellAddedObserver new_shell_observer;
  bool hasWindowReference = true;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send("
      "    openWindowWithTargetAndFeatures('/title2.html', '', 'noopener')"
      ");",
      &hasWindowReference));
  // We should not get a reference to the opened window.
  EXPECT_FALSE(hasWindowReference);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());

  EXPECT_EQ("/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Check that `window.opener` is not set.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Check that the referrer is set correctly.
  std::string expected_referrer =
      embedded_test_server()->GetURL("/click-noreferrer-links.html").spec();
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell,
      "window.domAutomationController.send(document.referrer == '" +
          expected_referrer + "');",
      &success));
  EXPECT_TRUE(success);

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noopener_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noopener_blank_site_instance);
}

// As of crbug.com/69267, we create a new BrowsingInstance (and SiteInstance)
// for rel=noreferrer links in new windows, even to same site pages and named
// targets.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithSameSiteRelNoreferrer) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a same-site rel=noreferrer + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteNoRefTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Same as above, but for 'noopener'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithSameSiteRelNoopener) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a same-site rel=noopener + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(),
                                          "window.domAutomationController.send("
                                          "clickSameSiteNoOpenerTargetedLink())"
                                          ";",
                                          &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with just
// target=_blank should not create a new SiteInstance, unless we
// are running in --site-per-process mode.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyTargetBlank) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  EXPECT_EQ("/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  if (AreAllSitesIsolatedForTesting())
    EXPECT_NE(orig_site_instance, blank_site_instance);
  else
    EXPECT_EQ(orig_site_instance, blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and no target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyRelNoreferrer) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  if (AreAllSitesIsolatedForTesting() ||
      IsProactivelySwapBrowsingInstanceEnabled()) {
    EXPECT_NE(orig_site_instance, noref_site_instance);
  } else {
    EXPECT_EQ(orig_site_instance, noref_site_instance);
  }
}

// Same as above, but for 'noopener'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyRelNoOpener) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  if (AreAllSitesIsolatedForTesting() ||
      IsProactivelySwapBrowsingInstanceEnabled()) {
    EXPECT_NE(orig_site_instance, noref_site_instance);
  } else {
    EXPECT_EQ(orig_site_instance, noref_site_instance);
  }
}

// Test for crbug.com/116192.  Targeted links should still work after the
// named target window has swapped processes.
// Disabled Flaky test - crbug.com/859487
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DISABLED_AllowTargetedNavigationsAfterSwap) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, cross_site_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Clicking the original link in the first tab should cause us to swap back.
  TestNavigationObserver navigation_observer(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back and shown the new window again.
  scoped_refptr<SiteInstance> revisit_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);

  // If it navigates away to another process, the original window should
  // still be able to close it (using a cross-process close message).
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, cross_site_url));
  EXPECT_EQ(new_site_instance.get(),
            new_shell->web_contents()->GetSiteInstance());
  WebContentsDestroyedWatcher close_watcher(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(testCloseWindow());",
      &success));
  EXPECT_TRUE(success);
  close_watcher.Wait();
}

// Test that setting the opener to null in a window affects cross-process
// navigations, including those to existing entries.  http://crbug.com/156669.
// This test crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_DisownOpener DISABLED_DisownOpener
#else
#define MAYBE_DisownOpener DisownOpener
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, MAYBE_DisownOpener) {
  StartEmbeddedServer();

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed to get a non-default
    // SiteInstance for navigations to this origin.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a target=_blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, cross_site_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Now disown the opener.
  EXPECT_TRUE(ExecuteScript(new_shell, "window.opener = null;"));
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());

  // Go back and ensure the opener is still null.
  {
    TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());

  // Now navigate forward again (creating a new process) and check opener.
  EXPECT_TRUE(NavigateToURL(new_shell, cross_site_url));
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());
}

// Test that subframes can disown their openers.  http://crbug.com/225528.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, DisownSubframeOpener) {
  const GURL frame_url("data:text/html,<iframe name=\"foo\"></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), frame_url));

  // Give the frame an opener using window.open.
  EXPECT_TRUE(ExecuteScript(shell(), "window.open('about:blank','foo');"));

  // Check that the browser process updates the subframe's opener.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(root, root->child_at(0)->opener());
  EXPECT_EQ(nullptr, root->child_at(0)->original_opener());

  // Now disown the frame's opener.  Shouldn't crash.
  EXPECT_TRUE(ExecuteScript(shell(), "window.frames[0].opener = null;"));

  // Check that the subframe's opener in the browser process is disowned.
  EXPECT_EQ(nullptr, root->child_at(0)->opener());
  EXPECT_EQ(nullptr, root->child_at(0)->original_opener());
}

// Check that window.name is preserved for top frames when they navigate
// cross-process.  See https://crbug.com/504164.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PreserveTopFrameWindowNameOnCrossProcessNavigations) {
  StartEmbeddedServer();
  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed it is placed in a different
    // process.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Open a popup using window.open with a 'foo' window.name.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(new_shell);

  // The window.name for the new popup should be "foo".
  std::string name;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell, "window.domAutomationController.send(window.name);", &name));
  EXPECT_EQ("foo", name);

  // Now navigate the new tab to a different site.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, foo_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance->GetProcess(), new_site_instance->GetProcess());

  // window.name should still be "foo".
  name = "";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell, "window.domAutomationController.send(window.name);", &name));
  EXPECT_EQ("foo", name);

  // Open another popup from the 'foo' popup and navigate it cross-site.
  Shell* new_shell2 = OpenPopup(new_shell, GURL(url::kAboutBlankURL), "bar");
  EXPECT_TRUE(new_shell2);
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(new_shell2, bar_url));

  // Check that the new popup's window.opener has name "foo", which verifies
  // that new swapped-out RenderViews also propagate window.name.  This has to
  // be done via window.open, since window.name isn't readable cross-origin.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell2,
      "window.domAutomationController.send("
      "    window.opener === window.open('','foo'));",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/99202.  PostMessage calls should still work after
// navigating the source and target windows to different sites.
// Specifically:
// 1) Create 3 windows (opener, "foo", and _blank) and send "foo" cross-process.
// 2) Fail to post a message from "foo" to opener with the wrong target origin.
// 3) Post a message from "foo" to opener, which replies back to "foo".
// 4) Post a message from _blank to "foo".
// 5) Post a message from "foo" to a subframe of opener, which replies back.
// 6) Post a message from _blank to a subframe of "foo".
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SupportCrossProcessPostMessage) {
  StartEmbeddedServer();
  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed it is placed in a different
    // process.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);
  RenderFrameHostManager* opener_manager =
      static_cast<WebContentsImpl*>(opener_contents)
          ->GetRenderManagerForTesting();

  // 1) Open two more windows, one named.  These initially have openers but no
  // reference to each other.  We will later post a message between them.

  // First, a named target=foo window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      new_shell,
      embedded_test_server()->GetURL("foo.com", "/post_message.html")));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // Second, a target=_blank window.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on the original site.
  Shell* new_shell2 = new_shell_observer2.GetShell();
  WebContents* new_contents = new_shell2->web_contents();
  WaitForLoadStop(new_contents);
  EXPECT_EQ("/title2.html", new_contents->GetLastCommittedURL().path());
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      new_shell2, embedded_test_server()->GetURL("/post_message.html")));
  EXPECT_EQ(orig_site_instance.get(), new_contents->GetSiteInstance());
  RenderFrameHostManager* new_manager =
      static_cast<WebContentsImpl*>(new_contents)->GetRenderManagerForTesting();

  // We now have three windows.  The opener should have a swapped out RVH
  // for the new SiteInstance, but the _blank window should not.
  EXPECT_EQ(3u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));
  EXPECT_FALSE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Fail to post a message from the foo window to the opener if the target
  // origin is wrong.  We won't see an error, but we can check for the right
  // number of received messages below.
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg',"
      "    'http://google.com'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));

  // 3) Post a message from the foo window to the opener.  The opener will
  // reply, causing the foo window to update its own title.
  base::string16 expected_title = ASCIIToUTF16("msg");
  TitleWatcher title_watcher(foo_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // We should have received only 1 message in the opener and "foo" tabs,
  // and updated the title.
  int opener_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &opener_received_messages));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, opener_received_messages);
  EXPECT_EQ(ASCIIToUTF16("msg"), foo_contents->GetTitle());

  // 4) Now post a message from the _blank window to the foo window.  The
  // foo window will update its title and will not reply.
  expected_title = ASCIIToUTF16("msg2");
  TitleWatcher title_watcher2(foo_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_contents, "window.domAutomationController.send(postToFoo('msg2'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_EQ(expected_title, title_watcher2.WaitAndGetTitle());

  // This postMessage should have created a swapped out RVH for the new
  // SiteInstance in the target=_blank window.
  EXPECT_TRUE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // TODO(nasko): Test subframe targeting of postMessage once
  // http://crbug.com/153701 is fixed.
}

// Test for crbug.com/278336. MessagePorts should work cross-process. Messages
// which contain Transferables that need to be forwarded between processes via
// RenderFrameProxy::willCheckAndDispatchMessageEvent should work.
// Specifically:
// 1) Create 2 windows (opener and "foo") and send "foo" cross-process.
// 2) Post a message containing a message port from opener to "foo".
// 3) Post a message from "foo" back to opener via the passed message port.
// The test will be enabled when the feature implementation lands.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SupportCrossProcessPostMessageWithMessagePort) {
  StartEmbeddedServer();
  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed it is placed in a different
    // process.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);
  RenderFrameHostManager* opener_manager =
      static_cast<WebContentsImpl*>(opener_contents)
          ->GetRenderManagerForTesting();

  // 1) Open a named target=foo window. We will later post a message between the
  // opener and the new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      new_shell,
      embedded_test_server()->GetURL("foo.com", "/post_message.html")));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // We now have two windows. The opener should have a swapped out RVH
  // for the new SiteInstance.
  EXPECT_EQ(2u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Post a message containing a MessagePort from opener to the the foo
  // window. The foo window will reply via the passed port, causing the opener
  // to update its own title.
  base::string16 expected_title = ASCIIToUTF16("msg-back-via-port");
  TitleWatcher title_observer(opener_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(postWithPortToFoo());", &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  ASSERT_EQ(expected_title, title_observer.WaitAndGetTitle());

  // Check message counts.
  int opener_received_messages_via_port = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessagesViaPort);",
      &opener_received_messages_via_port));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  int foo_received_messages_with_port = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessagesWithPort);",
      &foo_received_messages_with_port));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, foo_received_messages_with_port);
  EXPECT_EQ(1, opener_received_messages_via_port);
  EXPECT_EQ(ASCIIToUTF16("msg-with-port"), foo_contents->GetTitle());
  EXPECT_EQ(ASCIIToUTF16("msg-back-via-port"), opener_contents->GetTitle());
}

// Test for crbug.com/116192.  Navigations to a window's opener should
// still work after a process swap.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       AllowTargetedNavigationsInOpenerAfterSwap) {
  StartEmbeddedServer();

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed to get a non-default
    // SiteInstance for navigations to this origin.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original tab and SiteInstance for later comparison.
  WebContents* orig_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      orig_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the original (opener) tab to a different site.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The opened tab should be able to navigate the opener back to its process.
  TestNavigationObserver navigation_observer(orig_contents);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(navigateOpener());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back into this process.
  scoped_refptr<SiteInstance> revisit_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);
}

// Test that subframes do not crash when sending a postMessage to the top frame
// from an unload handler while the top frame is being swapped out as part of
// navigating cross-process.  https://crbug.com/475651.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PostMessageFromSubframeUnloadHandler) {
  StartEmbeddedServer();

  GURL frame_url(embedded_test_server()->GetURL("/post_message.html"));
  GURL main_url("data:text/html,<iframe name='foo' src='" + frame_url.spec() +
                "'></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(nullptr, orig_site_instance.get());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());

  // Register an unload handler that sends a postMessage to the top frame.
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "registerUnload();"));

  // Navigate the top frame cross-site.  This will cause the top frame to be
  // swapped out and run unload handlers, and the original renderer process
  // should then terminate since it's not rendering any other frames.
  RenderProcessHostWatcher exit_observer(
      root->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Ensure that the original renderer process exited cleanly without crashing.
  exit_observer.Wait();
  EXPECT_TRUE(exit_observer.did_exit_normally());
}

// Test that opening a new window in the same SiteInstance and then navigating
// both windows to a different SiteInstance allows the first process to exit.
// See http://crbug.com/126333.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ProcessExitWithSwappedOutViews) {
  StartEmbeddedServer();
  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed to get a non-default
    // SiteInstance for navigations to this origin.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> opened_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, opened_site_instance);

  // Now navigate the opened window to a different site.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, cross_site_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The original process should still be alive, since it is still used in the
  // first window.
  RenderProcessHost* orig_process = orig_site_instance->GetProcess();
  EXPECT_TRUE(orig_process->IsInitializedAndNotDead());

  // Navigate the first window to a different site as well.  The original
  // process should exit, since all of its views are now swapped out.
  RenderProcessHostWatcher exit_observer(
      orig_process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(shell(), cross_site_url));
  exit_observer.Wait();
  scoped_refptr<SiteInstance> new_site_instance2(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(new_site_instance, new_site_instance2);
}

// Test for crbug.com/76666.  A cross-site navigation that fails with a 204
// error should not make us ignore future renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, ClickLinkAfter204Error) {
  StartServer();

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != nullptr);

  // Load a cross-site page that fails with a 204 error.
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GetCrossSiteURL("/nocontent")));

  // We should still be looking at the normal page.  Because we started from a
  // blank new tab, the typed URL will still be visible until the user clears it
  // manually.  The last committed URL will be the previous page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/nocontent", shell()->web_contents()->GetVisibleURL().path());
  EXPECT_FALSE(
      shell()->web_contents()->GetController().GetLastCommittedEntry());

  // Renderer-initiated navigations should work.
  base::string16 expected_title = ASCIIToUTF16("Title Of Awesomeness");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  GURL url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(ExecuteScript(
      shell(), base::StringPrintf("location.href = '%s'", url.spec().c_str())));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Opens in same tab.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, new_site_instance);
}

// A collection of tests to prevent URL spoofs when showing pending URLs above
// initial empty documents, ensuring that the URL reverts to about:blank if the
// document is accessed. See https://crbug.com/9682.
class RenderFrameHostManagerSpoofingTest : public RenderFrameHostManagerTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    base::CommandLine new_command_line(command_line->GetProgram());
    base::CommandLine::SwitchMap switches = command_line->GetSwitches();

    // Injecting the DOM automation controller causes false positives, since it
    // triggers the DidAccessInitialDocument() callback by mutating the global
    // object.
    switches.erase(switches::kDomAutomationController);

    for (const auto& it : switches)
      new_command_line.AppendSwitchNative(it.first, it.second);

    *command_line = new_command_line;
  }

 protected:
  // Custom ExecuteScript() helper that doesn't depend on DOM automation
  // controller. This is used to guarantee the script has completed execution,
  // but the spoofing tests synchronize execution using window title changes.
  void ExecuteScript(const ToRenderFrameHost& adapter, const char* script) {
    adapter.render_frame_host()->ExecuteJavaScriptForTests(
        base::UTF8ToUTF16(script), base::NullCallback());
  }
};

// Helper to wait until a WebContent's NavigationController has a visible entry.
class VisibleEntryWaiter : public WebContentsObserver {
 public:
  explicit VisibleEntryWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() {
    if (web_contents()->GetController().GetVisibleEntry())
      return;
    run_loop_.Run();
  }

  // WebContentsObserver overrides:
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

// Sanity test that a newly opened window shows the pending URL if the initial
// empty document is not modified. This is intentionally structured as similarly
// as possible to the subsequent ShowLoadingURLUntil*Spoof tests: it performs
// the same operations as the subsequent tests except DOM modification. This
// should help catch instances where the subsequent tests incorrectly pass due
// to a side effect of the test infrastructure.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLIfNotModified) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/click-nocontent-link.html")));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the provisional load, so the
  // associated navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now get another reference to the window object, but don't otherwise access
  // it. This is to ensure that DidAccessInitialDocument() notifications are not
  // incorrectly generated when nothing is modified.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "getNewWindowReference();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // The destination URL should still be visible, since nothing was modified.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());
}

// Test for crbug.com/9682.  We should show the URL for a pending renderer-
// initiated navigation in a new tab, until the content of the initial
// about:blank page is modified by another window.  At that point, we should
// revert to showing about:blank to prevent a URL spoof.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLUntilSpoof) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/click-nocontent-link.html")));
  WebContents* orig_contents = shell()->web_contents();

  // Change the link to be cross-site.
  GURL target_url = embedded_test_server()->GetURL("foo.com", "/nocontent");
  ExecuteScript(
      orig_contents,
      base::StringPrintf(
          "document.getElementById('nocontent_targeted_link').href = '%s';",
          target_url.spec().c_str())
          .c_str());

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the navigation, so the associated
  // navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ(target_url, contents->GetController().GetVisibleEntry()->GetURL());

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "modifyNewWindow();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Same as ShowLoadingURLUntilSpoof, but reloads the new popup before modifying
// it, to test https://crbug.com/847718.  The reload should not cause the
// visible entry to stick around after the modification, even though it is
// triggered in the browser process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLUntilSpoofAfterReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/click-nocontent-link.html")));
  WebContents* orig_contents = shell()->web_contents();

  // Change the link to be cross-site.
  GURL target_url = embedded_test_server()->GetURL("foo.com", "/nocontent");
  ExecuteScript(
      orig_contents,
      base::StringPrintf(
          "document.getElementById('nocontent_targeted_link').href = '%s';",
          target_url.spec().c_str())
          .c_str());

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the navigation, so the associated
  // navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ(target_url, contents->GetController().GetVisibleEntry()->GetURL());

  // Reload the popup before modifying it.  See https://crbug.com/847718.
  contents->GetController().Reload(ReloadType::NORMAL, false);

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "modifyNewWindow();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Similar but using document.open(): once a Document is opened, subsequent
// document.write() calls can insert arbitrary content into the target Document.
// Since this could result in URL spoofing, the pending URL should no longer be
// shown in the omnibox.
//
// Note: document.write() implicitly invokes document.open() if the Document has
// not already been opened, so there's no need to test document.write()
// separately.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLUntilDocumentOpenSpoof) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/click-nocontent-link.html")));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the provisional load, so the
  // associated navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "modifyNewWindowWithDocumentOpen();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       WasDiscardedWhenNavigationInterruptsReload) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL discarded_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), discarded_url));
  // Discard the page.
  shell()->web_contents()->SetWasDiscarded(true);
  // Reload the discarded page, but pretend that it's slow to commit.
  TestNavigationManager first_reload(shell()->web_contents(), discarded_url);
  shell()->web_contents()->GetController().Reload(
      ReloadType::ORIGINAL_REQUEST_URL, false);
  EXPECT_TRUE(first_reload.WaitForRequestStart());
  // Before the response is received, simulate user navigating to another URL.
  GURL second_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager second_navigation(shell()->web_contents(), second_url);
  shell()->LoadURL(second_url);
  second_navigation.WaitForNavigationFinished();
  const char kDiscardedStateJS[] =
      "window.domAutomationController.send(window.document.wasDiscarded);";
  bool discarded_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(shell(), kDiscardedStateJS,
                                                   &discarded_result));
  EXPECT_FALSE(discarded_result);
}

// Ensures that a pending navigation's URL  is no longer visible after the
// speculative RFH is discarded due to a concurrent renderer-initiated
// navigation.  See https://crbug.com/760342.
// TODO(https://crbug.com/945194): Disabled due to flaky timeouts.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostManagerTest,
    DISABLED_ResetVisibleURLOnCrossProcessNavigationInterrupted) {
  const std::string kVictimPath = "/victim.html";
  const std::string kAttackPath = "/attack.html";
  net::test_server::ControllableHttpResponse victim_response(
      embedded_test_server(), kVictimPath);
  net::test_server::ControllableHttpResponse attack_response(
      embedded_test_server(), kAttackPath);
  EXPECT_TRUE(embedded_test_server()->Start());

  const GURL kVictimURL = embedded_test_server()->GetURL("a.com", kVictimPath);
  const GURL kAttackURL = embedded_test_server()->GetURL("b.com", kAttackPath);

  // First navigate to the attacker page. This page will be cross-site compared
  // to the next navigations we will attempt.
  const GURL kAttackInitialURL =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), kAttackInitialURL));
  EXPECT_EQ(kAttackInitialURL, shell()->web_contents()->GetVisibleURL());

  // Now, start a browser-initiated cross-site navigation to a new page that
  // will hang at ready to commit stage.
  TestNavigationManager victim_navigation(shell()->web_contents(), kVictimURL);
  shell()->LoadURL(kVictimURL);
  EXPECT_TRUE(victim_navigation.WaitForRequestStart());
  victim_navigation.ResumeNavigation();

  victim_response.WaitForRequest();
  victim_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(victim_navigation.WaitForResponse());
  victim_navigation.ResumeNavigation();

  // The navigation is ready to commit: it has been handed to the speculative
  // RenderFrameHost for commit.
  RenderFrameHostImpl* speculative_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->render_manager()
          ->speculative_frame_host();
  CHECK(speculative_rfh);
  EXPECT_TRUE(speculative_rfh->is_loading());

  // Since we have a browser-initiated pending navigation, the navigation URL is
  // showing in the address bar.
  EXPECT_EQ(kVictimURL, shell()->web_contents()->GetVisibleURL());

  // The attacker page requests a navigation to a new document while the
  // browser-initiated navigation hasn't committed yet.
  TestNavigationManager attack_navigation(shell()->web_contents(), kAttackURL);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      shell()->web_contents(),
      "location.href = \"" + kAttackURL.spec() + "\";"));
  EXPECT_TRUE(attack_navigation.WaitForRequestStart());

  // This deletes the speculative RenderFrameHost that was supposed to commit
  // the browser-initiated navigation.
  speculative_rfh = static_cast<WebContentsImpl*>(shell()->web_contents())
                        ->GetFrameTree()
                        ->root()
                        ->render_manager()
                        ->speculative_frame_host();
  EXPECT_FALSE(speculative_rfh);

  // The URL of the browser-initiated navigation should no longer be shown in
  // the address bar since the RenderFrameHost that was supposed to commit it
  // has been discarded. Instead, we should be showing the URL of the current
  // page as we do not show the URL of pending navigations when they are
  // renderer-initiated.
  EXPECT_NE(kVictimURL, shell()->web_contents()->GetVisibleURL());
  EXPECT_EQ(kAttackInitialURL, shell()->web_contents()->GetVisibleURL());

  // The attacker navigation results in a 204.
  attack_navigation.ResumeNavigation();
  attack_response.WaitForRequest();
  attack_response.Send(
      "HTTP/1.1 204 OK\r\n"
      "Connection: close\r\n"
      "\r\n");
  attack_navigation.WaitForNavigationFinished();
  speculative_rfh = static_cast<WebContentsImpl*>(shell()->web_contents())
                        ->GetFrameTree()
                        ->root()
                        ->render_manager()
                        ->speculative_frame_host();
  EXPECT_FALSE(speculative_rfh);

  // We are still showing the URL of the current page.
  EXPECT_EQ(kAttackInitialURL, shell()->web_contents()->GetVisibleURL());
}

// Ensures that deleting a speculative RenderFrameHost trying to commit a
// navigation to the pending NavigationEntry will not crash if it happens
// because a new navigation to the same pending NavigationEntry started. This is
// a regression test for crbug.com/796135.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostManagerTest,
    DeleteSpeculativeRFHPendingCommitOfPendingEntryOnInterrupted1) {
  const std::string kOriginalPath = "/original.html";
  const std::string kFirstRedirectPath = "/redirect1.html";
  const std::string kSecondRedirectPath = "/reidrect2.html";
  net::test_server::ControllableHttpResponse original_response1(
      embedded_test_server(), kOriginalPath);
  net::test_server::ControllableHttpResponse original_response2(
      embedded_test_server(), kOriginalPath);
  net::test_server::ControllableHttpResponse original_response3(
      embedded_test_server(), kOriginalPath);
  net::test_server::ControllableHttpResponse first_redirect_response(
      embedded_test_server(), kFirstRedirectPath);
  net::test_server::ControllableHttpResponse second_redirect_response(
      embedded_test_server(), kSecondRedirectPath);
  EXPECT_TRUE(embedded_test_server()->Start());

  const GURL kOriginalURL =
      embedded_test_server()->GetURL("a.com", kOriginalPath);
  const GURL kFirstRedirectURL =
      embedded_test_server()->GetURL("b.com", kFirstRedirectPath);
  const GURL kSecondRedirectURL =
      embedded_test_server()->GetURL("c.com", kSecondRedirectPath);

  // First navigate to the initial URL. This page will have a cross-site
  // redirect.
  shell()->LoadURL(kOriginalURL);
  original_response1.WaitForRequest();
  original_response1.Send(
      "HTTP/1.1 302 FOUND\r\n"
      "Location: " +
      kFirstRedirectURL.spec() +
      "\r\n"
      "\r\n");
  original_response1.Done();
  first_redirect_response.WaitForRequest();
  first_redirect_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  first_redirect_response.Send(
      "<html>"
      "<body></body>"
      "</html>");
  first_redirect_response.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(kFirstRedirectURL, shell()->web_contents()->GetLastCommittedURL());

  // Now reload the original request, but redirect to yet another site.
  TestNavigationManager first_reload(shell()->web_contents(), kOriginalURL);
  shell()->web_contents()->GetController().Reload(
      ReloadType::ORIGINAL_REQUEST_URL, false);
  EXPECT_TRUE(first_reload.WaitForRequestStart());
  first_reload.ResumeNavigation();

  original_response2.WaitForRequest();
  original_response2.Send(
      "HTTP/1.1 302 FOUND\r\n"
      "Location: " +
      kSecondRedirectURL.spec() +
      "\r\n"
      "\r\n");
  original_response2.Done();
  second_redirect_response.WaitForRequest();
  second_redirect_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(first_reload.WaitForResponse());
  first_reload.ResumeNavigation();

  // The navigation is ready to commit: it has been handed to the speculative
  // RenderFrameHost for commit if Site Isolation is enabled, otherwise it
  // commits in the same RenderFrameHost.
  RenderFrameHostImpl* speculative_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->render_manager()
          ->speculative_frame_host();
  if (AreAllSitesIsolatedForTesting()) {
    CHECK(speculative_rfh);
  } else {
    CHECK(!speculative_rfh);
  }

  // The user requests a new reload while the previous reload hasn't committed
  // yet. The navigation start deletes the speculative RenderFrameHost that was
  // supposed to commit the browser-initiated navigation, unless Site Isolation
  // is enabled. This should not crash.
  TestNavigationManager second_reload(shell()->web_contents(), kOriginalURL);
  shell()->web_contents()->GetController().Reload(
      ReloadType::ORIGINAL_REQUEST_URL, false);
  EXPECT_TRUE(second_reload.WaitForRequestStart());
  speculative_rfh = static_cast<WebContentsImpl*>(shell()->web_contents())
                        ->GetFrameTree()
                        ->root()
                        ->render_manager()
                        ->speculative_frame_host();
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(speculative_rfh);
  } else {
    EXPECT_FALSE(speculative_rfh);
  }

  // The second reload results in a 204.
  second_reload.ResumeNavigation();
  original_response3.WaitForRequest();
  original_response3.Send(
      "HTTP/1.1 204 OK\r\n"
      "Connection: close\r\n"
      "\r\n");
  second_reload.WaitForNavigationFinished();
  speculative_rfh = static_cast<WebContentsImpl*>(shell()->web_contents())
                        ->GetFrameTree()
                        ->root()
                        ->render_manager()
                        ->speculative_frame_host();
  EXPECT_FALSE(speculative_rfh);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_DeleteSpeculativeRFHPendingCommitOfPendingEntryOnInterrupted2 \
  DISABLED_DeleteSpeculativeRFHPendingCommitOfPendingEntryOnInterrupted2
#else
#define MAYBE_DeleteSpeculativeRFHPendingCommitOfPendingEntryOnInterrupted2 \
  DeleteSpeculativeRFHPendingCommitOfPendingEntryOnInterrupted2
#endif
// Ensures that deleting a speculative RenderFrameHost trying to commit a
// navigation to the pending NavigationEntry will not crash if it happens
// because a new navigation to the same pending NavigationEntry started.  This
// is a variant of the previous test, where we destroy the speculative
// RenderFrameHost to create another speculative RenderFrameHost. This is a
// regression test for crbug.com/796135.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostManagerTest,
    MAYBE_DeleteSpeculativeRFHPendingCommitOfPendingEntryOnInterrupted2) {
  const std::string kOriginalPath = "/original.html";
  const std::string kRedirectPath = "/redirect.html";
  net::test_server::ControllableHttpResponse original_response1(
      embedded_test_server(), kOriginalPath);
  net::test_server::ControllableHttpResponse original_response2(
      embedded_test_server(), kOriginalPath);
  net::test_server::ControllableHttpResponse redirect_response(
      embedded_test_server(), kRedirectPath);
  EXPECT_TRUE(embedded_test_server()->Start());

  const GURL kOriginalURL =
      embedded_test_server()->GetURL("a.com", kOriginalPath);
  const GURL kRedirectURL =
      embedded_test_server()->GetURL("b.com", kRedirectPath);
  const GURL kCrossSiteURL =
      embedded_test_server()->GetURL("c.com", "/title1.html");

  const GURL kOriginalSiteURL = SiteInstance::GetSiteForURL(
      shell()->web_contents()->GetBrowserContext(), kOriginalURL);
  const GURL kRedirectSiteURL = SiteInstance::GetSiteForURL(
      shell()->web_contents()->GetBrowserContext(), kRedirectURL);

  // First navigate to the initial URL.
  shell()->LoadURL(kOriginalURL);
  original_response1.WaitForRequest();
  original_response1.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Cache-Control: no-cache, no-store, must-revalidate\r\n"
      "Pragma: no-cache\r\n"
      "\r\n");
  original_response1.Send(
      "<html>"
      "<body></body>"
      "</html>");
  original_response1.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(kOriginalURL, shell()->web_contents()->GetLastCommittedURL());

  // Navigate cross-site.
  EXPECT_TRUE(NavigateToURL(shell(), kCrossSiteURL));

  // Now go back to the original request, which will do a cross-site redirect.
  TestNavigationManager first_back(shell()->web_contents(), kOriginalURL);
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(first_back.WaitForRequestStart());
  first_back.ResumeNavigation();

  original_response2.WaitForRequest();
  original_response2.Send(
      "HTTP/1.1 302 FOUND\r\n"
      "Location: " +
      kRedirectURL.spec() +
      "\r\n"
      "\r\n");
  original_response2.Done();
  redirect_response.WaitForRequest();
  redirect_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(first_back.WaitForResponse());
  first_back.ResumeNavigation();

  // The navigation is ready to commit: it has been handed to the speculative
  // RenderFrameHost for commit.
  RenderFrameHostImpl* speculative_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->render_manager()
          ->speculative_frame_host();
  CHECK(speculative_rfh);
  EXPECT_TRUE(speculative_rfh->is_loading());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(kRedirectSiteURL,
              speculative_rfh->GetSiteInstance()->GetSiteURL());
  } else if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(kOriginalSiteURL,
              speculative_rfh->GetSiteInstance()->GetSiteURL());
  }
  int site_instance_id = speculative_rfh->GetSiteInstance()->GetId();

  // The user starts a navigation towards the redirected URL, for which we have
  // a speculative RenderFrameHost. This shouldn't delete the speculative
  // RenderFrameHost.
  TestNavigationManager navigation_to_redirect(shell()->web_contents(),
                                               kRedirectURL);
  shell()->LoadURL(kRedirectURL);
  EXPECT_TRUE(navigation_to_redirect.WaitForRequestStart());
  speculative_rfh = static_cast<WebContentsImpl*>(shell()->web_contents())
                        ->GetFrameTree()
                        ->root()
                        ->render_manager()
                        ->speculative_frame_host();
  CHECK(speculative_rfh);
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(kRedirectSiteURL,
              speculative_rfh->GetSiteInstance()->GetSiteURL());
    if (AreAllSitesIsolatedForTesting())
      EXPECT_EQ(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());
  }

  // The user requests to go back again while the previous back hasn't committed
  // yet. This should delete the speculative RenderFrameHost trying to commit
  // the back, and re-create a new speculative RenderFrameHost. This shouldn't
  // crash.
  TestNavigationManager second_back(shell()->web_contents(), kOriginalURL);
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(second_back.WaitForRequestStart());
  speculative_rfh = static_cast<WebContentsImpl*>(shell()->web_contents())
                        ->GetFrameTree()
                        ->root()
                        ->render_manager()
                        ->speculative_frame_host();
  CHECK(speculative_rfh);
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(kOriginalSiteURL,
              speculative_rfh->GetSiteInstance()->GetSiteURL());
  }
  if (AreAllSitesIsolatedForTesting())
    EXPECT_NE(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());
}

// Test for crbug.com/9682.  We should not show the URL for a pending renderer-
// initiated navigation in a new tab if it is not the initial navigation.  In
// this case, the renderer will not notify us of a modification, so we cannot
// show the pending URL without allowing a spoof.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontShowLoadingURLIfNotInitialNav) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/click-nocontent-link.html")));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  // By using an onclick handler that first creates the window, the slow
  // navigation is not considered an initial navigation.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(orig_contents,
                                  "window.domAutomationController.send("
                                  "clickNoContentScriptedTargetedLink());",
                                  &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Ensure the destination URL is not visible, because it is not the initial
  // navigation.
  WebContents* contents = new_shell->web_contents();
  EXPECT_FALSE(contents->GetController().IsInitialNavigation());
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_BackForwardNotStale DISABLED_BackForwardNotStale
#else
#define MAYBE_BackForwardNotStale BackForwardNotStale
#endif
// Test for http://crbug.com/93427.  Ensure that cross-site navigations
// do not cause back/forward navigations to be considered stale by the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, MAYBE_BackForwardNotStale) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Visit a page on first site.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Visit three pages on second site.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title2.html")));
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title3.html")));

  // History is now [blank, A1, B1, B2, *B3].
  WebContents* contents = shell()->web_contents();
  EXPECT_EQ(5, contents->GetController().GetEntryCount());

  // Open another window in same process to keep this process alive.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("foo.com", "/title1.html")));

  // Go back three times to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward twice to B2.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }

  // Go back twice to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward directly to B3.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToIndex(4);
    forward_nav_load_observer.Wait();
  }
}

// This class ensures that all the given RenderViewHosts have properly been
// shutdown.
class RenderViewHostDestructionObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostDestructionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderViewHostDestructionObserver() override {}
  void EnsureRVHGetsDestructed(RenderViewHost* rvh) {
    watched_render_view_hosts_.insert(rvh);
  }
  size_t GetNumberOfWatchedRenderViewHosts() const {
    return watched_render_view_hosts_.size();
  }

 private:
  // WebContentsObserver implementation:
  void RenderViewDeleted(RenderViewHost* rvh) override {
    watched_render_view_hosts_.erase(rvh);
  }

  std::set<RenderViewHost*> watched_render_view_hosts_;
};

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_LeakingRenderViewHosts DISABLED_LeakingRenderViewHosts
#else
#define MAYBE_LeakingRenderViewHosts LeakingRenderViewHosts
#endif
// Test for crbug.com/90867. Make sure we don't leak render view hosts since
// they may cause crashes or memory corruptions when trying to call dead
// delegate_. This test also verifies crbug.com/117420 and crbug.com/143255 to
// ensure that a separate SiteInstance is created when navigating to view-source
// URLs, regardless of current URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_LeakingRenderViewHosts) {
  StartEmbeddedServer();

  // Observe the created render_view_host's to make sure they will not leak.
  RenderViewHostDestructionObserver rvh_observers(shell()->web_contents());

  GURL navigated_url(embedded_test_server()->GetURL("/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       navigated_url.spec());

  // Let's ensure that when we start with a blank window, navigating away to a
  // view-source URL, we create a new SiteInstance.
  RenderViewHost* blank_rvh = shell()->web_contents()->GetRenderViewHost();
  SiteInstance* blank_site_instance = blank_rvh->GetSiteInstance();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), GURL::EmptyGURL());
  EXPECT_EQ(blank_site_instance->GetSiteURL(), GURL::EmptyGURL());
  rvh_observers.EnsureRVHGetsDestructed(blank_rvh);

  // Now navigate to the view-source URL and ensure we got a different
  // SiteInstance and RenderViewHost.
  EXPECT_TRUE(NavigateToURL(shell(), view_source_url));
  EXPECT_NE(blank_rvh, shell()->web_contents()->GetRenderViewHost());
  EXPECT_NE(blank_site_instance,
            shell()->web_contents()->GetRenderViewHost()->GetSiteInstance());
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  // Load a random page and then navigate to view-source: of it.
  // This used to cause two RVH instances for the same SiteInstance, which
  // was a problem.  This is no longer the case.
  EXPECT_TRUE(NavigateToURL(shell(), navigated_url));
  SiteInstance* site_instance1 =
      shell()->web_contents()->GetRenderViewHost()->GetSiteInstance();
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  EXPECT_TRUE(NavigateToURL(shell(), view_source_url));
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());
  SiteInstance* site_instance2 =
      shell()->web_contents()->GetRenderViewHost()->GetSiteInstance();

  // Ensure that view-source navigations force a new SiteInstance.
  EXPECT_NE(site_instance1, site_instance2);

  // Now navigate to a different instance so that we swap out again.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title2.html")));
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  // This used to leak a render view host.
  shell()->Close();

  RunAllPendingInMessageLoop();  // Needed on ChromeOS.

  EXPECT_EQ(0U, rvh_observers.GetNumberOfWatchedRenderViewHosts());
}

// Test for crbug.com/143155.  Frame tree updates during unload should not
// interrupt the intended navigation.
// Specifically:
// 1) Open 2 tabs in an HTTP SiteInstance, with a subframe in the opener.
// 2) Send the second tab to a different foo.com SiteInstance.
//    This created a swapped out opener for the first tab in the foo process.
// 3) Navigate the first tab to the foo.com SiteInstance, and have the first
//    tab's unload handler remove its frame.
// In older versions of Chrome, this caused an update to the frame tree that
// resulted in showing an internal page rather than the real page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontPreemptNavigationWithFrameTreeUpdate) {
  StartEmbeddedServer();

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed to get a non-default
    // SiteInstance for navigations to this origin.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  // 1. Load a page that deletes its iframe during unload.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/remove_frame_on_unload.html")));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Open a same-site page in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(openWindow());", &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/title1.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance.get(),
            new_shell->web_contents()->GetSiteInstance());

  // 2. Send the second tab to a different process.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, cross_site_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // 3. Send the first tab to the second tab's process.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(shell(), cross_site_url));

  // Make sure it ends up at the right page.
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(cross_site_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(new_site_instance, shell()->web_contents()->GetSiteInstance());
}

// Ensure that renderer-side debug URLs do not cause a process swap, since they
// are meant to run in the current page.  We had a bug where we expected a
// BrowsingInstance swap to occur on pages like view-source and extensions,
// which broke chrome://crash and javascript: URLs.
// See http://crbug.com/335503.
// The test fails on Mac OSX with ASAN.
// See http://crbug.com/699062.
#if defined(OS_MACOSX) && defined(THREAD_SANITIZER)
#define MAYBE_RendererDebugURLsDontSwap DISABLED_RendererDebugURLsDontSwap
#else
#define MAYBE_RendererDebugURLsDontSwap RendererDebugURLsDontSwap
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_RendererDebugURLsDontSwap) {
  StartEmbeddedServer();

  GURL original_url(embedded_test_server()->GetURL("/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       original_url.spec());

  EXPECT_TRUE(NavigateToURL(shell(), view_source_url));

  // Check that javascript: URLs work.
  base::string16 expected_title = ASCIIToUTF16("msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  shell()->LoadURL(GURL("javascript:document.title='msg'"));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Crash the renderer of the view-source page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();
}

// Ensure that renderer-side debug URLs don't take effect on crashed renderers.
// Otherwise, we might try to load an unprivileged about:blank page into a
// WebUI-enabled RenderProcessHost, failing a safety check in InitRenderView.
// See http://crbug.com/334214.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       IgnoreRendererDebugURLsWhenCrashed) {
  // Visit a WebUI page with bindings.
  GURL webui_url = GURL(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));

  // Crash the renderer of the WebUI page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();

  // Load the crash URL again but don't wait for any action.  If it is not
  // ignored this time, we will fail the WebUI CHECK in InitRenderView.
  shell()->LoadURL(GURL(kChromeUICrashURL));

  // Ensure that such URLs can still work as the initial navigation of a tab.
  // We postpone the initial navigation of the tab using an empty GURL, so that
  // we can add a watcher for crashes.
  Shell* shell2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());
  RenderProcessHostWatcher crash_observer2(
      shell2->web_contents(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell2, GURL(kChromeUIKillURL)));
  crash_observer2.Wait();
}

class RFHMProcessPerTabTest : public RenderFrameHostManagerTest {
 public:
  RFHMProcessPerTabTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kProcessPerTab);
  }
};

// Test that we still swap processes for BrowsingInstance changes even in
// --process-per-tab mode.  See http://crbug.com/343017.
// Disabled on Android: http://crbug.com/345873.
// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(OS_ANDROID) || defined(THREAD_SANITIZER)
#define MAYBE_BackFromWebUI DISABLED_BackFromWebUI
#else
#define MAYBE_BackFromWebUI BackFromWebUI
#endif
IN_PROC_BROWSER_TEST_F(RFHMProcessPerTabTest, MAYBE_BackFromWebUI) {
  StartEmbeddedServer();
  GURL original_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), original_url));

  // Visit a WebUI page with bindings.
  GURL webui_url(GURL(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost)));
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));

  // Go back and ensure we have no WebUI bindings.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
}

// crbug.com/372360
// The test loads url1, opens a link pointing to url2 in a new tab, and
// navigates the new tab to url1.
// The following is needed for the bug to happen:
//  - url1 must require webui bindings;
//  - navigating to url2 in the site instance of url1 should not swap
//   browsing instances, but should require a new site instance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, WebUIGetsBindings) {
  GURL url1(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIGpuHost));
  GURL url2(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIHistogramHost));

  // Visit a WebUI page with bindings.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  SiteInstance* site_instance1 = shell()->web_contents()->GetSiteInstance();
  int process1_id = site_instance1->GetProcess()->GetID();

  // Open a new tab. Initially it gets a render view in the original tab's
  // current site instance.
  TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  ShellAddedObserver shao;
  OpenUrlViaClickTarget(shell(), url2);
  nav_observer.Wait();
  Shell* new_shell = shao.GetShell();
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  SiteInstance* site_instance2 = new_web_contents->GetSiteInstance();
  int process2_id = site_instance2->GetProcess()->GetID();

  // The 2nd WebUI page should swap to a different process (and SiteInstance),
  // but should stay in the same BrowsingInstance as the 1st WebUI page.
  EXPECT_NE(process1_id, process2_id);
  EXPECT_NE(site_instance2, site_instance1);
  EXPECT_TRUE(site_instance2->IsRelatedSiteInstance(site_instance1));

  RenderViewHost* initial_rvh =
      new_web_contents->GetRenderManagerForTesting()
          ->GetSwappedOutRenderViewHost(site_instance1);
  ASSERT_TRUE(initial_rvh);

  // Navigate to url1 and check bindings.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, url1));
  // The navigation should have used the first SiteInstance, otherwise
  // |initial_rvh| did not have a chance to be used.
  EXPECT_EQ(new_web_contents->GetSiteInstance(), site_instance1);
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            new_web_contents->GetMainFrame()->GetEnabledBindings());
}

// crbug.com/424526
// The test loads a WebUI page in process-per-tab mode, then navigates to a
// blank page and then to a regular page. The bug reproduces if blank page is
// visited in between WebUI and regular page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ForceSwapAfterWebUIBindings) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  StartEmbeddedServer();

  const GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating to about:blank to ensure
  // it doesn't change.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_NE(orig_site_instance, shell()->web_contents()->GetSiteInstance());

  GURL regular_page_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), regular_page_url));
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
}

// crbug.com/615274
// This test ensures that after an RFH is swapped out, the associated WebUI
// instance is no longer allowed to send JavaScript messages. This is necessary
// because WebUI currently (and unusually) always sends JavaScript messages to
// the current main frame, rather than the RFH that owns the WebUI.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       WebUIJavascriptDisallowedAfterSwapOut) {
  StartEmbeddedServer();

  const GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));

  RenderFrameHostImpl* rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())->GetMainFrame();

  // Set up a slow unload handler to force the RFH to linger in the swapped
  // out but not-yet-deleted state.
  EXPECT_TRUE(
      ExecuteScript(rfh, "window.onunload=function(e){ while(1); };\n"));

  WebUIImpl* web_ui = rfh->web_ui();

  EXPECT_TRUE(web_ui->CanCallJavascript());
  auto handler_owner = std::make_unique<TestWebUIMessageHandler>();
  TestWebUIMessageHandler* handler = handler_owner.get();

  web_ui->AddMessageHandler(std::move(handler_owner));
  EXPECT_FALSE(handler->IsJavascriptAllowed());

  handler->AllowJavascript();
  EXPECT_TRUE(handler->IsJavascriptAllowed());

  rfh->DisableSwapOutTimerForTesting();
  RenderFrameHostDestructionObserver rfh_observer(rfh);

  // Navigate, but wait for commit, not the actual load to finish.
  SiteInstanceImpl* web_ui_site_instance = rfh->GetSiteInstance();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(GURL(url::kAboutBlankURL));
  commit_observer.WaitForCommit();
  EXPECT_NE(web_ui_site_instance, shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(
      root->render_manager()->GetRenderFrameProxyHost(web_ui_site_instance));

  // The previous RFH should still be pending deletion, as we wait for either
  // the SwapOut ACK or a timeout.
  ASSERT_TRUE(rfh->IsRenderFrameLive());
  ASSERT_FALSE(rfh->is_active());

  // We specifically want verify behavior between swap-out and RFH destruction.
  ASSERT_FALSE(rfh_observer.deleted());

  EXPECT_FALSE(handler->IsJavascriptAllowed());
}

// Test for http://crbug.com/703303.  Ensures that the renderer process does not
// try to select files whose paths cannot be converted to WebStrings.  This
// check is done in the renderer because it is hard to predict which paths will
// turn into empty WebStrings, and the behavior varies by platform.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, DontSelectInvalidFiles) {
  StartServer();
  base::RunLoop run_loop;

  // Use a file path with an invalid encoding, such that it can't be converted
  // to a WebString (on all platforms but Windows).
  base::FilePath file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file));
  file = file.Append(FILE_PATH_LITERAL("foo\337bar"));

  // Navigate and try to get page to reference this file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  int process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file, run_loop.QuitClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('fileinput').click();"));
  run_loop.Run();

  // The browser process grants access to the file whether or not the renderer
  // process realizes that it can't use it.  This is ok, since the user actually
  // did select the file from the chooser.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Disable the swap out timer so we wait for the UpdateState message.
  static_cast<WebContentsImpl*>(shell()->web_contents())
      ->GetMainFrame()
      ->DisableSwapOutTimerForTesting();

  // Navigate to a different process and wait for the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), GetCrossSiteURL("/title1.html")));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(), file));

  // The renderer process should not have been killed.  This is the important
  // part of the test.  If this fails, then we didn't get a PageState to check
  // below, so use an assert (since the test can't meaningfully proceed).
  ASSERT_TRUE(exit_observer.did_exit_normally());

  // Ensure that the file did not end up in the PageState of the previous entry,
  // except on Windows where the path is valid and WebString can handle it.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
#if defined(OS_WIN)
  EXPECT_EQ(1U, files.size());
#else
  EXPECT_EQ(0U, files.size());
#endif
}

// Test for http://crbug.com/262948.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RestoreFileAccessForHistoryNavigation) {
  StartServer();
  base::RunLoop run_loop;
  base::FilePath file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  int process_id = wc->GetMainFrame()->GetProcess()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file, run_loop.QuitClosure()));
  wc->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('fileinput').click();"));
  run_loop.Run();
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Disable the swap out timer so we wait for the UpdateState message.
  wc->GetMainFrame()->DisableSwapOutTimerForTesting();

  // Navigate to a different process without access to the file, and wait for
  // the old process to exit.
  RenderProcessHostWatcher exit_observer(
      wc->GetMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), GetCrossSiteURL("/title1.html")));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      wc->GetMainFrame()->GetProcess()->GetID(), file));

  // Ensure that the file ended up in the PageState of the previous entry.
  NavigationEntry* prev_entry = wc->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, files.size());
  EXPECT_EQ(file, files.at(0));

  // Go back, ending up in a different RenderProcessHost than before.
  TestNavigationObserver back_nav_load_observer(wc);
  wc->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_NE(process_id, wc->GetMainFrame()->GetProcess()->GetID());

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      wc->GetMainFrame()->GetProcess()->GetID(), file));

  // Navigate to a same site page to trigger a PageState update and ensure the
  // renderer is not killed.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
}

// Same as RenderFrameHostManagerTest.RestoreFileAccessForHistoryNavigation, but
// replace the cross-origin navigation by a crash, followed by a reload.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RestoreFileAccessForHistoryNavigationAfterCrash) {
  StartServer();
  base::RunLoop run_loop;
  base::FilePath file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  int process_id = wc->GetMainFrame()->GetProcess()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file, run_loop.QuitClosure()));
  wc->SetDelegate(delegate.get());
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('fileinput').click();"));
  run_loop.Run();
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // The PageState hasn't been updated yet. It requires a navigation.
  {
    NavigationEntry* prev_entry = wc->GetController().GetEntryAtIndex(0);
    EXPECT_EQ(url1, prev_entry->GetURL());
    const std::vector<base::FilePath>& files =
        prev_entry->GetPageState().GetReferencedFiles();
    ASSERT_EQ(0U, files.size());
  }

  // Same-document navigation
  EXPECT_TRUE(ExecJs(shell(), "history.pushState({},'title','#foo')"));

  // The PageState has been updated, it now contains the file.
  {
    NavigationEntry* prev_entry = wc->GetController().GetEntryAtIndex(0);
    EXPECT_EQ(url1, prev_entry->GetURL());
    const std::vector<base::FilePath>& files =
        prev_entry->GetPageState().GetReferencedFiles();
    ASSERT_EQ(1U, files.size());
    EXPECT_EQ(file, files.at(0));
  }

  // Crash.
  {
    RenderProcessHost* process = wc->GetMainFrame()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(0);
    crash_observer.Wait();
  }

  // The renderer process is still allowed to read the file, even if it is
  // crashed.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      wc->GetMainFrame()->GetProcess()->GetID(), file));

  // Reload
  wc->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(wc));

  // After recovering from the crash, the renderer process is allowed to read
  // the file.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      wc->GetMainFrame()->GetProcess()->GetID(), file));

  // Same-document history back navigation.
  {
    TestNavigationObserver back_nav_load_observer(wc);
    wc->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      wc->GetMainFrame()->GetProcess()->GetID(), file));

  // Navigate to a same site page to trigger a PageState update and ensure the
  // renderer is not killed.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
}

// Test for http://crbug.com/441966.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RestoreSubframeFileAccessForHistoryNavigation) {
  StartServer();
  base::RunLoop run_loop;
  base::FilePath file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input_subframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  int process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file, run_loop.QuitClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "document.getElementById('fileinput').click();"));
  run_loop.Run();
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Disable the swap out timer so we wait for the UpdateState message.
  root->current_frame_host()->DisableSwapOutTimerForTesting();

  // Do an in-page navigation in the child to make sure we hear a PageState with
  // the chosen file before the subframe's FrameTreeNode is deleted.  In
  // practice, we'll get the PageState 1 second after the file is chosen.
  // TODO(creis): Remove this in-page navigation once we keep track of
  // FrameTreeNodes that are pending deletion.  See https://crbug.com/609963.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    std::string script = "location.href='#foo';";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    nav_observer.Wait();
  }

  // Navigate to a different process without access to the file, and wait for
  // the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(NavigateToURL(shell(), GetCrossSiteURL("/title1.html")));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(), file));

  // Ensure that the file ended up in the PageState of the previous entry.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, files.size());
  EXPECT_EQ(file, files.at(0));

  // Go back, ending up in a different RenderProcessHost than before.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoToIndex(0);
  back_nav_load_observer.Wait();
  EXPECT_NE(process_id,
            shell()->web_contents()->GetMainFrame()->GetProcess()->GetID());

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(), file));

  // Do another in-page navigation in the child to make sure we hear a PageState
  // with the chosen file.
  // TODO(creis): Remove this in-page navigation once we keep track of
  // FrameTreeNodes that are pending deletion.  See https://crbug.com/609963.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    std::string script = "location.href='#foo';";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    nav_observer.Wait();
  }

  // Also try cloning the tab by creating a new NavigationEntry with the same
  // PageState.  This exercises a different path, by combining the frame
  // specific PageStates into a full-tree PageState and converting back.  There
  // was a bug where this caused us to lose the list of referenced files.  See
  // https://crbug.com/620261.
  std::unique_ptr<NavigationEntryImpl> cloned_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url1, Referrer(), base::nullopt, ui::PAGE_TRANSITION_RELOAD,
              false, std::string(),
              shell()->web_contents()->GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  prev_entry = shell()->web_contents()->GetController().GetEntryAtIndex(0);
  cloned_entry->SetPageState(prev_entry->GetPageState());
  const std::vector<base::FilePath>& cloned_files =
      cloned_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, cloned_files.size());
  EXPECT_EQ(file, cloned_files.at(0));

  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(cloned_entry));
  Shell* new_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1,
                         RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(url1, new_root->current_url());

  // Ensure that the file access exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      new_root->current_frame_host()->GetProcess()->GetID(), file));

  // Also, extract the file from the renderer process to ensure that the
  // response made it over successfully and the proper filename is set.
  std::string file_name;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_root->child_at(0),
      "window.domAutomationController.send("
      "document.getElementById('fileinput').files[0].name);",
      &file_name));
  EXPECT_EQ("bar", file_name);

  // Navigate to a same site page to trigger a PageState update and ensure the
  // renderer is not killed.
  EXPECT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title2.html")));
}

// Ensures that no RenderFrameHost/RenderViewHost objects are leaked when
// doing a simple cross-process navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CleanupOnCrossProcessNavigation) {
  StartEmbeddedServer();

  // Do an initial navigation and capture objects we expect to be cleaned up
  // on cross-process navigation.
  GURL start_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  int32_t orig_site_instance_id =
      root->current_frame_host()->GetSiteInstance()->GetId();
  int initial_process_id =
      root->current_frame_host()->GetSiteInstance()->GetProcess()->GetID();
  int initial_rfh_id = root->current_frame_host()->GetRoutingID();
  int initial_rvh_id =
      root->current_frame_host()->render_view_host()->GetRoutingID();

  // Navigate cross-process and ensure that cleanup is performed as expected.
  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/title2.html");
  RenderFrameHostDestructionObserver rfh_observer(root->current_frame_host());
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  rfh_observer.Wait();

  EXPECT_NE(orig_site_instance_id,
            root->current_frame_host()->GetSiteInstance()->GetId());
  EXPECT_FALSE(RenderFrameHost::FromID(initial_process_id, initial_rfh_id));
  EXPECT_FALSE(RenderViewHost::FromID(initial_process_id, initial_rvh_id));
}

// Ensure that the opener chain proxies and RVHs are properly reinitialized if
// a tab crashes and reloads.  See https://crbug.com/505090.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ReinitializeOpenerChainAfterCrashAndReload) {
  StartEmbeddedServer();

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed to get a non-default
    // SiteInstance for navigations to this origin.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance);

  // Open a popup and navigate it cross-site.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(new_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();

  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, cross_site_url));

  scoped_refptr<SiteInstance> foo_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(foo_site_instance, orig_site_instance);

  // Kill the popup's process.
  RenderProcessHost* popup_process =
      popup_root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      popup_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  popup_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(
      popup_root->current_frame_host()->render_view_host()->IsRenderViewLive());

  // The swapped-out RVH and proxy for the opener page in the foo.com
  // SiteInstance should not be live.
  RenderFrameHostManager* opener_manager = root->render_manager();
  RenderViewHostImpl* opener_rvh =
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get());
  EXPECT_TRUE(opener_rvh);
  EXPECT_FALSE(opener_rvh->IsRenderViewLive());
  RenderFrameProxyHost* opener_rfph =
      opener_manager->GetRenderFrameProxyHost(foo_site_instance.get());
  EXPECT_TRUE(opener_rfph);
  EXPECT_FALSE(opener_rfph->is_render_frame_proxy_live());

  // Re-navigate the popup to the same URL and check that this recreates the
  // opener's swapped out RVH and proxy in the foo.com SiteInstance.
  EXPECT_TRUE(NavigateToURL(new_shell, cross_site_url));
  EXPECT_TRUE(opener_rvh->IsRenderViewLive());
  EXPECT_TRUE(opener_rfph->is_render_frame_proxy_live());
}

// Test that when a frame's opener is updated via window.open, the browser
// process and the frame's proxies in other processes find out about the new
// opener.  Open two popups in different processes, set one popup's opener to
// the other popup, and ensure that the opener is updated in all processes.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, UpdateOpener) {
  StartEmbeddedServer();
  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed it is placed in a different
    // process.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  GURL main_url = embedded_test_server()->GetURL("/post_message.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance);

  // Open a cross-site popup named "foo" and a same-site popup named "bar".
  Shell* foo_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(foo_shell, foo_url));

  GURL bar_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_post_message_frames.html"));
  Shell* bar_shell = OpenPopup(shell(), bar_url, "bar");
  EXPECT_TRUE(bar_shell);

  EXPECT_NE(shell()->web_contents()->GetSiteInstance()->GetProcess(),
            foo_shell->web_contents()->GetSiteInstance()->GetProcess());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance()->GetProcess(),
            bar_shell->web_contents()->GetSiteInstance()->GetProcess());

  // Initially, both popups' openers should point to main window.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();
  FrameTreeNode* bar_root =
      static_cast<WebContentsImpl*>(bar_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(root, foo_root->opener());
  EXPECT_EQ(root, foo_root->original_opener());
  EXPECT_EQ(root, bar_root->opener());
  EXPECT_EQ(root, bar_root->original_opener());

  // From the bar process, use window.open to update foo's opener to point to
  // bar. This is allowed since bar is same-origin with foo's opener.  Use
  // window.open with an empty URL, which should return a reference to the
  // target frame without navigating it.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      bar_shell,
      "window.domAutomationController.send(!!window.open('','foo'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(foo_shell->web_contents()->IsLoading());
  EXPECT_EQ(foo_url, foo_root->current_url());

  // Check that updated opener propagated to the browser process.
  EXPECT_EQ(bar_root, foo_root->opener());
  EXPECT_EQ(root, foo_root->original_opener());

  // Check that foo's opener was updated in foo's process. Send a postMessage
  // to the opener and check that the right window (bar_shell) receives it.
  base::string16 expected_title = ASCIIToUTF16("opener-msg");
  TitleWatcher title_watcher(bar_shell->web_contents(), expected_title);
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_shell,
      "window.domAutomationController.send(postToOpener('opener-msg', '*'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that a non-null assignment to the opener doesn't change the opener
  // in the browser process.
  EXPECT_TRUE(ExecuteScript(foo_shell, "window.opener = window;"));
  EXPECT_EQ(bar_root, foo_root->opener());
  EXPECT_EQ(root, foo_root->original_opener());
}

// Tests that when a popup is opened, which is then navigated cross-process and
// back, it can be still accessed through the original window reference in
// JavaScript. See https://crbug.com/537657
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PopupKeepsWindowReferenceCrossProcesAndBack) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Click a target=foo link to open a popup.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Wait for the navigation in the popup to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Capture the window reference, so we can check that accessing its location
  // works after navigating cross-process and back.
  GURL expected_url = new_shell->web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(ExecuteScript(shell(), "saveWindowReference();"));

  // Now navigate the popup to a different site and then go back.
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("foo.com", "/title1.html")));
  TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
  new_shell->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();

  // Check that the location.href window attribute is accessible and is correct.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send(getLastOpenedWindowLocation());",
      &result));
  EXPECT_EQ(expected_url.spec(), result);
}

// Tests that going back to the same SiteInstance as a pending RenderFrameHost
// doesn't create a duplicate RenderFrameProxyHost. For example:
// 1. Navigate to a page on the opener site - a.com
// 2. Navigate to a page on site b.com
// 3. Start a navigation to another page on a.com, but commit is delayed.
// 4. Go back.
// See https://crbug.com/541619.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PopupPendingAndBackToSameSiteInstance) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Open a popup to navigate.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the popup to a different site.
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("b.com", "/title2.html")));

  // Navigate again to the original site, but to a page that will take a while
  // to commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  TestNavigationManager stalled_navigation(new_shell->web_contents(),
                                           same_site_url);
  new_shell->LoadURL(same_site_url);
  EXPECT_TRUE(stalled_navigation.WaitForRequestStart());

  // Going back in history should work and the test should not crash.
  TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
  new_shell->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
}

// Tests that InputMsg type IPCs are ignored by swapped out RenderViews. It
// uses the SetFocus IPC, as RenderView has a CHECK to ensure that condition
// never happens.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       InputMsgToSwappedOutRVHIsIgnored) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup to navigate cross-process.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Keep a pointer to the RenderViewHost, which will be in swapped out
  // state after navigating cross-process. This is how this test is causing
  // a swapped out RenderView to receive InputMsg IPC message.
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  FrameTreeNode* new_root = new_web_contents->GetFrameTree()->root();
  RenderViewHostImpl* rvh = new_web_contents->GetRenderViewHost();

  // Navigate the popup to a different site, so the |rvh| is swapped out.
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("b.com", "/title2.html")));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(rvh, new_root->render_manager()->GetSwappedOutRenderViewHost(
                     shell()->web_contents()->GetSiteInstance()));

  // Setup a process observer to ensure there is no crash and send the IPC
  // message.
  RenderProcessHostWatcher watcher(
      rvh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rvh->GetWidget()->GetWidgetInputHandler()->SetFocus(true);

  // The test must wait for a process to exit, but if the IPC message is
  // properly ignored, there will be no crash. Therefore, navigate the
  // original window to the same site as the popup, which will just exit the
  // process cleanly.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

// Tests that navigating cross-process and reusing an existing RenderViewHost
// (whose process has been killed/crashed) recreates properly the RenderView and
// RenderFrameProxy on the renderer side.
// See https://crbug.com/544271
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RenderViewInitAfterProcessKill) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup to navigate.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the popup to a different site.
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("b.com", "/title2.html")));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Kill the process hosting the popup.
  RenderProcessHost* process = popup_root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(
      popup_root->current_frame_host()->render_view_host()->IsRenderViewLive());

  // Navigate the main tab to the site of the popup. This will cause the
  // RenderView for b.com in the main tab to be recreated. If the issue
  // is not fixed, this will result in process crash and failing test.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));
}

// Ensure that we don't crash the renderer in CreateRenderView if a proxy goes
// away between swapout and the next navigation.  See https://crbug.com/581912.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CreateRenderViewAfterProcessKillAndClosedProxy) {
  StartEmbeddedServer();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Give an initial page an unload handler that never completes.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(
      ExecuteScript(root, "window.onunload=function(e){ while(1); };\n"));

  // Open a popup in the same process.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the first tab to a different site, and only wait for commit, not
  // load stop.
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();
  SiteInstanceImpl* site_instance_a = rfh_a->GetSiteInstance();
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title2.html"));
  commit_observer.WaitForCommit();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // The previous RFH should still be pending deletion, as we wait for either
  // the SwapOut ACK or a timeout.
  ASSERT_TRUE(rfh_a->IsRenderFrameLive());
  ASSERT_FALSE(rfh_a->is_active());

  // The corresponding RVH should still be referenced by the proxy and the old
  // frame.
  RenderViewHostImpl* rvh_a = rfh_a->render_view_host();
  EXPECT_FALSE(rvh_a->HasOneRef());
  EXPECT_TRUE(rvh_a->HasAtLeastOneRef());

  // Kill the old process.
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  // |rfh_a| is now deleted, thanks to the bug fix.

  // With |rfh_a| gone, the RVH should only be referenced by the (dead) proxy.
  EXPECT_TRUE(rvh_a->HasOneRef());
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));
  EXPECT_FALSE(root->render_manager()
                   ->GetRenderFrameProxyHost(site_instance_a)
                   ->is_render_frame_proxy_live());

  // Close the popup so there is no proxy for a.com in the original tab.
  new_shell->Close();
  EXPECT_FALSE(
      root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // This should delete the RVH as well.
  EXPECT_FALSE(root->frame_tree()->GetRenderViewHost(site_instance_a));

  // Go back in the main frame from b.com to a.com. In https://crbug.com/581912,
  // the browser process would crash here because there was no main frame
  // routing ID or proxy in RVHI::CreateRenderView.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
}

// Ensure that we don't crash in RenderViewImpl::Init if a proxy is created
// after swapout and before navigation.  See https://crbug.com/544755.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RenderViewInitAfterNewProxyAndProcessKill) {
  StartEmbeddedServer();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Give an initial page an unload handler that never completes.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(
      ExecuteScript(root, "window.onunload=function(e){ while(1); };\n"));

  // Navigate the tab to a different site, and only wait for commit, not load
  // stop.
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();
  SiteInstanceImpl* site_instance_a = rfh_a->GetSiteInstance();
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title2.html"));
  commit_observer.WaitForCommit();
  EXPECT_NE(site_instance_a, shell()->web_contents()->GetSiteInstance());

  // The previous RFH should still be pending deletion, as we wait for either
  // the SwapOut ACK or a timeout.
  ASSERT_TRUE(rfh_a->IsRenderFrameLive());
  ASSERT_FALSE(rfh_a->is_active());

  // When the previous RFH was swapped out, it should have still gotten a
  // replacement proxy even though it's the last active frame in the process.
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Open a popup in the new B process.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the popup to the original site, but don't wait for commit (which
  // won't happen).  This should reuse the proxy in the original tab, which at
  // this point exists alongside the RFH pending deletion.
  new_shell->LoadURL(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Kill the old process.
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  // |rfh_a| is now deleted, thanks to the bug fix.

  // Go back in the main frame from b.com to a.com.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // In https://crbug.com/581912, the renderer process would crash here because
  // there was a frame, view, and proxy (and is_swapped_out was true).
  EXPECT_EQ(site_instance_a, root->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(new_shell->web_contents()->GetMainFrame()->IsRenderFrameLive());
}

// Ensure that we use the same pending RenderFrameHost if a second navigation to
// its site occurs before it commits.  Otherwise the renderer process will have
// two competing pending RenderFrames that both try to swap with the same
// RenderFrameProxy.  See https://crbug.com/545900.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ConsecutiveNavigationsToSite) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to b.com to keep the b.com process alive.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "popup");
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("b.com", "/title3.html")));

  // Start a cross-site navigation to the same site but don't commit.
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager stalled_navigation(shell()->web_contents(),
                                           cross_site_url);
  shell()->LoadURL(cross_site_url);
  EXPECT_TRUE(stalled_navigation.WaitForResponse());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* next_rfh =
      web_contents->GetRenderManagerForTesting()->speculative_frame_host();
  ASSERT_TRUE(next_rfh);

  // Navigate to the same new site and verify that we commit in the same RFH.
  GURL cross_site_url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationObserver navigation_observer(web_contents, 1);
  shell()->LoadURL(cross_site_url2);
  EXPECT_EQ(
      next_rfh,
      web_contents->GetRenderManagerForTesting()->speculative_frame_host());
  navigation_observer.Wait();
  EXPECT_EQ(cross_site_url2, web_contents->GetLastCommittedURL());
  EXPECT_EQ(next_rfh, web_contents->GetMainFrame());
  EXPECT_FALSE(
      web_contents->GetRenderManagerForTesting()->speculative_frame_host());
}

// Check that if a sandboxed subframe opens a cross-process popup such that the
// popup's opener won't be set, the popup still inherits the subframe's sandbox
// flags.  This matters for rel=noopener and rel=noreferrer links, as well as
// for some situations in non-site-per-process mode where the popup would
// normally maintain the opener, but loses it due to being placed in a new
// process and not creating subframe proxies.  The latter might happen when
// opening the default search provider site.  See https://crbug.com/576204.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CrossProcessPopupInheritsSandboxFlagsWithNoOpener) {
  StartEmbeddedServer();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Add a sandboxed about:blank iframe.
  {
    std::string script =
        "var frame = document.createElement('iframe');\n"
        "frame.sandbox = 'allow-scripts allow-popups';\n"
        "document.body.appendChild(frame);\n";
    EXPECT_TRUE(ExecuteScript(shell(), script));
  }

  // Navigate iframe to a page with target=_blank links, and rewrite the links
  // to point to valid cross-site URLs.
  GURL frame_url(
      embedded_test_server()->GetURL("a.com", "/click-noreferrer-links.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  std::string script = "setOriginForLinks('http://b.com:" +
                       embedded_test_server()->base_url().port() + "/');";
  EXPECT_TRUE(ExecuteScript(root->child_at(0), script));

  // Helper to click on the 'rel=noreferrer target=_blank' and 'rel=noopener
  // target=_blank' links.  Checks that these links open a popup that ends up
  // in a new SiteInstance even without site-per-process and then verifies that
  // the popup is still sandboxed.
  auto click_link_and_verify_popup = [this,
                                      root](std::string link_opening_script) {
    ShellAddedObserver new_shell_observer;
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        root->child_at(0),
        "window.domAutomationController.send(" + link_opening_script + ")",
        &success));
    EXPECT_TRUE(success);

    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
    EXPECT_NE(new_shell->web_contents()->GetSiteInstance(),
              shell()->web_contents()->GetSiteInstance());

    // Check that the popup is sandboxed by checking its self.origin, which
    // should be unique.
    std::string origin;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        new_shell, "domAutomationController.send(self.origin)", &origin));
    EXPECT_EQ("null", origin);
  };

  click_link_and_verify_popup("clickNoOpenerTargetBlankLink()");
  click_link_and_verify_popup("clickNoRefTargetBlankLink()");
}

// When two frames are same-origin but cross-process, they should behave as if
// they are not same-origin and should not crash.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SameOriginFramesInDifferentProcesses) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("a.com", "/click-noreferrer-links.html")));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(nullptr, orig_site_instance.get());

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());"
      "saveWindowReference();",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Do a cross-site navigation that winds up same-site. The same-site
  // navigation to a.com will commit in a different process than the original
  // a.com window.
  EXPECT_TRUE(NavigateToURL(
      new_shell,
      embedded_test_server()->GetURL("b.com", "/cross-site/a.com/title1.html"),
      /* expected_commit_url */
      embedded_test_server()->GetURL("a.com", "/title1.html")));
  // The SiteInstance for the navigation is determined after the redirect.
  // So both windows will actually be in the same process.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send((function() {\n"
      "  try {\n"
      "    return getLastOpenedWindowLocation();\n"
      "  } catch (e) {\n"
      "    return e.toString();\n"
      "  }\n"
      "})())",
      &result));
  EXPECT_THAT(result, ::testing::MatchesRegex("http://a.com:\\d+/title1.html"));
}

// Test coverage for attempts to open subframe links in new windows, to prevent
// incorrect invariant checks.  See https://crbug.com/605055.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, CtrlClickSubframeLink) {
  StartEmbeddedServer();

  // Load a page with a subframe link.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/ctrl-click-subframe-link.html")));

  // Simulate a ctrl click on the link.  This won't actually create a new Shell
  // because Shell::OpenURLFromTab only supports CURRENT_TAB, but it's enough to
  // trigger the crash from https://crbug.com/605055.
  EXPECT_TRUE(ExecuteScript(
      shell(), "window.domAutomationController.send(ctrlClickLink());"));
}

// Ensure that we don't update the wrong NavigationEntry's title after an
// ignored commit during a cross-process navigation.
// See https://crbug.com/577449.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       UnloadPushStateOnCrossProcessNavigation) {
  shell()
      ->web_contents()
      ->GetController()
      .GetBackForwardCache()
      .DisableForTesting(content::BackForwardCache::TEST_USES_UNLOAD_EVENT);

  StartEmbeddedServer();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // Give an initial page an unload handler that does a pushState, which will be
  // ignored by the browser process.  It then does a title update which is
  // meant for a NavigationEntry that will never be created.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  EXPECT_TRUE(ExecuteScript(root,
                            "window.onunload=function(e){"
                            "history.pushState({}, 'foo', 'foo');"
                            "document.title='foo'; };\n"));
  base::string16 title = web_contents->GetTitle();
  NavigationEntryImpl* entry = web_contents->GetController().GetEntryAtIndex(0);

  // Navigate the first tab to a different site and wait for the old process to
  // complete its unload handler and exit.
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();
  RenderProcessHostWatcher exit_observer(
      rfh_a->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  TestNavigationObserver commit_observer(web_contents);
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  commit_observer.Wait();
  exit_observer.Wait();

  // Ensure the entry's title hasn't changed after the ignored commit.
  EXPECT_EQ(title, entry->GetTitle());
}

// Ensure that document hosted on file: URL can successfully execute pushState
// with arbitrary origin, when universal access setting is enabled.
// TODO(nasko): The test is disabled on Mac, since universal access from file
// scheme behaves differently.  See also https://crbug.com/981018.
#if defined(OS_MACOSX)
#define MAYBE_EnsureUniversalAccessFromFileSchemeSucceeds \
  DISABLED_EnsureUniversalAccessFromFileSchemeSucceeds
#else
#define MAYBE_EnsureUniversalAccessFromFileSchemeSucceeds \
  EnsureUniversalAccessFromFileSchemeSucceeds
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_EnsureUniversalAccessFromFileSchemeSucceeds) {
  StartEmbeddedServer();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  WebPreferences prefs =
      web_contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.allow_universal_access_from_file_urls = true;
  web_contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  GURL file_url = GetTestUrl("", "title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), file_url));
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  EXPECT_TRUE(ExecuteScript(
      root, "window.history.pushState({}, '', 'https://chromium.org');"));
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
}

// Ensure that navigating back from a sad tab to an existing process works
// correctly. See https://crbug.com/591984.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       NavigateBackToExistingProcessFromSadTab) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html"), "foo");
  EXPECT_TRUE(NavigateToURL(
      popup, embedded_test_server()->GetURL("b.com", "/title3.html")));

  // Kill the b.com process.
  RenderProcessHost* b_process =
      popup->web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      b_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  b_process->Shutdown(0);
  crash_observer.Wait();

  // The popup should now be showing the sad tab.  Main tab should not be.
  EXPECT_NE(base::TERMINATION_STATUS_STILL_RUNNING,
            popup->web_contents()->GetCrashedStatus());
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            shell()->web_contents()->GetCrashedStatus());

  // Go back in the popup from b.com to a.com/title2.html.
  TestNavigationObserver back_observer(popup->web_contents());
  popup->web_contents()->GetController().GoBack();
  back_observer.Wait();

  // In the bug, after the back navigation the popup was still showing
  // the sad tab.  Ensure this is not the case.
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            popup->web_contents()->GetCrashedStatus());
  EXPECT_TRUE(popup->web_contents()->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(popup->web_contents()->GetMainFrame()->GetSiteInstance(),
            shell()->web_contents()->GetMainFrame()->GetSiteInstance());
}

// Verify that GetLastCommittedOrigin() is correct for the full lifetime of a
// RenderFrameHost, including when it's pending, current, and pending deletion.
// This is checked both for main frames and subframes.
// See https://crbug.com/590035.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, LastCommittedOrigin) {
  StartEmbeddedServer();

  // Disable the back-forward cache so that documents are always deleted when
  // navigating.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();

  EXPECT_EQ(url::Origin::Create(url_a), rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_a, web_contents->GetMainFrame());

  // Start a navigation to a b.com URL, and don't wait for commit.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(root);
  RenderFrameDeletedObserver deleted_observer(rfh_a);
  shell()->LoadURL(url_b);

  // The speculative RFH shouln't have a last committed origin (the default
  // value is a unique origin). The current RFH shouldn't change its last
  // committed origin before commit.
  RenderFrameHostImpl* rfh_b = root->render_manager()->speculative_frame_host();
  EXPECT_EQ("null", rfh_b->GetLastCommittedOrigin().Serialize());
  EXPECT_EQ(url::Origin::Create(url_a), rfh_a->GetLastCommittedOrigin());

  // Verify that the last committed origin is set for the b.com RHF once it
  // commits.
  commit_observer.WaitForCommit();
  EXPECT_EQ(url::Origin::Create(url_b), rfh_b->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_b, web_contents->GetMainFrame());

  // The old RFH should now be pending deletion.  Verify it still has correct
  // last committed origin.
  EXPECT_EQ(url::Origin::Create(url_a), rfh_a->GetLastCommittedOrigin());
  EXPECT_FALSE(rfh_a->is_active());

  // Wait for |rfh_a| to be deleted and double-check |rfh_b|'s origin.
  deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(url::Origin::Create(url_b), rfh_b->GetLastCommittedOrigin());

  // Navigate to a same-origin page with an about:blank iframe.  The iframe
  // should also have a b.com origin.
  GURL url_b_with_frame(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b_with_frame));
  EXPECT_EQ(rfh_b, web_contents->GetMainFrame());
  EXPECT_EQ(url::Origin::Create(url_b), rfh_b->GetLastCommittedOrigin());
  FrameTreeNode* child = root->child_at(0);
  RenderFrameHostImpl* child_rfh_b = root->child_at(0)->current_frame_host();
  child_rfh_b->DisableSwapOutTimerForTesting();
  EXPECT_EQ(url::Origin::Create(url_b), child_rfh_b->GetLastCommittedOrigin());

  // Navigate subframe to c.com.  Wait for commit but not full load, and then
  // verify the subframe's origin.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title3.html"));
  {
    TestFrameNavigationObserver commit_observer(root->child_at(0));
    EXPECT_TRUE(
        ExecuteScript(child, "location.href = '" + url_c.spec() + "';"));
    commit_observer.WaitForCommit();
  }
  EXPECT_EQ(url::Origin::Create(url_c),
            child->current_frame_host()->GetLastCommittedOrigin());

  // With OOPIFs, this navigation used a cross-process transfer.  Ensure that
  // the iframe's old RFH still has correct origin, even though it's pending
  // deletion.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_FALSE(child_rfh_b->is_active());
    EXPECT_NE(child_rfh_b, child->current_frame_host());
    EXPECT_EQ(url::Origin::Create(url_b),
              child_rfh_b->GetLastCommittedOrigin());
  }
}

// Ensure that loading a page with cross-site coreferencing iframes does not
// cause an infinite number of nested iframes to be created.
// See https://crbug.com/650332.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, CoReferencingFrames) {
  // Load a page with a cross-site coreferencing iframe. "Coreferencing" here
  // refers to two separate pages that contain subframes with URLs to each
  // other.
  StartEmbeddedServer();
  GURL url_1(
      embedded_test_server()->GetURL("a.com", "/coreferencingframe_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // The FrameTree contains two successful instances of each site plus an
  // unsuccessfully-navigated third instance of B with a blank URL.  When not in
  // site-per-process mode, the FrameTreeVisualizer depicts all nodes as
  // referencing Site A because iframes are identified with their root site.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site A -- proxies for B\n"
        "             +--Site B -- proxies for A\n"
        "                  +--Site B -- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        FrameTreeVisualizer().DepictFrameTree(root));
  } else {
    const GURL kExpectedSiteURL = AreDefaultSiteInstancesEnabled()
                                      ? SiteInstanceImpl::GetDefaultSiteURL()
                                      : GURL("http://a.com/");
    EXPECT_EQ(std::string(" Site A\n"
                          "   +--Site A\n"
                          "        +--Site A\n"
                          "             +--Site A\n"
                          "                  +--Site A\n"
                          "Where A = ") +
                  kExpectedSiteURL.spec(),
              FrameTreeVisualizer().DepictFrameTree(root));
  }
  FrameTreeNode* bottom_child =
      root->child_at(0)->child_at(0)->child_at(0)->child_at(0);
  EXPECT_TRUE(bottom_child->current_url().is_empty());
  EXPECT_FALSE(bottom_child->has_committed_real_load());
}

// Ensures that nested subframes with the same URL but different fragments can
// only be nested once.  See https://crbug.com/650332.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingFragmentFrames) {
  StartEmbeddedServer();
  GURL url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html#123"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // ExecuteScript is used here and once more below because it is important to
  // use renderer-initiated navigations since browser-initiated navigations are
  // bypassed in the self-referencing navigation check.
  TestFrameNavigationObserver observer1(child);
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + url.spec() + "456" + "';"));
  observer1.Wait();

  FrameTreeNode* grandchild = child->child_at(0);
  GURL expected_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_EQ(expected_url, grandchild->current_url());

  // This navigation should be blocked.
  GURL blocked_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_iframe.html#123456789"));
  TestNavigationManager manager(web_contents, blocked_url);
  EXPECT_TRUE(ExecuteScript(grandchild,
                            "location.href = '" + blocked_url.spec() + "';"));
  // Wait for WillStartRequest and verify that the request is aborted before
  // starting it.
  EXPECT_FALSE(manager.WaitForRequestStart());
  WaitForLoadStop(web_contents);

  // The FrameTree contains two successful instances of the url plus an
  // unsuccessfully-navigated third instance with a blank URL.
  const GURL kExpectedSiteURL = AreDefaultSiteInstancesEnabled()
                                    ? SiteInstanceImpl::GetDefaultSiteURL()
                                    : GURL("http://a.com/");
  EXPECT_EQ(std::string(" Site A\n"
                        "   +--Site A\n"
                        "        +--Site A\n"
                        "Where A = ") +
                kExpectedSiteURL.spec(),
            FrameTreeVisualizer().DepictFrameTree(root));

  // The URL of the grandchild has not changed.
  EXPECT_EQ(expected_url, grandchild->current_url());
}

// Ensure that loading a page with a meta refresh iframe does not cause an
// infinite number of nested iframes to be created.  This test loads a page with
// an about:blank iframe where the page injects html containing a meta refresh
// into the iframe.  This test then checks that this does not cause infinite
// nested iframes to be created.  See https://crbug.com/527367.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingMetaRefreshFrames) {
  // Load a page with a blank iframe.
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/page_with_meta_refresh_frame.html"));
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 3);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // The third navigation should fail and be cancelled, leaving a FrameTree with
  // a height of 2.
  const GURL kExpectedSiteURL = AreDefaultSiteInstancesEnabled()
                                    ? SiteInstanceImpl::GetDefaultSiteURL()
                                    : GURL("http://a.com/");
  EXPECT_EQ(std::string(" Site A\n"
                        "   +--Site A\n"
                        "        +--Site A\n"
                        "Where A = ") +
                kExpectedSiteURL.spec(),
            FrameTreeVisualizer().DepictFrameTree(root));

  EXPECT_EQ(GURL(url::kAboutBlankURL),
            root->child_at(0)->child_at(0)->current_url());

  EXPECT_FALSE(root->child_at(0)->child_at(0)->has_committed_real_load());
}

// Ensure that navigating a subframe to the same URL as its parent twice in a
// row is not blocked by the self-reference check.
// See https://crbug.com/650332.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingSameURLRenavigation) {
  StartEmbeddedServer();
  GURL first_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL second_url(first_url.spec() + "#123");
  EXPECT_TRUE(NavigateToURL(shell(), first_url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  TestFrameNavigationObserver observer1(child);
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + second_url.spec() + "';"));
  observer1.Wait();

  EXPECT_EQ(child->current_url(), second_url);

  TestFrameNavigationObserver observer2(child);
  // This navigation shouldn't be blocked. Blocking should only occur when more
  // than one ancestor has the same URL (excluding fragments), and the
  // navigating frame's current URL shouldn't count toward that.
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + first_url.spec() + "';"));
  observer2.Wait();

  EXPECT_EQ(child->current_url(), first_url);
}

// Ensures that POST requests bypass self-referential URL checks. See
// https://crbug.com/710008.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingFramesWithPOST) {
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL child_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_EQ(url, root->current_url());
  EXPECT_EQ(child_url, child->current_url());

  // Navigate the child frame to the same URL as parent via POST.
  std::string script =
      "var f = document.createElement('form');\n"
      "f.method = 'POST';\n"
      "f.action = '/page_with_iframe.html';\n"
      "document.body.appendChild(f);\n"
      "f.submit();";
  {
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecuteScript(child, script));
    observer.Wait();
  }

  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(url, child->current_url());
  EXPECT_EQ(child_url, grandchild->current_url());

  // Now navigate the grandchild to the same URL as its two ancestors. This
  // should be allowed since it uses POST; it was blocked prior to
  // fixing https://crbug.com/710008.
  {
    TestFrameNavigationObserver observer(grandchild);
    EXPECT_TRUE(ExecuteScript(grandchild, script));
    observer.Wait();
  }

  EXPECT_EQ(url, grandchild->current_url());
  ASSERT_EQ(1U, grandchild->child_count());
  EXPECT_EQ(child_url, grandchild->child_at(0)->current_url());
}

// Ensures that we don't reset a speculative RFH if a JavaScript URL is loaded
// while there's an ongoing cross-process navigation. See
// https://crbug.com/793432.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       JavaScriptLoadDoesntResetSpeculativeRFH) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL site1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL site2 = embedded_test_server()->GetURL("b.com", "/title2.html");

  EXPECT_TRUE(NavigateToURL(shell(), site1));

  TestNavigationManager cross_site_navigation(shell()->web_contents(), site2);
  shell()->LoadURL(site2);
  EXPECT_TRUE(cross_site_navigation.WaitForRequestStart());

  RenderFrameHostImpl* speculative_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->render_manager()
          ->speculative_frame_host();
  CHECK(speculative_rfh);
  shell()->web_contents()->GetController().LoadURL(
      GURL("javascript:(0)"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string());

  cross_site_navigation.ResumeNavigation();
  // No crash means everything worked!
}

// Test that unrelated browsing contexts cannot find each other's windows,
// even when they end up using the same renderer process (e.g. because of
// hitting a process limit).  See also https://crbug.com/718489.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ProcessReuseVsBrowsingInstance) {
  // Set max renderers to 1 to force reusing a renderer process between two
  // unrelated tabs.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Navigate 2 tabs to a web page (regular web pages can share renderers
  // among themselves without any restrictions, unlike extensions, apps, etc.).
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHost* tab1 = shell()->web_contents()->GetMainFrame();
  EXPECT_EQ(url1, tab1->GetLastCommittedURL());
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  Shell* shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), url2, nullptr, gfx::Size());
  EXPECT_TRUE(NavigateToURL(shell2, url2));
  RenderFrameHost* tab2 = shell2->web_contents()->GetMainFrame();
  EXPECT_EQ(url2, tab2->GetLastCommittedURL());

  // Sanity-check test setup: 2 frames share a renderer process, but are not in
  // a related browsing instance.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(tab1->GetProcess(), tab2->GetProcess());
  EXPECT_FALSE(
      tab1->GetSiteInstance()->IsRelatedSiteInstance(tab2->GetSiteInstance()));

  // Name the 2 frames.
  EXPECT_TRUE(ExecuteScript(tab1, "window.name = 'tab1';"));
  EXPECT_TRUE(ExecuteScript(tab2, "window.name = 'tab2';"));

  // Verify that |tab1| cannot find named frames belonging to |tab2| (i.e. that
  // window.open will end up creating a new tab rather than returning the old
  // |tab2| tab).
  WebContentsAddedObserver new_contents_observer;
  std::string location_of_opened_window;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      tab1,
      "var w = window.open('', 'tab2');\n"
      "window.domAutomationController.send(w.location.href);",
      &location_of_opened_window));
  EXPECT_EQ(url::kAboutBlankURL, location_of_opened_window);
  EXPECT_TRUE(new_contents_observer.GetWebContents());
}

// Verify that cross-site main frame navigations will swap BrowsingInstances
// for certain browser-initiated navigations, such as user typing the URL into
// the address bar.  This helps avoid unneeded process sharing and should
// happen even if the current frame has an opener.  See
// https://crbug.com/803367.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       BrowserInitiatedNavigationsSwapBrowsingInstance) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Start with a page on a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstance> a_site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Open a popup for b.com. This should stay in the current BrowsingInstance.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup = OpenPopup(shell(), b_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  scoped_refptr<SiteInstance> b_site_instance(
      popup->web_contents()->GetSiteInstance());
  EXPECT_TRUE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));

  // 1. Same-site browser-initiated navigations shouldn't swap BrowsingInstances
  //    or SiteInstances.
  EXPECT_TRUE(NavigateToURL(
      popup, embedded_test_server()->GetURL("b.com", "/title2.html")));
  EXPECT_EQ(b_site_instance, popup->web_contents()->GetSiteInstance());

  // 2. A cross-site browser-initiated navigation should swap BrowsingInstances,
  //    despite having an opener in the same site as the destination URL.
  EXPECT_TRUE(NavigateToURL(
      popup, embedded_test_server()->GetURL("a.com", "/title3.html")));
  EXPECT_NE(b_site_instance, popup->web_contents()->GetSiteInstance());
  EXPECT_NE(a_site_instance, popup->web_contents()->GetSiteInstance());
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(
      popup->web_contents()->GetSiteInstance()));
  EXPECT_FALSE(b_site_instance->IsRelatedSiteInstance(
      popup->web_contents()->GetSiteInstance()));

  auto transitions_forcing_browsing_instance_swap = {
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, /* user clicking on a bookmark */
      ui::PAGE_TRANSITION_GENERATED,     /* search query */
      ui::PAGE_TRANSITION_KEYWORD, /* search within a site from address bar */
      ui::PAGE_TRANSITION_TYPED,   /* user typing URL into address bar */
  };
  auto transitions_not_forcing_browsing_instance_swap = {
      ui::PAGE_TRANSITION_LINK, /* user clicked on a link in the document */
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      ui::PAGE_TRANSITION_FORM_SUBMIT,
      ui::PAGE_TRANSITION_KEYWORD_GENERATED,
      ui::PAGE_TRANSITION_RELOAD,
  };
  int current_site = 0;
  scoped_refptr<SiteInstance> curr_instance(
      popup->web_contents()->GetSiteInstance());

  // 3. Perform several cross-site browser-initiated navigations in the popup
  //    that do not force a BrowsingInstance swap.
  //
  // When ProactivelySwapBrowsingInstance is disabled, the BrowsingInstance
  // isn't swapped, even if it could be.
  //
  // When ProactivelySwapBrowsingInstance is enabled, the BrowsingInstance is
  // now swapped. It can be swapped, because (2) caused the popup to live in a
  // different BrowsingInstance. The window and the popup are no longer related.
  for (auto transition : transitions_not_forcing_browsing_instance_swap) {
    GURL cross_site_url(embedded_test_server()->GetURL(
        base::StringPrintf("site%d.com", current_site++), "/title1.html"));
    SCOPED_TRACE(base::StringPrintf(
        "... wrong BrowsingInstance for '%s' transition to %s",
        ui::PageTransitionGetCoreTransitionString(transition),
        cross_site_url.spec().c_str()));

    TestNavigationObserver observer(popup->web_contents());
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = transition;
    popup->web_contents()->GetController().LoadURLWithParams(params);
    observer.Wait();

    if (IsProactivelySwapBrowsingInstanceEnabled()) {
      EXPECT_FALSE(curr_instance->IsRelatedSiteInstance(
          popup->web_contents()->GetSiteInstance()));
    } else {
      EXPECT_TRUE(curr_instance->IsRelatedSiteInstance(
          popup->web_contents()->GetSiteInstance()));
    }
  }

  // 4. Perform several cross-site browser-initiated navigations in the popup,
  //    all using page transitions that force BrowsingInstance swaps:
  for (auto transition : transitions_forcing_browsing_instance_swap) {
    GURL cross_site_url(embedded_test_server()->GetURL(
        base::StringPrintf("site%d.com", current_site++), "/title1.html"));
    scoped_refptr<SiteInstance> prev_instance(
        popup->web_contents()->GetSiteInstance());
    SCOPED_TRACE(base::StringPrintf(
        "... expected BrowsingInstance swap for '%s' transition to %s",
        ui::PageTransitionGetCoreTransitionString(transition),
        cross_site_url.spec().c_str()));

    TestNavigationObserver observer(popup->web_contents());
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = transition;
    popup->web_contents()->GetController().LoadURLWithParams(params);
    observer.Wait();

    // This should swap BrowsingInstances.
    scoped_refptr<SiteInstance> curr_instance(
        popup->web_contents()->GetSiteInstance());
    EXPECT_NE(a_site_instance, curr_instance);
    EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(curr_instance.get()));
    EXPECT_NE(prev_instance, curr_instance);
    EXPECT_FALSE(prev_instance->IsRelatedSiteInstance(curr_instance.get()));
  }
}

// Ensure that these two browser-initiated navigations:
//   foo.com -> about:blank -> foo.com
// stay in the same SiteInstance.  This isn't technically required for
// correctness, but some tests (e.g., testEnsureHotFromScratch from
// telemetry_unittests) currently depend on this behavior.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       NavigateToAndFromAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  scoped_refptr<SiteInstance> site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Navigate to about:blank from address bar.  This stays in the foo.com
  // SiteInstance.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_EQ(site_instance, shell()->web_contents()->GetSiteInstance());

  // Perform a browser-initiated navigation to foo.com.  This should also stay
  // in the original foo.com SiteInstance and BrowsingInstance.
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  EXPECT_EQ(site_instance, shell()->web_contents()->GetSiteInstance());
}

// Check that with the following sequence of navigations:
//   foo.com -(1)-> bar.com -(2)-> about:blank -(3)-> foo.com
// where (1) is renderer-initiated and (2)+(3) are browser-initiated, the last
// navigation goes back to the first SiteInstance without --site-per-process,
// and to a new SiteInstance and BrowsingInstance with --site-per-process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       NavigateToFooThenBarThenAboutBlankThenFoo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  scoped_refptr<SiteInstance> site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Do a renderer-initiated navigation to bar.com, then navigate to
  // about:blank from address bar, then back to foo.com.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), bar_url));
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // This should again go back to the original foo.com SiteInstance without
  // --site-per-process, as in that case both the bar.com and
  // about:blank navigation will stay in the foo.com SiteInstance, and the
  // final navigation to foo.com will be considered same-site with the current
  // SiteInstance.
  //
  // With --site-per-process, bar.com should get its own SiteInstance, the
  // about:blank navigation will stay in it, and thus the final foo.com
  // navigation should be considered cross-site from the current SiteInstance.
  // Since this is a browser-initiated, cross-site navigation, it will swap
  // BrowsingInstances, and create a new foo.com SiteInstance, distinct from
  // the initial one.
  if (!AreAllSitesIsolatedForTesting() &&
      !IsProactivelySwapBrowsingInstanceEnabled()) {
    EXPECT_EQ(site_instance, shell()->web_contents()->GetSiteInstance());
  } else {
    EXPECT_NE(site_instance, shell()->web_contents()->GetSiteInstance());
    EXPECT_FALSE(site_instance->IsRelatedSiteInstance(
        shell()->web_contents()->GetSiteInstance()));
    EXPECT_EQ(site_instance->GetSiteURL(),
              shell()->web_contents()->GetSiteInstance()->GetSiteURL());
  }
}

// Test to verify that navigations in the main frame, which result in an error
// page, properly commit the error page in its own dedicated process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationInMainFrame) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  // Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  scoped_refptr<SiteInstance> success_site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  // Browser-initiated navigation to an error page should result in changing the
  // SiteInstance and process.
  {
    NavigationHandleObserver observer(shell()->web_contents(), error_url);
    EXPECT_FALSE(NavigateToURL(shell(), error_url));
    EXPECT_TRUE(observer.is_error());
    EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());

    scoped_refptr<SiteInstance> error_site_instance =
        shell()->web_contents()->GetMainFrame()->GetSiteInstance();
    EXPECT_NE(success_site_instance, error_site_instance);
    EXPECT_TRUE(success_site_instance->IsRelatedSiteInstance(
        error_site_instance.get()));
    EXPECT_NE(success_site_instance->GetProcess()->GetID(),
              error_site_instance->GetProcess()->GetID());
    EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());

    // Verify that the error page process is locked to origin
    EXPECT_EQ(
        GURL(kUnreachableWebDataURL),
        policy->GetOriginLock(error_site_instance->GetProcess()->GetID()));
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
  }

  // Navigate successfully again to a document, then perform a
  // renderer-initiated navigation and verify it behaves the same way.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  success_site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      policy->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));

  {
    NavigationHandleObserver observer(shell()->web_contents(), error_url);
    TestFrameNavigationObserver frame_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                              "location.href = '" + error_url.spec() + "';"));
    frame_observer.Wait();
    EXPECT_TRUE(observer.is_error());
    EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());

    scoped_refptr<SiteInstance> error_site_instance =
        shell()->web_contents()->GetMainFrame()->GetSiteInstance();
    EXPECT_NE(success_site_instance, error_site_instance);
    EXPECT_TRUE(success_site_instance->IsRelatedSiteInstance(
        error_site_instance.get()));
    EXPECT_NE(success_site_instance->GetProcess()->GetID(),
              error_site_instance->GetProcess()->GetID());
    EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());

    // Verify that the error page process is locked to origin
    EXPECT_EQ(
        GURL(kUnreachableWebDataURL),
        policy->GetOriginLock(error_site_instance->GetProcess()->GetID()));
  }
}

// Test to verify that navigations in subframes, which result in an error
// page, commit the error page in the same process and not in the dedicated
// error page process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationInChildFrame) {
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);

  // Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  scoped_refptr<SiteInstance> success_site_instance =
      child->current_frame_host()->GetSiteInstance();

  NavigationHandleObserver observer(web_contents, error_url);
  TestFrameNavigationObserver frame_observer(child);
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + error_url.spec() + "';"));
  frame_observer.Wait();

  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());

  scoped_refptr<SiteInstance> error_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(success_site_instance, error_site_instance);
  EXPECT_NE(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());
  EXPECT_TRUE(IsOriginOpaqueAndCompatibleWithURL(child, error_url));
}

// Test to verify that navigations in new window, which result in an error
// page, commit the error page in the dedicated error page process and not in
// the one for the destination site.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationInNewWindow) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);

  // Start with a successful navigation to a document.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  scoped_refptr<SiteInstance> main_site_instance =
      root->current_frame_host()->GetSiteInstance();

  Shell* new_shell = OpenPopup(shell(), error_url, "foo");
  EXPECT_TRUE(new_shell);

  scoped_refptr<SiteInstance> error_site_instance =
      new_shell->web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_NE(main_site_instance, error_site_instance);
  EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());

  // Verify that the error page process is locked to origin
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                error_site_instance->GetProcess()->GetID()));
  EXPECT_TRUE(
      IsMainFrameOriginOpaqueAndCompatibleWithURL(new_shell, error_url));
}

// Test to verify that windows that are not part of the same
// BrowsingInstance end up using the same error page process, even though
// their SiteInstances are not related.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationInUnrelatedWindows) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);

  // Navigate the main window to an error page and verify.
  {
    NavigationHandleObserver observer(shell()->web_contents(), error_url);
    EXPECT_FALSE(NavigateToURL(shell(), error_url));
    scoped_refptr<SiteInstance> error_site_instance =
        shell()->web_contents()->GetMainFrame()->GetSiteInstance();
    EXPECT_TRUE(observer.is_error());
    EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());
    EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
  }

  // Creat a new, unrelated, window, navigate it to an error page and
  // verify.
  Shell* new_shell = CreateBrowser();
  {
    NavigationHandleObserver observer(new_shell->web_contents(), error_url);
    EXPECT_FALSE(NavigateToURL(new_shell, error_url));
    scoped_refptr<SiteInstance> error_site_instance =
        new_shell->web_contents()->GetMainFrame()->GetSiteInstance();
    EXPECT_TRUE(observer.is_error());
    EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());
    EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(new_shell, error_url));
  }

  // Verify the two SiteInstanes are not related, but they end up using the
  // same underlying RenderProcessHost.
  EXPECT_FALSE(
      shell()->web_contents()->GetSiteInstance()->IsRelatedSiteInstance(
          new_shell->web_contents()->GetSiteInstance()));
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance()->GetProcess(),
            new_shell->web_contents()->GetSiteInstance()->GetProcess());

  // Verify that the process is locked to origin
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));
}

// Test to verify that reloading an error page once the error condition has
// cleared up is successful and does not create a new navigation entry.
// See https://crbug.com/840485.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, ErrorPageNavigationReload) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  GURL end_url(embedded_test_server()->GetURL("/title2.html"));
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  // Build session history with three entries, where the middle one will be
  // tested for successful and failed reloads. This allows checking whether
  // reload accidentally clears the forward session history if it is
  // incorrectly classified.
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_TRUE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(NavigateToURL(shell(), end_url));
  {
    TestNavigationObserver back_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_observer.Wait();
    EXPECT_TRUE(back_observer.last_navigation_succeeded());
  }
  EXPECT_EQ(3, nav_controller.GetEntryCount());
  EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(error_url, shell()->web_contents()->GetLastCommittedURL());

  scoped_refptr<SiteInstance> success_site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  url::Origin expected_origin =
      shell()->web_contents()->GetMainFrame()->GetLastCommittedOrigin();

  EXPECT_EQ(url::Origin::Create(error_url), expected_origin);

  // Install an interceptor which will cause network failure for |error_url|,
  // reload the existing entry and verify.
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    // TODO(nasko): Investigate making a failing reload of a successful
    // navigation be classified as NEW_PAGE instead, since with error page
    // isolation it involves a SiteInstance swap.
    EXPECT_EQ(NavigationType::NAVIGATION_TYPE_EXISTING_PAGE,
              reload_observer.last_navigation_type());
  }
  EXPECT_EQ(3, nav_controller.GetEntryCount());
  EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  int process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  EXPECT_EQ(GURL(kUnreachableWebDataURL), policy->GetOriginLock(process_id));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));

  // Reload while it will still fail to ensure it stays in the same process.
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(NavigationType::NAVIGATION_TYPE_EXISTING_PAGE,
              reload_observer.last_navigation_type());
  }
  EXPECT_EQ(process_id,
            shell()->web_contents()->GetMainFrame()->GetProcess()->GetID());
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));

  // Reload the error page after clearing the error condition, such that the
  // navigation is successful and verify that no new entry was added to
  // session history and forward history is not pruned.
  url_interceptor.reset();
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
    // The successful reload should be classified as a NEW_PAGE navigation
    // with replacement, since it needs to stay at the same entry in session
    // history, but needs a new entry because of the change in SiteInstance.
    EXPECT_EQ(NavigationType::NAVIGATION_TYPE_NEW_PAGE,
              reload_observer.last_navigation_type());
  }
  EXPECT_EQ(3, nav_controller.GetEntryCount());
  EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      policy->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_EQ(expected_origin,
            shell()->web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Test the same scenario as above, but this time initiated by the
  // renderer process.
  url_interceptor = SetupRequestFailForURL(error_url);
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    // TODO(nasko): Investigate making a failing reload of a successful
    // navigation be classified as NEW_PAGE instead, since with error page
    // isolation it involves a SiteInstance swap.
    EXPECT_EQ(NavigationType::NAVIGATION_TYPE_EXISTING_PAGE,
              reload_observer.last_navigation_type());
  }
  EXPECT_EQ(3, nav_controller.GetEntryCount());
  EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      policy->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));

  url_interceptor.reset();
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell(), "location.reload();"));
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
    // TODO(nasko): Investigate making renderer initiated reloads that change
    // SiteInstance be classified as NEW_PAGE as well.
    EXPECT_EQ(NavigationType::NAVIGATION_TYPE_EXISTING_PAGE,
              reload_observer.last_navigation_type());
  }
  EXPECT_EQ(3, nav_controller.GetEntryCount());
  EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      policy->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_EQ(expected_origin,
            shell()->web_contents()->GetMainFrame()->GetLastCommittedOrigin());
}

// Version of ErrorPageNavigationReload test that targets a subframe (because
// subframes are currently [~2019Q1] not subject to error page isolation).
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationReload_InSubframe_NetworkError) {
  StartEmbeddedServer();

  // Isolating a.com helps more robustly exercise platforms without strict site
  // isolation - we want to ensure that enforcing |initiator_origin| in
  // BeginNavigation is compatible with process locks, even when only one of the
  // frames requires isolation.
  IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                           {"b.com"});

  // Start on a page with a main frame and a subframe.
  GURL page_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL test_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  // We start at 3, because IsolateOriginsForTesting is sneeking in extra two
  // navigations.
  EXPECT_EQ(3, nav_controller.GetEntryCount());

  // Grab child's site URL.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child = root->child_at(0);
  GURL child_site_url =
      child->current_frame_host()->GetSiteInstance()->GetSiteURL();

  // Navigate the subframe to a URL that is cross-site from the main frame.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    ASSERT_TRUE(ExecJs(child, JsReplace("window.location = $1", test_url)));
    nav_observer.Wait();

    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(4, nav_controller.GetEntryCount());
    EXPECT_EQ(test_url, child->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(url::Origin::Create(test_url),
              child->current_frame_host()->GetLastCommittedOrigin());
    EXPECT_EQ(child_site_url,
              child->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }

  // Reload the subframe while the network is down.
  {
    std::unique_ptr<URLLoaderInterceptor> url_interceptor =
        SetupRequestFailForURL(test_url);
    TestNavigationObserver nav_observer(shell()->web_contents());
    ASSERT_TRUE(ExecJs(child, "window.location.reload()"));
    nav_observer.Wait();

    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(4, nav_controller.GetEntryCount());
    EXPECT_EQ(test_url, child->current_frame_host()->GetLastCommittedURL());

    // Error pages should commit in an opaque origin.
    EXPECT_TRUE(IsOriginOpaqueAndCompatibleWithURL(child, test_url));

    // Error page isolation should apply only to the main frame - subframes
    // should commit in their usual process / SiteInstance.
    EXPECT_EQ(child_site_url,
              child->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }

  // Reload the subframe after the network is restored.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    // Note that some error pages actually do embed a button that will do an
    // equivalent of the renderer-initiated reload that is triggered below.
    ASSERT_TRUE(ExecJs(child, "window.location.reload()"));
    nav_observer.Wait();

    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(4, nav_controller.GetEntryCount());
    EXPECT_EQ(test_url, child->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(url::Origin::Create(test_url),
              child->current_frame_host()->GetLastCommittedOrigin());
    EXPECT_EQ(child_site_url,
              child->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }
}

// Version of ErrorPageNavigationReload test that targets a subframe (because
// subframes are currently [~2019Q1] not subject to error page isolation).
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationReload_InSubframe_BlockedByClient) {
  StartEmbeddedServer();

  // Isolating a.com and b.com helps more robustly exercise platforms without
  // strict site isolation - we want to ensure that enforcing |initiator_origin|
  // in BeginNavigation is compatible with process locks, even when only some of
  // the frames requires isolation.
  IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                           {"a.com", "b.com"});

  // Start on a page with a same-site main frame and a subframe.
  GURL page_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  GURL test_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  // We start at 3, because IsolateOriginsForTesting calls are sneeking in extra
  // two navigations.
  EXPECT_EQ(3, nav_controller.GetEntryCount());

  // Grab child's site URL.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  GURL a_site_url = root->current_frame_host()->GetSiteInstance()->GetSiteURL();
  EXPECT_EQ("a.com", a_site_url.host());
  GURL b_site_url =
      child2->current_frame_host()->GetSiteInstance()->GetSiteURL();
  EXPECT_EQ("b.com", b_site_url.host());

  // Navigate the subframe to a cross-site URL, while blocking the request with
  // ERR_BLOCKED_BY_CLIENT.
  {
    // Name the first child, so it can be found and navigated by the other child
    // in the next test step below.
    ASSERT_TRUE(ExecJs(child1, "window.name = 'child1';"));

    // Have |child2| initiate navigation of |child1| - the navigation should
    // result in ERR_BLOCKED_BY_CLIENT (because of the throttle created below).
    content::TestNavigationThrottleInserter throttle_inserter(
        shell()->web_contents(),
        base::BindRepeating(&RequestBlockingNavigationThrottle::Create));
    const char kScriptTemplate[] = R"(
        var child1 = window.open('', 'child1');
        child1.location = $1;
    )";
    TestNavigationObserver nav_observer(shell()->web_contents());
    ASSERT_TRUE(ExecJs(child2, JsReplace(kScriptTemplate, test_url)));
    nav_observer.Wait();
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(4, nav_controller.GetEntryCount());
    EXPECT_EQ(test_url, child1->current_frame_host()->GetLastCommittedURL());

    // Error pages should commit in an opaque origin.
    EXPECT_TRUE(IsOriginOpaqueAndCompatibleWithURL(child1, test_url));

    // net::ERR_BLOCKED_BY_CLIENT errors in subframes should commit in their
    // initiator's process (not in their parent's process).
    EXPECT_EQ(b_site_url,
              child1->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }

  // Reload the subframe when no longer blocking the navigation.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    // Note that some error pages actually do embed a button that will do an
    // equivalent of the renderer-initiated reload that is triggered below.
    ASSERT_TRUE(ExecJs(child1, "window.location.reload()"));
    nav_observer.Wait();

    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(4, nav_controller.GetEntryCount());
    EXPECT_EQ(test_url, child1->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(url::Origin::Create(test_url),
              child1->current_frame_host()->GetLastCommittedOrigin());

    SiteInstanceImpl* child1_site_instance =
        child1->current_frame_host()->GetSiteInstance();

    GURL c_site_url = child1_site_instance->GetSiteURL();
    if (AreAllSitesIsolatedForTesting()) {
      EXPECT_EQ("c.com", c_site_url.host());
      EXPECT_EQ(test_url.host(), c_site_url.host());
    } else {
      EXPECT_TRUE(child1_site_instance->IsDefaultSiteInstance());
    }
    EXPECT_NE(a_site_url, c_site_url);
    EXPECT_NE(b_site_url, c_site_url);
  }
}

// Make sure that reload works properly if it redirects to a different site than
// the initial navigation.  The initial purpose of this test was to make sure
// the corresponding unit test matches the actual product code behavior
// (e.g. see NavigationControllerTest.Reload_GeneratesNewPage).
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ReloadRedirectsToDifferentCrossSitePage) {
  // Set-up http server handlers for |start_url|.
  //
  // |response1| is only required to make sure that |response2| doesn't kick-in
  // and intercept the http request until step 4.  Note that step 3 won't hit
  // the http server and therefore doesn't require handling here.
  auto response1 = std::make_unique<net::test_server::ControllableHttpResponse>(
      embedded_test_server(), "/title1.html");
  auto response2 = std::make_unique<net::test_server::ControllableHttpResponse>(
      embedded_test_server(), "/title1.html");

  // Start the server after all the required handlers have been already
  // registered.
  StartEmbeddedServer();
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // URLs used in the test:
  // - Step1: Navigate to |start_url|
  // - Step2: Navigate to |second_url|
  // - Step3: Go back (to |start_url|)
  // - Step4: Reload (redirects to a different destination - |redirect_url|).
  GURL start_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL second_url(embedded_test_server()->GetURL("foo.com", "/title3.html"));
  GURL redirect_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));

  // Test Step 1: Simple navigation that won't redirect and will just
  // successfully commit http://foo.com/title1.html.
  {
    // Navigate...
    TestNavigationObserver nav_observer(shell()->web_contents());
    shell()->LoadURL(start_url);
    response1->WaitForRequest();
    response1->Send(net::HTTP_OK, "text/html; charset=utf-8", "<p>Blah</p>");
    response1->Done();
    nav_observer.Wait();

    // Verify...
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(1, nav_controller.GetEntryCount());
    EXPECT_EQ(0, nav_controller.GetLastCommittedEntryIndex());
    EXPECT_EQ(start_url, nav_controller.GetLastCommittedEntry()->GetURL());
  }

  // Test Step 2: Simple navigation that won't redirect and will just
  // successfully commit http://foo.com/title3.html.
  {
    // Navigate...
    EXPECT_TRUE(NavigateToURL(shell(), second_url));

    // Verify...
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());
    EXPECT_EQ(second_url, nav_controller.GetLastCommittedEntry()->GetURL());
  }

  // Test Step 3: Go back (to |start_url|).
  {
    // Navigate...
    TestNavigationObserver nav_observer(shell()->web_contents());
    nav_controller.GoBack();
    nav_observer.Wait();

    // Verify...
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_EQ(0, nav_controller.GetLastCommittedEntryIndex());
    EXPECT_EQ(start_url, nav_controller.GetLastCommittedEntry()->GetURL());
  }

  // Test Step 4: Reload that will redirect to http://bar.com/title2.html (which
  // is a different, cross-site location compared to what the initial navigation
  // committed).
  {
    // Navigate...
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    response2->WaitForRequest();
    response2->Send("HTTP/1.1 302 Moved Temporarily\n");
    response2->Send("Location: " + redirect_url.spec());
    response2->Done();
    reload_observer.Wait();

    // Verify that reload 1) replaced the current entry (rather than creating a
    // new entry) and 2) the tail of the history (e.g. the |second_url|
    // navigation) was preserved rather than truncated.
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_EQ(0, nav_controller.GetLastCommittedEntryIndex());
    EXPECT_EQ(redirect_url, nav_controller.GetLastCommittedEntry()->GetURL());
    EXPECT_EQ(second_url, nav_controller.GetEntryAtIndex(1)->GetURL());
    if (AreAllSitesIsolatedForTesting()) {
      // The successful reload should be classified as a NEW_PAGE navigation
      // with replacement, since it needs to stay at the same entry in session
      // history, but needs a new entry because of the change in SiteInstance.
      // (the same as expectations in the ErrorPageNavigationReload test above).
      EXPECT_EQ(NavigationType::NAVIGATION_TYPE_NEW_PAGE,
                reload_observer.last_navigation_type());
    } else {
      EXPECT_EQ(NavigationType::NAVIGATION_TYPE_EXISTING_PAGE,
                reload_observer.last_navigation_type());
    }
  }
}

// Test to verify that navigating away from an error page results in correct
// change in SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationAfterError) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  scoped_refptr<SiteInstance> success_site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_EQ(1, nav_controller.GetEntryCount());

  // Navigate to an url resulting in an error page.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_EQ(2, nav_controller.GetEntryCount());
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));

  // Navigate again to the initial successful document, expecting a new
  // navigation and new SiteInstance. A new SiteInstance is expected here
  // because we are doing a cross-site navigation from |kUnreachableWebDataURL|
  // to a site for |url|. This triggers the creation of a new BrowsingInstance
  // and therefore a new SiteInstance.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify that we get the default SiteInstance because the original URL does
    // not require a dedicated process.
    EXPECT_TRUE(static_cast<SiteInstanceImpl*>(
                    shell()->web_contents()->GetMainFrame()->GetSiteInstance())
                    ->IsDefaultSiteInstance());
  }
  EXPECT_EQ(
      success_site_instance->GetSiteURL(),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(success_site_instance,
            shell()->web_contents()->GetMainFrame()->GetSiteInstance());

  EXPECT_EQ(3, nav_controller.GetEntryCount());

  // Repeat again using a renderer-initiated navigation for the successful one.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
          shell()->web_contents()->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_EQ(4, nav_controller.GetEntryCount());
  {
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecuteScript(shell(), "location.href = '" + url.spec() + "';"));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  EXPECT_EQ(5, nav_controller.GetEntryCount());
  EXPECT_NE(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
}

// Test to verify that when an error page is hit and its process is terminated,
// a successful reload correctly commits in a different process.
// See https://crbug.com/866549.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationReloadWithTerminatedProcess) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);
  WebContents* web_contents = shell()->web_contents();
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(web_contents->GetController());

  // Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  scoped_refptr<SiteInstance> success_site_instance =
      web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_EQ(1, nav_controller.GetEntryCount());

  // Navigate to an url resulting in an error page.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                web_contents->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_EQ(2, nav_controller.GetEntryCount());
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));

  // Terminate the renderer process.
  {
    RenderProcessHostWatcher termination_observer(
        web_contents->GetMainFrame()->GetProcess(),
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    web_contents->GetMainFrame()->GetProcess()->Shutdown(0);
    termination_observer.Wait();
  }

  // Clear the interceptor so the navigation will succeed on reload.
  url_interceptor.reset();

  // Reload the URL and execute a Javascript statement to verify that the
  // renderer process is still around and responsive.
  TestNavigationObserver reload_observer(shell()->web_contents());
  nav_controller.Reload(ReloadType::NORMAL, false);
  reload_observer.Wait();
  EXPECT_TRUE(reload_observer.last_navigation_succeeded());

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "window.domAutomationController.send(location.href);", &result));
  EXPECT_EQ(error_url.spec(), result);
}

// Test to verify that navigation to existing history entry, which results in
// an error page, is correctly placed in the error page SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationHistoryNavigationFailure) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();

  // Perform successful navigations to two URLs to establish session history.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  WebContents* web_contents = shell()->web_contents();
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(web_contents->GetController());

  // There should be two NavigationEntries.
  EXPECT_EQ(2, nav_controller.GetEntryCount());

  // Create an interceptor to cause navigations to url1 to fail and go back
  // in session history.
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(url1);
  TestNavigationObserver back_observer(web_contents);
  nav_controller.GoBack();
  back_observer.Wait();
  EXPECT_FALSE(back_observer.last_navigation_succeeded());
  EXPECT_EQ(2, nav_controller.GetEntryCount());
  EXPECT_EQ(0, nav_controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                web_contents->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), url1));
}

// Test to verify that a successful navigation to existing history entry,
// which initially resulted in an error page, is correctly placed in a
// SiteInstance different than the error page one.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationHistoryNavigationSuccess) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  WebContents* web_contents = shell()->web_contents();

  // Start with a successful navigation.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Navigate to URL that results in an error page and verify its SiteInstance.
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(url2);

  EXPECT_FALSE(NavigateToURL(shell(), url2));
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                web_contents->GetSiteInstance()->GetProcess()->GetID()));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), url2));

  // There should be two NavigationEntries.
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(web_contents->GetController());
  EXPECT_EQ(2, nav_controller.GetEntryCount());

  // Navigate once more to create another session history entry.
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url3));
  EXPECT_EQ(3, nav_controller.GetEntryCount());

  // Navigate back, this time remove the interceptor so the navigation will
  // succeed.
  url_interceptor.reset();
  TestNavigationObserver back_observer(web_contents);
  nav_controller.GoBack();
  back_observer.Wait();
  EXPECT_TRUE(back_observer.last_navigation_succeeded());
  EXPECT_EQ(3, nav_controller.GetEntryCount());
  EXPECT_EQ(1, nav_controller.GetLastCommittedEntryIndex());

  EXPECT_NE(GURL(kUnreachableWebDataURL),
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(GURL(kUnreachableWebDataURL),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                web_contents->GetSiteInstance()->GetProcess()->GetID()));
}

// Test to verify that navigations to WebUI URL which results in an error
// commits properly in the error page process and does not give it WebUI
// bindings.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationToWebUIResourceWithError) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  StartEmbeddedServer();
  GURL webui_url = GURL(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  GURL error_url(webui_url.Resolve("/foo"));

  // Navigate to the main WebUI URL and ensure it is successful.
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));

  // Ensure that the subsequent navigation is blocked, resulting in an
  // error.
  TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindRepeating(&RequestBlockingNavigationThrottle::Create));

  // Navigate to an error URL and verify the error page process does not get
  // WebUI bindings.
  NavigationHandleObserver observer(shell()->web_contents(), error_url);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  scoped_refptr<SiteInstance> error_site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
                error_site_instance->GetProcess()->GetID()));
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      error_site_instance->GetProcess()->GetID()));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
}

// A test ContentBrowserClient implementation which enforces
// BrowsingInstance swap on every navigation. It is used to verify that
// reloading of an error page to an URL that requires BrowsingInstance swap
// works correctly.
class BrowsingInstanceSwapContentBrowserClient
    : public TestContentBrowserClient {
 public:
  BrowsingInstanceSwapContentBrowserClient() = default;

  bool ShouldIsolateErrorPage(bool in_main_frame) override {
    return in_main_frame;
  }

  bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_effective_url,
      const GURL& destination_effective_url) override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingInstanceSwapContentBrowserClient);
};

// Test to verify that reloading of an error page which resulted from a
// navigation to an URL which requires a BrowsingInstance swap, correcly
// reloads in the same SiteInstance for the error page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ErrorPageNavigationReloadBrowsingInstanceSwap) {
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      SetupRequestFailForURL(error_url);
  NavigationControllerImpl& nav_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // Start with a successful navigation to a document and verify there is
  // only one entry in session history.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  scoped_refptr<SiteInstance> success_site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_EQ(1, nav_controller.GetEntryCount());

  BrowsingInstanceSwapContentBrowserClient content_browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Navigate to an url resulting in an error page and ensure a new entry
  // was added to session history.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(2, nav_controller.GetEntryCount());

  scoped_refptr<SiteInstance> initial_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_EQ(GURL(kUnreachableWebDataURL), initial_instance->GetSiteURL());
  EXPECT_TRUE(
      success_site_instance->IsRelatedSiteInstance(initial_instance.get()));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));

  // Reload of the error page that still results in an error should stay in
  // the same SiteInstance. Ensure this works for both browser-initiated
  // reloads and renderer-initiated ones.
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_EQ(initial_instance,
              shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
  }
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell(), "location.reload();"));
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_EQ(initial_instance,
              shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
  }

  // Allow the navigation to succeed and ensure it swapped to a non-related
  // SiteInstance.
  url_interceptor.reset();
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell(), "location.reload();"));
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_FALSE(initial_instance->IsRelatedSiteInstance(
        shell()->web_contents()->GetMainFrame()->GetSiteInstance()));
    EXPECT_FALSE(success_site_instance->IsRelatedSiteInstance(
        shell()->web_contents()->GetMainFrame()->GetSiteInstance()));
  }

  SetBrowserClientForTesting(old_client);
}

// Helper class to simplify testing of unload handlers.  It allows waiting for
// particular HTTP requests to be made to the embedded_test_server(); the tests
// use this to wait for termination pings (e.g., navigator.sendBeacon()) made
// from unload handlers.
class RenderFrameHostManagerUnloadBrowserTest
    : public RenderFrameHostManagerTest {
 public:
  RenderFrameHostManagerUnloadBrowserTest() {}

  // Starts monitoring requests made to the embedded_http_server() looking for
  // one made to |url|.  To be used together with WaitForMonitoredRequest().
  void StartMonitoringRequestsFor(const GURL& url) {
    base::AutoLock lock(lock_);
    request_url_ = url;
    saw_request_url_ = false;
  }

  // Waits for a request to a URL set earlier via StartMonitoringRequestsFor().
  // Returns right away if that request was already made.
  void WaitForMonitoredRequest() {
    base::AutoLock lock(lock_);
    if (saw_request_url_)
      return;

    run_loop_.reset(new base::RunLoop());
    {
      base::RunLoop* run_loop = run_loop_.get();
      base::AutoUnlock unlock(lock_);
      run_loop->Run();
    }
    run_loop_.reset();
  }

  // Returns the body of the monitored request if it was a POST.
  const std::string& GetRequestContent() {
    base::AutoLock lock(lock_);
    return request_content_;
  }

  // Adds an unload handler to |rfh| and verifies that the unload state
  // bookkeeping on |rfh| is updated properly.
  void AddUnloadHandler(RenderFrameHostImpl* rfh, const std::string& script) {
    EXPECT_FALSE(rfh->GetSuddenTerminationDisablerState(blink::kUnloadHandler));
    EXPECT_TRUE(ExecuteScript(
        rfh, base::StringPrintf("window.onunload = function(e) { %s }",
                                script.c_str())));
    EXPECT_TRUE(rfh->GetSuddenTerminationDisablerState(blink::kUnloadHandler));
  }

  // Extend the timeout for keeping the subframe process alive for unload
  // processing to prevent any test flakiness.  This is the time that the ping
  // request will have to make it from the renderer to the test server.
  void ExtendSubframeUnloadTimeoutForTerminationPing(RenderFrameHostImpl* rfh) {
    rfh->SetSubframeUnloadTimeoutForTesting(base::TimeDelta::FromSeconds(30));
  }

 protected:
  void SetUpOnMainThread() override {
    // Request interceptor needs to be installed before the test server is
    // started.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &RenderFrameHostManagerUnloadBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));

    RenderFrameHostManagerTest::SetUpOnMainThread();

    StartEmbeddedServer();
  }

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // |request.GetURL()| gives us the URL after it's already resolved to
    // 127.0.0.1, so reconstruct the requested host via the Host header (which
    // includes host+port).
    GURL requested_url = request.GetURL();
    auto it = request.headers.find("Host");
    if (it != request.headers.end())
      requested_url = GURL("http://" + it->second + request.relative_url);

    base::AutoLock lock(lock_);
    if (!saw_request_url_ && request_url_ == requested_url) {
      saw_request_url_ = true;
      request_content_ = request.content;
      if (run_loop_)
        run_loop_->Quit();
    }
  }

  base::Lock lock_;
  GURL request_url_ GUARDED_BY(lock_);
  std::string request_content_ GUARDED_BY(lock_);
  bool saw_request_url_ GUARDED_BY(lock_) = false;
  std::unique_ptr<base::RunLoop> run_loop_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManagerUnloadBrowserTest);
};

// Ensure that after a main frame with a cross-site iframe is itself navigated
// cross-site, the unload handler in the iframe can use navigator.sendBeacon()
// to do a termination ping.  See https://crbug.com/852204, where this was
// broken with site isolation if the iframe was in its own process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerUnloadBrowserTest,
                       SubframeTerminationPing_SendBeacon) {
  // See BackForwardCache::DisableForTestingReason for explanation.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_USES_UNLOAD_EVENT);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderFrameHostImpl* child_rfh = root->child_at(0)->current_frame_host();

  // Add a subframe unload handler to do a termination ping via sendBeacon.
  GURL ping_url(embedded_test_server()->GetURL("b.com", "/empty.html"));
  AddUnloadHandler(child_rfh,
                   base::StringPrintf("navigator.sendBeacon('%s', 'ping');",
                                      ping_url.spec().c_str()));
  ExtendSubframeUnloadTimeoutForTerminationPing(child_rfh);

  // Navigate the main frame to c.com and wait for the ping.
  StartMonitoringRequestsFor(ping_url);
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), c_url));
  // Test succeeds if this doesn't time out while waiting for |ping_url|.
  WaitForMonitoredRequest();
  EXPECT_EQ("ping", GetRequestContent());
}

// Ensure that after a main frame with a cross-site iframe is itself navigated
// cross-site, the unload handler in the iframe can use an image load to do a
// termination ping. See https://crbug.com/852204, where this was broken with
// site isolation if the iframe was in its own process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerUnloadBrowserTest,
                       SubframeTerminationPing_Image) {
  // See BackForwardCache::DisableForTestingReason for explanation.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_USES_UNLOAD_EVENT);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderFrameHostImpl* child_rfh = root->child_at(0)->current_frame_host();

  // Add a subframe unload handler to do a termination ping by loading an
  // image.
  GURL ping_url(embedded_test_server()->GetURL("b.com", "/blank.jpg"));
  AddUnloadHandler(child_rfh,
                   base::StringPrintf("var img = document.createElement('img');"
                                      "img.src = '%s';"
                                      "document.body.appendChild(img);",
                                      ping_url.spec().c_str()));
  ExtendSubframeUnloadTimeoutForTerminationPing(child_rfh);

  // Navigate the main frame to c.com and wait for the ping.
  StartMonitoringRequestsFor(ping_url);
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), c_url));
  // Test succeeds if this doesn't time out while waiting for |ping_url|.
  WaitForMonitoredRequest();
}

// Ensure that when closing a window containing a page with a cross-site
// iframe, the iframe still runs its unload handler and can do a sendBeacon
// termination ping.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerUnloadBrowserTest,
                       SubframeTerminationPingWhenWindowCloses) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Open a popup window with a page containing a cross-site iframe.
  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c)"));
  Shell* popup = OpenPopup(root, popup_url, "popup");
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_contents));
  EXPECT_EQ(popup_url, popup_contents->GetLastCommittedURL());

  FrameTreeNode* popup_root = popup_contents->GetFrameTree()->root();
  RenderFrameHostImpl* child_rfh =
      popup_root->child_at(0)->current_frame_host();

  // In the popup, add a subframe unload handler to do a termination ping via
  // sendBeacon.
  GURL ping_url(embedded_test_server()->GetURL("c.com", "/empty.html"));
  AddUnloadHandler(child_rfh,
                   base::StringPrintf("navigator.sendBeacon('%s', 'ping');",
                                      ping_url.spec().c_str()));
  ExtendSubframeUnloadTimeoutForTerminationPing(child_rfh);

  // Close the popup and wait for the ping.
  StartMonitoringRequestsFor(ping_url);
  popup->Close();
  // Test succeeds if this doesn't time out while waiting for |ping_url|.
  WaitForMonitoredRequest();
  EXPECT_EQ("ping", GetRequestContent());
}

// Ensure that after a main frame with a cross-site iframe is navigated
// cross-site, and the iframe had an unload handler which never finishes, the
// iframe's process eventually exits.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerUnloadBrowserTest,
                       SubframeProcessGoesAwayAfterUnloadTimeout) {
  // See BackForwardCache::DisableForTestingReason for explanation.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_USES_UNLOAD_EVENT);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderFrameHostImpl* child_rfh = root->child_at(0)->current_frame_host();

  // Add an unload handler which never finishes to b.com subframe.
  AddUnloadHandler(child_rfh, "while(1);");

  // Navigate the main frame to c.com and wait for the subframe process to
  // shut down.  This should happen when the subframe unload timeout happens,
  // roughly in one second.  Note that depending on whether site isolation is
  // enabled, the subframe process may or may not be the same as the old main
  // frame process, but it should shut down regardless.
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  RenderProcessHostWatcher process_exit_observer(
      child_rfh->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(NavigateToURL(shell(), c_url));
  process_exit_observer.Wait();
}

// Verify that when an OOPIF with an unload handler navigates cross-process,
// its unload handler is able to send a postMessage to the parent frame.
// See https://crbug.com/857274.
#if defined(OS_MACOSX) || (defined(OS_WIN) && defined(ADDRESS_SANITIZER))
#define MAYBE_PostMessageToParentWhenSubframeNavigates \
  DISABLED_PostMessageToParentWhenSubframeNavigates
#else
#define MAYBE_PostMessageToParentWhenSubframeNavigates \
  PostMessageToParentWhenSubframeNavigates
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerUnloadBrowserTest,
                       MAYBE_PostMessageToParentWhenSubframeNavigates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child = root->child_at(0);

  // Add an onmessage listener in the main frame.
  EXPECT_TRUE(ExecuteScript(root, R"(
      window.addEventListener('message', function(e) {
        domAutomationController.send(e.data);
      });)"));

  // Add an unload handler in the child frame to send a postMessage to the
  // parent frame.
  AddUnloadHandler(child->current_frame_host(),
                   "parent.postMessage('foo', '*')");

  // Navigate the subframe cross-site to c.com and wait for the message.
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  std::string message;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root,
      base::StringPrintf("document.querySelector('iframe').src = '%s';",
                         c_url.spec().c_str()),
      &message));
  EXPECT_EQ("foo", message);

  // Now repeat the test with a remote-to-local navigation that brings the
  // subframe back to a.com.
  AddUnloadHandler(child->current_frame_host(),
                   "parent.postMessage('bar', '*')");
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root,
      base::StringPrintf("document.querySelector('iframe').src = '%s';",
                         a_url.spec().c_str()),
      &message));
  EXPECT_EQ("bar", message);
}

// Ensure that when a pending delete RenderFrameHost's process dies, the
// current RenderFrameHost does not lose its child frames.  See
// https://crbug.com/867274.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerUnloadBrowserTest,
                       PendingDeleteRFHProcessShutdownDoesNotRemoveSubframes) {
  GURL first_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), first_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderFrameHostImpl* rfh = root->current_frame_host();

  // Set up an unload handler which never finishes to force |rfh| to stay
  // around in pending delete state and never receive the swapout ACK.
  EXPECT_TRUE(
      ExecuteScript(rfh, "window.onunload = function(e) { while(1); };\n"));
  rfh->DisableSwapOutTimerForTesting();

  // Navigate to another page with two subframes.
  RenderFrameDeletedObserver rfh_observer(rfh);
  GURL second_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), second_url));

  // At this point, |rfh| should still be live and pending deletion.
  EXPECT_FALSE(rfh_observer.deleted());
  EXPECT_FALSE(rfh->is_active());
  EXPECT_TRUE(rfh->IsRenderFrameLive());

  // Meanwhile, the new page should have two subframes.
  EXPECT_EQ(2U, root->child_count());

  // Kill the pending delete RFH's process.
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  // The process kill should simulate a swapout ACK and trigger destruction of
  // the pending delete RFH.
  rfh_observer.WaitUntilDeleted();

  // Ensure that the process kill didn't incorrectly remove subframes from the
  // new page.
  ASSERT_EQ(2U, root->child_count());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(1)->current_frame_host()->IsRenderFrameLive());
}

namespace {

// A helper to post a recurring check that a renderer process is foregrounded.
// The recurring check uses WeakPtr semantic and will die when this class goes
// out of scope.
class AssertForegroundHelper {
 public:
  AssertForegroundHelper() {}

#if defined(OS_MACOSX)
  // Asserts that |renderer_process| isn't backgrounded and reposts self to
  // check again shortly. |renderer_process| must outlive this
  // AssertForegroundHelper instance.
  void AssertForegroundAndRepost(const base::Process& renderer_process,
                                 base::PortProvider* port_provider) {
    ASSERT_FALSE(renderer_process.IsProcessBackgrounded(port_provider));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AssertForegroundHelper::AssertForegroundAndRepost,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::cref(renderer_process), port_provider),
        base::TimeDelta::FromMilliseconds(1));
  }
#else   // defined(OS_MACOSX)
  // Same as above without the Mac specific base::PortProvider.
  void AssertForegroundAndRepost(const base::Process& renderer_process) {
    ASSERT_FALSE(renderer_process.IsProcessBackgrounded());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AssertForegroundHelper::AssertForegroundAndRepost,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::cref(renderer_process)),
        base::TimeDelta::FromMilliseconds(1));
  }
#endif  // defined(OS_MACOSX)

 private:
  base::WeakPtrFactory<AssertForegroundHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssertForegroundHelper);
};

}  // namespace

// This is a regression test for https://crbug.com/560446. It ensures the
// newly launched process for cross-process navigation in the foreground
// WebContents isn't backgrounded prior to the navigation committing and a
// "visible" widget being added to the process. This test discards the spare
// RenderProcessHost if present, to ensure that it is not used in the
// cross-process navigation.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostManagerTest,
    ForegroundNavigationIsNeverBackgroundedWithoutSpareProcess) {
  StartEmbeddedServer();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

#if defined(OS_MACOSX)
  base::PortProvider* port_provider =
      BrowserChildProcessHost::GetPortProvider();
#endif  //  defined(OS_MACOSX)

  // Start off navigating to a.com and capture the process used to commit.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderProcessHost* start_rph = web_contents->GetMainFrame()->GetProcess();

  // Discard the spare RenderProcessHost to ensure a new RenderProcessHost
  // is created and has the right prioritization.
  RenderProcessHostImpl::DiscardSpareRenderProcessHostForTesting();
  EXPECT_FALSE(RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());

  // Start a navigation to b.com to ensure a cross-process navigation is
  // in progress and ensure the process for the speculative host is different.
  GURL url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  content::TestNavigationManager navigation_manager(web_contents, url);

  shell()->LoadURL(url);
  RenderProcessHost* speculative_rph = web_contents->GetFrameTree()
                                           ->root()
                                           ->render_manager()
                                           ->speculative_frame_host()
                                           ->GetProcess();
  EXPECT_NE(start_rph, speculative_rph);
  EXPECT_FALSE(speculative_rph->IsReady());

#if !defined(OS_ANDROID)
  // TODO(gab, nasko): On Android IsProcessBackgrounded is currently giving
  // incorrect value at this stage of the process lifetime. This should be
  // fixed in follow up cleanup work. See https://crbug.com/560446.
  EXPECT_FALSE(speculative_rph->IsProcessBackgrounded());
#endif

  // Wait for the underlying OS process to have launched and be ready to
  // receive IPCs.
  RenderProcessHostWatcher process_observer(
      speculative_rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
  process_observer.Wait();

  // Kick off an infinite check against self that the process used for
  // navigation is never backgrounded. The WaitForNavigationFinished will wait
  // inside a RunLoop() and hence perform this check regularly throughout the
  // navigation.
  const base::Process& process = speculative_rph->GetProcess();
  EXPECT_TRUE(process.IsValid());
  AssertForegroundHelper assert_foreground_helper;
#if defined(OS_MACOSX)
  assert_foreground_helper.AssertForegroundAndRepost(process, port_provider);
#else
  assert_foreground_helper.AssertForegroundAndRepost(process);
#endif

  // The process should be foreground priority before commit because it is
  // pending, and foreground after commit because it has a visible widget.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_NE(start_rph, web_contents->GetMainFrame()->GetProcess());
  EXPECT_EQ(speculative_rph, web_contents->GetMainFrame()->GetProcess());
}

// Similar to the test above, but verifies the spare RenderProcessHost uses the
// right priority.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostManagerTest,
    ForegroundNavigationIsNeverBackgroundedWithSpareProcess) {
  // This test applies only when spare RenderProcessHost is enabled and in use.
  if (!RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes())
    return;

  StartEmbeddedServer();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

#if defined(OS_MACOSX)
  base::PortProvider* port_provider =
      BrowserChildProcessHost::GetPortProvider();
#endif  //  defined(OS_MACOSX)

  // Start off navigating to a.com and capture the process used to commit.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderProcessHost* start_rph = web_contents->GetMainFrame()->GetProcess();

  // At this time, there should be a spare RenderProcesHost. Capture it for
  // testing expectations later.
  RenderProcessHost* spare_rph =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_TRUE(spare_rph);
  EXPECT_TRUE(spare_rph->IsProcessBackgrounded());

  // Start a navigation to b.com to ensure a cross-process navigation is
  // in progress and ensure the process for the speculative host is
  // different, but matches the spare RenderProcessHost.
  GURL url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  content::TestNavigationManager navigation_manager(web_contents, url);

  shell()->LoadURL(url);
  RenderProcessHost* speculative_rph = web_contents->GetFrameTree()
                                           ->root()
                                           ->render_manager()
                                           ->speculative_frame_host()
                                           ->GetProcess();
  EXPECT_NE(start_rph, speculative_rph);

  // In this test case, the spare RenderProcessHost will be used, so verify it
  // and ensure it is ready.
  EXPECT_EQ(spare_rph, speculative_rph);
  EXPECT_TRUE(spare_rph->IsReady());

  // The creation of the speculative RenderFrameHost should change the
  // RenderProcessHost's copy of the priority of the spare process from
  // background to foreground.
  EXPECT_FALSE(spare_rph->IsProcessBackgrounded());

  // The OS process itself is updated on the process launcher thread, so it
  // cannot be observed immediately here. Perform a thread hop to and back to
  // allow for the priority change to occur before using the
  // AssertForegroundHelper object to check the OS process priority.
  {
    base::RunLoop run_loop;
    GetProcessLauncherTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Kick off an infinite check against self that the process used for
  // navigation is never backgrounded. The WaitForNavigationFinished will wait
  // inside a RunLoop() and hence perform this check regularly throughout the
  // navigation.
  const base::Process& process = spare_rph->GetProcess();
  EXPECT_TRUE(process.IsValid());
  AssertForegroundHelper assert_foreground_helper;
#if defined(OS_MACOSX)
  assert_foreground_helper.AssertForegroundAndRepost(process, port_provider);
#else
  assert_foreground_helper.AssertForegroundAndRepost(process);
#endif

  // The process should be foreground priority before commit because it is
  // pending, and foreground after commit because it has a visible widget.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_NE(start_rph, web_contents->GetMainFrame()->GetProcess());
  EXPECT_EQ(speculative_rph, web_contents->GetMainFrame()->GetProcess());
}

namespace {

// ContentBrowserClient that skips assigning a site URL for all URLs that match
// a given URL's scheme and host.
class DontAssignSiteContentBrowserClient : public TestContentBrowserClient {
 public:
  // Any visit to |url_to_skip| will not cause the site to be assigned to the
  // SiteInstance.
  explicit DontAssignSiteContentBrowserClient(const GURL& url_to_skip)
      : url_to_skip_(url_to_skip) {}

  bool ShouldAssignSiteForURL(const GURL& url) override {
    return url.host() != url_to_skip_.host() ||
           url.scheme() != url_to_skip_.scheme();
  }

 private:
  GURL url_to_skip_;

  DISALLOW_COPY_AND_ASSIGN(DontAssignSiteContentBrowserClient);
};

}  // namespace

// Ensure that coming back to a NavigationEntry with a previously unassigned
// SiteInstance (which is now used for another site) properly switches processes
// and SiteInstances.  See https://crbug.com/945399.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       NavigateWithUnassignedSiteInstance) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to a URL that does not assign site URLs.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  DontAssignSiteContentBrowserClient content_browser_client(url1);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_EQ(url1, web_contents->GetLastCommittedURL());
  scoped_refptr<SiteInstanceImpl> instance1(
      web_contents->GetMainFrame()->GetSiteInstance());
  RenderProcessHost* process1 = instance1->GetProcess();
  EXPECT_EQ(GURL(), instance1->GetSiteURL());

  // Navigate to foo.com, which uses the previous SiteInstance and sets its site
  // URL.
  GURL url2(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_EQ(instance1, web_contents->GetMainFrame()->GetSiteInstance());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(instance1->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(GURL("http://foo.com"), instance1->GetSiteURL());
  }

  // The previously committed entry should get a new, related instance to avoid
  // a SiteInstance mismatch when returning to it. See http://crbug.com/992198
  // for further context.
  SiteInstanceImpl* prev_entry_instance = web_contents->GetController()
                                              .GetEntryAtIndex(0)
                                              ->root_node()
                                              ->frame_entry->site_instance();
  EXPECT_NE(prev_entry_instance, instance1);
  EXPECT_NE(prev_entry_instance, nullptr);
  EXPECT_TRUE(prev_entry_instance->IsRelatedSiteInstance(instance1.get()));
  EXPECT_EQ(GURL(), prev_entry_instance->GetSiteURL());

  // Navigate to bar.com, which destroys the previous RenderProcessHost.
  GURL url3(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  RenderProcessHostWatcher exit_observer(
      process1, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(NavigateToURL(shell(), url3));
  exit_observer.Wait();

  if (AreDefaultSiteInstancesEnabled()) {
    // Verify that the new navigation also results in a default SiteInstance,
    // and verify that it is not related to |instance1| because the navigation
    // swapped to a new BrowsingInstance.
    EXPECT_TRUE(web_contents->GetMainFrame()
                    ->GetSiteInstance()
                    ->IsDefaultSiteInstance());
    EXPECT_FALSE(instance1->IsRelatedSiteInstance(
        web_contents->GetMainFrame()->GetSiteInstance()));
  } else {
    EXPECT_NE(instance1, web_contents->GetMainFrame()->GetSiteInstance());
  }

  // At this point, process1 is deleted, and the first entry is unfortunately
  // pointing to instance1, which has been locked to url2 and has no process.
  EXPECT_FALSE(instance1->HasProcess());
  if (AreAllSitesIsolatedForTesting()) {
    // In site-per-process, we cannot use foo.com's SiteInstance for a.com.
    EXPECT_FALSE(instance1->IsSuitableForURL(url1));
  } else if (AreDefaultSiteInstancesEnabled()) {
    // Since |instance1| is a default SiteInstance AND this test explicitly
    // ensures that ShouldAssignSiteForURL(url1) will return false, |url1|
    // cannot be placed in the default SiteInstance. This also means that |url1|
    // cannot be placed in the same process as the default SiteInstance.
    EXPECT_FALSE(instance1->IsSuitableForURL(url1));
  } else {
    // If neither foo.com nor a.com require dedicated processes, then we can use
    // the same process.
    EXPECT_TRUE(instance1->IsSuitableForURL(url1));
  }

  // Go back to url1's entry, which should swap to a new SiteInstance with an
  // unused site URL.
  TestNavigationObserver observer(web_contents);
  web_contents->GetController().GoToOffset(-2);
  observer.Wait();
  scoped_refptr<SiteInstanceImpl> new_instance =
      web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_EQ(url1, web_contents->GetLastCommittedURL());
  if (AreAllSitesIsolatedForTesting() || AreDefaultSiteInstancesEnabled()) {
    EXPECT_NE(instance1, new_instance);
    EXPECT_EQ(GURL(), new_instance->GetSiteURL());
  } else {
    EXPECT_EQ(instance1, new_instance);
  }
  EXPECT_TRUE(new_instance->HasProcess());

  // Because url1 does not set a site URL, it should not lock the new process
  // either, so that it can be used for subsequent navigations.
  content::RenderProcessHost* new_process = new_instance->GetProcess();
  auto* policy = ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(policy->CanAccessDataForOrigin(new_process->GetID(), url1));
  EXPECT_TRUE(policy->CanAccessDataForOrigin(new_process->GetID(), url2));

  SetBrowserClientForTesting(old_client);
}

namespace {

// A helper class to run a predefined callback just before processing the
// DidCommitProvisionalLoad IPC for |deferred_url|.
class CommitMessageDelayer : public DidCommitNavigationInterceptor {
 public:
  using DidCommitCallback = base::OnceCallback<void(RenderFrameHost*)>;

  explicit CommitMessageDelayer(WebContents* web_contents,
                                const GURL& deferred_url,
                                DidCommitCallback deferred_action)
      : DidCommitNavigationInterceptor(web_contents),
        deferred_url_(deferred_url),
        deferred_action_(std::move(deferred_action)) {}

  void Wait() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (params->url == deferred_url_) {
      std::move(deferred_action_).Run(render_frame_host);
      if (run_loop_)
        run_loop_->Quit();
    }
    return true;
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  const GURL deferred_url_;
  DidCommitCallback deferred_action_;

  DISALLOW_COPY_AND_ASSIGN(CommitMessageDelayer);
};

}  // namespace

// Check that when a navigation to a URL that doesn't require assigning a site
// URL is in progress, another navigation can't reuse the same process in the
// meantime.  Such reuse previously led to a renderer kill when the siteless
// URL later committed; a real-world example of the siteless URL was
// chrome-native://newtab.  See https://crbug.com/970046.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       NavigationRacesWithCommitInUnassignedSiteInstance) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up a URL for which ShouldAssignSiteForURL will return false.  The
  // corresponding SiteInstance's site will be left unassigned, and its process
  // won't be locked.  The test will navigate to this URL first.
  GURL siteless_url(
      embedded_test_server()->GetURL("siteless.com", "/title1.html"));
  DontAssignSiteContentBrowserClient content_browser_client(siteless_url);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Prepare for a second navigation to a normal URL.  Ensure it's isolated so
  // that it requires a process lock on all platforms.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins(
      {url::Origin::Create(foo_url)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  // Create a new shell where the foo.com origin isolation will take effect.
  Shell* shell = CreateBrowser();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  RenderProcessHost* foo_process = nullptr;
  TestNavigationManager foo_manager(web_contents, foo_url);
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      GURL("http://foo.com")));

  // Set up the work to be done after the renderer is asked to commit
  // |siteless_url|, but before the corresponding DidCommitProvisionalLoad IPC
  // is processed.  This will start a navigation to |foo_url| and wait for its
  // response.
  auto did_commit_callback =
      base::BindLambdaForTesting([&](RenderFrameHost* rfh) {
        // The navigation should stay in the initial empty SiteInstance, with
        // the site still unassigned.
        EXPECT_FALSE(
            static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance())->HasSite());
        EXPECT_FALSE(root->render_manager()->speculative_frame_host());

        shell->LoadURL(foo_url);

        // The foo.com navigation should swap to a new process, since it is not
        // safe to reuse |siteless_url|'s process before |siteless_url|
        // commits.
        EXPECT_TRUE(root->render_manager()->speculative_frame_host());
        foo_process =
            root->render_manager()->speculative_frame_host()->GetProcess();

        // Wait for response.  This will cause |foo_manager| to spin up a
        // nested message loop while we're blocked in the current message loop
        // (within DidCommitNavigationInterceptor).  Thus, it's important to
        // allow nestable tasks in |foo_manager|'s message loop, so that it can
        // process the response before we unblock the
        // DidCommitNavigationInterceptor's message loop and finish processing
        // the commit.
        foo_manager.AllowNestableTasks();
        EXPECT_TRUE(foo_manager.WaitForResponse());

        foo_manager.ResumeNavigation();
        // After returning here, the commit for |siteless_url| will be
        // processed.
      });

  CommitMessageDelayer commit_delayer(web_contents,
                                      siteless_url /* deferred_url */,
                                      std::move(did_commit_callback));

  // Start the first navigation, which does not assign a site URL.
  base::HistogramTester histograms;
  shell->LoadURL(siteless_url);

  // The navigation should stay in the initial empty SiteInstance, so there
  // shouldn't be a speculative RFH at this point.
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());

  // Wait for the DidCommit IPC for |siteless_url|, and before processing it,
  // trigger a navigation to |foo_url| and wait for its response.
  commit_delayer.Wait();

  // Check that the renderer hasn't been killed.  At this point, it should've
  // successfully committed the navigation to |siteless_url|, and it shouldn't
  // be locked.
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(siteless_url, web_contents->GetMainFrame()->GetLastCommittedURL());
  RenderProcessHost* process1 = web_contents->GetMainFrame()->GetProcess();
  EXPECT_FALSE(web_contents->GetMainFrame()->GetSiteInstance()->HasSite());
  EXPECT_EQ(GURL(), policy->GetOriginLock(process1->GetID()));

  // Now wait for second navigation to finish and ensure it also succeeds.
  foo_manager.WaitForNavigationFinished();
  EXPECT_TRUE(foo_manager.was_successful());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(foo_url, web_contents->GetMainFrame()->GetLastCommittedURL());

  // The foo.com navigation should've used a different process, locked to
  // foo.com.
  RenderProcessHost* process2 = web_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(process1, process2);
  EXPECT_EQ(GURL("http://foo.com"),
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(GURL("http://foo.com"), policy->GetOriginLock(process2->GetID()));

  // Ensure also that the foo.com process didn't change midway through the
  // navigation.
  EXPECT_EQ(foo_process, process2);

  // Ensure we've logged the UMA for disallowing problematic process reuse.
  // Since IsSuitableHost() is checked multiple times during a particular
  // navigation, just make sure that this is logged at least once.
  EXPECT_GE(histograms.GetBucketCount(
                "SiteIsolation.PendingSitelessNavigationDisallowsProcessReuse",
                1 /* has_disqualifying_pending_navigation */),
            1);

  SetBrowserClientForTesting(old_client);
}

// When ProactivelySwapBrowsingInstance is enabled, the browser switch to a new
// BrowsingInstance on cross-site HTTP(S) main frame navigations, when there are
// no other windows in the BrowsingInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ProactivelySwapBrowsingInstance) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstance> a_site_instance =
      web_contents->GetMainFrame()->GetSiteInstance();

  // Navigate to B. The navigation is document initiated. It swaps
  // BrowsingInstance only if  ProactivelySwapBrowsingInstance is enabled.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location.href = $1", b_url)));
  WaitForLoadStop(web_contents);
  scoped_refptr<SiteInstance> b_site_instance =
      web_contents->GetMainFrame()->GetSiteInstance();

  if (IsProactivelySwapBrowsingInstanceEnabled())
    EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  else
    EXPECT_TRUE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
}

// Tests specific to the "default process" mode (which creates strict
// SiteInstances that can share a default process per BrowsingInstance).
class RenderFrameHostManagerDefaultProcessTest
    : public RenderFrameHostManagerTest {
 public:
  RenderFrameHostManagerDefaultProcessTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kProcessSharingWithStrictSiteInstances);
  }
  ~RenderFrameHostManagerDefaultProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    RenderFrameHostManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);

    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING) << "This test should be run without strict site isolation. "
                   << "It does nothing when --site-per-process is specified.";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManagerDefaultProcessTest);
};

// Ensure that the default process can be used for URLs that don't assign a site
// to the SiteInstance, when Site Isolation is not enabled.
// 1. Visit foo.com.
// 2. Start to navigate to a siteless URL.
// 3. When the commit is pending, start a navigation to bar.com in a popup.
// (Using a popup avoids a crash when replacting the speculative RFH, per
// https://crbug.com/838348.)
// All navigations should use the default process, and we should not crash.
// See https://crbug.com/977956.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerDefaultProcessTest,
                       NavigationRacesWithSitelessCommitInDefaultProcess) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));

  // Step 1: Visit foo.com in the default process.
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  RenderProcessHost* original_process =
      web_contents->GetMainFrame()->GetProcess();
  EXPECT_EQ(original_process, web_contents->GetMainFrame()
                                  ->GetSiteInstance()
                                  ->GetDefaultProcessIfUsable());

  // This test expect a cross-site navigation to by same BrowsingInstance. With
  // ProactivelySwapBrowsingInstance, it won't be the case. Opening a popup
  // prevent the BrowsingInstance to change.
  if (IsProactivelySwapBrowsingInstanceEnabled()) {
    GURL popup_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(OpenPopup(web_contents->GetMainFrame(), popup_url, ""));
  }

  // Set up a URL for which ShouldAssignSiteForURL will return false.  The
  // corresponding SiteInstance's site will be left unassigned, and its process
  // won't be locked.
  GURL siteless_url(
      embedded_test_server()->GetURL("siteless.com", "/title1.html"));
  DontAssignSiteContentBrowserClient content_browser_client(siteless_url);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Set up the work to be done after the renderer is asked to commit
  // |siteless_url|, but before the corresponding DidCommitProvisionalLoad IPC
  // is processed.  This will start a navigation to |bar_url| in a popup and
  // wait for its response.  We use a popup to avoid trampling the speculative
  // RFH while it is committing (per https://crbug.com/838348).
  auto did_commit_callback =
      base::BindLambdaForTesting([&](RenderFrameHost* rfh) {
        Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
        EXPECT_TRUE(new_shell);

        // Step 3: Navigate to bar.com in the same BrowsingInstance, while the
        // commit to siteless_url is pending.  This used to crash because it
        // picked the default process, but IsSuitableHost said it was not ok due
        // to the pending siteless URL commit (in https://crbug.com/977956).
        TestNavigationManager bar_manager(new_shell->web_contents(), bar_url);
        EXPECT_TRUE(ExecuteScript(new_shell,
                                  base::StringPrintf("location.href = '%s'",
                                                     bar_url.spec().c_str())));

        // Wait for response.  This will cause |bar_manager| to spin up a
        // nested message loop while we're blocked in the current message loop
        // (within DidCommitNavigationInterceptor).  Thus, it's important to
        // allow nestable tasks in |bar_manager|'s message loop, so that it can
        // process the response before we unblock the
        // DidCommitNavigationInterceptor's message loop and finish processing
        // the commit.
        bar_manager.AllowNestableTasks();
        EXPECT_TRUE(bar_manager.WaitForResponse());

        bar_manager.ResumeNavigation();

        // After returning here, the commit for |siteless_url| will be
        // processed.
      });

  // Step 2: Visit siteless_url in the same BrowsingInstance, but wait before
  // the commit IPC is processed.  (See did_commit_callback above for step 3.)
  CommitMessageDelayer commit_delayer(web_contents,
                                      siteless_url /* deferred_url */,
                                      std::move(did_commit_callback));
  EXPECT_TRUE(ExecuteScript(
      shell(),
      base::StringPrintf("location.href = '%s'", siteless_url.spec().c_str())));

  // Wait for the DidCommit IPC for |siteless_url|, and before processing it,
  // trigger a navigation to |foo_url| and wait for its response.
  commit_delayer.Wait();

  EXPECT_EQ(original_process, web_contents->GetMainFrame()->GetProcess());

  SetBrowserClientForTesting(old_client);
}

}  // namespace content
