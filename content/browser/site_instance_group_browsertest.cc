// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/content_switches.h"
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
                       SiteInstanceGroupDataURLSubframe) {
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
    EXPECT_EQ(
        " Site A ------------ proxies for {B,C}\n"
        "   |--Site B ------- proxies for A\n"
        "   +--Site C ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = data:nonce_B\n"
        "      C = http://b.com/",
        DepictFrameTree(*main_frame()));
  } else {
    // The data: subframe shares a SiteInstance with the other B subframe.
    EXPECT_EQ(child0_instance, child1_instance);
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(*main_frame()));
  }
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

  if (ShouldCreateSiteInstanceForDataUrls()) {
    EXPECT_EQ(
        " Site A ------------ proxies for {B,C}\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site B -- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = data:nonce_B\n"
        "      C = http://b.com/",
        DepictFrameTree(*main_frame()));
  } else {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site B -- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(*main_frame()));
  }
}

// Test that data: subframes are added to the correct SiteInstanceGroup. In the
// case of A(B_sandbox, B(data_sandbox)), data_sandbox should go in the group of
// its initiator, B. However, because data is sandboxed and B isn't, B's group
// is not the correct group, but B_sandbox's group is.
IN_PROC_BROWSER_TEST_P(DataURLSiteInstanceGroupTest,
                       SandboxedDataFindsExistingSiteInstance) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed cross-origin child frame.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame_sandboxed'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        b_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }
  FrameTreeNode* child0 = main_frame()->child_at(0);
  SiteInstanceImpl* child0_instance =
      child0->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(child0_instance->GetSiteInfo().is_sandboxed());

  // Create a non-sandboxed cross-origin child frame, which has the same site as
  // the sandboxed subframe, but is in a different (non-sandboxed) SiteInstance.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame_not_sandboxed'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        b_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }
  FrameTreeNode* child1 = main_frame()->child_at(1);
  SiteInstanceImpl* child1_instance =
      child1->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(child1_instance->GetSiteInfo().is_sandboxed());
  EXPECT_NE(child0_instance, child1_instance);

  // Add a sandboxed data: URL subframe as a subframe of `child1`. It should get
  // its own SiteInstance.
  {
    GURL data_url("data:text/html,test");
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame_data_sandboxed'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        data_url.spec().c_str());
    EXPECT_TRUE(ExecJs(child1, js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }
  FrameTreeNode* data = child1->child_at(0);
  SiteInstanceImpl* data_instance =
      data->current_frame_host()->GetSiteInstance();

  // The last committed URL is the data: URL, but because sandboxed data frames
  // are not handled, the data SiteInstance is in the `child0` instance.
  // TODO(crbug.com/40269084): After sandboxed data: subframes are supported,
  // update expectation for `data_instance` to have a separate SiteInstance in
  // the `child0` group with a data: site URL.
  EXPECT_TRUE(data->current_frame_host()->GetLastCommittedURL().SchemeIs(
      url::kDataScheme));
  EXPECT_EQ(data_instance, child0_instance);
  EXPECT_NE(data_instance, child1_instance);
  EXPECT_EQ(data_instance->group(), child0_instance->group());
  EXPECT_NE(data_instance->group(), child1_instance->group());
}

// Test where a main frame has multiple data: URL subframes. This tests that
// the data: URL subframes each have their own SiteInstance, that the
// SiteInstances are in the same SiteInstanceGroup, and that a SiteInstanceGroup
// can have multiple data: URL SiteInstances.
IN_PROC_BROWSER_TEST_P(DataURLSiteInstanceGroupTest, ParentNavigatesSubframes) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string data_url1_str("data:text/html,test1");
  std::string data_url2_str("data:text/html,test2");
  {
    std::string js_str = base::StringPrintf(
        "for (let i = 0; i < 2; i++) { "
        "  var frame = document.createElement('iframe'); "
        "  frame.id = 'test_frame'; "
        "  frame.src = 'data:text/html,' + i; "
        "  document.body.appendChild(frame);"
        "}");
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  FrameTreeNode* child0 = main_frame()->child_at(0);
  SiteInstanceImpl* child0_instance =
      child0->current_frame_host()->GetSiteInstance();
  FrameTreeNode* child1 = main_frame()->child_at(1);
  SiteInstanceImpl* child1_instance =
      child1->current_frame_host()->GetSiteInstance();

  if (ShouldCreateSiteInstanceForDataUrls()) {
    EXPECT_NE(child0_instance, child1_instance);
  } else {
    EXPECT_EQ(child0_instance, child1_instance);
  }
  EXPECT_EQ(child0_instance->group(), child1_instance->group());
  EXPECT_EQ(main_frame_host()->GetSiteInstance()->group(),
            child0_instance->group());
}

// The same as DataURLSiteInstanceGroupTest, but with kSitePerProcess disabled.
class DataURLSiteInstanceGroupTestWithoutSiteIsolation
    : public DataURLSiteInstanceGroupTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->RemoveSwitch(switches::kSitePerProcess);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }
};

// Ensure that when SiteIsolation is disabled, a data: subframe in an isolated
// main frame can create and navigate a subframe successfully. E.g.
// A_isolated(data(B)). This is a regression test for crbug.com/379385125.
IN_PROC_BROWSER_TEST_P(DataURLSiteInstanceGroupTestWithoutSiteIsolation,
                       DataSubframeOpensSubframeAndNavigates) {
  // Explicitly isolate a.com since SiteIsolation is disabled.
  IsolateOriginsForTesting(embedded_test_server(), web_contents(), {"a.com"});

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // Add a data: URL subframe.
  {
    TestNavigationObserver observer(web_contents());
    GURL data_url("data:text/html,test");
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'data_frame'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        data_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(data_url, observer.last_navigation_url());
  }
  SiteInstanceImpl* a_instance = main_frame_host()->GetSiteInstance();

  FrameTreeNode* data_frame = main_frame()->child_at(0);
  SiteInstanceImpl* data_instance =
      data_frame->current_frame_host()->GetSiteInstance();

  if (ShouldCreateSiteInstanceForDataUrls()) {
    EXPECT_NE(a_instance, data_instance);
  } else {
    EXPECT_EQ(a_instance, data_instance);
  }
  EXPECT_EQ(a_instance->group(), data_instance->group());

  // Add a subframe of the data: URL, and navigate it to b.com.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    TestNavigationObserver observer(web_contents());
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'child_frame_of_data'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        url_b.spec().c_str());
    EXPECT_TRUE(ExecJs(data_frame, js_str));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(url_b, observer.last_navigation_url());
  }

  FrameTreeNode* b_frame = data_frame->child_at(0);
  SiteInstanceImpl* b_instance =
      b_frame->current_frame_host()->GetSiteInstance();
  EXPECT_NE(data_instance, b_instance);
  EXPECT_NE(data_instance->group(), b_instance->group());
  EXPECT_NE(data_instance->GetProcess(), b_instance->GetProcess());
}

INSTANTIATE_TEST_SUITE_P(All,
                         DataURLSiteInstanceGroupTest,
                         ::testing::Bool(),
                         &DataURLSiteInstanceGroupTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         DataURLSiteInstanceGroupTestWithoutSiteIsolation,
                         ::testing::Bool(),
                         &DataURLSiteInstanceGroupTest::DescribeParams);
}  // namespace content
