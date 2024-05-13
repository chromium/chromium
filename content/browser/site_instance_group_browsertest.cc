// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_base.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// A set of tests for loading data: URL subframes in their own SiteInstance in
// their initiator's SiteInstanceGroup. Parameterized to also run in the legacy
// mode where data: URLs load in their initiator's SiteInstance. See
// https://crbug.com/40176090.
class DataURLSiteInstanceGroupTest
    : public ContentBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);

    if (IsSiteInstanceGroupEnabled()) {
      feature_list_.InitAndEnableFeature(
          features::kSiteInstanceGroupsForDataUrls);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kSiteInstanceGroupsForDataUrls);
    }
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "DataSubframeHasOwnSiteInstance"
                      : "DataSubframeInInitiatorSiteInstance";
  }

 protected:
  bool IsSiteInstanceGroupEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A b.com subframe navigates itself to a data: URL. This allows testing that a
// navigation from b.com to a data: URL is a local-to-local frame navigation and
// does not create a proxy, despite being cross-SiteInstance.
IN_PROC_BROWSER_TEST_P(DataURLSiteInstanceGroupTest,
                       DISABLED_SiteInstanceGroupDataURLSubframe) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Both subframes share a site and therefore SiteInstance.
  EXPECT_EQ(main_frame()
                ->child_at(0)
                ->render_manager()
                ->current_frame_host()
                ->GetSiteInstance(),
            main_frame()
                ->child_at(1)
                ->render_manager()
                ->current_frame_host()
                ->GetSiteInstance());

  // The first B subframe navigates itself to a data: URL, so the data: subframe
  // has initiator b.com.
  GURL data_url("data:text/html,test");
  EXPECT_TRUE(NavigateToURLFromRenderer(main_frame()->child_at(0), data_url));

  SiteInstanceGroup* root_group = main_frame_host()->GetSiteInstance()->group();
  SiteInstanceImpl* child0_instance = main_frame()
                                          ->child_at(0)
                                          ->render_manager()
                                          ->current_frame_host()
                                          ->GetSiteInstance();
  SiteInstanceImpl* child1_instance = main_frame()
                                          ->child_at(1)
                                          ->render_manager()
                                          ->current_frame_host()
                                          ->GetSiteInstance();

  // In both parameterization modes, the root is cross-site from the subframes
  // and should not share a process or SiteInstanceGroup.
  EXPECT_NE(root_group, child0_instance->group());

  if (ShouldCreateSiteInstanceForDataUrls()) {
    // The data: subframe should have its own SiteInstance in the same
    // SiteInstanceGroup as the other B subframe.
    EXPECT_NE(child0_instance, child1_instance);
    EXPECT_EQ(child0_instance->group(), child1_instance->group());
  } else {
    // The data: subframe shares a SiteInstance with the other B subframe.
    EXPECT_EQ(child0_instance, child1_instance);
  }

  // TODO(https://crbug.com/40269084, yangsharon): Add DepictFrameTree calls
  // once they fully support SiteInstanceGroups.
}

// Check that for a main frame data: URL, about:blank frames end up in the data:
// URL's SiteInstance and can be scripted.
IN_PROC_BROWSER_TEST_P(DataURLSiteInstanceGroupTest,
                       MainFrameDataURLWithAboutBlank) {
  // Load a main frame data: URL with an about:blank subframe. The main frame
  // and subframe should be in the same SiteInstance.
  GURL data_url(
      "data:text/html,<body> This page has one about:blank iframe:"
      "<iframe name='frame1' src='about:blank'></iframe> </body>");

  EXPECT_TRUE(NavigateToURL(shell(), data_url));
  EXPECT_EQ(main_frame_host()->GetSiteInstance(),
            main_frame()->child_at(0)->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = data:nonce_A",
      DepictFrameTree(*main_frame()));

  // The main frame should be able to script its about:blank subframe.
  EXPECT_TRUE(ExecJs(main_frame(), "frames[0].window.name = 'new-name'"));
  EXPECT_EQ(main_frame()->child_at(0)->frame_name(), "new-name");
}

// Check that for a subframe data: URL, about:blank frames end up in the data:
// URL's SiteInstance and can be scripted.
IN_PROC_BROWSER_TEST_P(DataURLSiteInstanceGroupTest,
                       SubframeDataURLWithAboutBlank) {
  // Navigate to a main frame with a cross-site iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate the subframe to a data: URL with an about:blank subframe.
  GURL data_url(
      "data:text/html,<body> This page has one about:blank iframe:"
      "<iframe name='frame1' src='about:blank'></iframe> </body>");
  TestNavigationObserver observer(web_contents());
  FrameTreeNode* child = main_frame()->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child, data_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // The data: frame should be able to script its about:blank subframe.
  EXPECT_TRUE(ExecJs(child, "frames[0].window.name = 'new-name'"));
  EXPECT_EQ(child->child_at(0)->frame_name(), "new-name");

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(*main_frame()));

  // TODO(https://crbug.com/40269084, yangsharon): Add SiteInstance
  // comparisons, and a SiteInstanceGroup-enabled DepictFrameTree check once
  // data: URLs in SiteInstanceGroups are supported.
}

INSTANTIATE_TEST_SUITE_P(All,
                         DataURLSiteInstanceGroupTest,
                         ::testing::Bool(),
                         &DataURLSiteInstanceGroupTest::DescribeParams);
}  // namespace content
