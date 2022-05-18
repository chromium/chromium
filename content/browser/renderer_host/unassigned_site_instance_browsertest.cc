// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/render_document_feature.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

// Unassigned SiteInstances occur when ShouldAssignSiteForURL returns false,
// allowing a given SiteInstance to be reused for a future navigation. This test
// suite covers how Chrome should behave in these cases, and when SiteInstances
// should be reused or replaced. Note that there are some differences between
// about:blank and custom embedder-defined cases.
//
// TODO(crbug.com/1296173): We would like to enforce the fact that unassigned
// SiteInstances only ever exist in their own BrowsingInstance. The exact way to
// achieve that is still unclear. We might only allow leaving SiteInstances
// unassigned for empty schemes, or make the siteless behavior kick in only for
// the first navigation in a BrowsingInstance, for example.
//
// The test suite covers expectations for the following cases:
// - Navigation to about:blank, renderer/browser initiated.
// - Navigation from about:blank, renderer/browser initiated.
// - Navigation to embedder defined url, renderer/browser initiated.
// - Navigation from embedder defined url, renderer/browser initiated.
// - Initial empty document in popups.
// - Navigation in a popup, to about:blank, renderer/browser initiated.
// - Navigation in a popup, to embedder defined url, renderer/browser initiated.
// - Initial empty document in iframes.
// - Navigation in an iframe, to about:blank, renderer initiated.
// - Navigation in an iframe, to embedder defined url, renderer initiated.
// - Some bug reproducers, testing things like races and history navigations.

namespace content {

namespace {

// ContentBrowserClient that skips assigning a site URL for all URLs that match
// a given URL's scheme and host.
class DontAssignSiteContentBrowserClient : public TestContentBrowserClient {
 public:
  // Any visit to |url_to_skip| will not cause the site to be assigned to the
  // SiteInstance.
  explicit DontAssignSiteContentBrowserClient(const GURL& url_to_skip)
      : url_to_skip_(url_to_skip) {}

  DontAssignSiteContentBrowserClient(
      const DontAssignSiteContentBrowserClient&) = delete;
  DontAssignSiteContentBrowserClient& operator=(
      const DontAssignSiteContentBrowserClient&) = delete;

  bool ShouldAssignSiteForURL(const GURL& url) override {
    return url.host() != url_to_skip_.host() ||
           url.scheme() != url_to_skip_.scheme();
  }

 private:
  GURL url_to_skip_;
};

void InitBackForwardCacheFeature(base::test::ScopedFeatureList* feature_list,
                                 bool enable_back_forward_cache) {
  if (enable_back_forward_cache) {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams> features;
    features.push_back(
        {features::kBackForwardCache, {{"enable_same_site", "true"}}});
    features.push_back({kBackForwardCacheNoTimeEviction, {}});
    features.push_back({features::kBackForwardCacheMemoryControls, {}});
    feature_list->InitWithFeaturesAndParameters(features, {});
  } else {
    feature_list->InitAndDisableFeature(features::kBackForwardCache);
  }
}

}  // namespace

// Note that this test suite is parametrized for RenderDocument and
// BackForwardCache, like many tests involving navigations, and SiteInstance
// picking. This is due to the fact that both features have an important impact
// on navigations and are likely to interact.
class UnassignedSiteInstanceBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::tuple<std::string, bool>> {
 public:
  UnassignedSiteInstanceBrowserTest() {
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    InitBackForwardCacheFeature(&feature_list_for_back_forward_cache_,
                                std::get<1>(GetParam()));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, enable_back_forward_cache] = info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled");
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    regular_url_ = embedded_test_server()->GetURL("a.test", "/title1.html");
    unassigned_url_ = GURL("about:blank");
    embedder_defined_unassigned_url_ =
        embedded_test_server()->GetURL("b.test", "/title1.html");

    // Set up a URL for which ShouldAssignSiteForURL will return false. The
    // corresponding SiteInstance's site will be left unassigned, and its
    // process won't be locked. Note that this applies to the entire site
    // instead of the specific url, so we use b.test origin to keep a.test
    // assigned.
    content_browser_client_override_ =
        std::make_unique<DontAssignSiteContentBrowserClient>(
            embedder_defined_unassigned_url_);
    old_client_ =
        SetBrowserClientForTesting(content_browser_client_override_.get());
  }

  void TearDownOnMainThread() override {
    if (old_client_)
      SetBrowserClientForTesting(old_client_);
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void DisableBackForwardCache(
      BackForwardCacheImpl::DisableForTestingReason reason) const {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetController()
        .GetBackForwardCache()
        .DisableForTesting(reason);
  }

  // Returns an url that assigns a site to the SiteInstance it lives in.
  const GURL& regular_url() const { return regular_url_; }

  // Returns an url that does not assign a site to the SiteInstance it lives in.
  const GURL& unassigned_url() const { return unassigned_url_; }

  // Returns an url that does not assign a site to the SiteInstance it lives in,
  // as decided by the content embedder.
  const GURL& embedder_defined_unassigned_url() const {
    return embedder_defined_unassigned_url_;
  }

 private:
  GURL regular_url_;
  GURL unassigned_url_;
  GURL embedder_defined_unassigned_url_;

  std::unique_ptr<DontAssignSiteContentBrowserClient>
      content_browser_client_override_;
  ContentBrowserClient* old_client_ = nullptr;

  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
};

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       RendererInitiatedNavigationTo) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), regular_url()));
  RenderFrameHostImpl* initial_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> initial_si = initial_rfh->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());

  // Do a renderer-initiated navigation to an unassigned url.
  EXPECT_TRUE(NavigateToURLFromRenderer(initial_rfh, unassigned_url()));
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      web_contents()->GetMainFrame()->GetSiteInstance();

  if (!BackForwardCache::IsSameSiteBackForwardCacheFeatureEnabled()) {
    // Normally we reuse the SiteInstance.
    EXPECT_TRUE(unassigned_si->HasSite());
    EXPECT_EQ(unassigned_si, initial_si);
  } else {
    // With same-site BFCache, we will swap browsing instance for same-site
    // navigations, not reusing the previous SiteInstance.
    EXPECT_FALSE(unassigned_si->HasSite());
    EXPECT_FALSE(unassigned_si->IsRelatedSiteInstance(initial_si.get()));
  }
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       BrowserInitiatedNavigationTo) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), regular_url()));
  scoped_refptr<SiteInstanceImpl> initial_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());

  // Do a browser initiated navigation to an unassigned url.
  EXPECT_TRUE(NavigateToURL(shell(), unassigned_url()));
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      web_contents()->GetMainFrame()->GetSiteInstance();

  if (!BackForwardCache::IsSameSiteBackForwardCacheFeatureEnabled()) {
    // Normally we reuse the SiteInstance.
    EXPECT_TRUE(unassigned_si->HasSite());
    EXPECT_EQ(unassigned_si, initial_si);
  } else {
    // With same-site BFCache, we will swap browsing instance for same-site
    // navigations, not reusing the previous SiteInstance.
    EXPECT_FALSE(unassigned_si->HasSite());
    EXPECT_FALSE(unassigned_si->IsRelatedSiteInstance(initial_si.get()));
  }
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       RendererInitiatedNavigationFrom) {
  // Get a base page without a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), unassigned_url()));
  RenderFrameHostImpl* unassigned_url_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      unassigned_url_rfh->GetSiteInstance();
  EXPECT_FALSE(unassigned_si->HasSite());

  // Do a renderer-initiated navigation to an assigned url. We reuse the
  // unassigned SiteInstance and set its site.
  EXPECT_TRUE(NavigateToURLFromRenderer(unassigned_url_rfh, regular_url()));
  scoped_refptr<SiteInstanceImpl> initial_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());
  EXPECT_EQ(unassigned_si, initial_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       BrowserInitiatedNavigationFrom) {
  // Get a base page without a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), unassigned_url()));
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_FALSE(unassigned_si->HasSite());

  // Do a browser-initiated navigation to an assigned url. We reuse the
  // unassigned SiteInstance and set its site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  scoped_refptr<SiteInstanceImpl> initial_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());
  EXPECT_EQ(unassigned_si, initial_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       RendererInitiatedNavigationTo_CustomUrl) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* initial_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> initial_si = initial_rfh->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());

  // Do a renderer-initiated navigation to an embedder-defined unassigned url.
  // We use a new related or unrelated SiteInstance depending on the
  // ProactivelySwapBrowsingInstance feature. This differs from the base
  // unassigned behavior, because about:blank is explicitly considered as same
  // site with all other site, but custom embedder-defined unassigned urls are
  // not.
  EXPECT_TRUE(NavigateToURLFromRenderer(initial_rfh,
                                        embedder_defined_unassigned_url()));
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_FALSE(unassigned_si->HasSite());
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_FALSE(unassigned_si->IsRelatedSiteInstance(initial_si.get()));
  } else {
    EXPECT_TRUE(unassigned_si->IsRelatedSiteInstance(initial_si.get()));
  }
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       BrowserInitiatedNavigationTo_CustomUrl) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), regular_url()));
  scoped_refptr<SiteInstanceImpl> initial_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());

  // Do a browser initiated navigation to an embedder-defined unassigned url.
  // We do not reuse the assigned SiteInstance. We use a new SiteInstance in its
  // own BrowsingInstance.
  // Note: This differs from the renderer initiated behavior. This comes from
  // the fact that IsBrowsingInstanceSwapAllowedForPageTransition() explicitly
  // excludes renderer initiated navigations from generic cross-site
  // BrowsingInstance swap.
  EXPECT_TRUE(NavigateToURL(web_contents(), embedder_defined_unassigned_url()));
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_FALSE(unassigned_si->HasSite());
  EXPECT_FALSE(unassigned_si->IsRelatedSiteInstance(initial_si.get()));
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       RendererInitiatedNavigationFrom_CustomUrl) {
  // Get a base page without a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), embedder_defined_unassigned_url()));
  RenderFrameHostImpl* unassigned_url_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      unassigned_url_rfh->GetSiteInstance();
  EXPECT_FALSE(unassigned_si->HasSite());

  // Do a renderer-initiated navigation to an assigned url. We reuse the
  // unassigned SiteInstance and set its site.
  // TODO(crbug.com/1296173): We shouldn't let an embedder load arbitrary
  // content without assigning a site. This should be restricted to empty
  // schemes (that do not load content into a renderer and commit synchronously)
  // to avoid breaking Site Isolation.
  EXPECT_TRUE(NavigateToURLFromRenderer(unassigned_url_rfh, regular_url()));
  scoped_refptr<SiteInstanceImpl> initial_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());
  EXPECT_EQ(unassigned_si, initial_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       BrowserInitiatedNavigationFrom_CustomUrl) {
  // Get a base page without a site.
  EXPECT_TRUE(NavigateToURL(web_contents(), embedder_defined_unassigned_url()));
  scoped_refptr<SiteInstanceImpl> unassigned_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_FALSE(unassigned_si->HasSite());

  // Do a browser-initiated navigation to an assigned url. We reuse the
  // unassigned SiteInstance and set its site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  scoped_refptr<SiteInstanceImpl> initial_si =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(initial_si->HasSite());
  EXPECT_EQ(unassigned_si, initial_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InPopup_InitialAboutBlank) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a popup. It should reuse the opener SiteInstance.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(original_rfh, "window.open()"));
  scoped_refptr<SiteInstanceImpl> popup_si =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetMainFrame()
          ->GetSiteInstance();
  EXPECT_TRUE(popup_si->HasSite());
  EXPECT_EQ(popup_si, original_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InPopup_RendererInitiatedNavigateTo) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a same-origin popup.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(original_rfh, JsReplace("window.open($1)", regular_url())));
  WebContentsImpl* popup_web_contents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_web_contents));
  scoped_refptr<SiteInstanceImpl> popup_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(popup_si->HasSite());
  EXPECT_EQ(popup_si, original_si);

  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(popup_si->IsDefaultSiteInstance());

  // In the popup, do a renderer-initiated navigation to an unassigned url. We
  // should reuse the SiteInstance.
  EXPECT_TRUE(NavigateToURLFromRenderer(popup_web_contents->GetMainFrame(),
                                        unassigned_url()));
  scoped_refptr<SiteInstanceImpl> post_navigation_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(post_navigation_si->HasSite());
  EXPECT_EQ(post_navigation_si, original_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InPopup_BrowserInitiatedNavigateTo) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a same-origin popup.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(original_rfh, JsReplace("window.open($1)", regular_url())));
  WebContentsImpl* popup_web_contents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_web_contents));
  scoped_refptr<SiteInstanceImpl> popup_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(popup_si->HasSite());
  EXPECT_EQ(popup_si, original_si);

  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(popup_si->IsDefaultSiteInstance());

  // In the popup, do a browser-initiated navigation to an unassigned url. We
  // should reuse the SiteInstance.
  EXPECT_TRUE(NavigateToURL(popup_web_contents, unassigned_url()));
  scoped_refptr<SiteInstanceImpl> post_navigation_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(post_navigation_si->HasSite());
  EXPECT_EQ(post_navigation_si, original_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InPopup_RendererInitiatedNavigateTo_CustomUrl) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a same-origin popup.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(original_rfh, JsReplace("window.open($1)", regular_url())));
  WebContentsImpl* popup_web_contents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_web_contents));
  scoped_refptr<SiteInstanceImpl> popup_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(popup_si->HasSite());
  EXPECT_EQ(popup_si, original_si);

  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(popup_si->IsDefaultSiteInstance());

  // In the popup, do a renderer-initiated navigation to an embedder-defined
  // unassigned url. We use another related SiteInstance. Note that contrary to
  // its main window counterpart, here we never swap BrowsingInstance, because
  // ProactivelySwapBrowsingInstance never applies to popups.
  EXPECT_TRUE(NavigateToURLFromRenderer(popup_web_contents->GetMainFrame(),
                                        embedder_defined_unassigned_url()));
  scoped_refptr<SiteInstanceImpl> post_navigation_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_FALSE(post_navigation_si->HasSite());
  EXPECT_TRUE(post_navigation_si->IsRelatedSiteInstance(original_si.get()));
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InPopup_BrowserInitiatedNavigateTo_CustomUrl) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a same-origin popup.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(original_rfh, JsReplace("window.open($1)", regular_url())));
  WebContentsImpl* popup_web_contents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_web_contents));
  scoped_refptr<SiteInstanceImpl> popup_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_TRUE(popup_si->HasSite());
  EXPECT_EQ(popup_si, original_si);

  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(popup_si->IsDefaultSiteInstance());

  // In the popup, do a browser-initiated navigation to an embedder-defined
  // unassigned url. We swap browsing instance because the navigation is
  // considered cross-site.
  EXPECT_TRUE(
      NavigateToURL(popup_web_contents, embedder_defined_unassigned_url()));
  scoped_refptr<SiteInstanceImpl> post_navigation_si =
      popup_web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_FALSE(post_navigation_si->HasSite());
  EXPECT_FALSE(post_navigation_si->IsRelatedSiteInstance(original_si.get()));
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InIframe_InitialAboutBlank) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create an iframe. The initial empty document should reuse the parent
  // SiteInstance.
  ASSERT_TRUE(ExecJs(original_rfh, R"(
    const frame = document.createElement('iframe');
    document.body.appendChild(frame);
  )"));
  scoped_refptr<SiteInstanceImpl> iframe_si =
      original_rfh->child_at(0)->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(iframe_si->HasSite());
  EXPECT_EQ(iframe_si, original_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InIframe_RendererInitiatedNavigateTo) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a same-origin iframe. It should reuse the parent SiteInstance.
  ASSERT_TRUE(ExecJs(original_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                             regular_url())));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      original_rfh->child_at(0)->current_frame_host();
  scoped_refptr<SiteInstanceImpl> iframe_si = iframe_rfh->GetSiteInstance();
  EXPECT_TRUE(iframe_si->HasSite());
  EXPECT_EQ(iframe_si, original_si);

  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(iframe_si->IsDefaultSiteInstance());

  // In the iframe, navigate to an unassigned url. We reuse the SiteInstance.
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe_rfh, unassigned_url()));
  scoped_refptr<SiteInstanceImpl> post_navigation_si =
      original_rfh->child_at(0)->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(post_navigation_si->HasSite());
  EXPECT_EQ(post_navigation_si, original_si);
}

IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       InIframe_RendererInitiatedNavigateTo_CustomUrl) {
  // Get a base page with a site.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  RenderFrameHostImpl* original_rfh = web_contents()->GetMainFrame();
  scoped_refptr<SiteInstanceImpl> original_si = original_rfh->GetSiteInstance();
  EXPECT_TRUE(original_si->HasSite());

  // Create a same-origin iframe. It should reuse the parent SiteInstance.
  ASSERT_TRUE(ExecJs(original_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                             regular_url())));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      original_rfh->child_at(0)->current_frame_host();
  scoped_refptr<SiteInstanceImpl> iframe_si = iframe_rfh->GetSiteInstance();
  EXPECT_TRUE(iframe_si->HasSite());
  EXPECT_EQ(iframe_si, original_si);

  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(iframe_si->IsDefaultSiteInstance());

  // In the iframe, navigate to an embedder defined unassigned url. We use a new
  // related SiteInstance because the navigation is considered cross-site.
  EXPECT_TRUE(
      NavigateToURLFromRenderer(iframe_rfh, embedder_defined_unassigned_url()));
  scoped_refptr<SiteInstanceImpl> post_navigation_si =
      original_rfh->child_at(0)->current_frame_host()->GetSiteInstance();

  // On Android, we sometimes do not get a related SiteInstance, but the same
  // default SiteInstance to reduce memory footprint.
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(post_navigation_si->IsDefaultSiteInstance());
    EXPECT_TRUE(post_navigation_si->HasSite());
    EXPECT_EQ(post_navigation_si, original_si);
  } else {
    EXPECT_FALSE(post_navigation_si->HasSite());
    EXPECT_TRUE(post_navigation_si->IsRelatedSiteInstance(original_si.get()));
  }
}

// Ensure that coming back to a NavigationEntry with a previously unassigned
// SiteInstance (which is now used for another site) properly switches processes
// and SiteInstances.  See https://crbug.com/945399.
IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       BackToNowAssignedSiteInstance) {
  // Navigate to a URL that does not assign site URLs.
  EXPECT_TRUE(NavigateToURL(shell(), embedder_defined_unassigned_url()));
  EXPECT_EQ(embedder_defined_unassigned_url(),
            web_contents()->GetLastCommittedURL());
  scoped_refptr<SiteInstanceImpl> instance1(
      web_contents()->GetMainFrame()->GetSiteInstance());
  RenderProcessHost* process1 = instance1->GetProcess();
  EXPECT_EQ(GURL(), instance1->GetSiteURL());

  // Navigate to page that uses up the site. It should reuse the previous
  // SiteInstance and set its site URL.
  EXPECT_TRUE(NavigateToURL(shell(), regular_url()));
  EXPECT_EQ(instance1, web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_TRUE(instance1->HasSite());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(instance1->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(GURL("http://a.test"), instance1->GetSiteURL());
  }

  // The previously committed entry should get a new, related instance to avoid
  // a SiteInstance mismatch when returning to it. See http://crbug.com/992198
  // for further context.
  scoped_refptr<SiteInstanceImpl> prev_entry_instance =
      web_contents()
          ->GetController()
          .GetEntryAtIndex(0)
          ->root_node()
          ->frame_entry->site_instance();
  EXPECT_NE(prev_entry_instance, instance1);
  EXPECT_NE(prev_entry_instance, nullptr);
  EXPECT_TRUE(prev_entry_instance->IsRelatedSiteInstance(instance1.get()));
  EXPECT_EQ(GURL(), prev_entry_instance->GetSiteURL());

  // Navigate to bar.com, which destroys the previous RenderProcessHost.
  GURL other_regular_url(
      embedded_test_server()->GetURL("another.test", "/title1.html"));
  RenderProcessHostWatcher exit_observer(
      process1, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // With BackForwardCache, old process won't be deleted on navigation as it is
  // still in use by the bfcached document, disable back-forward cache to ensure
  // that the process gets deleted.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(shell(), other_regular_url));
  exit_observer.Wait();

  if (AreDefaultSiteInstancesEnabled()) {
    // Verify that the new navigation also results in a default SiteInstance,
    // and verify that it is not related to |instance1| because the navigation
    // swapped to a new BrowsingInstance.
    EXPECT_TRUE(web_contents()
                    ->GetMainFrame()
                    ->GetSiteInstance()
                    ->IsDefaultSiteInstance());
    EXPECT_FALSE(instance1->IsRelatedSiteInstance(
        web_contents()->GetMainFrame()->GetSiteInstance()));
  } else {
    EXPECT_NE(instance1, web_contents()->GetMainFrame()->GetSiteInstance());
  }

  // At this point, process1 is deleted, and the first entry is unfortunately
  // pointing to instance1, which has been locked to url2 and has no process.
  EXPECT_FALSE(instance1->HasProcess());
  if (AreAllSitesIsolatedForTesting()) {
    // In site-per-process, we cannot use foo.com's SiteInstance for a.com.
    EXPECT_FALSE(instance1->IsSuitableForUrlInfo(
        UrlInfo::CreateForTesting(embedder_defined_unassigned_url())));
  } else if (AreDefaultSiteInstancesEnabled()) {
    // Since |instance1| is a default SiteInstance AND this test explicitly
    // ensures that ShouldAssignSiteForURL(url1) will return false, |url1|
    // cannot be placed in the default SiteInstance. This also means that |url1|
    // cannot be placed in the same process as the default SiteInstance.
    EXPECT_FALSE(instance1->IsSuitableForUrlInfo(
        UrlInfo::CreateForTesting(embedder_defined_unassigned_url())));
  } else {
    // If neither foo.com nor a.com require dedicated processes, then we can use
    // the same process.
    EXPECT_TRUE(instance1->IsSuitableForUrlInfo(
        UrlInfo::CreateForTesting(embedder_defined_unassigned_url())));
  }

  // Go back to url1's entry, which should swap to a new SiteInstance with an
  // unused site URL.
  TestNavigationObserver observer(web_contents());
  web_contents()->GetController().GoToOffset(-2);
  observer.Wait();
  scoped_refptr<SiteInstanceImpl> new_instance =
      web_contents()->GetMainFrame()->GetSiteInstance();
  EXPECT_EQ(embedder_defined_unassigned_url(),
            web_contents()->GetLastCommittedURL());
  EXPECT_NE(instance1, new_instance);
  EXPECT_EQ(GURL(), new_instance->GetSiteURL());
  EXPECT_TRUE(new_instance->HasProcess());

  // Because embedder_defined_unassigned_url does not set a site URL, it should
  // not lock the new process either, so that it can be used for subsequent
  // navigations.
  content::RenderProcessHost* new_process = new_instance->GetProcess();
  auto* policy = ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(policy->CanAccessDataForOrigin(
      new_process->GetID(),
      url::Origin::Create(embedder_defined_unassigned_url())));
  EXPECT_TRUE(policy->CanAccessDataForOrigin(
      new_process->GetID(), url::Origin::Create(regular_url())));
}

// Check that when a navigation to a URL that doesn't require assigning a site
// URL is in progress, another navigation can't reuse the same process in the
// meantime.  Such reuse previously led to a renderer kill when the unassigned
// URL later committed; a real-world example of the unassigned URL was
// chrome-native://newtab.  See https://crbug.com/970046.
IN_PROC_BROWSER_TEST_P(UnassignedSiteInstanceBrowserTest,
                       NavigationRacesWithCommitInunassignedSiteInstance) {
  // Prepare for a second navigation to a normal URL.  Ensure it's isolated so
  // that it requires a process lock on all platforms.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(regular_url())},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  // Create a new shell where the normal url origin isolation will take effect.
  Shell* shell = CreateBrowser();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  RenderProcessHost* regular_process = nullptr;
  TestNavigationManager regular_manager(web_contents, regular_url());
  auto& current_isolation_context =
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext();
  auto site_info = SiteInfo::CreateForTesting(current_isolation_context,
                                              GURL("http://a.test"));
  EXPECT_TRUE(site_info.RequiresDedicatedProcess(current_isolation_context));

  // Set up the work to be done after the renderer is asked to commit
  // |embedder_defined_unassigned_url|, but before the corresponding
  // DidCommitProvisionalLoad IPC is processed.  This will start a navigation to
  // |regular_url| and wait for its response.
  auto did_commit_callback =
      base::BindLambdaForTesting([&](RenderFrameHost* rfh) {
        // The navigation should stay in the initial empty SiteInstance, with
        // the site still unassigned.
        EXPECT_FALSE(
            static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance())->HasSite());
        EXPECT_FALSE(root->render_manager()->speculative_frame_host());

        shell->LoadURL(regular_url());

        // The foo.com navigation should swap to a new process, since it is not
        // safe to reuse |embedder_defined_unassigned_url|'s process before
        // |embedder_defined_unassigned_url| commits.
        EXPECT_TRUE(root->render_manager()->speculative_frame_host());
        regular_process =
            root->render_manager()->speculative_frame_host()->GetProcess();

        // Wait for response.  This will cause |regular_manager| to spin up a
        // nested message loop while we're blocked in the current message loop
        // (within DidCommitNavigationInterceptor).  Thus, it's important to
        // allow nestable tasks in |regular_manager|'s message loop, so that it
        // can process the response before we unblock the
        // DidCommitNavigationInterceptor's message loop and finish processing
        // the commit.
        regular_manager.AllowNestableTasks();
        EXPECT_TRUE(regular_manager.WaitForResponse());

        regular_manager.ResumeNavigation();
        // After returning here, the commit for
        // |embedder_defined_unassigned_url| will be processed.
      });

  CommitMessageDelayer commit_delayer(
      web_contents, embedder_defined_unassigned_url() /* deferred_url */,
      std::move(did_commit_callback));

  // Start the first navigation, which does not assign a site URL.
  shell->LoadURL(embedder_defined_unassigned_url());

  // The navigation should stay in the initial empty SiteInstance, so there
  // shouldn't be a speculative RFH at this point.
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());

  // Wait for the DidCommit IPC for |embedder_defined_unassigned_url|, and
  // before processing it, trigger a navigation to |regular_url| and wait for
  // its response.
  commit_delayer.Wait();

  // Check that the renderer hasn't been killed.  At this point, it should've
  // successfully committed the navigation to |embedder_defined_unassigned_url|,
  // and it shouldn't be locked.
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(embedder_defined_unassigned_url(),
            web_contents->GetMainFrame()->GetLastCommittedURL());
  RenderProcessHost* process1 = web_contents->GetMainFrame()->GetProcess();
  EXPECT_FALSE(web_contents->GetMainFrame()->GetSiteInstance()->HasSite());
  auto process1_lock = process1->GetProcessLock();
  EXPECT_FALSE(process1_lock.is_invalid());
  EXPECT_TRUE(process1_lock.allows_any_site());

  // Now wait for second navigation to finish and ensure it also succeeds.
  regular_manager.WaitForNavigationFinished();
  EXPECT_TRUE(regular_manager.was_successful());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(regular_url(), web_contents->GetMainFrame()->GetLastCommittedURL());

  // The regular url navigation should've used a different process, locked to
  // a.test.
  BrowserContext* browser_context = web_contents->GetBrowserContext();
  RenderProcessHost* process2 = web_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(process1, process2);
  EXPECT_EQ(GURL("http://a.test"),
            web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(
      ProcessLock::FromSiteInfo(SiteInfo(
          GURL("http://a.test"), GURL("http://a.test"),
          false /* requires_origin_keyed_process */, false /* is_sandboxed */,
          StoragePartitionConfig::CreateDefault(browser_context),
          WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
          false /* does_site_request_dedicated_process_for_coop */,
          false /* is_jit_disabled */, false /* is_pdf */)),
      policy->GetProcessLock(process2->GetID()));

  // Ensure also that the regular url process didn't change midway through the
  // navigation.
  EXPECT_EQ(regular_process, process2);
}

static auto kTestParams =
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         UnassignedSiteInstanceBrowserTest,
                         kTestParams,
                         UnassignedSiteInstanceBrowserTest::DescribeParams);

}  // namespace content
