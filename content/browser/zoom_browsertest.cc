// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "url/gurl.h"

namespace content {

// This class contains basic tests of zoom functionality.
class ZoomBrowserTest : public ContentBrowserTest {
 public:
  ZoomBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};


// This class contains tests to make sure that subframes zoom in a manner
// consistent with the top-level frame, even when the subframes are cross-site.
// Particular things we want to make sure of:
//
// * Subframes should always have the same zoom level as their main frame, even
// if the subframe's domain has a different zoom level stored in HostZoomMap.
//
// * The condition above should continue to hold after a navigation of the
// subframe.
//
// * Zoom changes applied to the mainframe should propagate to all subframes,
// regardless of whether they are same site or cross-site to the frame they are
// children of.
//
// The tests in this file rely on the notion that, when a page zooms, that
// subframes have both (1) a change in their frame rect, and (2) a change in
// their frame's scale. Since the page should scale as a unit, this means the
// innerWidth value of any subframe should be the same before and after the
// zoom (though it may transiently take on a different value). The
// FrameSizeObserver serves to watch for onresize events, and observes when
// the innerWidth is correctly set.
class IFrameZoomBrowserTest : public ContentBrowserTest {
 public:
  IFrameZoomBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

namespace {

const double kTolerance = 0.1;  // In CSS pixels.

double GetMainframeWindowBorder(const ToRenderFrameHost& adapter) {
  return EvalJs(adapter, "window.outerWidth - window.innerWidth")
      .ExtractDouble();
}

double GetMainFrameZoomFactor(const ToRenderFrameHost& adapter, double border) {
  return EvalJs(
             adapter,
             JsReplace("(window.outerWidth - $1) / window.innerWidth;", border))
      .ExtractDouble();
}

double GetSubframeWidth(const ToRenderFrameHost& adapter) {
  return EvalJs(adapter, "window.innerWidth").ExtractDouble();
}

// This struct is used to track changes to subframes after a main frame zoom
// change, so that we can test subframe inner widths with assurance that all the
// changes have finished propagating.
struct FrameResizeObserver {
  FrameResizeObserver(RenderFrameHost* host,
                      std::string label,
                      double inner_width,
                      double tolerance)
      : frame_host(host),
        msg_label(std::move(label)),
        zoomed_correctly(false),
        expected_inner_width(inner_width),
        tolerance(tolerance) {
    SetupOnResizeCallback(host, msg_label);
  }

  void SetupOnResizeCallback(const ToRenderFrameHost& adapter,
                             const std::string& label) {
    const char kOnResizeCallbackSetup[] =
        "document.body.onresize = function(){"
        "  window.domAutomationController.send('%s ' + window.innerWidth);"
        "};";
    EXPECT_TRUE(ExecJs(
        adapter, base::StringPrintf(kOnResizeCallbackSetup, label.c_str())));
  }

  void Check(const std::string& status_msg) {
    if (!base::StartsWith(status_msg, msg_label, base::CompareCase::SENSITIVE))
      return;

    double inner_width = std::stod(status_msg.substr(msg_label.length() + 1));
    zoomed_correctly = std::abs(expected_inner_width - inner_width) < tolerance;
  }

  FrameResizeObserver* toThis() {return this;}

  raw_ptr<RenderFrameHost> frame_host;
  std::string msg_label;
  bool zoomed_correctly;
  double expected_inner_width;
  double tolerance;
};

// This struct is used to wait until a resize has occurred.
struct ResizeObserver {
  ResizeObserver(RenderFrameHost* host)
      : frame_host(host) {
    SetupOnResizeCallback(host);
  }

  void SetupOnResizeCallback(const ToRenderFrameHost& adapter) {
    const char kOnResizeCallbackSetup[] =
        "document.body.onresize = function(){"
        "  window.domAutomationController.send('Resized');"
        "};";
    EXPECT_TRUE(ExecJs(adapter, kOnResizeCallbackSetup));
  }

  bool IsResizeCallback(const std::string& status_msg) {
    return status_msg == "Resized";
  }

  raw_ptr<RenderFrameHost> frame_host;
};

void WaitForResize(DOMMessageQueue& msg_queue, ResizeObserver& observer) {
  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    // Strip the double quotes from the message.
    status = status.substr(1, status.length() -2);
    if (observer.IsResizeCallback(status))
      break;
  }
}

void WaitAndCheckFrameZoom(
    DOMMessageQueue& msg_queue,
    std::vector<FrameResizeObserver>& frame_observers) {
  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    // Strip the double quotes from the message.
    status = status.substr(1, status.length() -2);

    bool all_zoomed_correctly = true;

    // Use auto& to operate on a reference, and not a copy.
    for (auto& observer : frame_observers) {
      observer.Check(status);
      all_zoomed_correctly = all_zoomed_correctly && observer.zoomed_correctly;
    }

    if (all_zoomed_correctly)
      break;
  }
}

}  // namespace

// Flaky. crbug.com/1055282
IN_PROC_BROWSER_TEST_F(ZoomBrowserTest, DISABLED_ZoomPreservedOnReload) {
  std::string top_level_host("a.com");

  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  const double new_zoom_factor = 2.5;

  // Set the new zoom, wait for the page to be resized, and sanity-check that
  // the zoom was applied.
  {
    DOMMessageQueue msg_queue(web_contents());
    ResizeObserver observer(root->current_frame_host());

    const double new_zoom_level =
        default_zoom_level + blink::ZoomFactorToZoomLevel(new_zoom_factor);
    host_zoom_map->SetZoomLevelForHost(top_level_host, new_zoom_level);

    WaitForResize(msg_queue, observer);
  }

  // Make this comparison approximate for Nexus5X test;
  // https://crbug.com/622858.
  EXPECT_NEAR(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      0.01);

  // Now the actual test: Reload the page and check that the main frame is
  // still properly zoomed.
  LoadStopObserver load_stop_observer(shell()->web_contents());
  shell()->Reload();
  load_stop_observer.Wait();

  EXPECT_NEAR(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      0.01);
}

// http://crbug.com/1174371
IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest, DISABLED_SubframesZoomProperly) {
  std::string top_level_host("a.com");
  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* grandchild =
      root->child_at(0)->child_at(0)->current_frame_host();

  // The following calls must be made when the page's scale factor = 1.0.
  double scale_one_child_width = GetSubframeWidth(child);
  double scale_one_grandchild_width = GetSubframeWidth(grandchild);
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  const double new_zoom_factor = 2.5;
  {
    DOMMessageQueue msg_queue(web_contents());

    std::vector<FrameResizeObserver> frame_observers;
    frame_observers.emplace_back(child, "child",
                                 scale_one_child_width, kTolerance);
    frame_observers.emplace_back(grandchild, "grandchild",
                                 scale_one_grandchild_width, kTolerance);

    const double new_zoom_level =
        default_zoom_level + blink::ZoomFactorToZoomLevel(new_zoom_factor);
    host_zoom_map->SetZoomLevelForHost(top_level_host, new_zoom_level);

    WaitAndCheckFrameZoom(msg_queue, frame_observers);
  }

  // Make this comparison approximate for Nexus5X test;
  // https://crbug.com/622858.
  EXPECT_NEAR(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      0.01);
}

IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest, SubframesDontZoomIndependently) {
  std::string top_level_host("a.com");
  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* grandchild =
      root->child_at(0)->child_at(0)->current_frame_host();

  // The following calls must be made when the page's scale factor = 1.0.
  double scale_one_child_width = GetSubframeWidth(child);
  double scale_one_grandchild_width = GetSubframeWidth(grandchild);
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  const double new_zoom_factor = 2.0;
  const double new_zoom_level =
      default_zoom_level + blink::ZoomFactorToZoomLevel(new_zoom_factor);

  // This should not cause the nested iframe to change its zoom.
  host_zoom_map->SetZoomLevelForHost("b.com", new_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));
  EXPECT_EQ(scale_one_child_width, GetSubframeWidth(child));
  EXPECT_EQ(scale_one_grandchild_width, GetSubframeWidth(grandchild));

  // When we navigate so that b.com is the top-level site, then it has the
  // expected zoom.
  GURL new_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
  EXPECT_DOUBLE_EQ(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border));
}

// This test is flaky. https://crbug.com/1171748
IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest,
                       DISABLED_AllFramesGetDefaultZoom) {
  std::string top_level_host("a.com");
  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* grandchild =
      root->child_at(0)->child_at(0)->current_frame_host();

  // The following calls must be made when the page's scale factor = 1.0.
  double scale_one_child_width = GetSubframeWidth(child);
  double scale_one_grandchild_width = GetSubframeWidth(grandchild);
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  const double new_default_zoom_factor = 2.0;
  {
    DOMMessageQueue msg_queue(web_contents());

    std::vector<FrameResizeObserver> frame_observers;
    frame_observers.emplace_back(child, "child",
                                 scale_one_child_width, kTolerance);
    frame_observers.emplace_back(grandchild, "grandchild",
                                 scale_one_grandchild_width, kTolerance);

    const double new_default_zoom_level =
        default_zoom_level +
        blink::ZoomFactorToZoomLevel(new_default_zoom_factor);

    host_zoom_map->SetZoomLevelForHost("b.com", new_default_zoom_level + 1.0);
    host_zoom_map->SetDefaultZoomLevel(new_default_zoom_level);

    WaitAndCheckFrameZoom(msg_queue, frame_observers);
  }
  // Make this comparison approximate for Nexus5X test;
  // https://crbug.com/622858.
  EXPECT_NEAR(
      new_default_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      0.01
  );
}

// Flaky on mac, https://crbug.com/1055282
#if BUILDFLAG(IS_MAC)
#define MAYBE_SiblingFramesZoom DISABLED_SiblingFramesZoom
#else
#define MAYBE_SiblingFramesZoom SiblingFramesZoom
#endif
IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest, MAYBE_SiblingFramesZoom) {
  std::string top_level_host("a.com");
  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* child1 = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* child2 = root->child_at(1)->current_frame_host();

  // The following calls must be made when the page's scale factor = 1.0.
  double scale_one_child1_width = GetSubframeWidth(child1);
  double scale_one_child2_width = GetSubframeWidth(child2);
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  const double new_zoom_factor = 2.5;
  {
    DOMMessageQueue msg_queue(web_contents());

    std::vector<FrameResizeObserver> frame_observers;
    frame_observers.emplace_back(child1, "child1",
                                 scale_one_child1_width, kTolerance);
    frame_observers.emplace_back(child2, "child2",
                                 scale_one_child2_width, kTolerance);

    const double new_zoom_level =
        default_zoom_level + blink::ZoomFactorToZoomLevel(new_zoom_factor);
    host_zoom_map->SetZoomLevelForHost(top_level_host, new_zoom_level);

    WaitAndCheckFrameZoom(msg_queue, frame_observers);
  }

  // Make this comparison approximate for Nexus5X test;
  // https://crbug.com/622858.
  EXPECT_NEAR(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      0.01);
}

IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest, SubframeRetainsZoomOnNavigation) {
  std::string top_level_host("a.com");
  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();

  // The following calls must be made when the page's scale factor = 1.0.
  double scale_one_child_width = GetSubframeWidth(child);
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);

  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  const double new_zoom_factor = 0.5;
  {
    DOMMessageQueue msg_queue(web_contents());

    std::vector<FrameResizeObserver> frame_observers;
    frame_observers.emplace_back(child, "child",
                                 scale_one_child_width, kTolerance);

    const double new_zoom_level =
        default_zoom_level + blink::ZoomFactorToZoomLevel(new_zoom_factor);
    host_zoom_map->SetZoomLevelForHost(top_level_host, new_zoom_level);

    WaitAndCheckFrameZoom(msg_queue, frame_observers);
  }

  // Make this comparison approximate for Nexus5X test;
  // https://crbug.com/622858.
  EXPECT_NEAR(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      0.01
  );

  // Navigate child frame cross site, and make sure zoom is the same.
  TestNavigationObserver observer(web_contents());
  GURL url = embedded_test_server()->GetURL("c.com", "/title1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check that the child frame maintained the same scale after navigating
  // cross-site.
  double new_child_width =
      GetSubframeWidth(root->child_at(0)->current_frame_host());
  EXPECT_EQ(scale_one_child_width, new_child_width);
}

// http://crbug.com/609213
IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest,
                       RedirectToPageWithSubframeZoomsCorrectly) {
  std::string initial_host("a.com");
  std::string redirected_host("b.com");
  EXPECT_TRUE(NavigateToURL(shell(), GURL(embedded_test_server()->GetURL(
                                         initial_host, "/title2.html"))));
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());
  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  // Set a zoom level for b.com before we navigate to it.
  const double kZoomFactorForRedirectedHost = 1.5;
  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  host_zoom_map->SetZoomLevelForHost(
      redirected_host,
      blink::ZoomFactorToZoomLevel(kZoomFactorForRedirectedHost));

  // Navigation to a.com doesn't change the zoom level, but when it redirects
  // to b.com, and then a subframe loads, the zoom should change.
  GURL redirect_url(embedded_test_server()->GetURL(
      redirected_host, "/cross_site_iframe_factory.html?b(b)"));
  GURL url(embedded_test_server()->GetURL(
      initial_host, "/client-redirect?" + redirect_url.spec()));

  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 2);
  EXPECT_TRUE(IsLastCommittedEntryOfPageType(web_contents(), PAGE_TYPE_NORMAL));
  EXPECT_EQ(redirect_url, web_contents()->GetLastCommittedURL());

  EXPECT_NEAR(kZoomFactorForRedirectedHost,
              GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
              0.01);
}

// Tests that on cross-site navigation from a page that has a subframe, the
// appropriate zoom is applied to the new page.
// crbug.com/673065
IN_PROC_BROWSER_TEST_F(IFrameZoomBrowserTest,
                       SubframesDontBreakConnectionToRenderer) {
  std::string top_level_host("a.com");
  GURL main_url(embedded_test_server()->GetURL(
      top_level_host, "/page_with_iframe_and_link.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  GURL loaded_url = HostZoomMap::GetURLFromEntry(entry);
  EXPECT_EQ(top_level_host, loaded_url.host());

  // The following calls must be made when the page's scale factor = 1.0.
  double main_frame_window_border = GetMainframeWindowBorder(web_contents());

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(web_contents());
  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  EXPECT_EQ(0.0, default_zoom_level);
  EXPECT_DOUBLE_EQ(
      1.0, GetMainFrameZoomFactor(web_contents(), main_frame_window_border));

  // Set a zoom for a host that will be navigated to below.
  const double new_zoom_factor = 2.0;
  const double new_zoom_level =
      default_zoom_level + blink::ZoomFactorToZoomLevel(new_zoom_factor);
  host_zoom_map->SetZoomLevelForHost("foo.com", new_zoom_level);

  // Navigate forward in the same RFH to a site with that host via a
  // renderer-initiated navigation.
  {
    uint16_t port_number = embedded_test_server()->port();
    EXPECT_EQ(true, EvalJs(shell(), base::StringPrintf("setPortNumber(%d)",
                                                       port_number)));
    TestNavigationObserver observer(shell()->web_contents());
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    EXPECT_EQ(true, EvalJs(shell(), "clickCrossSiteLink();"));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Check that the requested zoom has been applied to the new site.
  // NOTE: Local observation on Linux has shown that this comparison has to be
  // approximate. As the common failure mode would be that the zoom is ~1
  // instead of ~2, this approximation shouldn't be problematic.
  EXPECT_NEAR(
      new_zoom_factor,
      GetMainFrameZoomFactor(web_contents(), main_frame_window_border),
      .1);
}

}  // namespace content
