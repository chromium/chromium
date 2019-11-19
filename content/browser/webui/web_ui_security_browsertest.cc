// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {

void GetResource(const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (id == "error") {
    callback.Run(nullptr);
    return;
  }

  std::string contents;
  base::FilePath path;
  CHECK(base::PathService::Get(content::DIR_TEST_DATA, &path));
  path = path.AppendASCII(id.substr(0, id.find("?")));
  CHECK(base::ReadFileToString(path, &contents)) << path.value();

  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  callback.Run(ref_contents);
}

struct WebUIControllerConfig {
  int bindings = BINDINGS_POLICY_WEB_UI;
  std::string child_src = "child-src 'self' chrome://web-ui-subframe/;";
  bool disable_xfo = false;
};

class TestWebUIController : public WebUIController {
 public:
  TestWebUIController(WebUI* web_ui,
                      const GURL& base_url,
                      const WebUIControllerConfig& config)
      : WebUIController(web_ui) {
    web_ui->SetBindings(config.bindings);

    WebUIDataSource* data_source = WebUIDataSource::Create(base_url.host());
    data_source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) { return true; }),
        base::BindRepeating(&GetResource));

    if (!config.child_src.empty())
      data_source->OverrideContentSecurityPolicyChildSrc(config.child_src);

    if (config.disable_xfo)
      data_source->DisableDenyXFrameOptions();

    WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                         data_source);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() {}

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    if (!url.SchemeIs(kChromeUIScheme))
      return nullptr;

    WebUIControllerConfig config;
    config.disable_xfo = disable_xfo_;

    if (url.has_query()) {
      std::string value;
      bool has_value = net::GetValueForKeyInQuery(url, "bindings", &value);
      if (has_value)
        EXPECT_TRUE(base::StringToInt(value, &(config.bindings)));

      has_value = net::GetValueForKeyInQuery(url, "noxfo", &value);
      if (has_value && value == "true")
        config.disable_xfo = true;
    }

    return std::make_unique<TestWebUIController>(web_ui, url, config);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    if (!url.SchemeIs(kChromeUIScheme))
      return WebUI::kNoWebUI;

    return reinterpret_cast<WebUI::TypeID>(base::Hash(url.host()));
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }

  void set_disable_xfo(bool disable) { disable_xfo_ = disable; }

 private:
  bool disable_xfo_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

}  // namespace

class WebUISecurityTest : public ContentBrowserTest {
 public:
  WebUISecurityTest() { WebUIControllerFactory::RegisterFactory(&factory_); }

  ~WebUISecurityTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUISecurityTest);
};

// Loads a WebUI which does not have any bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, NoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=0"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(0, shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which has WebUI bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which has Mojo bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, MojoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(BINDINGS_POLICY_MOJO_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Loads a WebUI which has both WebUI and Mojo bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIAndMojoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_WEB_UI |
                                                 BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI | BINDINGS_POLICY_MOJO_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());
}

// Verify that reloading a WebUI document or navigating between documents on
// the same WebUI will result in using the same SiteInstance and will not
// create a new WebUI instance.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIReuse) {
  GURL test_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Capture the SiteInstance and WebUI used in the first navigation to compare
  // with the ones used after the reload.
  scoped_refptr<SiteInstance> initial_site_instance =
      root->current_frame_host()->GetSiteInstance();
  WebUI* initial_web_ui = root->current_frame_host()->web_ui();

  // Reload the document and check that SiteInstance and WebUI are reused.
  TestFrameNavigationObserver observer(root);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(test_url, observer.last_committed_url());

  EXPECT_EQ(initial_site_instance,
            root->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_web_ui, root->current_frame_host()->web_ui());

  // Navigate to another document on the same WebUI and check that SiteInstance
  // and WebUI are reused.
  GURL next_url(GetWebUIURL("web-ui/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), next_url));

  EXPECT_EQ(initial_site_instance,
            root->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_web_ui, root->current_frame_host()->web_ui());
}

// Verify that a WebUI can add a subframe for its own WebUI.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUISameSiteSubframe) {
  GURL test_url(GetWebUIURL("web-ui/page_with_blank_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());

  TestFrameNavigationObserver observer(root->child_at(0));
  GURL subframe_url(GetWebUIURL("web-ui/title1.html?noxfo=true"));
  NavigateFrameToURL(root->child_at(0), subframe_url);

  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(subframe_url, observer.last_committed_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      GetWebUIURL("web-ui"),
      root->child_at(0)->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // The subframe should have its own WebUI object different from the parent
  // frame.
  EXPECT_NE(nullptr, root->child_at(0)->current_frame_host()->web_ui());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            root->child_at(0)->current_frame_host()->web_ui());
}

// Verify that a WebUI can add a subframe to another WebUI and they will be
// correctly isolated in separate SiteInstances and processes. The subframe
// also uses WebUI with bindings different than the parent to ensure this is
// successfully handled.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUICrossSiteSubframe) {
  GURL main_frame_url(GetWebUIURL("web-ui/page_with_blank_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            root->current_frame_host()->GetEnabledBindings());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Using a browser-initiated navigation will create a pending
  // NavigationEntry based on the first commit. In that case, the bindings
  // saved on the NavigationEntry are from the main frame WebUI and are used
  // for the navigation, which mismatch the ones being used by the subframe.
  // Navigate the child frame through a renderer-initiated navigation for now
  // until the browser-initiated path supports this scenario.
  // TODO(nasko): Add coverage for browser-initiated navigations when they no
  // longer have this limitation.
  TestFrameNavigationObserver observer(child);
  GURL child_frame_url(
      GetWebUIURL("web-ui-subframe/title1.html?noxfo=true&bindings=" +
                  base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(ExecJs(shell(),
                     JsReplace("document.getElementById($1).src = $2;",
                               "test_iframe", child_frame_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(child_frame_url, observer.last_committed_url());
  EXPECT_EQ(BINDINGS_POLICY_MOJO_WEB_UI,
            child->current_frame_host()->GetEnabledBindings());

  EXPECT_EQ(url::Origin::Create(child_frame_url),
            child->current_frame_host()->GetLastCommittedOrigin());
  EXPECT_EQ(GetWebUIURL("web-ui-subframe"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            child->current_frame_host()->web_ui());
  EXPECT_NE(root->current_frame_host()->GetEnabledBindings(),
            child->current_frame_host()->GetEnabledBindings());
}

// Verify that SiteInstance and WebUI reuse happens in subframes as well.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIReuseInSubframe) {
  // Disable X-Frame-Options on all WebUIs in this test, since subframe WebUI
  // reuse is expected. If the initial creation does not disable XFO, then
  // subsequent navigations will fail.
  factory()->set_disable_xfo(true);

  GURL main_frame_url(GetWebUIURL("web-ui/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Capture the SiteInstance and WebUI used in the first navigation to compare
  // with the ones used after the reload.
  scoped_refptr<SiteInstance> initial_site_instance =
      child->current_frame_host()->GetSiteInstance();
  WebUI* initial_web_ui = child->current_frame_host()->web_ui();
  GlobalFrameRoutingId initial_rfh_id =
      child->current_frame_host()->GetGlobalFrameRoutingId();

  GURL subframe_same_site_url(GetWebUIURL("web-ui/title2.html"));
  {
    TestFrameNavigationObserver observer(child);
    NavigateFrameToURL(child, subframe_same_site_url);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_same_site_url, observer.last_committed_url());
  }
  EXPECT_EQ(initial_site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_web_ui, child->current_frame_host()->web_ui());

  // Navigate the child frame cross-site.
  GURL subframe_cross_site_url(GetWebUIURL("web-ui-subframe/title1.html"));
  {
    TestFrameNavigationObserver observer(child);
    NavigateFrameToURL(child, subframe_cross_site_url);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_cross_site_url, observer.last_committed_url());
  }
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            child->current_frame_host()->web_ui());
  EXPECT_NE(initial_web_ui, child->current_frame_host()->web_ui());

  // Capture the new SiteInstance and WebUI of the subframe and navigate it to
  // another document on the same site.
  scoped_refptr<SiteInstance> second_site_instance =
      child->current_frame_host()->GetSiteInstance();
  WebUI* second_web_ui = child->current_frame_host()->web_ui();

  GURL subframe_cross_site_url2(GetWebUIURL("web-ui-subframe/title2.html"));
  {
    TestFrameNavigationObserver observer(child);
    NavigateFrameToURL(child, subframe_cross_site_url2);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_cross_site_url2, observer.last_committed_url());
  }
  EXPECT_EQ(second_site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(second_web_ui, child->current_frame_host()->web_ui());

  // Navigate back to the first document in the subframe, which should bring
  // it back to the initial SiteInstance, but use a different RenderFrameHost
  // and by that a different WebUI instance.
  {
    TestFrameNavigationObserver observer(child);
    shell()->web_contents()->GetController().GoToOffset(-2);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(subframe_same_site_url, observer.last_committed_url());
  }
  EXPECT_EQ(initial_site_instance,
            child->current_frame_host()->GetSiteInstance());
  // Use routing id comparison for the RenderFrameHost as the memory allocator
  // sometime places the newly created RenderFrameHost for the back navigation
  // at the same memory location as the initial one. For this reason too, it
  // is not possible to check the web_ui() for inequality, since in some runs
  // the memory in which two different WebUI instances of the same type are
  // placed is the same.
  EXPECT_NE(initial_rfh_id,
            child->current_frame_host()->GetGlobalFrameRoutingId());
}

// Verify that if one WebUI does a window.open() to another WebUI, then the two
// are not sharing a BrowsingInstance, are isolated from each other, and both
// processes have bindings granted to them.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WindowOpenWebUI) {
  GURL test_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(test_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(shell()->web_contents()->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);

  TestNavigationObserver new_contents_observer(nullptr, 1);
  new_contents_observer.StartWatchingNewWebContents();
  // Execute the script in isolated world since the default CSP disables eval
  // which ExecJs depends on.
  GURL new_tab_url(GetWebUIURL("another-web-ui/title2.html"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.open($1);", new_tab_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  new_contents_observer.Wait();
  EXPECT_TRUE(new_contents_observer.last_navigation_succeeded());

  ASSERT_EQ(2u, Shell::windows().size());
  Shell* new_shell = Shell::windows()[1];

  EXPECT_EQ(new_tab_url, new_shell->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(new_shell->web_contents()->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);

  // SiteInstances should be different and unrelated due to the
  // BrowsingInstance swaps on navigation.
  EXPECT_NE(new_shell->web_contents()->GetMainFrame()->GetSiteInstance(),
            shell()->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(
      new_shell->web_contents()
          ->GetMainFrame()
          ->GetSiteInstance()
          ->IsRelatedSiteInstance(
              shell()->web_contents()->GetMainFrame()->GetSiteInstance()));

  EXPECT_NE(shell()->web_contents()->GetWebUI(),
            new_shell->web_contents()->GetWebUI());
}

// Test to verify correctness of WebUI and process model in the following
// sequence of navigations:
// * successful navigation to WebUI
// * failed navigation to WebUI
// * failed navigation to http URL
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIFailedNavigation) {
  GURL start_url(GetWebUIURL("web-ui/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(start_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            shell()->web_contents()->GetMainFrame()->GetEnabledBindings());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  GURL webui_error_url(GetWebUIURL("web-ui/error"));
  EXPECT_FALSE(NavigateToURL(shell(), webui_error_url));
  EXPECT_FALSE(root->current_frame_host()->web_ui());
  EXPECT_EQ(0, root->current_frame_host()->GetEnabledBindings());

  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance()->GetSiteURL(),
              GURL(kUnreachableWebDataURL));
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_error_url(
      embedded_test_server()->GetURL("foo.com", "/nonexistent"));
  EXPECT_FALSE(NavigateToURL(shell(), http_error_url));
  EXPECT_FALSE(root->current_frame_host()->web_ui());
  EXPECT_EQ(0, root->current_frame_host()->GetEnabledBindings());
}

}  // namespace content
