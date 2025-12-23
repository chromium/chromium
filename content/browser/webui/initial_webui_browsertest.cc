// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"

namespace content {

#if !BUILDFLAG(IS_ANDROID)
namespace {

class InitialWebUIOverrideContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit InitialWebUIOverrideContentBrowserClient(
      const GURL& initial_webui_url)
      : initial_webui_url_(initial_webui_url) {}

  InitialWebUIOverrideContentBrowserClient(
      const InitialWebUIOverrideContentBrowserClient&) = delete;
  InitialWebUIOverrideContentBrowserClient& operator=(
      const InitialWebUIOverrideContentBrowserClient&) = delete;

  bool IsInitialWebUIURL(const GURL& url) override {
    return initial_webui_url_ == url;
  }

 private:
  GURL initial_webui_url_;
};

class WebUITestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    return HasWebUIScheme(url) ? std::make_unique<WebUIController>(web_ui)
                               : nullptr;
  }
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    return HasWebUIScheme(url) ? reinterpret_cast<WebUI::TypeID>(1) : nullptr;
  }
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return HasWebUIScheme(url);
  }
};

}  // namespace

class InitialWebUINavigationBrowserTest : public ContentBrowserTest {
 public:
  InitialWebUINavigationBrowserTest() {
    WebUIControllerFactory::RegisterFactory(&factory_);
    scoped_feature_list_.InitWithFeatures(
        {features::kInitialWebUISyncNavStartToCommit,
         features::kWebUIInProcessResourceLoadingV2},
        {});
  }

 protected:
  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  WebUITestWebUIControllerFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest, CommitInitialWebUI) {
  GURL url("chrome://foo");
  // Make `url` an initial WebUI URL.
  InitialWebUIOverrideContentBrowserClient content_browser_client(url);

  // Create a new WebContents, since initial WebUI navigations are only allowed
  // to happen as the first navigation in a new WebContents.
  WebContents::CreateParams new_contents_params(contents()->GetBrowserContext(),
                                                contents()->GetSiteInstance());
  // Make sure the initial SiteInstance for the WebContents uses `url` as the
  // site URL, rather than an empty URL. This is required to make sure that the
  // renderer process flags are clear.
  new_contents_params.site_instance =
      SiteInstance::CreateForURL(contents()->GetBrowserContext(), url);
  std::unique_ptr<WebContents> new_web_contents(
      WebContents::Create(new_contents_params));
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(new_web_contents->GetController());

  // Setup a fake resource path for the body. Note that we're adding a direct
  // string response instead of adding a ResourceBundle resource ID. This is
  // different from what happens in production (which will use a resource ID),
  // but this is significantly easier to test.
  WebUIDataSource* source =
      WebUIDataSource::CreateAndAdd(controller.GetBrowserContext(), "foo");
  source->SetResourcePathToResponse("", "<!doctype html><body>bar</body>");

  // Ensure that there are no navigations yet on the new WebContents.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(new_web_contents.get())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_FALSE(root->navigation_request());

  // Navigate to `url`.
  TestNavigationObserver navigation_observer(url);
  navigation_observer.WatchExistingWebContents();
  controller.LoadURLWithParams(NavigationController::LoadURLParams(url));

  // The navigation reached CommitNavigation synchronously, since it is now
  // owned by the primary main RenderFrameHost instead of the root
  // FrameTreeNode.
  EXPECT_FALSE(root->navigation_request());
  EXPECT_TRUE(
      root->current_frame_host()->HasPendingCommitForCrossDocumentNavigation());
  {
    std::vector<base::SafeRef<NavigationHandle>>
        committing_navigation_requests =
            root->current_frame_host()
                ->GetPendingCommitCrossDocumentNavigations();
    EXPECT_EQ(1u, committing_navigation_requests.size());
    EXPECT_EQ(url, committing_navigation_requests[0]->GetURL());
  }
  navigation_observer.Wait();

  // Ensure the navigation successfully commits and loads the body HTML.
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(navigation_observer.last_navigation_url(), url);

  RenderFrameHostImplWrapper rfh(root->current_frame_host());
  EXPECT_EQ("bar", EvalJs(rfh.get(), "document.body.innerHTML"));

  // Ensure that the process has the correct flag set.
  EXPECT_TRUE(rfh->GetProcess()->IsForInitialWebUI());

  // Check that CSP was set.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        rfh->policy_container_host()->policies().content_security_policies;
    EXPECT_EQ(1u, root_csp.size());
    EXPECT_EQ(
        "child-src 'none';"
        "object-src 'none';"
        "require-trusted-types-for 'script';"
        "script-src chrome://resources 'self';"
        "trusted-types;frame-ancestors 'none';",
        root_csp[0]->header->header_value);
  }
}
#endif

}  // namespace content
