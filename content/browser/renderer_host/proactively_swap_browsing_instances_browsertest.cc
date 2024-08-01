// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_manager_browsertest.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

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

  const GURL url_origin = url.DeprecatedGetOriginAsURL();
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
          ->GetPrimaryFrameTree()
          .root(),
      url);
}

bool HasErrorPageSiteInfo(SiteInstance* site_instance) {
  auto* site_instance_impl = static_cast<SiteInstanceImpl*>(site_instance);
  return site_instance_impl->GetSiteInfo().is_error_page();
}

class BrowsingContextGroupSwapObserver : public WebContentsObserver {
 public:
  explicit BrowsingContextGroupSwapObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    latest_swap_ = NavigationRequest::From(navigation_handle)
                       ->browsing_context_group_swap();
  }

  BrowsingContextGroupSwap GetLatestBrowsingContextGroupSwap() const {
    EXPECT_TRUE(latest_swap_.has_value());
    return latest_swap_.value();
  }

 private:
  std::optional<BrowsingContextGroupSwap> latest_swap_;
};

}  // namespace

class ProactivelySwapBrowsingInstancesTest : public RenderFrameHostManagerTest {
 public:
  // Enable BFCache so that BrowsingInstance swap will happen.
  ProactivelySwapBrowsingInstancesTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  ~ProactivelySwapBrowsingInstancesTest() override = default;

  void ExpectTotalCount(std::string_view name,
                        base::HistogramBase::Count count) {
    FetchHistogramsFromChildProcesses();
    histogram_tester_.ExpectTotalCount(name, count);
  }

  template <typename T>
  void ExpectBucketCount(std::string_view name,
                         T sample,
                         base::HistogramBase::Count expected_count) {
    FetchHistogramsFromChildProcesses();
    histogram_tester_.ExpectBucketCount(name, sample, expected_count);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// Test to ensure that the error page navigation does not change
// BrowsingInstances when window.open is present.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesTest,
    ErrorPageNavigationWithWindowOpenDoesNotChangeBrowsingInstance) {
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
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_EQ(1, nav_controller.GetEntryCount());

  // Open a new window to ensure that we can't swap BrowsingInstances
  // as we have to preserve the scripting relationship.
  EXPECT_TRUE(OpenPopup(shell(), GURL(url::kAboutBlankURL), ""));

  // Navigate to an url resulting in an error page and ensure a new entry
  // was added to session history.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(2, nav_controller.GetEntryCount());

  scoped_refptr<SiteInstance> initial_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(HasErrorPageSiteInfo(initial_instance.get()));
  EXPECT_TRUE(IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
  EXPECT_TRUE(success_site_instance->IsRelatedSiteInstance(
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance()));

  // Reload of the error page that still results in an error should stay in
  // the related SiteInstance. Ensure this works for both browser-initiated
  // reloads and renderer-initiated ones.
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
    EXPECT_TRUE(success_site_instance->IsRelatedSiteInstance(
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance()));
  }
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), "location.reload();"));
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_TRUE(
        IsMainFrameOriginOpaqueAndCompatibleWithURL(shell(), error_url));
    EXPECT_TRUE(success_site_instance->IsRelatedSiteInstance(
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance()));
  }

  // Allow the navigation to succeed and ensure the new SiteInstance
  // stays related.
  url_interceptor.reset();
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), "location.reload();"));
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(2, nav_controller.GetEntryCount());
    EXPECT_TRUE(success_site_instance->IsRelatedSiteInstance(
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance()));
  }
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       ReloadShouldNotChangeBrowsingInstance) {
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("/title1.html"));

  // 1) Navigate to the page.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  scoped_refptr<SiteInstance> site_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // 2) Reload page.
  shell()->web_contents()->GetPrimaryMainFrame()->Reload();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Ensure that we do not change BrowsingInstances for reload.
  // We should keep this even when we start swapping BrowsingInstances
  // for same-site navigations.
  EXPECT_EQ(site_instance,
            shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
}

class ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest
    : public RenderFrameHostManagerTest {
 public:
  ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  ~ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    RenderFrameHostManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING)
          << "This test should be run without strict site isolation. "
          << "It's going to fail when  --site-per-process is specified.";
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When we do a BrowsingInstance swap on renderer-initiated cross-site
// navigation, the current process will not be reused.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    RendererInitiatedCrossSiteNavigationDoesNotReuseProcess) {
  if (AreAllSitesIsolatedForTesting())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstanceImpl> a_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Navigate to B. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that A and B are in different BrowsingInstances and renderer
  // process. When default SiteInstances are enabled, A and B are
  // both default SiteInstances of different BrowsingInstances.
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(a_site_instance->GetProcess(), b_site_instance->GetProcess());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            a_site_instance->IsDefaultSiteInstance());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            b_site_instance->IsDefaultSiteInstance());
}

// Different from renderer-initiated cross-site navigations, browser-initiated
// cross-site navigations do swap BrowsingInstances and processes without
// ProactivelySwapBrowsingInstance. Because of that, we shouldn't reuse the
// process for the new BrowsingInstance.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    BrowserInitiatedCrossSiteNavigationDoesNotReuseProcess) {
  if (AreAllSitesIsolatedForTesting())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstanceImpl> a_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Navigate to B. The navigation is browser initiated.
  EXPECT_TRUE(NavigateToURL(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that A and B are in different BrowsingInstances and renderer
  // processes. When default SiteInstances are enabled, A and B are
  // both default SiteInstances of different BrowsingInstances.
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(a_site_instance->GetProcess(), b_site_instance->GetProcess());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            a_site_instance->IsDefaultSiteInstance());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            b_site_instance->IsDefaultSiteInstance());
}

// A test ContentBrowserClient implementation that enforce process-per-site mode
// if |should_use_process_per_site_| is true. It is used to verify that we don't
// reuse the current page's renderer process when navigating to sites that uses
// process-per-site.
class ProcessPerSiteContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  ProcessPerSiteContentBrowserClient() = default;

  ProcessPerSiteContentBrowserClient(
      const ProcessPerSiteContentBrowserClient&) = delete;
  ProcessPerSiteContentBrowserClient& operator=(
      const ProcessPerSiteContentBrowserClient&) = delete;

  void SetShouldUseProcessPerSite(bool should_use_process_per_site) {
    should_use_process_per_site_ = should_use_process_per_site;
  }

  bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                               const GURL& site_url) override {
    return should_use_process_per_site_;
  }

 private:
  bool should_use_process_per_site_ = false;
};

// We should not reuse the current process on renderer-initiated navigations to
// sites that needs to use process-per-site, and should create a new process for
// the site if there isn't already a process for that site.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    RendererInitiatedCrossSiteNavigationToProcessPerSiteURLCreatesNewProcess) {
  if (AreAllSitesIsolatedForTesting())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ProcessPerSiteContentBrowserClient content_browser_client;
  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstanceImpl> a_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  RenderProcessHost* original_process = a_site_instance->GetProcess();

  // Navigate to B. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that A and B are in different BrowsingInstances and renderer
  // processes.
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(b_site_instance->GetProcess(), original_process);
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            a_site_instance->IsDefaultSiteInstance());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            b_site_instance->IsDefaultSiteInstance());

  // Make sure we will use process-per-site for C.
  // Note this is enforcing process-per-site for all sites, which is why we turn
  // it off right after the navigation to C. We might reconsider after
  // crbug.com/1062211 is fixed.
  content_browser_client.SetShouldUseProcessPerSite(true);

  // Navigate to C. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), c_url));
  scoped_refptr<SiteInstanceImpl> c_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that B and C are in different BrowsingInstances and renderer
  // processes.
  EXPECT_FALSE(b_site_instance->IsRelatedSiteInstance(c_site_instance.get()));
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            c_site_instance->IsDefaultSiteInstance());
  EXPECT_NE(c_site_instance->GetProcess(), original_process);
  // C is using the process for C's site.
  EXPECT_EQ(c_site_instance->GetProcess(),
            RenderProcessHostImpl::GetSoleProcessHostForSite(
                c_site_instance->GetIsolationContext(),
                c_site_instance->GetSiteInfo()));

  // Make sure we will not use process-per-site for B.
  content_browser_client.SetShouldUseProcessPerSite(false);

  // Navigate to B again. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b2_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(b2_site_instance->IsRelatedSiteInstance(c_site_instance.get()));
  EXPECT_FALSE(b2_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            b2_site_instance->IsDefaultSiteInstance());
  EXPECT_NE(b2_site_instance->GetProcess(), original_process);
  // Check that B and C are in different renderer processes.
  EXPECT_NE(b2_site_instance->GetProcess(), c_site_instance->GetProcess());
}

// We should not reuse the current process on renderer-initiated navigations to
// sites that needs to use process-per-site, and should use the sole process for
// that site if it already exists.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    RendererInitiatedCrossSiteNavigationToProcessPerSiteURLUsesProcessForSite) {
  if (AreAllSitesIsolatedForTesting())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ProcessPerSiteContentBrowserClient content_browser_client;

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstanceImpl> a_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  RenderProcessHost* original_process = a_site_instance->GetProcess();

  // Create a new process and set it as the sole process host for B.
  scoped_refptr<SiteInstanceImpl> placeholder_b_site_instance =
      SiteInstanceImpl::CreateForTesting(web_contents->GetBrowserContext(),
                                         b_url);
  RenderProcessHost* process_for_b =
      RenderProcessHostImpl::CreateRenderProcessHost(
          web_contents->GetBrowserContext(), placeholder_b_site_instance.get());
  RenderProcessHostImpl::RegisterSoleProcessHostForSite(
      process_for_b, placeholder_b_site_instance.get());
  EXPECT_EQ(process_for_b,
            RenderProcessHostImpl::GetSoleProcessHostForSite(
                placeholder_b_site_instance->GetIsolationContext(),
                placeholder_b_site_instance->GetSiteInfo()));
  // Make sure we will use process-per-site for B.
  content_browser_client.SetShouldUseProcessPerSite(true);

  // Navigate to B. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that A and B are in different BrowsingInstances but B should use the
  // sole process assigned to site B.
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            a_site_instance->IsDefaultSiteInstance());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            b_site_instance->IsDefaultSiteInstance());
  EXPECT_NE(b_site_instance->GetProcess(), original_process);
  EXPECT_EQ(b_site_instance->GetProcess(), process_for_b);
  EXPECT_EQ(b_site_instance->GetProcess(),
            RenderProcessHostImpl::GetSoleProcessHostForSite(
                b_site_instance->GetIsolationContext(),
                b_site_instance->GetSiteInfo()));
}

// We should not reuse the current process on renderer-initiated navigations to
// sites that require a dedicated process.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    NavigationToSiteThatRequiresDedicatedProcess) {
  if (AreAllSitesIsolatedForTesting())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // The client will make sure b.com require a dedicated process.
  EffectiveURLContentBrowserTestContentBrowserClient modified_client(
      b_url /* url_to_modify */, b_url, true /* requires_dedicated_process */);
  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstanceImpl> a_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(a_site_instance->RequiresDedicatedProcess());

  // 2) Navigate cross-site to B. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_TRUE(b_site_instance->RequiresDedicatedProcess());

  // Check that A and B are in different BrowsingInstances and processes.
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(a_site_instance->GetProcess(), b_site_instance->GetProcess());
}

// We should not reuse the current process on renderer-initiated navigations to
// sites that require a dedicated process.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    NavigationFromSiteThatRequiresDedicatedProcess) {
  if (AreAllSitesIsolatedForTesting())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // The client will make sure a.com require a dedicated process.
  EffectiveURLContentBrowserTestContentBrowserClient modified_client(
      a_url /* url_to_modify */, a_url, true /* requires_dedicated_process */);
  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  scoped_refptr<SiteInstanceImpl> a_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_TRUE(a_site_instance->RequiresDedicatedProcess());

  // 2) Navigate cross-site to B. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(b_site_instance->RequiresDedicatedProcess());

  // Check that A and B are in different BrowsingInstances and processes.
  EXPECT_FALSE(a_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(a_site_instance->GetProcess(), b_site_instance->GetProcess());
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       RendererInitiatedSameSiteNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Navigate to title2.html. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       BrowserInitiatedSameSiteNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // 2) Navigate to title2.html. The navigation is browser initiated.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Do a back navigation to title1.html.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We will reuse the SiteInstance and renderer process of |site_instance_1|.
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_1->GetProcess());
}

// Tests that navigations that started but haven't committed yet will be
// overridden by navigations started later if both navigations created
// speculative RFHs.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       MultipleNavigationsStarted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a1_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL a2_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL b1_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL b2_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), a1_url));
  auto* a1_rfh = web_contents->GetPrimaryMainFrame();
  FrameTreeNode* node = a1_rfh->frame_tree_node();
  scoped_refptr<SiteInstanceImpl> a1_site_instance =
      static_cast<SiteInstanceImpl*>(a1_rfh->GetSiteInstance());

  // 2) Start same-site navigation to A2 without committing.
  TestNavigationManager navigation_a2(shell()->web_contents(), a2_url);
  shell()->LoadURL(a2_url);
  // Chrome in Android will use unisolated site instance, we have to
  // use `WaitForResponse` since the speculative RFH won't be necessariliy
  // created.
  EXPECT_TRUE(navigation_a2.WaitForResponse());
  // Verify that we're now navigating to |a2_url|.
  EXPECT_EQ(node->navigation_request()->GetURL(), a2_url);
  // We should have a speculative RFH for this navigation.
  RenderFrameHostImpl* a2_speculative_rfh =
      node->render_manager()->speculative_frame_host();
  EXPECT_TRUE(a2_speculative_rfh);
  EXPECT_NE(a1_rfh, a2_speculative_rfh);
  // The speculative RFH should use a different BrowsingInstance than the
  // current RFH.
  scoped_refptr<SiteInstanceImpl> a2_site_instance =
      static_cast<SiteInstanceImpl*>(a2_speculative_rfh->GetSiteInstance());
  EXPECT_FALSE(a1_site_instance->IsRelatedSiteInstance(a2_site_instance.get()));

  // 3) Start cross-site navigation to B1 without committing.
  TestNavigationManager navigation_b1(shell()->web_contents(), b1_url);
  shell()->LoadURL(b1_url);
  EXPECT_TRUE(navigation_b1.WaitForResponse());
  // Verify that we're now navigating to |b1_url|.
  EXPECT_EQ(node->navigation_request()->GetURL(), b1_url);
  // We should have a speculative RFH for this navigation.
  RenderFrameHostImpl* b1_speculative_rfh =
      node->render_manager()->speculative_frame_host();
  EXPECT_TRUE(b1_speculative_rfh);
  EXPECT_NE(a1_rfh, b1_speculative_rfh);
  // The speculative RFH should use a different BrowsingInstance than the
  // current RFH.
  scoped_refptr<SiteInstanceImpl> b1_site_instance =
      static_cast<SiteInstanceImpl*>(b1_speculative_rfh->GetSiteInstance());
  EXPECT_FALSE(a1_site_instance->IsRelatedSiteInstance(b1_site_instance.get()));

  // 4) Start same-site navigation to B2 without committing.
  TestNavigationManager navigation_b2(shell()->web_contents(), b2_url);
  // B2 shall reuse the speculative RFH. No RFH creation.
  shell()->LoadURL(b2_url);
  // Verify that we're now navigating to |b2_url|.
  EXPECT_EQ(node->navigation_request()->GetURL(), b2_url);
  EXPECT_TRUE(navigation_b2.WaitForResponse());
  // We should have a speculative RFH for this navigation.
  RenderFrameHostImpl* b2_speculative_rfh =
      node->render_manager()->speculative_frame_host();
  EXPECT_TRUE(b2_speculative_rfh);
  EXPECT_NE(a1_rfh, b2_speculative_rfh);
  // The speculative RFH should use a different BrowsingInstance than the
  // current RFH.
  scoped_refptr<SiteInstanceImpl> b2_site_instance =
      static_cast<SiteInstanceImpl*>(b2_speculative_rfh->GetSiteInstance());
  EXPECT_FALSE(a1_site_instance->IsRelatedSiteInstance(b2_site_instance.get()));
}

// Tests history same-site process reuse:
// 1. Visit A1, A2, B.
// 2. Go back to A2 (should use new process).
// 3. Go back to A1 (should reuse A2's process).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       HistoryNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // 2) Navigate same-site to title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Navigate cross-site to b.com/title3.html.
  RenderFrameDeletedObserver rfh_2_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache. So, we should flush back-forward
  // cache.
  web_contents->GetController().GetBackForwardCache().Flush();
  // Wait until the RFH for title2.html got deleted, and check that
  // title2.html and b.com/title3.html are in different BrowsingInstances and
  // renderer processes (We check this by checking whether |site_instance_2|
  // still has a process or not - if it's gone then that means
  // |site_instance_3| uses a different process).
  rfh_2_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_2->IsRelatedSiteInstance(site_instance_3.get()));
  EXPECT_FALSE(site_instance_2->HasProcess());

  // 4) Do a back navigation to title2.html.
  RenderFrameDeletedObserver rfh_3_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_2);
  scoped_refptr<SiteInstanceImpl> site_instance_2_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache. So, we should flush back-forward
  // cache.
  web_contents->GetController().GetBackForwardCache().Flush();
  // We should use different BrowsingInstances and processes after going back to
  // title2.html because it's a cross-site navigation.
  rfh_3_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_2_history_nav->IsRelatedSiteInstance(
      site_instance_3.get()));
  EXPECT_FALSE(site_instance_3->HasProcess());

  // 5) Do a back navigation to title1.html.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should use different BrowsingInstances for title1.html and title2.html,
  // but reuse the process (because in the original navigation, the BI change
  // was caused by proactive BI swap).
  EXPECT_FALSE(site_instance_1_history_nav->IsRelatedSiteInstance(
      site_instance_2_history_nav.get()));
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_TRUE(site_instance_2_history_nav->HasProcess());
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_2_history_nav->GetProcess());
}

// Tests history same-site process reuse:
// 1. Visit A1, A2, B.
// 2. Go back two entries to A1 (should use new process).
// 3. Go forward to A2 (should reuse A1's process).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       HistoryNavigationReusesProcess_SkipSameSiteEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // 2) Navigate same-site to title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Navigate cross-site to b.com/title3.html.
  RenderFrameDeletedObserver rfh_2_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache. So, we should flush back-forward
  // cache.
  web_contents->GetController().GetBackForwardCache().Flush();
  // Wait until the RFH for title2.html got deleted, and check that
  // title2.html and b.com/title3.html are in different BrowsingInstances and
  // renderer processes (We check this by checking whether |site_instance_2|
  // still has a process or not - if it's gone then that means
  // |site_instance_3| uses a different process).
  rfh_2_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_2->IsRelatedSiteInstance(site_instance_3.get()));
  EXPECT_FALSE(site_instance_2->HasProcess());

  // 4) Navigate back 2 entries to title1.html.
  RenderFrameDeletedObserver rfh_3_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(ExecJs(shell(), "history.go(-2)"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache. So, we should flush back-forward
  // cache.
  web_contents->GetController().GetBackForwardCache().Flush();
  // We should use different BrowsingInstances and processes after going back to
  // title2.html because it's a cross-site navigation.
  rfh_3_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_1_history_nav->IsRelatedSiteInstance(
      site_instance_3.get()));
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_FALSE(site_instance_3->HasProcess());

  // 5) Navigate 1 entry forward to title2.html.
  EXPECT_TRUE(ExecJs(shell(), "history.go(1)"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_2);
  scoped_refptr<SiteInstanceImpl> site_instance_2_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should use different BrowsingInstances for title1.html and title2.html,
  // but reuse the process (because in the original navigation, the BI change
  // was caused by proactive BI swap).
  EXPECT_FALSE(site_instance_1_history_nav->IsRelatedSiteInstance(
      site_instance_2_history_nav.get()));
  EXPECT_EQ(site_instance_2_history_nav, site_instance_2);
  EXPECT_TRUE(site_instance_1_history_nav->HasProcess());
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_2_history_nav->GetProcess());
}

// Tests history same-site process reuse:
// 1. Visit A1, B, A3.
// 2. Go back two entries to A1 (should use A3's process).
// 3. Go forward to B (should use new process).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       HistoryNavigationReusesProcess_SkipCrossSiteEntry) {
  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache.  So, we should disable back-forward
  // cache for this test.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_REQUIRES_NO_CACHING);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  RenderFrameDeletedObserver rfh_1_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  // 2) Navigate cross-site to b.com/title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and b.com/title2.html are in different
  // BrowsingInstances and renderer processes (We check this by checking
  // whether |site_instance_1| still has a process or not - if it's gone then
  // that means |site_instance_2| uses a different process).
  rfh_1_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_FALSE(site_instance_1->HasProcess());

  // 3) Navigate cross-site to title3.html.
  RenderFrameDeletedObserver rfh_2_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Wait until the RFH for title2.html got deleted, and check that
  // b.com/title2.html and title3.html are in different BrowsingInstances and
  // renderer processes (We check this by checking whether |site_instance_2|
  // still has a process or not - if it's gone then that means
  // |site_instance_3| uses a different process).
  rfh_2_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_2->IsRelatedSiteInstance(site_instance_3.get()));
  EXPECT_FALSE(site_instance_2->HasProcess());

  // 4) Navigate back 2 entries from title3.html to title1.html.
  EXPECT_TRUE(ExecJs(shell(), "history.go(-2)"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // We should use different BrowsingInstances but reuse the process when going
  // back from title3.html to title1.html because it's a same-site history
  // navigation.
  EXPECT_FALSE(site_instance_1_history_nav->IsRelatedSiteInstance(
      site_instance_3.get()));
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_TRUE(site_instance_3->HasProcess());
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_3->GetProcess());
}

// Tests history same-site process reuse:
// 1. Visit A1 (which window.opens A2) then B.
// 2. Visit A3, which should use a new process (can't use A2's process).
// 2. Go back two entries to A1 (should use A2's process - the same process it
// used originally).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       HistoryNavigationReusesProcessThatIsStillAlive) {
  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache.  So, we should disable back-forward
  // cache for this test.
  DisableBackForwardCache(BackForwardCacheImpl::TEST_REQUIRES_NO_CACHING);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_to_open(embedded_test_server()->GetURL("/empty.html"));
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // 1) Navigate to title1.html and open a popup.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  OpenPopup(shell(), url_to_open, "foo");
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Navigate cross-site to b.com/title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and b.com/title2.html are in different
  // BrowsingInstances and renderer processes. title1.html's process will still
  // be around because the window it opened earlier is still alive.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_TRUE(site_instance_1->HasProcess());
  EXPECT_NE(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Navigate cross-site to title3.html (same-site with title1.html).
  RenderFrameDeletedObserver rfh_2_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Wait until the RFH for b.com/title2.html got deleted, and check that
  // b.com/title2.html and title3.html are in different BrowsingInstances and
  // renderer processes (We check this by checking whether |site_instance_2|
  // still has a process or not - if it's gone then that means
  // |site_instance_3| uses a different process).
  rfh_2_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_2->IsRelatedSiteInstance(site_instance_3.get()));
  EXPECT_FALSE(site_instance_2->HasProcess());
  // Even though title1.html and title3.html are same-site, they should use
  // different processes.
  EXPECT_NE(site_instance_1->GetProcess(), site_instance_3->GetProcess());

  // 4) Navigate back 2 entries from title3.html to title1.html.
  RenderFrameDeletedObserver rfh_3_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(ExecJs(shell(), "history.go(-2)"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // We should use different BrowsingInstances and not reuse the process when
  // going back from title3.html to title1.html because the original process
  // for title1.html is still around (also title3.html shouldn't be able to
  // script the window opened by title1.html).
  rfh_3_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_1_history_nav->IsRelatedSiteInstance(
      site_instance_3.get()));
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_FALSE(site_instance_3->HasProcess());
}

// If the navigation is same-document or ends up using the same NavigationEntry
// (e.g., enter in omnibox converted to a reload), we should not do a proactive
// BrowsingInstance swap.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       SameEntryAndSameDocumentNavigationDoesNotSwap) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html#foo.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html#foo")));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Navigate from title1.html#foo to title1.html.
  // This is a same-document, different-entry navigation.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that #1 and #2 are in the same SiteInstance.
  EXPECT_EQ(site_instance_1, site_instance_2);

  // 3) Navigate from title1.html to title1.html.
  // This is a different-document, same-entry navigation.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should keep the same SiteInstance again.
  EXPECT_EQ(site_instance_2, site_instance_3);

  // 4) Navigate from title1.html to title1.html#foo.
  // This is a same document navigation.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html#foo")));
  scoped_refptr<SiteInstanceImpl> site_instance_4 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should keep the same SiteInstance again.
  EXPECT_EQ(site_instance_3, site_instance_4);

  // 5) Navigate from title1.html#foo to title1.html#foo.
  // This is a different-document, same-entry navigation.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html#foo")));
  scoped_refptr<SiteInstanceImpl> site_instance_5 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should keep the same SiteInstance again.
  EXPECT_EQ(site_instance_4, site_instance_5);

  // 6) Navigate from title1.html#foo to title1.html#bar.
  // This is a same document navigation.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html#bar")));
  scoped_refptr<SiteInstanceImpl> site_instance_6 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should keep the same SiteInstance again.
  EXPECT_EQ(site_instance_5, site_instance_6);

  // 7) Do a history navigation from title1.html#bar to title1.html#foo.
  // This is a same-document, different-entry history navigation.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  scoped_refptr<SiteInstanceImpl> site_instance_7 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should keep the same SiteInstance again.
  EXPECT_EQ(site_instance_6, site_instance_7);
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       ReloadDoesNotSwap) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Request a reload to happen when the controller becomes active (e.g.
  // after the renderer gets killed in background on Android).
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  ASSERT_FALSE(controller.NeedsReload());
  controller.SetNeedsReload();
  // Set the restore type to `kRestored`, since `SetNeedsReload()` should only
  // be used for session restore.
  controller.GetLastCommittedEntry()->set_restore_type(RestoreType::kRestored);
  ASSERT_TRUE(controller.NeedsReload());

  // Set the controller as active, triggering the requested reload.
  controller.SetActive(true);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  ASSERT_FALSE(controller.NeedsReload());
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Check that we're still in the same SiteInstance.
  EXPECT_EQ(site_instance_1, site_instance_2);

  // 3) Trigger reload using Reload().
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
  }
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Check that we're still in the same SiteInstance.
  EXPECT_EQ(site_instance_2, site_instance_3);

  // 4) Trigger reload using location.reload().
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), "location.reload();"));
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
  }
  scoped_refptr<SiteInstanceImpl> site_instance_4 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Check that we're still in the same SiteInstance.
  EXPECT_EQ(site_instance_3, site_instance_4);

  // 5) Do a replaceState to another URL.
  {
    TestNavigationObserver observer(web_contents);
    std::string script = "history.replaceState({}, '', '/title2.html')";
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();
  }
  scoped_refptr<SiteInstanceImpl> site_instance_5 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Check that we're still in the same SiteInstance.
  EXPECT_EQ(site_instance_4, site_instance_5);

  // 6) Reload after a replaceState by simulating the user hitting Enter in the
  // omnibox without changing the URL.
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().LoadURL(web_contents->GetLastCommittedURL(),
                                          Referrer(), ui::PAGE_TRANSITION_LINK,
                                          std::string());
    observer.Wait();
  }
  scoped_refptr<SiteInstanceImpl> site_instance_6 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // Check that we're still in the same SiteInstance.
  EXPECT_EQ(site_instance_5, site_instance_6);
}

// Regression test for crbug.com/340606786. This test ensures that the browser
// doesn't crash if a reload happens on a post-commit error page.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       ReloadPostCommitErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // 2) Load post commit error page.
  RenderFrameHost* root = shell()->web_contents()->GetPrimaryMainFrame();
  std::string error_html = "Error page";
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(root, url, error_html);
  error_observer.Wait();

  // 3) Request a reload to happen when the controller becomes active (e.g.
  // after the renderer gets killed in background on Android).
  ASSERT_FALSE(controller.NeedsReload());
  controller.SetNeedsReload();
  // Set the restore type to `kRestored`, since `SetNeedsReload()` should only
  // be used for session restore.
  controller.GetLastCommittedEntry()->set_restore_type(RestoreType::kRestored);
  ASSERT_TRUE(controller.NeedsReload());

  // Set the controller as active, triggering the requested reload.
  controller.SetActive(true);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // The reload should not crash.
  ASSERT_FALSE(controller.NeedsReload());
}

class ProactivelySwapBrowsingInstancesTestWithoutSpeculativeRFHDelay
    : public ProactivelySwapBrowsingInstancesTest {
 public:
  ProactivelySwapBrowsingInstancesTestWithoutSpeculativeRFHDelay() {
    feature_list_for_defer_speculative_rfh_.InitAndEnableFeatureWithParameters(
        features::kDeferSpeculativeRFHCreation,
        {{"create_speculative_rfh_delay_ms", "0"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_for_defer_speculative_rfh_;
};

// The test disables delaying the speculative RFH creation when navigation
// starts since it checks the behavior of speculative RFH during redirection.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesTestWithoutSpeculativeRFHDelay,
    SwapOnNavigationToPageThatRedirects) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  // This is a same-site URL, and will redirect to another same-site URL.
  GURL same_site_redirector_url(
      embedded_test_server()->GetURL("/server-redirect?" + url_2.spec()));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));
  // This is a cross-site URL, but will redirect to a same-site URL.
  GURL cross_site_redirector_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + url_3.spec()));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Go to a same-site URL that will redirect us same-site to /title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), same_site_redirector_url,
                            url_2 /* expected_commit_url */));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that we are using a different BrowsingInstance but still using the
  // same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Go to a cross-site URL that will redirect us same-site to /title3.html.
  // Note that we're using a renderer-initiated navigation here. If we do a
  // browser-initiated navigation, it will hit the case at crbug.com/1094147
  // where we can't reuse |url_2|'s process even though |url_3| is same-site.
  // TODO(crbug.com/40135315): Test with browser-initiated navigation too once
  // the issue is fixed.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), cross_site_redirector_url,
                                        url_3 /* expected_commit_url */));
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that we are using a different BrowsingInstance but still using the
  // same renderer process.
  // If site isolation is turned off, it will hit the case at crbug.com/1094147.
  EXPECT_FALSE(site_instance_2->IsRelatedSiteInstance(site_instance_3.get()));
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(site_instance_2->GetProcess(), site_instance_3->GetProcess());
  } else {
    EXPECT_NE(site_instance_2->GetProcess(), site_instance_3->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       DoNotSwapWhenReplacingHistoryEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Do a location.replace() to title2.html.
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(
        ExecJs(shell(), JsReplace("window.location.replace($1)", url_2)));
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_2);
  }
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(site_instance_1, site_instance_2);
}

// When we do a same-document navigation from A to A#foo then a navigation that
// does replacement (e.g., cross-process reload, or location.replace, or other
// client redirects) such that B takes the place of A#foo, we can go back to A
// with the back navigation. In this case, we might want to do a proactive BI
// swap so that page A can be bfcached.
// However, this test is currently disabled because we won't swap on any
// navigation that will replace the current history entry.
// TODO(rakina): Support this case.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesTest,
    DISABLED_ShouldSwapWhenReplacingEntryWithSameDocumentPreviousEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_1_anchor(embedded_test_server()->GetURL("/title1.html#foo"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Navigate same-document to title1.html#foo.
  EXPECT_TRUE(NavigateToURL(shell(), url_1_anchor));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(site_instance_1, site_instance_2);

  // 3) Do a location.replace() to title2.html.
  {
    TestNavigationObserver navigation_observer(web_contents, 1);
    EXPECT_TRUE(
        ExecJs(shell(), JsReplace("window.location.replace($1)", url_2)));
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), url_2);
  }
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // We should swap BrowsingInstance here so that the page at url_1 (which is
  // now the previous history entry) can be bfcached.
  EXPECT_NE(site_instance_2, site_instance_3);

  // Assert that a back navigation will go to |url_1|.
  {
    TestNavigationObserver navigation_observer(web_contents);
    web_contents->GetController().GoBack();
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), url_1);
  }
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       DoNotSwapWhenRelatedContentsPresent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate and open a new window.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  OpenPopup(shell(), url_1, "foo");
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Navigate to title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and title2.html are using the same SiteInstance.
  EXPECT_EQ(site_instance_1, site_instance_2);
}

// We should reuse the current process on same-site navigations even if the
// site requires a dedicated process (because we are still in the same site).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       NavigationToSiteThatRequiresDedicatedProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // Make sure the site require a dedicated process.
  EffectiveURLContentBrowserTestContentBrowserClient modified_client(
      url_1 /* url_to_modify */, url_1, /* requires_dedicated_process */ true);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_TRUE(site_instance_1->RequiresDedicatedProcess());

  // 2) Navigate cross-site to B. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_TRUE(site_instance_2->RequiresDedicatedProcess());

  // Check that A and B are in different BrowsingInstances but reuse the same
  // process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());
}

// Tests that pagehide handlers of the old RFH are run during the commit
// of the new RFH when swapping RFH for same-site navigations due to proactive
// BrowsingInstance swap.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       PagehideRunsDuringCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/local_storage.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetPrimaryMainFrame();
  // Create a pagehide handler that sets item "pagehide_storage" in
  // localStorage.
  EXPECT_TRUE(ExecJs(
      main_frame_1,
      base::StringPrintf(R"(
            localStorage.setItem('pagehide_storage', 'not_dispatched');
            var dispatched_pagehide = false;
            window.onpagehide = function(e) {
              if (dispatched_pagehide) {
                // We shouldn't dispatch pagehide more than once.
                localStorage.setItem('pagehide_storage',
                  'dispatched_more_than_once');
              } else if (e.persisted != %s) {
                localStorage.setItem('pagehide_storage', 'wrong_persisted');
              } else {
                localStorage.setItem('pagehide_storage',
                  'dispatched_once');
              }
              dispatched_pagehide = true;
            })",
                         IsBackForwardCacheEnabled() ? "true" : "false")));

  // 2) Navigate to local_storage.html.
  RenderFrameDeletedObserver main_frame_1_deleted_observer(main_frame_1);
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Check that title1.html and local_storage.html are in different RFHs.
  RenderFrameHostImpl* main_frame_2 = web_contents->GetPrimaryMainFrame();
  EXPECT_NE(main_frame_1, main_frame_2);

  // Check that the value set by |main_frame_1|'s pagehide handler can be
  // accessed by |main_frame_2| at load time (the first time the new page runs
  // scripts), setting the |pagehide_storage_at_load| variable correctly.
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "pagehide_storage_at_load"));

  // Check that the value for 'pagehide_storage' stays the same after
  // |main_frame_2| finished loading (or |main_frame_1| deleted if bfcache is
  // not enabled).
  if (!IsBackForwardCacheEnabled())
    main_frame_1_deleted_observer.WaitUntilDeleted();
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "localStorage.getItem('pagehide_storage')"));
}

// Tests that visibilitychange handlers of the old RFH are run during the commit
// of the new RFH when swapping RFH for same-site navigations due to proactive
// BrowsingInstance swap.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       VisibilitychangeRunsDuringCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/local_storage.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetPrimaryMainFrame();
  // Create a visibilitychange handler that sets item "visibilitychange_storage"
  // localStorage.
  EXPECT_TRUE(ExecJs(main_frame_1, R"(
            localStorage.setItem('visibilitychange_storage', 'not_dispatched');
            var dispatched_visibilitychange = false;
            document.onvisibilitychange = function(e) {
              if (dispatched_visibilitychange) {
                // We shouldn't dispatch visibilitychange more than once.
                localStorage.setItem('visibilitychange_storage',
                  'dispatched_more_than_once');
              } else if (document.visibilityState != 'hidden') {
                // We should dispatch the event when the visibilityState is
                // 'hidden'.
                localStorage.setItem('visibilitychange_storage', 'not_hidden');
              } else {
                localStorage.setItem('visibilitychange_storage',
                  'dispatched_once');
              }
              dispatched_visibilitychange = true;
            })"));

  // 2) Navigate to local_storage.html.
  RenderFrameDeletedObserver main_frame_1_deleted_observer(main_frame_1);
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Check that title1.html and local_storage.html are in different RFHs.
  RenderFrameHostImpl* main_frame_2 = web_contents->GetPrimaryMainFrame();
  EXPECT_NE(main_frame_1, main_frame_2);

  // Check that the value set by |main_frame_1|'s pagehide handler can be
  // accessed by |main_frame_2| at load time (the first time the new page runs
  // scripts), setting the |visibilitychange_storage_at_load| variable
  // correctly.
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "visibilitychange_storage_at_load"));

  // Check that the value for 'visibilitychange_storage' stays the same after
  // |main_frame_2| finished loading (or |main_frame_1| got deleted, if bfcache
  // is not enabled).
  if (!IsBackForwardCacheEnabled())
    main_frame_1_deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(
      "dispatched_once",
      EvalJs(main_frame_2, "localStorage.getItem('visibilitychange_storage')"));
}

// Tests that unload handlers of the old RFH are run during commit of the new
// RFH when swapping RFH for same-site navigations due to proactive
// BrowsingInstance swap.
// TODO(crbug.com/40142288): support this.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       DISABLED_UnloadRunsDuringCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/local_storage.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetPrimaryMainFrame();
  // Create an unload handler that sets item "unload_storage" in localStorage.
  EXPECT_TRUE(ExecJs(main_frame_1, R"(
            localStorage.setItem('unload_storage', 'not_dispatched');
            var dispatched_unload = false;
            window.onunload = function(e) {
              if (dispatched_unload) {
                // We shouldn't dispatch unload more than once.
                localStorage.setItem('unload_storage',
                  'dispatched_more_than_once');
              } else {
                localStorage.setItem('unload_storage', 'dispatched_once');
              }
              dispatched_unload = true;
            };)"));

  // 2) Navigate to local_storage.html.
  RenderFrameDeletedObserver main_frame_1_deleted_observer(main_frame_1);
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Check that title1.html and local_storage.html are in different RFHs.
  RenderFrameHostImpl* main_frame_2 = web_contents->GetPrimaryMainFrame();
  EXPECT_NE(main_frame_1, main_frame_2);

  // Check that the value set by |main_frame_1|'s unload handler can be
  // accessed by |main_frame_2| at load time (the first time the new page runs
  // scripts), setting the |unload_storage_at_load| variable correctly.
  EXPECT_EQ("dispatched_once", EvalJs(main_frame_2, "unload_storage_at_load"));

  // Check that the value for 'unload_storage' stays the same after
  // |main_frame_2| finished loading (or |main_frame_1| got deleted, if bfcache
  // is not enabled).
  if (!IsBackForwardCacheEnabled())
    main_frame_1_deleted_observer.WaitUntilDeleted();
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "localStorage.getItem('unload_storage')"));
}

// Tests that pagehide and visibilitychange handlers of a subframe in the old
// page are run during the commit of a new main RFH when swapping RFH for
// same-site navigations due to proactive BrowsingInstance swap.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesTest,
    PagehideAndVisibilitychangeInSubframesAreRunDuringCommit) {
  if (IsBackForwardCacheEnabled()) {
    // bfcached subframes with unload/pagehide/visibilitychange handlers will
    // crash on a failed DCHECK due to crbug.com/1109742.
    // TODO(crbug.com/40141877): don't skip this test when bfcache is enabled.
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com(a.com)"));
  GURL child_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com()");
  GURL url_2(embedded_test_server()->GetURL("a.com", "/local_storage.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to |main_url|.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetPrimaryMainFrame();
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  // Check if the subframe is navigated to the correct URL.
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(child_url, child->current_url());

  // Create a pagehide handler that sets item "pagehide_storage" and a
  // visibilitychange handler that sets item "visibilitychange_storage" in
  // localStorage in the subframe.
  EXPECT_TRUE(ExecJs(
      child,
      base::StringPrintf(R"(
          localStorage.setItem('pagehide_storage', 'not_dispatched');
          var dispatched_pagehide = false;
          window.onpagehide = function(e) {
            if (dispatched_pagehide) {
              // We shouldn't dispatch pagehide more than once.
              localStorage.setItem('pagehide_storage',
                'dispatched_more_than_once');
            } else if (e.persisted != %s) {
              localStorage.setItem('pagehide_storage', 'wrong_persisted');
            } else {
              localStorage.setItem('pagehide_storage', 'dispatched_once');
            }
            dispatched_pagehide = true;
          }

          localStorage.setItem('visibilitychange_storage', 'not_dispatched');
          var dispatched_visibilitychange = false;
          document.onvisibilitychange = function(e) {
            if (dispatched_visibilitychange) {
              // We shouldn't dispatch visibilitychange more than once.
              localStorage.setItem('visibilitychange_storage',
                'dispatched_more_than_once');
            } else if (document.visibilityState != 'hidden') {
              // We should dispatch the event when the visibilityState is
              // 'hidden'.
              localStorage.setItem('visibilitychange_storage', 'not_hidden');
            } else {
              localStorage.setItem('visibilitychange_storage',
                'dispatched_once');
            }
            dispatched_visibilitychange = true;
          })",
                         IsBackForwardCacheEnabled() ? "true" : "false")));
  // 2) Navigate to local_storage.html.
  RenderFrameDeletedObserver main_frame_1_deleted_observer(main_frame_1);
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Check that |main_url| and local_storage.html are in different RFHs.
  RenderFrameHostImpl* main_frame_2 = web_contents->GetPrimaryMainFrame();
  EXPECT_NE(main_frame_1, main_frame_2);

  // Check that the value set by |child|'s pagehide and visibilitychange
  // handlers can be accessed by |main_frame_2| at load time (the first time the
  // new page runs scripts), setting the |pagehide_storage_at_load| and
  // |visibilitychange_storage_at_load| variable correctly.
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "pagehide_storage_at_load"));
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "visibilitychange_storage_at_load"));

  // Check that the value for 'pagehide_storage' and 'visibilitychange_storage'
  // stays the same after |main_frame_2| finished loading (or |main_frame_1| got
  // deleted, if bfcache is not enabled).
  if (!IsBackForwardCacheEnabled())
    main_frame_1_deleted_observer.WaitUntilDeleted();
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "localStorage.getItem('pagehide_storage')"));
  EXPECT_EQ(
      "dispatched_once",
      EvalJs(main_frame_2, "localStorage.getItem('visibilitychange_storage')"));
}

// Tests that pagehide handlers of the old RFH are run during the commit
// of the new RFH when swapping RFH for same-site navigations due to proactive
// BrowsingInstance swap even if the page is already hidden (and
// visibilitychange won't run).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesTest,
                       PagehideRunsDuringCommitOfHiddenPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/local_storage.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to |url_1| and hide the tab.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* main_frame_1 = web_contents->GetPrimaryMainFrame();
  // We need to set it to Visibility::VISIBLE first in case this is the first
  // time the visibility is updated.
  web_contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, web_contents->GetVisibility());

  // Create a pagehide handler that sets item "pagehide_storage" and a
  // visibilitychange handler that sets item "visibilitychange_storage" in
  // localStorage.
  EXPECT_TRUE(ExecJs(
      main_frame_1,
      base::StringPrintf(R"(
          localStorage.setItem('pagehide_storage', 'not_dispatched');
          var dispatched_pagehide = false;
          window.onpagehide = function(e) {
            if (dispatched_pagehide) {
              // We shouldn't dispatch pagehide more than once.
              localStorage.setItem('pagehide_storage',
                'dispatched_more_than_once');
            } else if (e.persisted != %s) {
              localStorage.setItem('pagehide_storage', 'wrong_persisted');
            } else {
              localStorage.setItem('pagehide_storage', 'dispatched_once');
            }
            dispatched_pagehide = true;
          }

          localStorage.setItem('visibilitychange_storage', 'not_dispatched');
          document.onvisibilitychange = function(e) {
            localStorage.setItem('visibilitychange_storage',
                'should_not_be_dispatched');
          })",
                         IsBackForwardCacheEnabled() ? "true" : "false")));
  // |visibilitychange_storage| should be set to its initial correct value.
  EXPECT_EQ(
      "not_dispatched",
      EvalJs(main_frame_1, "localStorage.getItem('visibilitychange_storage')"));

  // 2) Navigate to local_storage.html.
  RenderFrameDeletedObserver main_frame_1_deleted_observer(main_frame_1);
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Check that |url_1| and local_storage.html are in different RFHs.
  RenderFrameHostImpl* main_frame_2 = web_contents->GetPrimaryMainFrame();
  EXPECT_NE(main_frame_1, main_frame_2);

  // Check that the value set by |main_frame_1|'s pagehide handler can be
  // accessed by |main_frame_2| at load time (the first time the new page runs
  // scripts), setting the |pagehide_storage_at_load| and variable correctly.
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "pagehide_storage_at_load"));
  // |visibilitychange_storage_at_load| should not be modified.
  EXPECT_EQ("not_dispatched",
            EvalJs(main_frame_2, "visibilitychange_storage_at_load"));

  // Check that the value for 'pagehide_storage' and 'visibilitychange_storage'
  // stays the same after |main_frame_2| finished loading (or |main_frame_1| got
  // deleted, if bfcache is not enabled).
  if (!IsBackForwardCacheEnabled())
    main_frame_1_deleted_observer.WaitUntilDeleted();
  EXPECT_EQ("dispatched_once",
            EvalJs(main_frame_2, "localStorage.getItem('pagehide_storage')"));
  EXPECT_EQ(
      "not_dispatched",
      EvalJs(main_frame_2, "localStorage.getItem('visibilitychange_storage')"));
}

// To address breakage of named window reuse across a back navigation
// when a proactive BrowsingInstance swap has happened, we offer the
// option to use an explicit opener relation for same-window navigations
// as an opt-out from proactive swaps. These tests cover cases that should (and
// should not) have the opt-out mechanism take effect.
class ProactivelySwapBrowsingInstancesOptOutTest
    : public ProactivelySwapBrowsingInstancesTest {
 public:
  ProactivelySwapBrowsingInstancesOptOutTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kRelOpenerBcgDependencyHint);
  }

  ~ProactivelySwapBrowsingInstancesOptOutTest() override = default;

 protected:
  void RunOptOutTest(base::FunctionRef<void()> perform_navigation,
                     bool expect_opt_out_applies = true) {
    ASSERT_TRUE(CanSameSiteMainFrameNavigationsChangeSiteInstances());

    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL url_1(embedded_test_server()->GetURL("/title1.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());

    EXPECT_TRUE(NavigateToURL(shell(), url_1));
    scoped_refptr<SiteInstanceImpl> site_instance_1 =
        static_cast<SiteInstanceImpl*>(
            web_contents->GetPrimaryMainFrame()->GetSiteInstance());

    BrowsingContextGroupSwapObserver swap_observer(web_contents);

    perform_navigation();
    EXPECT_NE(url_1,
              web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

    BrowsingContextGroupSwap bcg_swap =
        swap_observer.GetLatestBrowsingContextGroupSwap();

    EXPECT_NE(expect_opt_out_applies, bcg_swap.ShouldSwap());

    scoped_refptr<SiteInstanceImpl> site_instance_2 =
        static_cast<SiteInstanceImpl*>(
            web_contents->GetPrimaryMainFrame()->GetSiteInstance());
    if (expect_opt_out_applies) {
      EXPECT_TRUE(
          site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
      EXPECT_EQ(
          bcg_swap.reason(),
          ShouldSwapBrowsingInstance::kNo_InitiatorRequestedNoProactiveSwap);
    } else {
      EXPECT_FALSE(
          site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
    }
  }

  void CreateAnchorAndNavigate(RenderFrameHost* rfh,
                               const GURL& url,
                               const std::string& target_name,
                               const std::string& rel) {
    TestNavigationObserver nav_observer(WebContents::FromRenderFrameHost(rfh));
    ExecuteScriptAsync(rfh, JsReplace(R"(
      let anchor = document.createElement('a');
      anchor.href = $1;
      anchor.target = $2;
      anchor.rel = $3;
      anchor.text = 'Link';
      document.body.appendChild(anchor);
      anchor.click();
    )",
                                      url, target_name, rel));
    nav_observer.Wait();
  }

  void WindowOpenNavigate(RenderFrameHost* rfh,
                          const GURL& url,
                          const std::string& target_name,
                          const std::string& window_features) {
    TestNavigationObserver nav_observer(WebContents::FromRenderFrameHost(rfh));
    ExecuteScriptAsync(rfh, JsReplace("window.open($1, $2, $3);", url,
                                      target_name, window_features));
    nav_observer.Wait();
  }

  void CreateFormAndSubmit(RenderFrameHost* rfh,
                           const GURL& action,
                           const std::string& target_name,
                           const std::string& rel) {
    TestNavigationObserver nav_observer(WebContents::FromRenderFrameHost(rfh));
    ExecuteScriptAsync(rfh, JsReplace(R"(
      let form = document.createElement('form');
      form.action = $1;
      form.target = $2;
      form.rel = $3;
      form.method = 'POST';
      document.body.appendChild(form);
      form.submit();
    )",
                                      action, target_name, rel));
    nav_observer.Wait();
  }

  FrameTreeNode* CreateIframe(RenderFrameHost* parent, const GURL& url) {
    RenderFrameHostImpl* parent_impl =
        static_cast<RenderFrameHostImpl*>(parent);
    EXPECT_TRUE(ExecJs(parent_impl,
                       "var child = document.createElement('iframe');"
                       "document.body.appendChild(child);"));

    FrameTreeNode* child =
        parent_impl->child_at(parent_impl->child_count() - 1);
    EXPECT_TRUE(child);

    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecJs(parent_impl, JsReplace("child.src = $1;", url)));
    observer.Wait();

    return child;
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithAnchorRelOpener) {
  RunOptOutTest([this]() {
    const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
    CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                            next_url,
                            /*target_name=*/"", /*rel=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithWindowOpener) {
  RunOptOutTest([this]() {
    const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
    WindowOpenNavigate(shell()->web_contents()->GetPrimaryMainFrame(), next_url,
                       /*target_name=*/"_self", /*window_features=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithFormRelOpener) {
  RunOptOutTest([this]() {
    const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
    CreateFormAndSubmit(shell()->web_contents()->GetPrimaryMainFrame(),
                        /*action=*/next_url,
                        /*target_name=*/"", /*rel=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       PreventingSwapAllowsFutureScripting) {
  RunOptOutTest([this]() {
    const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
    CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                            next_url,
                            /*target_name=*/"", /*rel=*/"opener");
  });

  // Beyond confirming the BrowsingInstance didn't change, we'll explicitly
  // check the behaviour seen in crbug.com/40281878 to ensure that named windows
  // can be reused across back navigations.
  const GURL popup_url(embedded_test_server()->GetURL("/title3.html"));
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("var w = window.open($1, 'namedWindow');"
                                "w.previouslyOpened = true;",
                                popup_url)));
  Shell* popup = new_shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));

  NavigationController& opener_controller =
      shell()->web_contents()->GetController();
  ASSERT_TRUE(opener_controller.CanGoBack());
  TestNavigationObserver back_observer(shell()->web_contents());
  opener_controller.GoBack();
  back_observer.Wait();

  EXPECT_EQ(true, EvalJs(shell(),
                         "!!window.open('', 'namedWindow').previouslyOpened;"));
  // The window opened previously should be reused here, so the navigation
  // should be in the same WebContents.
  const GURL next_popup_url(embedded_test_server()->GetURL("/title4.html"));
  TestNavigationObserver popup_nav_observer(popup->web_contents());
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("window.open($1, 'namedWindow');", next_popup_url)));
  popup_nav_observer.Wait();
  EXPECT_EQ(popup_nav_observer.last_navigation_url(), next_popup_url);
}

// Like PreventingSwapAllowsFutureScripting, but performs another navigation
// after the back navigation, to confirm that the next page can also reuse the
// opened window.
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       PreventingSwapAllowsFutureScriptingAfterAdditionalNav) {
  RunOptOutTest([this]() {
    const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
    CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                            next_url,
                            /*target_name=*/"", /*rel=*/"opener");
  });

  // Beyond confirming the BrowsingInstance didn't change, we'll explicitly
  // check the behaviour seen in crbug.com/40281878 to ensure that named windows
  // can be reused across back navigations.
  const GURL popup_url(embedded_test_server()->GetURL("/title3.html"));
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("var w = window.open($1, 'namedWindow');"
                                "w.previouslyOpened = true;",
                                popup_url)));
  Shell* popup = new_shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));

  NavigationController& opener_controller =
      shell()->web_contents()->GetController();
  ASSERT_TRUE(opener_controller.CanGoBack());
  TestNavigationObserver back_observer(shell()->web_contents());
  opener_controller.GoBack();
  back_observer.Wait();

  // Navigate to another page. That page should also be able to reuse the opened
  // window.
  const GURL next_url2(
      embedded_test_server()->GetURL("/title2.html?additionalnav"));
  // We intentionally don't use rel=opener here as the usage in the original nav
  // to title2.html is sufficient.
  CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                          next_url2,
                          /*target_name=*/"", /*rel=*/"");

  EXPECT_EQ(true, EvalJs(shell(),
                         "!!window.open('', 'namedWindow').previouslyOpened;"));
  // The window opened previously should be reused here, so the navigation
  // should be in the same WebContents.
  const GURL next_popup_url(embedded_test_server()->GetURL("/title4.html"));
  TestNavigationObserver popup_nav_observer(popup->web_contents());
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("window.open($1, 'namedWindow');", next_popup_url)));
  popup_nav_observer.Wait();
  EXPECT_EQ(popup_nav_observer.last_navigation_url(), next_popup_url);
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithAnchorRelOpenerCrossSite) {
  RunOptOutTest([this]() {
    const GURL cross_site_next_url(
        embedded_test_server()->GetURL("b.com", "/title2.html"));
    CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                            cross_site_next_url,
                            /*target_name=*/"", /*rel=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithAnchorRelOpenerWithParent) {
  RunOptOutTest([this]() {
    const GURL iframe_url(embedded_test_server()->GetURL("/title2.html"));
    const GURL next_url(embedded_test_server()->GetURL("/title3.html"));
    FrameTreeNode* child = CreateIframe(
        shell()->web_contents()->GetPrimaryMainFrame(), iframe_url);
    CreateAnchorAndNavigate(child->current_frame_host(), next_url,
                            /*target_name=*/"_top", /*rel=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithAnchorRelOpenerInOopif) {
  RunOptOutTest([this]() {
    const GURL cross_site_iframe_url(
        embedded_test_server()->GetURL("b.com", "/title2.html"));
    const GURL next_url(embedded_test_server()->GetURL("/title3.html"));
    FrameTreeNode* child = CreateIframe(
        shell()->web_contents()->GetPrimaryMainFrame(), cross_site_iframe_url);
    CreateAnchorAndNavigate(child->current_frame_host(), next_url,
                            /*target_name=*/"_top", /*rel=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       DoNotSwapWithAnchorRelOpenerOpenURL) {
  RunOptOutTest([this]() {
    // This has the renderer navigate though the OpenURL method.
    shell()
        ->web_contents()
        ->GetMutableRendererPrefs()
        ->browser_handles_all_top_level_requests = true;
    shell()->web_contents()->SyncRendererPrefs();
    const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
    CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                            next_url,
                            /*target_name=*/"", /*rel=*/"opener");
  });
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       CannotOptOutOfCoop) {
  // Browsing instance swaps required by COOP cannot be bypassed by the opt out.
  RunOptOutTest(
      [this]() {
        const GURL next_url(
            embedded_test_server()->GetURL("/cross-origin-isolated.html"));
        CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                                next_url,
                                /*target_name=*/"", /*rel=*/"opener");
      },
      /*expect_opt_out_applies=*/false);
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       CannotOptOutFromFencedFrame) {
  // _unfencedTop navigations force a browsing instance swap, which should take
  // priority over the requested opt out.
  RunOptOutTest(
      [this]() {
        const GURL fenced_frame_url(
            embedded_test_server()->GetURL("/fenced_frames/title0.html"));
        const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
        RenderFrameHost* ff_rfh = fenced_frame_test_helper().CreateFencedFrame(
            shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url,
            net::OK, blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);
        CreateAnchorAndNavigate(ff_rfh, next_url,
                                /*target_name=*/"_unfencedTop",
                                /*rel=*/"opener");
      },
      /*expect_opt_out_applies=*/false);
}

IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesOptOutTest,
                       RelOpenerAndNoopener) {
  // Contradicting rel types of both opener and noopener should be treated as
  // noopener.
  RunOptOutTest(
      [this]() {
        const GURL next_url(embedded_test_server()->GetURL("/title2.html"));
        CreateAnchorAndNavigate(shell()->web_contents()->GetPrimaryMainFrame(),
                                next_url,
                                /*target_name=*/"", /*rel=*/"noopener opener");
      },
      /*expect_opt_out_applies=*/false);
}

class ProactivelySwapBrowsingInstancesSameSiteCoopTest
    : public ProactivelySwapBrowsingInstancesTest {
 public:
  ProactivelySwapBrowsingInstancesSameSiteCoopTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRef> features;
    feature_list_.InitAndEnableFeature(
        network::features::kCrossOriginOpenerPolicy);
  }

  ~ProactivelySwapBrowsingInstancesSameSiteCoopTest() override = default;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  void SetUpOnMainThread() override {
    ProactivelySwapBrowsingInstancesTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProactivelySwapBrowsingInstancesTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ProactivelySwapBrowsingInstancesTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ProactivelySwapBrowsingInstancesTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

// Tests history same-site process reuse:
// 1. Visit A1 (non-COOP), A2 (non-COOP, should reuse A1's process), A3 (uses
// COOP + COEP, should use new process).
// 2. Go back to A2 (should use new process).
// 3. Go back to A1 (should reuse A2's process).
IN_PROC_BROWSER_TEST_P(ProactivelySwapBrowsingInstancesSameSiteCoopTest,
                       HistoryNavigationReusesProcess_COOP) {

  GURL url_1(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL coop_url(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_EQ(
      web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // 2) Navigate same-site to title2.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  EXPECT_EQ(
      web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Navigate same-site to a crossOriginIsolated page (uses COOP+COEP).
  RenderFrameDeletedObserver rfh_2_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);

  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache. So, we should flush back-forward
  // cache.
  web_contents->GetController().GetBackForwardCache().Flush();
  // Wait until the RFH for title2.html got deleted, and check that
  // title2.html and title3.html are in different BrowsingInstances and
  // renderer processes (We check this by checking whether |site_instance_2|
  // still has a process or not - if it's gone then that means
  // |site_instance_3| uses a different process).
  rfh_2_deleted_observer.WaitUntilDeleted();
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(site_instance_2->IsRelatedSiteInstance(site_instance_3.get()));
  EXPECT_FALSE(site_instance_2->HasProcess());
  EXPECT_NE(site_instance_2->GetProcess(), site_instance_3->GetProcess());

  // 4) Do a back navigation to title2.html.
  RenderFrameDeletedObserver rfh_3_deleted_observer(
      web_contents->GetPrimaryMainFrame());
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_2);
  scoped_refptr<SiteInstanceImpl> site_instance_2_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());
  // This test expects a renderer process to eventually get deleted when we
  // navigate away from the page using it, which won't happen if the page is
  // kept alive in the back-forward cache. So, we should flush back-forward
  // cache.
  web_contents->GetController().GetBackForwardCache().Flush();
  // We should use different BrowsingInstances and processes after going back to
  // title2.html because it's transitioning from a crossOriginIsolated page
  // (COOP+COEP) to a non-crossOriginIsolated page, even though the two are
  // same-site.
  rfh_3_deleted_observer.WaitUntilDeleted();
  EXPECT_FALSE(site_instance_2_history_nav->IsRelatedSiteInstance(
      site_instance_3.get()));
  EXPECT_FALSE(site_instance_3->HasProcess());

  // 5) Do a back navigation to title1.html.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      static_cast<SiteInstanceImpl*>(
          web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // We should use different BrowsingInstances for title1.html and title2.html,
  // but reuse the process (because in the original navigation, the BI change
  // was caused by proactive BI swap).
  EXPECT_FALSE(site_instance_1_history_nav->IsRelatedSiteInstance(
      site_instance_2_history_nav.get()));
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_TRUE(site_instance_2_history_nav->HasProcess());
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_2_history_nav->GetProcess());
}

// Tests that enable clearing window.name on on cross-site
// cross-BrowsingInstance navigations when
// ProactivelySwapBrowsingInstancesSameSite is enabled.
class ProactivelySwapBrowsingInstancesSameSiteClearWindowNameTest
    : public ProactivelySwapBrowsingInstancesTest {
 public:
  ProactivelySwapBrowsingInstancesSameSiteClearWindowNameTest() {
    feature_list_.InitAndEnableFeature(
        features::kClearCrossSiteCrossBrowsingContextGroupWindowName);
  }
  ~ProactivelySwapBrowsingInstancesSameSiteClearWindowNameTest() override =
      default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that same-site main frame navigation that swaps BrowsingInstances
// does not clear window.name.
IN_PROC_BROWSER_TEST_P(
    ProactivelySwapBrowsingInstancesSameSiteClearWindowNameTest,
    NotClearWindowNameSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to a.com/title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  // Set window.name.
  EXPECT_TRUE(content::ExecJs(web_contents, "window.name='foo'"));
  auto* frame_a1 = web_contents->GetPrimaryMainFrame();
  EXPECT_EQ("foo", frame_a1->GetFrameName());

  scoped_refptr<SiteInstance> site_instance_a1 = frame_a1->GetSiteInstance();

  // Navigate to a.com/title2.html. Even though we proactively swap
  // BrowsingInstances for same-site navigation as well, we should only clear
  // window.name for cross-BrowsingInstance navigation that's not same-site.
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#resetBCName.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_a2));
  auto* frame_a2 = web_contents->GetPrimaryMainFrame();
  // Check that title1.html and title2.html are in different BrowsingInstances.
  scoped_refptr<SiteInstance> site_instance_a2 = frame_a2->GetSiteInstance();
  EXPECT_FALSE(site_instance_a1->IsRelatedSiteInstance(site_instance_a2.get()));
  // Window.name should not be cleared.
  EXPECT_EQ("foo", frame_a2->GetFrameName());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProactivelySwapBrowsingInstancesTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         ProactivelySwapBrowsingInstancesOptOutTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(
    All,
    ProactivelySwapBrowsingInstancesCrossSiteDoesNotReuseProcessTest,
    testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         ProactivelySwapBrowsingInstancesSameSiteCoopTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(
    All,
    ProactivelySwapBrowsingInstancesSameSiteClearWindowNameTest,
    testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(
    All,
    ProactivelySwapBrowsingInstancesTestWithoutSpeculativeRFHDelay,
    testing::ValuesIn(RenderDocumentFeatureLevelValues()));
}  // namespace content
