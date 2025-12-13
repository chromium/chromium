// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"
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

namespace content {

#if !BUILDFLAG(IS_ANDROID)
namespace {

// Injects an HTML body for GetDataResourceBytes() calls, so that they would
// not actually reach into the ResourceBundle.
class InitialWebUITestContentClient : public ContentClient {
 public:
  InitialWebUITestContentClient() {
    html_body_ = base::MakeRefCounted<base::RefCountedString>(
        "<!doctype html><body>bar</body>");
  }

  InitialWebUITestContentClient(const InitialWebUITestContentClient&) = delete;
  InitialWebUITestContentClient& operator=(
      const InitialWebUITestContentClient&) = delete;

  // ContentClient:
  bool HasDataResource(int resource_id) const override { return true; }
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override {
    return html_body_.get();
  }

 private:
  scoped_refptr<base::RefCountedString> html_body_;
};

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
    SetContentClient(&client_);
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  ~InitialWebUINavigationBrowserTest() override { SetContentClient(nullptr); }

 protected:
  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  WebUITestWebUIControllerFactory factory_;
  InitialWebUITestContentClient client_;
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

  // Setup a fake resource path for the body. Note that because we're overriding
  // the ContentClient, we won't actually load a resource, but the mapping needs
  // to exist so that the WebUIURLLoader can run correctly during the
  // navigation.
  WebUIDataSource* source =
      WebUIDataSource::CreateAndAdd(controller.GetBrowserContext(), "foo");
  source->AddResourcePath("", 1);

  // Ensure that there are no navigations yet on the new WebContents.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(new_web_contents.get())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_FALSE(root->navigation_request());

  // Navigate to `url`.
  TestNavigationObserver navigation_observer(url);
  navigation_observer.WatchExistingWebContents();
  controller.LoadURLWithParams(NavigationController::LoadURLParams(url));

  // The navigation didn't reach commit synchronously, since it is still owned
  // by the root FrameTreeNode instead of the RenderFrameHost.
  // TODO(crbug.com/457618572): Make it reach commit synchronously.
  EXPECT_TRUE(root->navigation_request());
  EXPECT_EQ(url, root->navigation_request()->GetURL());
  EXPECT_NE(NavigationRequest::NavigationState::READY_TO_COMMIT,
            root->navigation_request()->state());
  EXPECT_FALSE(
      root->current_frame_host()->HasPendingCommitForCrossDocumentNavigation());
  navigation_observer.Wait();

  // Ensure the navigation successfully commits and loads the body HTML.
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(navigation_observer.last_navigation_url(), url);
  EXPECT_EQ("bar", EvalJs(new_web_contents->GetPrimaryMainFrame(),
                          "document.body.innerHTML"));

  // Ensure that the process has the correct flag set.
  EXPECT_TRUE(new_web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsForInitialWebUI());
}
#endif

}  // namespace content
