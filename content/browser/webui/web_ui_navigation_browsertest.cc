// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc_security_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"
#include "url/url_constants.h"

namespace content {

namespace {

const char kAddIframeScript[] =
    "var frame = document.createElement('iframe');\n"
    "frame.src = $1;\n"
    "document.body.appendChild(frame);\n";

const char kAdditionalScheme[] = "test-webui-scheme";

blink::mojom::OpenURLParamsPtr CreateOpenURLParams(const GURL& url) {
  auto params = blink::mojom::OpenURLParams::New();
  params->url = url;
  params->disposition = WindowOpenDisposition::CURRENT_TAB;
  params->should_replace_current_entry = false;
  params->user_gesture = true;
  return params;
}

bool DoesURLRequireDedicatedProcess(const IsolationContext& isolation_context,
                                    const GURL& url) {
  return SiteInfo::CreateForTesting(isolation_context, url)
      .RequiresDedicatedProcess(isolation_context);
}

}  // namespace

class WebUINavigationBrowserTest : public ContentBrowserTest {
 public:
  WebUINavigationBrowserTest() = default;

  WebUINavigationBrowserTest(const WebUINavigationBrowserTest&) = delete;
  WebUINavigationBrowserTest& operator=(const WebUINavigationBrowserTest&) =
      delete;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Verify that a document running in a process that has WebUI bindings,
  // regardless of scheme, can navigate an iframe to web content and the
  // resulting document is properly site isolated.
  // Note: The goal of test is to verify that isolation works correctly even
  // if somehow non-WebUI scheme gets granted WebUI bindings. See also
  // WebFrameInChromeSchemeIsAllowed, which tests the more typical case of a
  // WebUI scheme embedding a web iframe.
  void TestWebFrameInProcessWithWebUIBindings(BindingsPolicySet bindings) {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    // Start navigating to foo.com in the main frame.
    GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), foo_url));
    EXPECT_EQ(foo_url, root->current_frame_host()->GetLastCommittedURL());
    EXPECT_FALSE(
        ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
            root->current_frame_host()->GetProcess()->GetID()));

    // Grant WebUI bindings to the process. This will ensure that if there is
    // a mistake in the navigation logic and a process gets somehow WebUI
    // bindings, the web content is correctly isolated regardless of the scheme
    // of the parent document.
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        root->current_frame_host()->GetProcess()->GetID(), bindings);
    EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
        root->current_frame_host()->GetProcess()->GetID()));
    {
      GURL web_url(embedded_test_server()->GetURL("/title2.html"));
      std::string script = base::StringPrintf(
          "var frame = document.createElement('iframe');\n"
          "frame.src = '%s';\n"
          "document.body.appendChild(frame);\n",
          web_url.spec().c_str());

      TestNavigationObserver navigation_observer(shell()->web_contents());
      EXPECT_TRUE(ExecJs(shell(), script));
      navigation_observer.Wait();

      EXPECT_EQ(1U, root->child_count());
      EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
      EXPECT_EQ(web_url, navigation_observer.last_navigation_url());
      EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
                root->child_at(0)->current_frame_host()->GetSiteInstance());
      EXPECT_FALSE(
          ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
              root->child_at(0)->current_frame_host()->GetProcess()->GetID()));
    }
  }

  // Verify that a WebUI document in a subframe is allowed to target a new
  // window and navigate it to web content.
  void TestWebUISubframeNewWindowToWebAllowed(BindingsPolicySet bindings) {
    GURL main_frame_url(
        GetWebUIURL("web-ui/page_with_blank_iframe.html?bindings=" +
                    base::NumberToString(bindings.ToEnumBitmask())));
    EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    EXPECT_EQ(1U, root->child_count());
    FrameTreeNode* child = root->child_at(0);

    EXPECT_EQ(bindings, root->current_frame_host()->GetEnabledBindings());
    EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
    RenderFrameHost* webui_rfh = root->current_frame_host();
    EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
        webui_rfh->GetProcess()->GetID()));

    // Navigate the subframe to the same WebUI.
    {
      TestFrameNavigationObserver observer(root->child_at(0));
      GURL subframe_url(
          GetWebUIURL("web-ui/title1.html?noxfo=true&bindings=" +
                      base::NumberToString(bindings.ToEnumBitmask())));
      NavigateFrameToURL(root->child_at(0), subframe_url);

      EXPECT_TRUE(observer.last_navigation_succeeded());
      EXPECT_EQ(subframe_url, observer.last_committed_url());
    }

    // Add a link that targets a new window and click it.
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    std::string script = JsReplace(
        "var a = document.createElement('a');"
        "a.href = $1; a.target = '_blank'; a.click()",
        web_url.spec().c_str());

    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(root->child_at(0)->current_frame_host(), script,
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    Shell* new_shell = new_shell_observer.GetShell();
    WebContents* new_web_contents = new_shell->web_contents();
    EXPECT_TRUE(WaitForLoadStop(new_web_contents));

    EXPECT_EQ(web_url, new_web_contents->GetLastCommittedURL());

    FrameTreeNode* new_root = static_cast<WebContentsImpl*>(new_web_contents)
                                  ->GetPrimaryFrameTree()
                                  .root();
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              new_root->current_frame_host()->GetSiteInstance());
    EXPECT_NE(root->current_frame_host()->GetProcess(),
              new_root->current_frame_host()->GetProcess());
    EXPECT_NE(root->current_frame_host()->web_ui(),
              new_root->current_frame_host()->web_ui());
    EXPECT_NE(root->current_frame_host()->GetEnabledBindings(),
              new_root->current_frame_host()->GetEnabledBindings());
    EXPECT_FALSE(
        root->current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
            new_root->current_frame_host()->GetSiteInstance()));
  }

  void TestEmbeddingIFrameFailed(const GURL& embedder_url,
                                 const GURL& iframe_url) {
    ASSERT_TRUE(NavigateToURL(shell()->web_contents(), embedder_url));
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, iframe_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded()) << embedder_url;
  }

 private:
  TestWebUIControllerFactory factory_;
  ScopedWebUIControllerFactoryRegistration factory_registration_{&factory_};
};

// Verify that a chrome: scheme document can add iframes with web content, as
// long as X-Frame-Options and the default Content-Security-Policy are
// overridden to allow the frame to be embedded.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInChromeSchemeIsAllowed) {
  // Serve a WebUI with no iframe restrictions.
  GURL main_frame_url(GetWebUIURL("web-ui/title1.html?noxfo=true&childsrc="));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(BindingsPolicySet({BindingsPolicyValue::kWebUi}),
            root->current_frame_host()->GetEnabledBindings());
  EXPECT_EQ(0UL, root->child_count());

  // Navigate to a Web URL and verify that the navigation is allowed.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, web_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(1UL, root->child_count());
  }

  // Navigate to a data URL and verify that the navigation is allowed.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL data_url("data:text/html,foo");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, data_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(2UL, root->child_count());
  }

  // Verify that an iframe with "about:blank" URL is actually allowed. Not
  // sure why this would be useful, but from a security perspective it can
  // only host content coming from the parent document, so it effectively
  // has the same security context.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL about_blank_url("about:blank");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, about_blank_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(3UL, root->child_count());
  }
}

// Verify that a chrome-untrusted:// scheme document can add iframes with web
// content when the CSP allows it. This is different from chrome:// URLs where
// no web content can be loaded, even if the CSP allows it.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInChromeUntrustedSchemeAllowedByCSP) {
  // Add an untrusted WebUI with no iframe restrictions.
  TestUntrustedDataSourceHeaders headers;
  headers.child_src = "child-src * data:;";
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host", headers));

  GURL main_frame_url(GetChromeUntrustedUIURL("test-host/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(root->current_frame_host()->GetEnabledBindings().empty());

  // Add iframe and navigate it to a Web URL and verify that the navigation
  // succeeded.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, web_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Add iframe and navigate it to a data URL and verify that the navigation
  // succeeded.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL data_url("data:text/html,foo");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, data_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Add iframe and navigate it to "about:blank" and verify that the navigation
  // succeeded.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL about_blank_url("about:blank");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, about_blank_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
}

// Verify that a chrome: scheme document cannot add iframes with web content
// and does not crash if the navigation is blocked by CSP.
// See https://crbug.com/944086.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInChromeSchemeDisallowedByCSP) {
  // Use a WebUI with restrictive CSP that disallows subframes. This will cause
  // the navigation to fail due to the CSP check and ensure this behaves the
  // same way as the repro steps in https://crbug.com/944086.
  GURL main_frame_url(
      GetWebUIURL("web-ui/title1.html?childsrc=child-src 'none'"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_EQ(main_frame_url, shell()->web_contents()->GetLastCommittedURL());

  {
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, web_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    navigation_observer.Wait();

    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }
}

// Verify that a chrome-untrusted:// scheme document cannot add iframes with web
// content when the CSP disallows it.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInChromeUntrustedSchemeDisallowedByCSP) {
  // Add an untrusted WebUI which disallows iframes by default.
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));
  GURL main_frame_url(GetChromeUntrustedUIURL("test-host/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(root->current_frame_host()->GetEnabledBindings().empty());

  // Add iframe and navigate it to a Web URL and verify that the navigation was
  // blocked.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL web_url(embedded_test_server()->GetURL("/title2.html"));
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, web_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // Add iframe and navigate it to a data URL and verify that the navigation was
  // blocked.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL data_url("data:text/html,foo");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, data_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // Add iframe and navigate it to a chrome-untrusted URL and verify that the
  // navigation was blocked.
  {
    TestNavigationObserver observer(shell()->web_contents());
    // Add a DataSource for chrome-untrusted:// that can be iframe'd.
    TestUntrustedDataSourceHeaders headers;
    headers.no_xfo = true;
    WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
        std::make_unique<ui::TestUntrustedWebUIConfig>("test-iframe-host",
                                                       headers));
    GURL untrusted_url(GetChromeUntrustedUIURL("test-iframe-host/title1.html"));
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, untrusted_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // Add iframe and verify that an iframe with "about:blank" URL is actually
  // allowed. Not sure why this would be useful, but from a security perspective
  // it can only host content coming from the parent document, so it effectively
  // has the same security context.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL about_blank_url("about:blank");
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, about_blank_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
}

// Verify that a browser check stops websites from embeding chrome:// iframes.
// This tests the OpenURL Mojo method.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       DisallowEmbeddingChromeSchemeFromWebFrameBrowserCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // Add iframe but don't navigate it to a chrome:// URL yet.
  EXPECT_TRUE(ExecJs(shell(),
                     "var frame = document.createElement('iframe');\n"
                     "document.body.appendChild(frame);\n",
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(1U, root->child_count());
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  EXPECT_EQ("about:blank", child->GetLastCommittedURL());

  // Simulate an IPC message to navigate the subframe to a chrome:// URL.
  // This bypasses the renderer-side check that would have stopped the
  // navigation.
  TestNavigationObserver observer(shell()->web_contents());
  static_cast<mojom::FrameHost*>(child)->OpenURL(
      CreateOpenURLParams(GetWebUIURL("web-ui/title1.html?noxfo=true")));
  observer.Wait();

  child = root->child_at(0)->current_frame_host();
  EXPECT_EQ(kBlockedURL, child->GetLastCommittedURL());
}

// Verify that a browser check stops websites from embeding chrome-untrusted://
// iframes. This tests the OpenURL Mojo method path.
IN_PROC_BROWSER_TEST_F(
    WebUINavigationBrowserTest,
    DisallowEmbeddingChromeUntrustedSchemeFromWebFrameBrowserCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  TestUntrustedDataSourceHeaders headers;
  headers.no_xfo = true;
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-iframe-host",
                                                     headers));

  // Add iframe but don't navigate it to a chrome-untrusted:// URL yet.
  EXPECT_TRUE(ExecJs(shell(),
                     "var frame = document.createElement('iframe');\n"
                     "document.body.appendChild(frame);\n",
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(1U, root->child_count());
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  EXPECT_EQ("about:blank", child->GetLastCommittedURL());

  // Simulate a Mojo message to navigate the subframe to a chrome-untrusted://
  // URL.
  TestNavigationObserver observer(shell()->web_contents());
  static_cast<mojom::FrameHost*>(child)->OpenURL(CreateOpenURLParams(
      GetChromeUntrustedUIURL("test-iframe-host/title1.html")));
  observer.Wait();

  child = root->child_at(0)->current_frame_host();
  EXPECT_EQ(kBlockedURL, child->GetLastCommittedURL());
}

// Verify an iframe with no frame ancestors is blocked from being embedded in
// other WebUIs and on the web.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       FrameAncestorsDisallowEmbedding) {
  auto* web_contents = shell()->web_contents();
  // Serve an iframe with no frame ancestors.
  GURL iframe_url(GetWebUIURL("web-ui/title1.html"));

  // Add the iframe to a WebUI with the same origin and verify it was blocked.
  {
    GURL main_frame_url(GetWebUIURL("web-ui/title1.html?childsrc="));
    TestEmbeddingIFrameFailed(main_frame_url, iframe_url);
  }

  // Add the iframe to a WebUI with a different origin and verify it was
  // blocked.
  {
    GURL main_frame_url(GetWebUIURL("different-web-ui/title1.html?childsrc="));
    TestEmbeddingIFrameFailed(main_frame_url, iframe_url);
  }

  // Add the iframe to a web page and verify it was blocked.
  {
    GURL main_frame_url(
        embedded_test_server()->GetURL("/title1.html?childsrc="));
    ASSERT_TRUE(NavigateToURL(web_contents, main_frame_url));

    WebContentsConsoleObserver console_observer(web_contents);
    console_observer.SetPattern("Not allowed to load local resource: " +
                                iframe_url.spec());

    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, iframe_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    ASSERT_TRUE(console_observer.Wait());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents)
                              ->GetPrimaryFrameTree()
                              .root();
    EXPECT_EQ(1U, root->child_count());
    RenderFrameHost* child = root->child_at(0)->current_frame_host();
    EXPECT_EQ(GURL(), child->GetLastCommittedURL());
  }
}

// Verify an iframe with frame ancestors of the same origin can only be embedded
// by itself.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       FrameAncestorsAllowEmbedding) {
  auto* web_contents = shell()->web_contents();
  // Serve an iframe with a frame ancestor that is the same origin as its own
  // URL.
  GURL iframe_url(GetWebUIURL("web-ui/title1.html?frameancestors=" +
                              GetWebUIURLString("web-ui")));

  // Add the iframe to a WebUI with the same origin 'chrome://web-ui' and verify
  // it can be allowed.
  {
    GURL main_frame_url(GetWebUIURL("web-ui/title1.html?childsrc="));
    ASSERT_TRUE(NavigateToURL(web_contents, main_frame_url));
    TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, iframe_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Add the iframe to a WebUI with a different origin
  // 'chrome://different-web-ui' and verify it was blocked.
  {
    GURL main_frame_url(GetWebUIURL("different-web-ui/title1.html?childsrc="));
    TestEmbeddingIFrameFailed(main_frame_url, iframe_url);
  }

  // Add the iframe to a web page and verify it was blocked.
  {
    GURL main_frame_url(
        embedded_test_server()->GetURL("/title1.html?childsrc="));
    EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

    WebContentsConsoleObserver console_observer(web_contents);
    console_observer.SetPattern("Not allowed to load local resource: " +
                                iframe_url.spec());

    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, iframe_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    ASSERT_TRUE(console_observer.Wait());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents)
                              ->GetPrimaryFrameTree()
                              .root();
    EXPECT_EQ(1U, root->child_count());
    RenderFrameHost* child = root->child_at(0)->current_frame_host();
    EXPECT_EQ(GURL(), child->GetLastCommittedURL());
  }
}

// Verify an iframe with a frame ancestor that is a different origin to its own
// URL is allowed to only be embedded in that WebUI.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       FrameAncestorsAllowEmbeddingFromOtherHosts) {
  auto* web_contents = shell()->web_contents();
  // Serve an iframe with frame-ancestor 'chrome://web-ui'.
  GURL iframe_url(GetWebUIURL("different-web-ui/title1.html?frameancestors=" +
                              GetWebUIURLString("web-ui")));

  // Add the iframe to 'chrome://web-ui' WebUI and verify it can be embedded.
  {
    GURL main_frame_url(GetWebUIURL("web-ui/title1.html?childsrc="));
    ASSERT_TRUE(NavigateToURL(web_contents, main_frame_url));
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, iframe_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Add the iframe to 'chrome://different-web-ui' WebUI and verify it was
  // blocked.
  {
    GURL main_frame_url(GetWebUIURL("different-web-ui/title1.html?childsrc="));
    TestEmbeddingIFrameFailed(main_frame_url, iframe_url);
  }

  // Add the iframe to a web page and verify it was blocked.
  {
    GURL main_frame_url(
        embedded_test_server()->GetURL("/title1.html?childsrc="));
    ASSERT_TRUE(NavigateToURL(web_contents, main_frame_url));
    WebContentsConsoleObserver console_observer(web_contents);
    console_observer.SetPattern("Not allowed to load local resource: " +
                                iframe_url.spec());

    EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, iframe_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    ASSERT_TRUE(console_observer.Wait());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents)
                              ->GetPrimaryFrameTree()
                              .root();
    EXPECT_EQ(1U, root->child_count());
    RenderFrameHost* child = root->child_at(0)->current_frame_host();
    EXPECT_EQ(GURL(), child->GetLastCommittedURL());
  }
}

// Verify that default WebUI cannot embed chrome-untrusted: iframes. To allow
// embedding, WebUI needs to call AddRequestableScheme to explicitly allow it.
IN_PROC_BROWSER_TEST_F(
    WebUINavigationBrowserTest,
    ChromeUntrustedFrameInChromeSchemeDisallowedInDefaultWebUI) {
  // Serve a WebUI with no iframe restrictions.
  GURL main_frame_url(GetWebUIURL("web-ui/title1.html?noxfo=true&childsrc="));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(BindingsPolicySet({BindingsPolicyValue::kWebUi}),
            root->current_frame_host()->GetEnabledBindings());
  EXPECT_EQ(0UL, root->child_count());

  // Add a DataSource for chrome-untrusted:// that can be iframe'd.
  TestUntrustedDataSourceHeaders headers;
  headers.no_xfo = true;
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host", headers));
  GURL untrusted_url(GetChromeUntrustedUIURL("test-host/title1.html"));

  // Navigate an iframe to a chrome-untrusted URL and verify that the navigation
  // was blocked. This tests the Frame::BeginNavigation path.
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, untrusted_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  observer.Wait();
  EXPECT_EQ(1UL, root->child_count());

  RenderFrameHost* child = root->child_at(0)->current_frame_host();
  EXPECT_EQ(kBlockedURL, child->GetLastCommittedURL());
}

// Verify that a chrome-untrusted:// scheme iframe can be embedded in chrome://
// frame. The test needs to specify requestableschemes parameter to the main
// frame WebUI URL, which will result in a call to AddRequestableScheme and
// permit the embedding to work.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       ChromeUntrustedFrameInChromeSchemeAllowed) {
  // Serve a WebUI with no iframe restrictions.
  GURL main_frame_url(
      GetWebUIURL("web-ui/"
                  "title1.html?childsrc=&requestableschemes=chrome-untrusted"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* webui_rfh = root->current_frame_host();
  scoped_refptr<SiteInstanceImpl> webui_site_instance =
      webui_rfh->GetSiteInstance();

  EXPECT_EQ(main_frame_url, webui_rfh->GetLastCommittedURL());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      webui_rfh->GetProcess()->GetID()));
  EXPECT_FALSE(
      webui_site_instance->GetSiteInfo().process_lock_url().is_empty());
  EXPECT_EQ(root->current_frame_host()->GetProcess()->GetProcessLock(),
            ProcessLock::FromSiteInfo(webui_site_instance->GetSiteInfo()));

  TestUntrustedDataSourceHeaders headers;
  std::vector<std::string> frame_ancestors({"chrome://web-ui"});
  headers.frame_ancestors =
      std::make_optional<std::vector<std::string>>(std::move(frame_ancestors));

  // Add a DataSource for the chrome-untrusted:// iframe with frame ancestor
  // chrome://web-ui.
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host", headers));
  GURL untrusted_url(GetChromeUntrustedUIURL("test-host/title1.html"));
  TestNavigationObserver observer(shell()->web_contents());

  // Add the iframe to the chrome://web-ui WebUI and verify it was successfully
  // embedded.
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, untrusted_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(untrusted_url,
            root->child_at(0)->current_frame_host()->GetLastCommittedURL());
}

// Verify that a renderer check stops websites from embeding chrome:// iframes.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       DisallowEmbeddingChromeSchemeFromWebFrameRendererCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL webui_url(GetWebUIURL("web-ui/title1.html?noxfo=true"));
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("Not allowed to load local resource: " +
                              webui_url.spec());

  // Add iframe and navigate it to a chrome:// URL and verify that the
  // navigation was blocked.
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, webui_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  ASSERT_TRUE(console_observer.Wait());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(1U, root->child_count());
  RenderFrameHost* child = root->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL(), child->GetLastCommittedURL());
}

const char kOpenUrlViaClickTargetFunc[] =
    "(function(url) {\n"
    "  var lnk = document.createElement(\"a\");\n"
    "  lnk.href = url;\n"
    "  lnk.rel = 'opener';\n"
    "  lnk.target = \"_blank\";\n"
    "  document.body.appendChild(lnk);\n"
    "  lnk.click();\n"
    "})";

// Adds a link with given url and target=_blank, and clicks on it.
void OpenUrlViaClickTarget(const ToRenderFrameHost& adapter, const GURL& url) {
  EXPECT_TRUE(ExecJs(adapter, std::string(kOpenUrlViaClickTargetFunc) + "(\"" +
                                  url.spec() + "\");"));
}

// Verify that two WebUIs with a shared domain have different SiteInstance
// and BrowsingInstance even when the WebUI is opened from the other WebUI. Even
// though they share a domain, their hosts are different, so they have different
// WebUI types which triggers a BrowsingInstance swap.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       SharedDomainDifferentSiteInstanceNavigation) {
  GURL url1(
      "chrome://foo.web-ui/title1.html?bindings=" +
      base::NumberToString(
          BindingsPolicySet({BindingsPolicyValue::kWebUi}).ToEnumBitmask()));
  GURL url2(
      "chrome://bar.web-ui/title1.html?bindings=" +
      base::NumberToString(
          BindingsPolicySet({BindingsPolicyValue::kWebUi}).ToEnumBitmask()));

  // Visit a WebUI page with bindings.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));
  SiteInstance* site_instance1 = shell()->web_contents()->GetSiteInstance();
  int process1_id = site_instance1->GetProcess()->GetID();

  // Visit the second WebUI page with bindings. Even though the navigation
  // itself doesn't intend to swap BrowsingInstances, we still swap them due to
  // a change in WebUI type.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(shell(), url2));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));
  SiteInstance* site_instance2 = shell()->web_contents()->GetSiteInstance();
  int process2_id = site_instance2->GetProcess()->GetID();

  // The 2nd WebUI page should swap to a different process, SiteInstance,
  // and BrowsingInstance.
  EXPECT_NE(process1_id, process2_id);
  EXPECT_NE(site_instance2, site_instance1);
  EXPECT_NE(static_cast<SiteInstanceImpl*>(site_instance2)->group(),
            static_cast<SiteInstanceImpl*>(site_instance1)->group());
  EXPECT_FALSE(site_instance2->IsRelatedSiteInstance(site_instance1));
}

// Verify that two WebUIs with a shared domain have different SiteInstance
// and BrowsingInstance even when the WebUI is opened from the other WebUI. Even
// though they share a domain, their hosts are different, so they have different
// WebUI types which triggers a BrowsingInstance swap.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       SharedDomainDifferentSiteInstanceUrlClick) {
  GURL url1(
      "chrome://foo.web-ui/title1.html?bindings=" +
      base::NumberToString(
          BindingsPolicySet({BindingsPolicyValue::kWebUi}).ToEnumBitmask()));
  GURL url2(
      "chrome://bar.web-ui/title1.html?bindings=" +
      base::NumberToString(
          BindingsPolicySet({BindingsPolicyValue::kWebUi}).ToEnumBitmask()));

  // Visit a WebUI page with bindings.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID()));
  SiteInstance* site_instance1 = shell()->web_contents()->GetSiteInstance();
  int process1_id = site_instance1->GetProcess()->GetID();

  // Open a new tab.
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

  // The 2nd WebUI page should swap to a different process, SiteInstance,
  // and BrowsingInstance.
  EXPECT_NE(process1_id, process2_id);
  EXPECT_NE(site_instance2, site_instance1);
  EXPECT_NE(static_cast<SiteInstanceImpl*>(site_instance2)->group(),
            static_cast<SiteInstanceImpl*>(site_instance1)->group());
  EXPECT_FALSE(site_instance2->IsRelatedSiteInstance(site_instance1));

  // TODO(crbug.com/40051335): Since we swap BrowsingInstances, we shouldn't
  // keep a proxy for the second tab in the first tab's SiteInstance.
  RenderFrameProxyHost* initial_rfph =
      new_web_contents->GetPrimaryMainFrame()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(
              static_cast<SiteInstanceImpl*>(site_instance1)->group());
  ASSERT_TRUE(initial_rfph);

  // Navigate to url1 and check bindings.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(new_shell, url1));
  EXPECT_NE(new_web_contents->GetSiteInstance(), site_instance1);
  EXPECT_EQ(BindingsPolicySet({BindingsPolicyValue::kWebUi}),
            new_web_contents->GetPrimaryMainFrame()->GetEnabledBindings());
}

// Used to test browser-side checks by disabling some renderer-side checks.
class WebUINavigationDisabledWebSecurityBrowserTest
    : public WebUINavigationBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disable Web Security to skip renderer-side checks so that we can test
    // browser-side checks.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }
};

// Verify that a browser check stops websites from embeding chrome:// iframes.
// This tests the Frame::BeginNavigation path.
IN_PROC_BROWSER_TEST_F(WebUINavigationDisabledWebSecurityBrowserTest,
                       DisallowEmbeddingChromeSchemeFromWebFrameBrowserCheck2) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL webui_url(GetWebUIURL("web-ui/title1.html?noxfo=true"));

  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kAddIframeScript, webui_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  observer.Wait();

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(1U, root->child_count());
  RenderFrameHost* child = root->child_at(0)->current_frame_host();
  EXPECT_EQ(kBlockedURL, child->GetLastCommittedURL());
}

// Verify that a browser check stops websites from navigating to
// chrome:// documents in the main frame. This tests the Frame::BeginNavigation
// path.
IN_PROC_BROWSER_TEST_F(
    WebUINavigationDisabledWebSecurityBrowserTest,
    DisallowNavigatingToChromeSchemeFromWebFrameBrowserCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL webui_url(GetWebUIURL("web-ui/title1.html"));
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location.href = $1", webui_url)));
  observer.Wait();
  EXPECT_EQ(kBlockedURL, shell()->web_contents()->GetLastCommittedURL());
}

// Verify that a browser check stops websites from navigating to
// chrome-untrusted:// documents in the main frame. This tests the
// Frame::BeginNavigation path.
IN_PROC_BROWSER_TEST_F(
    WebUINavigationDisabledWebSecurityBrowserTest,
    DisallowNavigatingToChromeUntrustedSchemeFromWebFrameBrowserCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  TestUntrustedDataSourceHeaders headers;
  headers.no_xfo = false;
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-iframe-host",
                                                     headers));
  GURL untrusted_url(GetChromeUntrustedUIURL("test-iframe-host/title1.html"));

  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location.href = $1", untrusted_url)));
  observer.Wait();
  EXPECT_EQ(kBlockedURL, shell()->web_contents()->GetLastCommittedURL());
}

// Verify that website cannot use window.open() to navigate succsesfully a new
// window to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(WebUINavigationDisabledWebSecurityBrowserTest,
                       DisallowWebWindowOpenToChromeURL) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  GURL chrome_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  ShellAddedObserver new_shell_observer;
  const char kWindowOpenScript[] = "var w = window.open($1, '_blank');";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kWindowOpenScript, chrome_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  Shell* popup = new_shell_observer.GetShell();

  // Wait for the navigation to complete and examine the state of the new
  // window. At this time, the navigation is not blocked by the
  // WebUINavigationThrottle, but rather by FilterURL which successfully commits
  // kBlockedURL in the same SiteInstance as the initiator of the navigation.
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  EXPECT_EQ(kBlockedURL, popup->web_contents()->GetLastCommittedURL());

  RenderFrameHost* main_rfh = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* popup_rfh = popup->web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(main_rfh->GetSiteInstance(), popup_rfh->GetSiteInstance());
  EXPECT_TRUE(main_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
}

// Verify that website cannot use window.open() to navigate successfully a new
// window to a chrome-untrusted:// URL.
IN_PROC_BROWSER_TEST_F(WebUINavigationDisabledWebSecurityBrowserTest,
                       DisallowWebWindowOpenToChromeUntrustedURL) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  GURL chrome_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  ShellAddedObserver new_shell_observer;
  const char kWindowOpenScript[] = "var w = window.open($1, '_blank');";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kWindowOpenScript, chrome_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  Shell* popup = new_shell_observer.GetShell();

  // Wait for the navigation to complete and examine the state of the new
  // window. At this time, the navigation is not blocked by the
  // WebUINavigationThrottle, but rather by FilterURL. This is why the
  // navigation is considered successful, however the last committed URL is
  // kBlockedURL.
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  EXPECT_EQ(kBlockedURL, popup->web_contents()->GetLastCommittedURL());

  RenderFrameHost* main_rfh = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* popup_rfh = popup->web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(main_rfh->GetSiteInstance(), popup_rfh->GetSiteInstance());
  EXPECT_TRUE(main_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
}

// Verify that a WebUI document in the main frame is allowed to navigate to
// web content and it properly does cross-process navigation.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest, WebUIMainFrameToWebAllowed) {
  GURL chrome_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), chrome_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* webui_rfh = root->current_frame_host();
  scoped_refptr<SiteInstanceImpl> webui_site_instance =
      webui_rfh->GetSiteInstance();

  EXPECT_EQ(chrome_url, webui_rfh->GetLastCommittedURL());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      webui_rfh->GetProcess()->GetID()));
  EXPECT_EQ(root->current_frame_host()->GetProcess()->GetProcessLock(),
            ProcessLock::FromSiteInfo(webui_site_instance->GetSiteInfo()));

  GURL web_url(embedded_test_server()->GetURL("/title2.html"));
  std::string script =
      base::StringPrintf("location.href = '%s';", web_url.spec().c_str());

  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), script));
  navigation_observer.Wait();

  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(web_url, root->current_frame_host()->GetLastCommittedURL());
  EXPECT_NE(webui_site_instance, root->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(webui_site_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      root->current_frame_host()->GetProcess()->GetID()));
  EXPECT_NE(root->current_frame_host()->GetProcess()->GetProcessLock(),
            ProcessLock::FromSiteInfo(webui_site_instance->GetSiteInfo()));
}

#if !BUILDFLAG(IS_ANDROID)
// The following tests rely on full site isolation behavior, which is not
// present on Android.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInWebUIProcessAllowed) {
  TestWebFrameInProcessWithWebUIBindings(
      BindingsPolicySet({BindingsPolicyValue::kWebUi}));
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInMojoWebUIProcessAllowed) {
  TestWebFrameInProcessWithWebUIBindings(
      BindingsPolicySet({BindingsPolicyValue::kMojoWebUi}));
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebFrameInHybridWebUIProcessAllowed) {
  TestWebFrameInProcessWithWebUIBindings(kWebUIBindingsPolicySet);
}
#endif

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebUISubframeNewWindowToWebAllowed) {
  TestWebUISubframeNewWindowToWebAllowed(
      BindingsPolicySet({BindingsPolicyValue::kWebUi}));
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       MojoWebUISubframeNewWindowToWebAllowed) {
  TestWebUISubframeNewWindowToWebAllowed(
      BindingsPolicySet({BindingsPolicyValue::kMojoWebUi}));
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       HybridWebUISubframeNewWindowToWebAllowed) {
  TestWebUISubframeNewWindowToWebAllowed(kWebUIBindingsPolicySet);
}

IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       WebUIOriginsRequireDedicatedProcess) {
  // chrome:// URLs should require a dedicated process.
  WebContents* web_contents = shell()->web_contents();
  BrowserContext* browser_context = web_contents->GetBrowserContext();
  IsolationContext isolation_context(browser_context);

  GURL chrome_url(GetWebUIURL("web-ui/title1.html"));
  auto expected_site_info =
      SiteInfo::CreateForTesting(isolation_context, chrome_url);

  EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context, chrome_url));

  // Navigate to a WebUI page.
  EXPECT_TRUE(NavigateToURL(shell(), chrome_url));

  // Verify that the "hostname" is also part of the site URL.
  auto site_info = static_cast<SiteInstanceImpl*>(
                       web_contents->GetPrimaryMainFrame()->GetSiteInstance())
                       ->GetSiteInfo();
  EXPECT_EQ(expected_site_info, site_info);

  // Ask the page to create a blob URL and return back the blob URL.
  const char* kScript = R"(
          var blob = new Blob(['foo'], {type : 'text/html'});
          var url = URL.createObjectURL(blob);
          url;
      )";
  GURL blob_url(
      EvalJs(shell(), kScript, EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */)
          .ExtractString());
  EXPECT_EQ(url::kBlobScheme, blob_url.scheme());

  // Verify that the blob also requires a dedicated process and that it would
  // use the same site url as the original page.
  EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context, blob_url));
  EXPECT_EQ(expected_site_info,
            SiteInfo::CreateForTesting(isolation_context, blob_url));
}

// Verify chrome-untrusted:// uses a dedicated process.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       UntrustedWebUIOriginsRequireDedicatedProcess) {
  // chrome-untrusted:// URLs should require a dedicated process.
  WebContents* web_contents = shell()->web_contents();
  BrowserContext* browser_context = web_contents->GetBrowserContext();
  IsolationContext isolation_context(browser_context);

  // Add a DataSource which disallows iframes by default.
  WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));
  GURL chrome_untrusted_url(GetChromeUntrustedUIURL("test-host/title1.html"));
  auto expected_site_info = SiteInfo::CreateForTesting(
      isolation_context, GetChromeUntrustedUIURL("test-host"));

  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, chrome_untrusted_url));

  // Navigate to a chrome-untrusted:// page.
  EXPECT_TRUE(NavigateToURL(shell(), chrome_untrusted_url));

  // Verify that the "hostname" is also part of the site URL.
  auto site_info = static_cast<SiteInstanceImpl*>(
                       web_contents->GetPrimaryMainFrame()->GetSiteInstance())
                       ->GetSiteInfo();
  EXPECT_EQ(expected_site_info, site_info);

  // Ask the page to create a blob URL and return back the blob URL.
  const char* kScript = R"(
          var blob = new Blob(['foo'], {type : 'text/html'});
          var url = URL.createObjectURL(blob);
          url;
      )";
  GURL blob_url(
      EvalJs(shell(), kScript, EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */)
          .ExtractString());
  EXPECT_EQ(url::kBlobScheme, blob_url.scheme());

  // Verify that the blob also requires a dedicated process and that it would
  // use the same site url as the original page.
  EXPECT_TRUE(DoesURLRequireDedicatedProcess(IsolationContext(browser_context),
                                             blob_url));
  EXPECT_EQ(expected_site_info,
            SiteInfo::CreateForTesting(isolation_context, blob_url));
}

// Verify that navigating back/forward between WebUI and an error page for a
// failed WebUI navigation works correctly.
IN_PROC_BROWSER_TEST_F(WebUINavigationBrowserTest,
                       SessionHistoryToFailedNavigation) {
  GURL start_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(start_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(
      BindingsPolicySet({BindingsPolicyValue::kWebUi}),
      shell()->web_contents()->GetPrimaryMainFrame()->GetEnabledBindings());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL webui_error_url(GetWebUIURL("web-ui/error"));
  EXPECT_FALSE(NavigateToURL(shell(), webui_error_url));
  EXPECT_FALSE(root->current_frame_host()->web_ui());
  EXPECT_TRUE(root->current_frame_host()->GetEnabledBindings().empty());

  GURL success_url(GetWebUIURL("web-ui/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), success_url));
  EXPECT_EQ(success_url, shell()->web_contents()->GetLastCommittedURL());

  {
    TestFrameNavigationObserver observer(root);
    shell()->web_contents()->GetController().GoBack();
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_FALSE(root->current_frame_host()->web_ui());
  }

  {
    TestFrameNavigationObserver observer(root);
    shell()->web_contents()->GetController().GoForward();
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_TRUE(root->current_frame_host()->web_ui());
    EXPECT_EQ(success_url, observer.last_committed_url());
  }
}

class AdditionalSchemesWebUINavigationBrowserTest : public ContentBrowserTest {
 public:
  AdditionalSchemesWebUINavigationBrowserTest() {
    url::AddStandardScheme(kAdditionalScheme,
                           url::SchemeType::SCHEME_WITH_HOST);
  }

  void SetUpOnMainThread() override {
    test_content_browser_client_ = std::make_unique<TestContentBrowserClient>();
    factory_.SetSupportedScheme(kAdditionalScheme);
  }

  void TearDownOnMainThread() override { test_content_browser_client_.reset(); }


  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTestRegisterStandardScheme,
                                    kAdditionalScheme);
  }

 private:
  class TestContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    TestContentBrowserClient() = default;
    TestContentBrowserClient(const TestContentBrowserClient&) = delete;
    TestContentBrowserClient& operator=(const TestContentBrowserClient&) =
        delete;
    ~TestContentBrowserClient() override = default;

    void GetAdditionalWebUISchemes(
        std::vector<std::string>* additional_schemes) override {
      additional_schemes->emplace_back(kAdditionalScheme);
    }
  };

 private:
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client_;
  url::ScopedSchemeRegistryForTests scheme_registry_;
  TestWebUIControllerFactory factory_;
  ScopedWebUIControllerFactoryRegistration factory_registration_{&factory_};
};

// Verify that WebUIDataSource can support non-default schemes.
IN_PROC_BROWSER_TEST_F(AdditionalSchemesWebUINavigationBrowserTest,
                       AdditionalSchemesWebUINavigation) {
  GURL start_url(base::StrCat({kAdditionalScheme, url::kStandardSchemeSeparator,
                               "web-ui/title1.html"}));
  EXPECT_FALSE(NavigateToURL(shell(), start_url));

  GURL success_url(base::StrCat(
      {kAdditionalScheme, url::kStandardSchemeSeparator,
       "web-ui/title2.html?supported_scheme=", kAdditionalScheme}));
  EXPECT_TRUE(NavigateToURL(shell(), success_url));
  EXPECT_EQ(success_url, shell()->web_contents()->GetLastCommittedURL());
}

}  // namespace content
