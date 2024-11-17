// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <concepts>

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom-shared.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace content {
namespace {

template <typename T>
concept IsWebContentsOrRenderFrameHost =
    (std::same_as<T, WebContents*> ||
     std::same_as<T, RenderFrameHost*>)&&requires(T p) { base::to_address(p); };

enum class TestAction {
  kNavigateToSameSitePage,
  kNavigateToSameSitePageWithJs,
  kNavigateToCrossSitePage,
  kNavigateToCrossSitePageWithJs,
  kNavigateToFragment,
  kNavigateToFragmentWithJs,
  kPushHistory,
  kReplaceHistory,
  kDestroy,
  kDestroyWithJs,
};

std::string GenerateTestName(const testing::TestParamInfo<TestAction>& info) {
  std::string test_name;
  switch (info.param) {
    case TestAction::kNavigateToSameSitePage:
      test_name = "NavigateToSameSitePage";
      break;
    case TestAction::kNavigateToSameSitePageWithJs:
      test_name = "NavigateToSameSitePageWithJs";
      break;
    case TestAction::kNavigateToCrossSitePage:
      test_name = "NavigateToCrossSitePage";
      break;
    case TestAction::kNavigateToCrossSitePageWithJs:
      test_name = "NavigateToCrossSitePageWithJs";
      break;
    case TestAction::kNavigateToFragment:
      test_name = "NavigateToFragment";
      break;
    case TestAction::kNavigateToFragmentWithJs:
      test_name = "NavigateToFragmentWithJs";
      break;
    case TestAction::kPushHistory:
      test_name = "PushHistory";
      break;
    case TestAction::kReplaceHistory:
      test_name = "ReplaceHistory";
      break;
    case TestAction::kDestroy:
      test_name = "Destroy";
      break;
    case TestAction::kDestroyWithJs:
      test_name = "DestroyWithJs";
      break;
  }
  return base::StringPrintf("%d_%s", info.index, test_name);
}

class PartitionedPopinsControllerBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<TestAction> {
 public:
  PartitionedPopinsControllerBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kPartitionedPopins);
  }

 protected:
  WebContents* GetMainWebContents() { return shell()->web_contents(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
    SetUpIframesAndNewWindow(CreateBrowser()->web_contents());
  }

  void FlushRunLoop() {
    base::RunLoop run_loop;
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  template <content::IsWebContentsOrRenderFrameHost Ptr>
  void DoAction(Ptr render_frame_host_or_web_contents) {
    switch (GetParam()) {
      case TestAction::kNavigateToSameSitePage:
        Navigate(render_frame_host_or_web_contents, GetUrl("/title1.html"));
        break;
      case TestAction::kNavigateToSameSitePageWithJs:
        NavigateWithJs(render_frame_host_or_web_contents,
                       GetUrl("/title1.html"));
        break;
      case TestAction::kNavigateToCrossSitePage:
        Navigate(render_frame_host_or_web_contents,
                 GetUrl("/title1.html", "b.test"));
        break;
      case TestAction::kNavigateToCrossSitePageWithJs:
        NavigateWithJs(render_frame_host_or_web_contents,
                       GetUrl("/title1.html", "b.test"));
        break;
      case TestAction::kNavigateToFragment:
        NavigateToFragment(render_frame_host_or_web_contents, "#ref");
        break;
      case TestAction::kNavigateToFragmentWithJs:
        NavigateToFragmentWithJs(render_frame_host_or_web_contents, "#ref");
        break;
      case TestAction::kPushHistory:
        PushHistory(render_frame_host_or_web_contents, "/title1.html");
        break;
      case TestAction::kReplaceHistory:
        ReplaceHistory(render_frame_host_or_web_contents, "/title1.html");
        break;
      case TestAction::kDestroy:
        Destroy(render_frame_host_or_web_contents);
        break;
      case TestAction::kDestroyWithJs:
        DestroyWithJs(render_frame_host_or_web_contents);
        break;
    }
  }

  WebContents* OpenPopin(const ToRenderFrameHost& execution_target) {
    const GURL url_popin = embedded_https_test_server().GetURL(
        "a.test", "/partitioned_popins/wildcard_policy.html");
    WebContentsAddedObserver new_tab_observer;
    TestNavigationObserver nav_observer(nullptr);
    nav_observer.StartWatchingNewWebContents();
    CHECK(ExecJs(execution_target,
                 "window.open('" + url_popin.spec() + "', '_blank', 'popin')"));
    WebContents* popin_web_contents = new_tab_observer.GetWebContents();
    CHECK(popin_web_contents);
    nav_observer.Wait();

    // Check that the popin is open and it's partitioned.
    CHECK_EQ(EvalJs(popin_web_contents, "window.popinContextType()"),
             "partitioned");

    return popin_web_contents;
  }

  WebContentsDestroyedWatcher OpenPopinAndWatchDestruction(
      const ToRenderFrameHost& execution_target) {
    return WebContentsDestroyedWatcher(OpenPopin(execution_target));
  }

  void SetUpIframesAndNewWindow(WebContents* new_window_web_contents) {
    DCHECK(new_window_web_contents);

    const GURL url = embedded_https_test_server().GetURL(
        "a.test", "/partitioned_popins/iframes_allow_popins.html");
    ASSERT_TRUE(NavigateToURL(GetMainWebContents(), url));
    ASSERT_TRUE(
        NavigateToURLFromRenderer(ChildFrameAt(GetMainWebContents(), 0), url));

    const GURL links_url =
        embedded_https_test_server().GetURL("a.test", "/links.html");
    ASSERT_TRUE(NavigateToURL(new_window_web_contents, links_url));
    new_window_web_contents_ = new_window_web_contents->GetWeakPtr();
  }

  WebContents* GetNewWindowWebContents() {
    DCHECK(new_window_web_contents_);
    return new_window_web_contents_.get();
  }

  GURL GetUrl(std::string_view relative_url,
              std::string_view hostname = "a.test") {
    return embedded_https_test_server().GetURL(hostname, relative_url);
  }

  void Navigate(WebContents* web_contents, const GURL& url) {
    EXPECT_TRUE(NavigateToURL(web_contents, url));
  }

  void Navigate(RenderFrameHost* render_frame_host, const GURL& url) {
    content::TestNavigationObserver observer(
        WebContents::FromRenderFrameHost(render_frame_host));
    auto params = blink::mojom::OpenURLParams::New();
    params->url = url;
    params->disposition = WindowOpenDisposition::CURRENT_TAB;
    static_cast<content::mojom::FrameHost*>(
        static_cast<content::RenderFrameHostImpl*>(render_frame_host))
        ->OpenURL(std::move(params));
    observer.Wait();
  }

  void NavigateWithJs(const ToRenderFrameHost& adapter, const GURL& url) {
    EXPECT_TRUE(NavigateToURLFromRenderer(adapter, GetUrl("/title1.html")));
  }

  void NavigateToFragment(WebContents* web_contents,
                          std::string_view fragment) {
    CHECK(base::StartsWith(fragment, "#"));
    Navigate(web_contents,
             web_contents->GetLastCommittedURL().Resolve(fragment));
  }

  void NavigateToFragment(RenderFrameHost* render_frame_host,
                          std::string_view fragment) {
    CHECK(base::StartsWith(fragment, "#"));
    Navigate(render_frame_host,
             render_frame_host->GetLastCommittedURL().Resolve(fragment));
  }

  void NavigateToFragmentWithJs(const ToRenderFrameHost& adapter,
                                std::string_view fragment) {
    CHECK(base::StartsWith(fragment, "#"));
    NavigateWithJs(
        adapter,
        adapter.render_frame_host()->GetLastCommittedURL().Resolve(fragment));
  }

  void Destroy(WebContents* web_contents) { web_contents->Close(); }

  void Destroy(RenderFrameHost* render_frame_host) {
    static_cast<content::RenderFrameHostImpl*>(render_frame_host)
        ->DeleteRenderFrame(mojom::FrameDeleteIntention::kNotMainFrame);
  }

  void DestroyWithJs(WebContents* web_contents) {
    blink::web_pref::WebPreferences prefs =
        web_contents->GetOrCreateWebPreferences();
    prefs.allow_scripts_to_close_windows = true;
    web_contents->SetWebPreferences(prefs);
    EXPECT_TRUE(ExecJs(web_contents, "window.close()"));
  }

  void DestroyWithJs(RenderFrameHost* render_frame_host) {
    CHECK_NE(render_frame_host->GetFrameName(), "");
    CHECK(render_frame_host->GetParentOrOuterDocumentOrEmbedder());
    EXPECT_TRUE(content::ExecJs(
        render_frame_host->GetParentOrOuterDocumentOrEmbedder(),
        JsReplace("document.querySelector('iframe[name=$1]').remove();",
                  render_frame_host->GetFrameName())));
  }

  void PushHistory(const ToRenderFrameHost& execution_target,
                   std::string_view url) {
    EXPECT_TRUE(content::ExecJs(
        execution_target, JsReplace("history.pushState(null, '', $1)", url)));
  }

  void ReplaceHistory(const ToRenderFrameHost& execution_target,
                      std::string_view url) {
    EXPECT_TRUE(
        content::ExecJs(execution_target,
                        JsReplace("history.replaceState(null, '', $1)", url)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtr<WebContents> new_window_web_contents_;
};

IN_PROC_BROWSER_TEST_P(PartitionedPopinsControllerBrowserTest,
                       TopFrameOpenerDoesAction) {
  WebContentsDestroyedWatcher popin_destroyed_watcher =
      OpenPopinAndWatchDestruction(GetMainWebContents());

  // Do action on unrelated contents/frames.
  DoAction(GetNewWindowWebContents());
  DoAction(ChildFrameAt(GetMainWebContents(), 0));
  FlushRunLoop();
  EXPECT_FALSE(popin_destroyed_watcher.IsDestroyed());

  DoAction(GetMainWebContents());
  popin_destroyed_watcher.Wait();
  EXPECT_TRUE(popin_destroyed_watcher.IsDestroyed());
}

IN_PROC_BROWSER_TEST_P(PartitionedPopinsControllerBrowserTest,
                       IframeOpenerDoesAction) {
  RenderFrameHost* iframe = ChildFrameAt(GetMainWebContents(), 0);
  ASSERT_TRUE(iframe);
  WebContentsDestroyedWatcher popin_destroyed_watcher =
      OpenPopinAndWatchDestruction(iframe);

  // Do action on unrelated contents/frames.
  DoAction(GetNewWindowWebContents());
  DoAction(ChildFrameAt(GetMainWebContents(), 1));
  DoAction(ChildFrameAt(iframe, 0));
  FlushRunLoop();
  EXPECT_FALSE(popin_destroyed_watcher.IsDestroyed());

  DoAction(iframe);
  popin_destroyed_watcher.Wait();
  EXPECT_TRUE(popin_destroyed_watcher.IsDestroyed());
}

IN_PROC_BROWSER_TEST_P(PartitionedPopinsControllerBrowserTest,
                       IframeOpenerTopFrameDoesAction) {
  RenderFrameHost* iframe = ChildFrameAt(GetMainWebContents(), 0);
  ASSERT_TRUE(iframe);
  WebContentsDestroyedWatcher popin_destroyed_watcher =
      OpenPopinAndWatchDestruction(iframe);

  // Do action on unrelated contents/frames.
  DoAction(GetNewWindowWebContents());
  DoAction(ChildFrameAt(GetMainWebContents(), 1));
  DoAction(ChildFrameAt(iframe, 0));
  FlushRunLoop();
  EXPECT_FALSE(popin_destroyed_watcher.IsDestroyed());

  DoAction(GetMainWebContents());
  popin_destroyed_watcher.Wait();
  EXPECT_TRUE(popin_destroyed_watcher.IsDestroyed());
}

IN_PROC_BROWSER_TEST_P(PartitionedPopinsControllerBrowserTest,
                       IframeOpenerIframeParentDoesAction) {
  RenderFrameHost* iframe = ChildFrameAt(GetMainWebContents(), 0);
  ASSERT_TRUE(iframe);
  RenderFrameHost* child_iframe = ChildFrameAt(iframe, 0);
  ASSERT_TRUE(child_iframe);
  WebContentsDestroyedWatcher popin_destroyed_watcher =
      OpenPopinAndWatchDestruction(child_iframe);

  // Do action on unrelated contents/frames.
  DoAction(GetNewWindowWebContents());
  DoAction(ChildFrameAt(GetMainWebContents(), 1));
  DoAction(ChildFrameAt(iframe, 1));
  FlushRunLoop();
  EXPECT_FALSE(popin_destroyed_watcher.IsDestroyed());

  DoAction(iframe);
  popin_destroyed_watcher.Wait();
  EXPECT_TRUE(popin_destroyed_watcher.IsDestroyed());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PartitionedPopinsControllerBrowserTest,
    ::testing::Values(TestAction::kNavigateToSameSitePage,
                      TestAction::kNavigateToSameSitePageWithJs,
                      TestAction::kNavigateToCrossSitePage,
                      TestAction::kNavigateToCrossSitePageWithJs,
                      TestAction::kNavigateToFragment,
                      TestAction::kNavigateToFragmentWithJs,
                      TestAction::kPushHistory,
                      TestAction::kReplaceHistory,
                      TestAction::kDestroy,
                      TestAction::kDestroyWithJs),
    &GenerateTestName);

}  // namespace
}  // namespace content
