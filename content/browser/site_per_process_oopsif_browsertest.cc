// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/render_document_feature.h"

namespace content {

// Out-of-process-sandboxed-iframe (OOPSIF) tests.
//
// Test classes for isolating sandboxed iframes and documents in a different
// process from the rest of their site.
// See https://crbug.com/510122.
class SitePerProcessIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessIsolatedSandboxedIframeTest() {
    feature_list_.InitAndEnableFeature(features::kIsolateSandboxedIframes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SitePerProcessNotIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessNotIsolatedSandboxedIframeTest() {
    feature_list_.InitAndDisableFeature(features::kIsolateSandboxedIframes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test class to allow testing isolated sandboxed iframes using the per-origin
// process model.
class SitePerProcessPerOriginIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessPerOriginIsolatedSandboxedIframeTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kIsolateSandboxedIframes, {{"grouping", "per-origin"}}}},
        {/* disabled_features */});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The following test should not crash. In this test the
// kIsolateSandboxedIframes flag is forced off, so we don't need to verify
// the process isolation details, as is done in
// SitePerProcessIsolatedSandboxedIframeTest.SrcdocCspSandboxIsIsolated below.
// https://crbug.com/1319430
IN_PROC_BROWSER_TEST_P(SitePerProcessNotIsolatedSandboxedIframeTest,
                       SrcdocSandboxFlagsCheck) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed srcdoc child frame, with csp sandbox.
  EXPECT_TRUE(ExecJs(shell(),
                     "var frame = document.createElement('iframe'); "
                     "frame.csp = 'sandbox'; "
                     "frame.srcdoc = 'foo'; "
                     "document.body.appendChild(frame);"));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
}

// Test that a srcdoc iframe that receives its sandbox flags from the CSP
// attribute also gets process isolation. This test starts the same as
// SitePerProcessNotIsolatedSandboxedIframeTest.SrcdocSandboxFlagsCheck, but in
// this test the kIsolateSandboxedIframes flag is on, so we also verify that
// the process isolation has indeed occurred.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SrcdocCspSandboxIsIsolated) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed srcdoc child frame, with csp sandbox.
  EXPECT_TRUE(ExecJs(shell(),
                     "var frame = document.createElement('iframe'); "
                     "frame.csp = 'sandbox'; "
                     "frame.srcdoc = 'foo'; "
                     "document.body.appendChild(frame);"));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->current_frame_host()->active_sandbox_flags());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

// A test to verify that an iframe that is sandboxed using the 'csp' attribute
// instead of the 'sandbox' attribute gets process isolation when the
// kIsolatedSandboxedIframes flag is enabled.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       CspIsolatedSandbox) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create csp-sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.csp = 'sandbox'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->current_frame_host()->active_sandbox_flags());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

// A test to verify that an iframe with a fully-restrictive sandbox is rendered
// in a separate process from its parent frame even if they have the same
// origin.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       IsolatedSandbox) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

// Test that sandboxed iframes that are same-site with their parent but
// cross-origin from each other are put in different processes from each other,
// when the 'per-origin' isolation grouping is active for
// kIsolateSandboxedIframes. (In 'per-site' isolation mode they would be in the
// same process.)
IN_PROC_BROWSER_TEST_P(SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                       CrossOriginIsolatedSandboxedIframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The children need to have the same origin as the parent, but be cross
  // origin from each other.
  GURL same_origin_child_url(main_url);
  GURL cross_origin_child_url(
      embedded_test_server()->GetURL("sub.a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frames, both same-origin and cross-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame1 = document.createElement('iframe'); "
        "frame1.sandbox = ''; "
        "frame1.src = '%s'; "
        "document.body.appendChild(frame1); "
        "var frame2 = document.createElement('iframe'); "
        "frame2.sandbox = ''; "
        "frame2.src = '%s'; "
        "document.body.appendChild(frame2);",
        same_origin_child_url.spec().c_str(),
        cross_origin_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* child1 = root->child_at(0);  // a.com
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child1->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child1->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child1->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());

  FrameTreeNode* child2 = root->child_at(1);  // sub.a.com
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child2->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child2->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child2->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  // This is the key result for this test: the sandboxed iframes for 'a.com' and
  // 'sub.a.com' should be in different SiteInstances.
  auto* child1_site_instance1 = child1->current_frame_host()->GetSiteInstance();
  auto* child2_site_instance1 = child2->current_frame_host()->GetSiteInstance();
  EXPECT_NE(child1_site_instance1, child2_site_instance1);
  EXPECT_NE(child1_site_instance1->GetProcess(),
            child2_site_instance1->GetProcess());
}

// Test that, while using 'per-origin' isolation grouping, navigating a
// sandboxed iframe from 'a.foo.com' to 'b.foo.com' results in the sandbox using
// two different SiteInstances.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                       CrossOriginNavigationSwitchesSiteInstances) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL cross_origin_child_url(
      embedded_test_server()->GetURL("a.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed cross-origin child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        cross_origin_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);  // a.foo.com
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  scoped_refptr<SiteInstanceImpl> site_instance_root =
      root->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(site_instance_root, site_instance1);
  EXPECT_TRUE(site_instance1->GetSiteInfo().is_sandboxed());
  EXPECT_FALSE(site_instance_root->GetSiteInfo().is_sandboxed());

  // Navigate sandboxed frame cross-origin to b.foo.com.
  EXPECT_TRUE(NavigateIframeToURL(
      shell()->web_contents(), "test_frame",
      GURL(embedded_test_server()->GetURL("b.foo.com", "/title1.html"))));

  scoped_refptr<SiteInstanceImpl> site_instance2 =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(site_instance_root, site_instance2);
  EXPECT_NE(site_instance1, site_instance2);
  EXPECT_NE(site_instance1->GetProcess(), site_instance2->GetProcess());
}

// Test that navigating cross-origin from a non-sandboxed iframe to a CSP
// sandboxed iframe results in switching to a new SiteInstance in a different
// process.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                       CrossOriginNavigationToCSPSwitchesSiteInstances) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL cross_origin_child_url(
      embedded_test_server()->GetURL("a.foo.com", "/title1.html"));
  GURL cross_origin_csp_child_url(
      embedded_test_server()->GetURL("b.foo.com",
                                     "/set-header?"
                                     "Content-Security-Policy: sandbox "));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create non-sandboxed cross-origin child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        cross_origin_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);  // a.foo.com
  scoped_refptr<SiteInstanceImpl> site_instance_root =
      root->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(site_instance_root, site_instance1);
  EXPECT_FALSE(site_instance1->GetSiteInfo().is_sandboxed());

  // Navigate child frame cross-origin to CSP-isolated b.foo.com.
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test_frame",
                                  cross_origin_csp_child_url));

  // The child frame should now have a different SiteInstance and process than
  // it did before the navigation.
  scoped_refptr<SiteInstanceImpl> site_instance2 =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(site_instance1, site_instance2);
  EXPECT_NE(site_instance1->GetProcess(), site_instance2->GetProcess());
  EXPECT_TRUE(site_instance2->GetSiteInfo().is_sandboxed());
}

// Check that two same-site sandboxed iframes in unrelated windows share the
// same process due to subframe process reuse.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxProcessReuse) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, same-origin.
  std::string js_str = base::StringPrintf(
      "var frame = document.createElement('iframe'); "
      "frame.sandbox = ''; "
      "frame.src = '%s'; "
      "document.body.appendChild(frame);",
      child_url.spec().c_str());
  EXPECT_TRUE(ExecJs(shell(), js_str));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());

  // Set up an unrelated window with the same frame hierarchy.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, main_url));
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_TRUE(ExecJs(new_shell, js_str));
  ASSERT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  FrameTreeNode* new_child = new_root->child_at(0);
  EXPECT_TRUE(new_child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(new_root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());

  // Check that the two sandboxed subframes end up in separate
  // BrowsingInstances but in the same process.
  EXPECT_FALSE(
      new_child->current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
          child->current_frame_host()->GetSiteInstance()));
  EXPECT_EQ(new_child->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// A test to verify that when an iframe has two sibling subframes, each with a
// fully-restrictive sandbox, that each of the three gets its own process
// even though they are all same-origin.
// Note: using "sandbox = ''" in this and the following tests creates fully
// restricted sandboxes, which will include the kOrigin case we are interested
// in.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       IsolatedSandboxSiblingSubframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame1 = document.createElement('iframe'); "
        "frame1.sandbox = ''; "
        "frame1.src = '%s'; "
        "document.body.appendChild(frame1); "
        "var frame2 = document.createElement('iframe'); "
        "frame2.sandbox = ''; "
        "frame2.src = '%s'; "
        "document.body.appendChild(frame2);",
        child_url.spec().c_str(), child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child1->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child2->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child1->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child2->current_frame_host()->GetSiteInstance());
  // Because the siblings are same-site to each other (in fact, same origin) we
  // expect them to share a process when sandboxed.
  EXPECT_EQ(child1->current_frame_host()->GetSiteInstance(),
            child2->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child1->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_TRUE(child2->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       IsolatedSandboxSrcdocSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, with srcdoc content.
  std::string child_inner_text("srcdoc sandboxed subframe");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = '%s'; "
        "document.body.appendChild(frame);",
        child_inner_text.c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  // Verify that the child has only the 'allow-scripts' permission set.
  EXPECT_EQ(child->effective_frame_policy().sandbox_flags,
            network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
  EXPECT_EQ(std::string(url::kAboutSrcdocURL),
            child->current_frame_host()->GetLastCommittedURL());
  EXPECT_TRUE(child->current_frame_host()->GetLastCommittedOrigin().opaque());
  // Verify that the child's precursor origin matches 'a.com'. Note: we create
  // the expected value using `main_url` so that the test server port will be
  // correctly matched.
  EXPECT_EQ(url::SchemeHostPort(main_url),
            child->current_origin().GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
  {
    std::string js_str("document.body.innerText;");
    EXPECT_EQ(child_inner_text, EvalJs(child->current_frame_host(), js_str));
  }
}

// A test to make sure that about:blank in a sandboxed iframe doesn't get
// process isolation. If it did, it would be impossible for the parent to inject
// any content, and it would be stuck as empty content.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       NotIsolatedSandboxAboutBlankSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, with about:blank content.
  {
    std::string js_str(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'child_frame'; "
        "frame.sandbox = ''; "
        "frame.src = 'about:blank'; "
        "document.body.appendChild(frame);");
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  // Verify that the child has no permissions set.
  EXPECT_EQ(child->effective_frame_policy().sandbox_flags,
            network::mojom::WebSandboxFlags::kAll);
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            child->current_frame_host()->GetLastCommittedURL());
  EXPECT_TRUE(child->current_frame_host()->GetLastCommittedOrigin().opaque());
  // Verify that the child's precursor origin matches 'a.com'. Note: we create
  // the expected value using `main_url` so that the test server port will be
  // correctly matched.
  EXPECT_EQ(url::SchemeHostPort(main_url),
            child->current_origin().GetTupleOrPrecursorTupleIfOpaque());
  // The child needs to be in the parent's SiteInstance.
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());

  // Navigate to a page that should get process isolation.
  GURL isolated_child_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateFrameToURL(child, isolated_child_url));
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());

  // Navigate back to about:blank, and verify it's put back into the parent's
  // SiteInstance.
  scoped_refptr<SiteInstanceImpl> child_previous_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child_frame",
                                  GURL("about:blank")));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child_previous_site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

// Test to make sure that javascript: urls don't execute in a sandboxed iframe.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxedIframeWithJSUrl) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame with a javascript: URL.
  std::string js_url_str("javascript:\"foo\"");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        js_url_str.c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Verify parent and child frames share a SiteInstance. A sandboxed iframe
  // with a javascript: url shouldn't get its own process.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Verify that the javascript: url did not execute. This is expected
  // regardless of IsolatedSandboxedIframes since sandboxed iframes get opaque
  // origins, and javascript: urls don't execute in opaque origins.
  EXPECT_TRUE(
      EvalJs(child->current_frame_host(), "document.body.innerHTML == ''")
          .ExtractBool());
}

// Test to make sure that an iframe with a data:url is process isolated.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxedIframeWithDataURLIsIsolated) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame with a data URL.
  std::string data_url_str("data:text/html,dataurl");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        data_url_str.c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Verify parent and child frames don't share a SiteInstance
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

// Test to make sure that an iframe with a data:url is appropriately sandboxed.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxedIframeWithDataURL) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create non-sandboxed child frame with a data URL.
  std::string data_url_str("data:text/html,dataurl");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        data_url_str.c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Verify parent and child frames share a SiteInstance
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Now make subframe sandboxed.
  {
    std::string js_str(
        "var frame = document.getElementById('test_frame'); "
        "frame.sandbox = ''; ");
    EXPECT_TRUE(ExecJs(shell(), js_str));
  }
  NavigateFrameToURL(child,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));
  // Child should now be in a different SiteInstance.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Go back and ensure the data: URL committed in the same SiteInstance as the
  // original navigation.
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  {
    TestFrameNavigationObserver frame_observer(child);
    web_contents()->GetController().GoBack();
    frame_observer.WaitForCommit();
  }
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(GURL(data_url_str),
            child->current_frame_host()->GetLastCommittedURL());
}

// Test to make sure that a sandboxed child iframe with a data url and a
// sandboxed parent end up in the same SiteInstance.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxedParentWithSandboxedChildWithDataURL) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::string parent_url_str = main_url.spec();
  std::string data_url_str("data:text/html,dataurl");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Allow "parent" to have the allow-scripts permissions so it can create
  // a child.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        parent_url_str.c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Give the grandchild the allow-scripts permissions so it matches the child.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        data_url_str.c_str());
    EXPECT_TRUE(ExecJs(child->current_frame_host(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(1U, child->child_count());
  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(GURL(data_url_str),
            grandchild->current_frame_host()->GetLastCommittedURL());
}

// Test to make sure that a sandboxed iframe with a (not-explicitly) sandboxed
// subframe ends up in the same SiteInstance/process as its subframe.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       IsolatedSandboxWithNonSandboxedSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check child vs. parent.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());

  // Check grandchild vs. child.
  ASSERT_EQ(1U, child->child_count());
  FrameTreeNode* grand_child = child->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            grand_child->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            grand_child->current_frame_host()->GetSiteInstance());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessNotIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

}  // namespace content
