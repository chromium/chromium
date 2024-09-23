// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/common/switches.h"

namespace content {

// Test class for tests involving base url inheritance behavior.
class BaseUrlInheritanceIframeTest : public ContentBrowserTest {
 public:
  BaseUrlInheritanceIframeTest() = default;

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
  void StartEmbeddedServer() {
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};  // class BaseUrlInheritanceIframeTest

// A test to ensure that a baseURI exceeding chromium's maximum length for urls
// is not inherited.
IN_PROC_BROWSER_TEST_F(BaseUrlInheritanceIframeTest,
                       InheritedBaseUrlIsLessThan2MB) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // The following JS modifies the document state so its baseURL exceeds the
  // maximum length of URL that chromium supports.
  EXPECT_LT(url::kMaxURLChars, EvalJs(root,
                                      R"(
                                        path = "xxxxxxxx";
                                        for (i = 0; i < 18; i++) {
                                          path = path + path;
                                        }
                                        history.replaceState(
                                            "", "", "path_" + path);
                                        document.baseURI
                                      )")
                                   .ExtractString()
                                   .length());
  // Navigate frame to about:blank. Normally it should inherit its baseURI from
  // the document initiating the navigation, but since it is too long, no
  // baseURI is sent, and the about:blank frame falls back to a baseURI of
  // 'about:blank'.
  {
    TestNavigationObserver iframe_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, "location.href = 'about:blank';"));
    iframe_observer.Wait();
  }
  // If we get here without a crash, we didn't hit the CHECK in
  // NavigationRequest for the initiator base url being either nulopt, or
  // non-empty.
  GURL new_frame_url = root->current_frame_host()->GetLastCommittedURL();
  EXPECT_TRUE(new_frame_url.IsAboutBlank());
  EXPECT_EQ("about:blank", EvalJs(root, "document.baseURI").ExtractString());
}

// A test to make sure that restoring a session history entry that was saved
// with an about:blank subframe never results in an initiator_base_url of
// an empty string. std::nullopt is expected instead of an empty GURL with
// legacy base url behavior, or the non-empty initiator base url in the
// new base url inheritance mode. This test runs in both modes.
IN_PROC_BROWSER_TEST_F(BaseUrlInheritanceIframeTest,
                       BaseURLFromSessionHistoryIsNulloptNotEmptyString) {
  StartEmbeddedServer();
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  // Navigate child to about:blank.
  {
    TestNavigationObserver iframe_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(child, "location.href = 'about:blank';"));
    iframe_observer.Wait();
  }
  GURL child_frame_url = child->current_frame_host()->GetLastCommittedURL();

  // Save the page state.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  blink::PageState page_state = entry->GetPageState();

  // Decode the page state so we can inspect what base url value it contains.
  blink::ExplodedPageState exploded_page_state;
  ASSERT_TRUE(
      blink::DecodePageState(page_state.ToEncodedData(), &exploded_page_state));
  EXPECT_EQ(1U, exploded_page_state.top.children.size());
  // Make sure the about:blank child has the correct initiator_base_url.
  GURL initiator_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(exploded_page_state.top.children[0]
                  .initiator_base_url_string.has_value());
  EXPECT_EQ(
      base::UTF8ToUTF16(initiator_url.spec()),
      exploded_page_state.top.children[0].initiator_base_url_string.value());
}

// Test class to allow testing srcdoc functionality both with and without
// `kIsolateSandboxedIframes` enabled. The tests verify the correct operation of
// plumbing of both srcdoc attribute values, as well as the srcdoc frame's
// parent's base url values, to the srcdoc's frame's renderer.
class SrcdocIsolatedSandboxedIframeTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SrcdocIsolatedSandboxedIframeTest() {
    feature_list_.InitWithFeatureState(
        blink::features::kIsolateSandboxedIframes, GetParam());
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
  void StartEmbeddedServer() {
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};  // class SrcdocIsolatedSandboxedIframeTest

// Test the scenario where a Site A mainframe contains a Site B subframe which
// in turn has a sandboxed srcdoc frame. If A tries to directly navigate
// the srcdoc to about:srcdoc, the navigation should be blocked.
IN_PROC_BROWSER_TEST_P(SrcdocIsolatedSandboxedIframeTest,
                       SrcdocNavigationForCrossOriginInitiatorIsBlocked) {
  StartEmbeddedServer();
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  GURL subframe_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_sandboxed_srcdoc_iframe.html"));
  {
    TestNavigationObserver srcdoc_observer(shell()->web_contents());
    EXPECT_TRUE(NavigateFrameToURL(child, subframe_url));
    srcdoc_observer.Wait();
  }

  // The srcdoc should have its parent's origin and base url.
  FrameTreeNode* srcdoc_frame = child->child_at(0);
  EXPECT_TRUE(
      srcdoc_frame->current_frame_host()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ(url::SchemeHostPort(subframe_url),
            srcdoc_frame->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_EQ(subframe_url,
            GURL(EvalJs(srcdoc_frame, "document.baseURI").ExtractString()));

  // Have mainframe attempt to navigate srcdoc to about:srcdoc.
  {
    TestNavigationObserver srcdoc_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, "frames[0][0].location.href = 'about:srcdoc';"));
    srcdoc_observer.Wait();
  }

  // Check final origin and base url.
  std::string expected_base_url_str = "chrome-error://chromewebdata/";
  EXPECT_EQ(expected_base_url_str,
            EvalJs(srcdoc_frame, "document.baseURI").ExtractString());

  EXPECT_TRUE(
      srcdoc_frame->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(url::SchemeHostPort(), srcdoc_frame->current_frame_host()
                                         ->GetLastCommittedOrigin()
                                         .GetTupleOrPrecursorTupleIfOpaque());
}

// Out-of-process-sandboxed-iframe (OOPSIF) tests.
//
// Test classes for isolating sandboxed iframes and documents in a different
// process from the rest of their site.
// See https://crbug.com/510122.
class SitePerProcessIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessIsolatedSandboxedIframeTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kIsolateSandboxedIframes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test class to test IsolatedSandboxedIframes with and without
// kOriginKeyedProcessesByDefault enabled.
class OriginKeyedProcessIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  OriginKeyedProcessIsolatedSandboxedIframeTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{blink::features::kIsolateSandboxedIframes,
                                blink::features::
                                    kOriginAgentClusterDefaultEnabled,
                                features::kOriginKeyedProcessesByDefault},
          /*disabled_features=*/{});
    } else {
      // Note: we don't explicitly disable kOriginAgentClusterDefaultEnabled
      // below, since by itself it shouldn't affect any process model decisions.
      // It's included above since kOriginKeyedProcessesByDefault requires it.
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{blink::features::kIsolateSandboxedIframes},
          /*disabled_features=*/{features::kOriginKeyedProcessesByDefault});
    }
  }

 protected:
  void SetUpOnMainThread() override {
    SitePerProcessBrowserTestBase::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);

    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SitePerProcessBrowserTestBase::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    SitePerProcessBrowserTestBase::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  // Need an https server because origin-keyed processes require HTTPS.
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

class SitePerProcessNotIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessNotIsolatedSandboxedIframeTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kIsolateSandboxedIframes);
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
        {{blink::features::kIsolateSandboxedIframes,
          {{"grouping", "per-origin"}}}},
        {/* disabled_features */});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest
    : public SitePerProcessIsolatedSandboxedIframeTest {
 public:
  SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kIsolateSandboxedIframes);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessIsolatedSandboxedIframeTest::SetUpCommandLine(command_line);
    // Because this test derives from SitePerProcessBrowserTestBase which
    // calls IsolateAllSitesForTesting, we need to manually remove the
    // kSitePerProcess switch to simulate the environment where not all sites
    // automatically get isolation.
    command_line->RemoveSwitch(switches::kSitePerProcess);
  }

  void SetUpOnMainThread() override {
    SitePerProcessIsolatedSandboxedIframeTest::SetUpOnMainThread();

    // Override BrowserClient to disable strict site isolation.
    browser_client_ =
        std::make_unique<PartialSiteIsolationContentBrowserClient>();
    // The custom ContentBrowserClient below typically ensures that this test
    // runs without strict site isolation, but it's still possible to
    // inadvertently override this when running with --site-per-process on the
    // command line. This might happen on try bots, so these tests take this
    // into account to prevent failures, but this is not an intended
    // configuration for these tests, since isolating sandboxed iframes in
    // these tests depends on use of default SiteInstances.
    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING) << "This test should be run without --site-per-process, "
                   << "as it's designed to exercise code paths when strict "
                   << "site isolation is turned off.";
    }
  }

  void TearDownOnMainThread() override {
    SitePerProcessIsolatedSandboxedIframeTest::TearDownOnMainThread();
    browser_client_.reset();
  }

  // A custom ContentBrowserClient to turn off strict site isolation, since
  // isolated sandboxed iframes behave differently in environments like Android
  // where it is not (generally) used. Note that kSitePerProcess is a
  // higher-layer feature, so we can't just disable it here.
  class PartialSiteIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldEnableStrictSiteIsolation() override { return false; }
    bool DoesSiteRequireDedicatedProcess(
        BrowserContext* browser_context,
        const GURL& effective_site_url) override {
      return effective_site_url == GURL("http://isolated.com");
    }
  };

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PartialSiteIsolationContentBrowserClient> browser_client_;
};

// A test class to allow testing isolated sandboxed iframes using the
// per-document grouping model.
class SitePerProcessPerDocumentIsolatedSandboxedIframeTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessPerDocumentIsolatedSandboxedIframeTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kIsolateSandboxedIframes,
          {{"grouping", "per-document"}}}},
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

// Test that a sandboxed data url is loaded correctly (i.e. doesn't crash) both
// with and without kOriginKeyedProcessesByDefault enabled.
IN_PROC_BROWSER_TEST_P(OriginKeyedProcessIsolatedSandboxedIframeTest,
                       DataUrlLoadsWithoutCrashing) {
  bool origin_keyed_processes_by_default_enabled = GetParam();
  EXPECT_EQ(origin_keyed_processes_by_default_enabled,
            SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault());
  GURL main_url(https_server()->GetURL("foo.a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed srcdoc child frame, with csp sandbox.
  TestNavigationObserver iframe_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(),
                     "var frame = document.createElement('iframe'); "
                     "frame.sandbox = ''; "
                     "frame.src = 'data:text/html,foo'; "
                     "document.body.appendChild(frame);"));
  iframe_observer.Wait();
  EXPECT_TRUE(iframe_observer.last_navigation_succeeded());

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->current_frame_host()->active_sandbox_flags());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  const SiteInfo& root_site_info =
      root->current_frame_host()->GetSiteInstance()->GetSiteInfo();
  const SiteInfo& child_site_info =
      child->current_frame_host()->GetSiteInstance()->GetSiteInfo();

  GURL expected_root_site_url = origin_keyed_processes_by_default_enabled
                                    ? url::Origin::Create(main_url).GetURL()
                                    : GURL("https://a.com/");
  EXPECT_EQ(origin_keyed_processes_by_default_enabled,
            root_site_info.requires_origin_keyed_process());
  EXPECT_EQ(expected_root_site_url, root_site_info.site_url());
  EXPECT_FALSE(root_site_info.is_sandboxed());

  // Note: unless IsolateSandboxedIframes is disabled, we expect the sandboxed
  // data-url frame to still have the full origin, since that is what the
  // frame got from its initiator.
  GURL expected_child_site_url =
      (origin_keyed_processes_by_default_enabled ||
       SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled())
          ? url::Origin::Create(main_url).GetURL()
          : GURL("https://a.com/");
  EXPECT_EQ(origin_keyed_processes_by_default_enabled,
            child_site_info.requires_origin_keyed_process());
  EXPECT_EQ(expected_child_site_url, child_site_info.site_url());
  EXPECT_TRUE(child_site_info.is_sandboxed());
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
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create csp-sandboxed child frame, same-origin.
  {
    EXPECT_TRUE(ExecJs(shell(),
                       "var frame = document.createElement('iframe'); "
                       "frame.csp = 'sandbox'; "
                       "frame.srcdoc = '<b>Hello!</b>'; "
                       "document.body.appendChild(frame);"));
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

// A test to verify that postMessages sent to/from sandboxed frames get
// delivered properly.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest, PostMessage) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL child1_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL child2_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed same-site child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child1_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Create sandboxed cross-site child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child2_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Verify test setup.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child1->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child2->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child1->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_TRUE(child2->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());

  // Verify that postMessage works between same-site sandboxed child and its
  // parent.
  const std::string kDefinePostMessageReply =
      "window.addEventListener('message', function(event) {"
      "  event.source.postMessage(event.data + '-reply', '*');"
      "});";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), kDefinePostMessageReply));
  const std::string kDefineOnMessagePromise =
      "var onMessagePromise = new Promise(resolve => {"
      "  window.addEventListener('message', function(event) {"
      "    resolve(event.data);"
      "  });"
      "});";
  EXPECT_TRUE(ExecJs(child1->current_frame_host(), kDefineOnMessagePromise));
  EXPECT_TRUE(
      ExecJs(child1->current_frame_host(), "parent.postMessage('foo', '*');"));
  EXPECT_EQ("foo-reply",
            EvalJs(child1->current_frame_host(), "onMessagePromise"));

  // Verify that postMessage works between cross-site sandboxed child and its
  // parent.
  EXPECT_TRUE(ExecJs(child2->current_frame_host(), kDefineOnMessagePromise));
  EXPECT_TRUE(
      ExecJs(child2->current_frame_host(), "parent.postMessage('bar', '*');"));
  EXPECT_EQ("bar-reply",
            EvalJs(child2->current_frame_host(), "onMessagePromise"));

  // Verify that postMessage works between the two sandboxed frames.
  EXPECT_TRUE(ExecJs(child2->current_frame_host(), kDefinePostMessageReply));
  EXPECT_TRUE(ExecJs(child1->current_frame_host(), kDefineOnMessagePromise));
  EXPECT_TRUE(ExecJs(child1->current_frame_host(),
                     "parent.frames[1].postMessage('baz', '*');"));
  EXPECT_EQ("baz-reply",
            EvalJs(child1->current_frame_host(), "onMessagePromise"));
}

// Test that a sandboxed srcdoc iframe loads properly when its parent's url is
// different from its site_url. The child should get its own SiteInstance with a
// site_url based on the full origin of the parent's original url.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                       SrcdocSandboxedFrameWithNonSiteParent) {
  GURL main_url(embedded_test_server()->GetURL("sub.a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed srcdoc child frame.
  {
    std::string js_str =
        "const frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frametree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);  // sub.a.com

  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);

  auto* parent_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(parent_site_instance, child_site_instance);
  EXPECT_TRUE(child_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_FALSE(parent_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_EQ(embedded_test_server()->GetURL("sub.a.com", "/"),
            child_site_instance->GetSiteInfo().site_url());
  EXPECT_EQ(GURL("http://a.com/"),
            parent_site_instance->GetSiteInfo().site_url());
}

namespace {

GURL GetFrameBaseUrl(RenderFrameHostImpl* rfhi) {
  return GURL(EvalJs(rfhi, "document.baseURI").ExtractString());
}

GURL GetFrameBaseUrl(Shell* shell) {
  return GURL(EvalJs(shell, "document.baseURI").ExtractString());
}

}  // namespace

IN_PROC_BROWSER_TEST_P(SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                       SrcdocSandboxedFrameInsideAboutBlank) {
  // Open main page on a.foo.com. It will be put in a site instance with site
  // url foo.com
  GURL main_url(embedded_test_server()->GetURL("a.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Create an about:blank frame.
  {
    std::string js_str =
        "const frame = document.createElement('iframe'); "
        "frame.src = 'about:blank'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Create a sandboxed srcdoc frame inside the about:blank child.
  {
    std::string js_str =
        "const frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(child, js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(1U, child->child_count());
  FrameTreeNode* grand_child = child->child_at(0);
  auto* grand_child_site_instance =
      grand_child->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(grand_child_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_EQ(embedded_test_server()->GetURL("a.foo.com", "/"),
            grand_child_site_instance->GetSiteInfo().site_url());
  EXPECT_EQ(main_url, GetFrameBaseUrl(grand_child->current_frame_host()));
  EXPECT_EQ(main_url, grand_child->current_frame_host()->GetInheritedBaseUrl());
}

// Similar to SrcdocSandboxedFrameWithNonSiteParent, but this time the srcdoc
// is opened from b.foo.com which is loaded in the SiteInstance that was
// created for a.foo.com, so the SiteInstance cannot be used to specify the
// origin the srcdoc should use, namely b.foo.com.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                       SrcdocSandboxedFrameWithNonSiteParent2) {
  GURL main_url(embedded_test_server()->GetURL("a.foo.com", "/title1.html"));
  GURL sibling_url(embedded_test_server()->GetURL("b.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a new window that will share the SiteInstance of the main window.
  Shell* new_shell = OpenPopup(root, sibling_url, "");
  FrameTreeNode* sibling =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            sibling->current_frame_host()->GetSiteInstance());

  {
    std::string js_str =
        "const frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(new_shell, js_str));
    ASSERT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  }
  ASSERT_EQ(1U, sibling->child_count());
  FrameTreeNode* child = sibling->child_at(0);  // b.foo.com

  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);

  auto* sibling_site_instance =
      sibling->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(sibling_site_instance, child_site_instance);
  EXPECT_TRUE(child_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_FALSE(sibling_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_EQ(embedded_test_server()->GetURL("b.foo.com", "/"),
            child_site_instance->GetSiteInfo().site_url());
  EXPECT_EQ(GURL("http://foo.com/"),
            sibling_site_instance->GetSiteInfo().site_url());
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

  // Because the subframe is a data: URL, the process should be locked to the
  // initiator origin's site, which is the parent in this case. Unlike the
  // parent, the data: subframe process should be sandboxed.
  EXPECT_EQ(
      child->current_frame_host()->GetProcess()->GetProcessLock().lock_url(),
      root->current_frame_host()->GetLastCommittedOrigin().GetURL());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
}

// Verify that a navigation from a sandboxed iframe, with an origin distinct
// from its parent, to about:blank succeeds.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxedNavigationToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame to a different site.
  GURL child_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    TestNavigationObserver iframe_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    iframe_observer.Wait();
    EXPECT_TRUE(iframe_observer.last_navigation_succeeded());
  }

  // Get child's FrameTreeNode.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstanceImpl> original_child_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(original_child_site_instance->GetSiteInfo().is_sandboxed());

  {
    TestNavigationObserver iframe_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(child, "location.href = 'about:blank';"));
    iframe_observer.Wait();
    EXPECT_TRUE(iframe_observer.last_navigation_succeeded());
  }

  // Verify that child origin is correct. This also helps validate that the
  // navigation didn't crash.
  EXPECT_EQ("null", EvalJs(child, "window.origin").ExtractString());
  // The child should remain in the same SiteInstance.
  EXPECT_EQ(original_child_site_instance,
            child->current_frame_host()->GetSiteInstance());
}

// Verify that a navigation from a sandboxed iframe, with an origin distinct
// from its parent, to about:blank succeeds. This is a variation on
// SandboxedNavigationToAboutBlank in which the parent removes the sandbox flag
// after B has loaded, and before it navigates to about:blank.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       SandboxedNavigationToAboutBlank_SandboxRevokedByParent) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame to a different site.
  GURL child_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    TestNavigationObserver iframe_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    iframe_observer.Wait();
    EXPECT_TRUE(iframe_observer.last_navigation_succeeded());
  }

  // Get child's FrameTreeNode.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstanceImpl> original_child_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(original_child_site_instance->GetSiteInfo().is_sandboxed());

  // The parent removes the iframe's sandbox attribute before the child
  // self-navigates to about:blank.
  EXPECT_TRUE(ExecJs(
      root, "document.querySelector('iframe').removeAttribute('sandbox');"));

  {
    TestNavigationObserver iframe_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(child, "location.href = 'about:blank';"));
    iframe_observer.Wait();
    EXPECT_TRUE(iframe_observer.last_navigation_succeeded());
  }

  // Verify that child origin is correct. This also helps validate that the
  // navigation didn't crash.
  EXPECT_EQ("null", EvalJs(child, "window.origin").ExtractString());
  // The child should remain in the same SiteInstance.
  EXPECT_EQ(original_child_site_instance,
            child->current_frame_host()->GetSiteInstance());
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

  // Verify parent and child frames share a non-sandboxed SiteInstance.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());

  // Now make the subframe sandboxed.
  {
    std::string js_str(
        "var frame = document.getElementById('test_frame'); "
        "frame.sandbox = ''; ");
    EXPECT_TRUE(ExecJs(shell(), js_str));
  }
  NavigateFrameToURL(child,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin b_origin = child->current_frame_host()->GetLastCommittedOrigin();

  // Child should now be in a different, sandboxed SiteInstance.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());

  // The child process should be in a process of its initiator origin.
  EXPECT_EQ(
      child->current_frame_host()->GetProcess()->GetProcessLock().lock_url(),
      b_origin.GetTupleOrPrecursorTupleIfOpaque().GetURL());

  // Go back and ensure the data: URL remains sandboxed, and committed in a
  // different SiteInstance from the original navigation. From the spec:
  // "Generally speaking, dynamically removing or changing the sandbox attribute
  // is ill-advised, because it can make it quite hard to reason about what will
  // be allowed and what will not."
  // https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  {
    TestFrameNavigationObserver frame_observer(child);
    web_contents()->GetController().GoBack();
    frame_observer.WaitForCommit();
  }
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_sandboxed());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_sandboxed());
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

// A test to verify that an iframe with a fully-restrictive sandbox is rendered
// in the same process as its parent frame when the parent frame is in a
// default SiteInstance.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    NotIsolatedSandbox) {
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
  auto* parent_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  EXPECT_FALSE(parent_site_instance->RequiresDedicatedProcess());
  EXPECT_EQ(parent_site_instance, child_site_instance);
  EXPECT_FALSE(child_site_instance->GetSiteInfo().is_sandboxed());
}

// Similar to the NotIsolatedSandbox test, but using a site that requires a
// dedicated process, and thus resulting in a separate process for the sandboxed
// iframe.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    IsolatedSandbox) {
  // Specify an isolated.com site to get the main frame into a dedicated
  // process.
  GURL main_url(embedded_test_server()->GetURL("isolated.com", "/title1.html"));
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
  auto* parent_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  EXPECT_TRUE(parent_site_instance->RequiresDedicatedProcess());
  EXPECT_NE(parent_site_instance, child_site_instance);
  EXPECT_NE(parent_site_instance->GetProcess(),
            child_site_instance->GetProcess());
  EXPECT_TRUE(child_site_instance->GetSiteInfo().is_sandboxed());
}

// In this test, a main frame requests sandbox isolation for a site that would
// not normally be given a dedicated process. This causes the sandbox isolation
// request to fail.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    CSPSandboxedMainFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/set-header?Content-Security-Policy: sandbox allow-scripts"));
  GURL child_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
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
  auto* parent_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(parent_site_instance->RequiresDedicatedProcess());
  EXPECT_FALSE(parent_site_instance->GetSiteInfo().is_sandboxed());
  // TODO(wjmaclean): It seems weird that the
  // effective_frame_policy().sandbox_flags don't get set in this case. Maybe
  // worth investigating this at some point. https://crbug.com/1346723
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  // Since the parent is sandboxed, the child is same process to it.
  EXPECT_EQ(parent_site_instance, child_site_instance);
}

// Same as CSPSandboxedMainframe, but this time the site is isolatable on its
// own, so it gets the sandbox attribute via the CSP header.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    CSPSandboxedMainframeIsolated) {
  GURL main_url(embedded_test_server()->GetURL(
      "isolated.com",
      "/set-header?Content-Security-Policy: sandbox allow-scripts"));
  GURL child_url(
      embedded_test_server()->GetURL("isolated.com", "/title1.html"));
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
  auto* parent_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(parent_site_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(parent_site_instance->GetSiteInfo().is_sandboxed());
  // TODO(wjmaclean): It seems weird that the
  // effective_frame_policy().sandbox_flags don't get set in this case. Maybe
  // worth investigating this at some point. https://crbug.com/1346723
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  // Since the parent is sandboxed, the child is same process to it.
  // Note: this assumes that we are running per-site isolation mode for isolated
  // sandboxed iframes.
  EXPECT_EQ(parent_site_instance, child_site_instance);
}

// Test to verify which IsolationContext is used when a BrowsingInstance swap is
// performed during a navigation.
// Note: this test does not work as hoped, see comment before the final
// expectation of the test.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    MainFrameBrowsingInstanceSwap) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  scoped_refptr<SiteInstanceImpl> site_instance_a =
      web_contents()->GetSiteInstance();
  EXPECT_FALSE(site_instance_a->GetSiteInfo().is_sandboxed());

  // Force BrowsingInstance swap to a URL with a CSP sandbox header.
  GURL isolated_url(embedded_test_server()->GetURL(
      "b.com", "/set-header?Content-Security-Policy: sandbox"));
  SiteInstance::StartIsolatingSite(
      shell()->web_contents()->GetController().GetBrowserContext(),
      isolated_url,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  scoped_refptr<SiteInstanceImpl> site_instance_b =
      web_contents()->GetSiteInstance();
  EXPECT_NE(site_instance_a, site_instance_b);
  EXPECT_NE(site_instance_a->GetIsolationContext().browsing_instance_id(),
            site_instance_b->GetIsolationContext().browsing_instance_id());
  // The SiteInstance is not considered sandboxed even in the new
  // BrowsingInstance. This is not the result we wanted, but without a massive
  // amount of work it's the best we can do. This happens because
  // NavigationRequest::GetUrlInfo() doesn't know (at the time
  // NavigationRequest::OnResponseStarted() calls
  // RenderFrameHostManager::GetFrameHostForNavigation()) that there will be
  // a BrowsingInstance swap, and it doesn't have access to the new
  // BrowsingInstance (IsolationContext) when deciding to add the `is_sandboxed`
  // attribute to UrlInfoInit.
  // This is an edge case we can live with since it only happens with the
  // main frame getting a CSP sandbox, and the main frame does get its own
  // process regardless in this case.
  EXPECT_FALSE(site_instance_b->GetSiteInfo().is_sandboxed());
}

IN_PROC_BROWSER_TEST_P(
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    MainFrameWithSandboxedOpener) {
  // Specify an isolated.com site to get the main frame into a dedicated
  // process.
  GURL main_url(embedded_test_server()->GetURL("isolated.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts allow-popups'; "
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
  auto* parent_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kPopups &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols;
  EXPECT_EQ(expected_flags, child->effective_frame_policy().sandbox_flags);
  EXPECT_TRUE(parent_site_instance->RequiresDedicatedProcess());
  EXPECT_NE(parent_site_instance, child_site_instance);
  EXPECT_TRUE(child_site_instance->GetSiteInfo().is_sandboxed());

  // Sandboxed child calls window.open.
  Shell* new_shell = OpenPopup(child, child_url, "");
  EXPECT_TRUE(new_shell);
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  auto* new_window_site_instance =
      new_root->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(new_window_site_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(new_window_site_instance->GetSiteInfo().is_sandboxed());
  // Note: this assumes per-site mode for sandboxed iframe isolation. If we
  // settle on per-document mode, this will change to EXPECT_NE.
  EXPECT_EQ(child_site_instance, new_window_site_instance);
}

// Test that sandboxed iframes that are same-site with their parent but
// same-origin to each other are put in different processes from each other,
// when the 'per-document' isolation grouping is active for
// kIsolateSandboxedIframes. (In 'per-site' and 'per-origin' isolation groupings
// they would be in the same process.)
IN_PROC_BROWSER_TEST_P(SitePerProcessPerDocumentIsolatedSandboxedIframeTest,
                       SameOriginIsolatedSandboxedIframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The children need to be same origin to each other, and be (at least)
  // same-site to the parent.
  GURL same_origin_child_url(
      embedded_test_server()->GetURL("sub.a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frames.
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
        same_origin_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* child1 = root->child_at(0);  // sub.a.com
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
  // This is the key result for this test: the sandboxed iframes for both child
  // frames should be in different SiteInstances, even though they are
  // same-origin.
  auto* child1_site_instance = child1->current_frame_host()->GetSiteInstance();
  auto* child2_site_instance = child2->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child1_site_instance->GetSiteInfo().site_url(),
            child2_site_instance->GetSiteInfo().site_url());
  EXPECT_NE(child1_site_instance->GetSiteInfo().unique_sandbox_id(),
            child2_site_instance->GetSiteInfo().unique_sandbox_id());
  EXPECT_NE(child1_site_instance, child2_site_instance);
  EXPECT_NE(child1_site_instance->GetProcess(),
            child2_site_instance->GetProcess());
}

// This test ensures that nested srcdoc iframes get correct base urls.
IN_PROC_BROWSER_TEST_P(SitePerProcessIsolatedSandboxedIframeTest,
                       NestedSrcdocIframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed srcdoc child frame.
  {
    std::string js_str =
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  // Make sure the parent's base url propagates properly to the child.
  GURL parent_base_url = GetFrameBaseUrl(root->current_frame_host());
  GURL child_base_url = GetFrameBaseUrl(child->current_frame_host());
  // Verify child inherited base url from parent as expected.
  EXPECT_EQ(parent_base_url, child_base_url);
  EXPECT_EQ(parent_base_url,
            child->current_frame_host()->GetInheritedBaseUrl());

  // Switch the base url of the root.
  GURL new_root_base_url("http://b.com/");
  {
    std::string js_str = base::StringPrintf(
        "var base_element = document.createElement('base'); "
        "base_element.href = '%s'; "
        "document.head.appendChild(base_element);",
        new_root_base_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    EXPECT_EQ(new_root_base_url, GetFrameBaseUrl(shell()));
  }

  // Create sandboxed srcdoc grandchild frame.
  {
    std::string js_str =
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(child->current_frame_host(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(1U, child->child_count());
  FrameTreeNode* grandchild = child->child_at(0);

  // Make sure the child's snapshotted base url propagates properly to the
  // grandchild. And make sure child's snapshotted base url hasn't changed with
  // the creation of the child.
  EXPECT_EQ(parent_base_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(parent_base_url,
            child->current_frame_host()->GetInheritedBaseUrl());
  EXPECT_EQ(parent_base_url, GetFrameBaseUrl(grandchild->current_frame_host()));
  EXPECT_EQ(parent_base_url,
            grandchild->current_frame_host()->GetInheritedBaseUrl());
}

// Test to verify that nested sandboxed iframes aren't put in the same
// SiteInstance.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerDocumentIsolatedSandboxedIframeTest,
                       NestedIsolatedSandboxedIframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  // The children need to be same origin to each other, and be (at least)
  // same-site to the parent.
  GURL same_origin_child_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        same_origin_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Create sandboxed grand-child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        same_origin_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(child->current_frame_host(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(1U, child->child_count());
  FrameTreeNode* grandchild = child->child_at(0);

  // Check frame tree.
  auto* child_site_instance = child->current_frame_host()->GetSiteInstance();
  auto* grandchild_site_instance =
      grandchild->current_frame_host()->GetSiteInstance();
  EXPECT_NE(child_site_instance, grandchild_site_instance);
  EXPECT_NE(child_site_instance->GetProcess(),
            grandchild_site_instance->GetProcess());
  EXPECT_NE(child_site_instance->GetSiteInfo().unique_sandbox_id(),
            grandchild_site_instance->GetSiteInfo().unique_sandbox_id());
}

// Verify same-document navigations in a sandboxed iframe stay in the same
// SiteInstance, and that the unique_sandbox_id changes for any
// non-same-document navigation.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerDocumentIsolatedSandboxedIframeTest,
                       SandboxedIframeNavigations) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to be same-site to the parent.
  GURL same_site_child_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        same_site_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstanceImpl> root_site_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(root_site_instance->GetSiteInfo().is_sandboxed());

  scoped_refptr<SiteInstanceImpl> child_site_instance1 =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child->effective_frame_policy().sandbox_flags);
  EXPECT_NE(root_site_instance, child_site_instance1);
  EXPECT_TRUE(child_site_instance1->GetSiteInfo().is_sandboxed());

  // Navigate child same-site, same-origin, same-document.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.getElementById('test_frame'); "
        "frame.src = '%s#foo';",
        same_site_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  scoped_refptr<SiteInstanceImpl> child_site_instance1a =
      child->current_frame_host()->GetSiteInstance();
  // Since the sandboxed iframe is navigated same-document, we expect the
  // SiteInstance to remain the same.
  EXPECT_EQ(child_site_instance1, child_site_instance1a);

  // Navigate child same-site, same-origin, cross-document.
  GURL same_site_child_url2(
      embedded_test_server()->GetURL("a.com", "/title2.html"));
  ASSERT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test_frame",
                                  same_site_child_url2));
  scoped_refptr<SiteInstanceImpl> child_site_instance2 =
      child->current_frame_host()->GetSiteInstance();
  // Since the sandboxed iframe is navigated same-site but to a different
  // document, we expect the SiteInstance to change.
  EXPECT_NE(child_site_instance1, child_site_instance2);
  EXPECT_NE(child_site_instance1->GetSiteInfo().unique_sandbox_id(),
            child_site_instance2->GetSiteInfo().unique_sandbox_id());

  // Navigate child cross-site.
  GURL cross_site_child_url(
      embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test_frame",
                                  cross_site_child_url));
  scoped_refptr<SiteInstanceImpl> child_site_instance3 =
      child->current_frame_host()->GetSiteInstance();
  // Since the sandboxed iframe is navigated same-site, but cross-document
  // we expect the SiteInstance to change.
  EXPECT_NE(child_site_instance1, child_site_instance3);
  EXPECT_NE(child_site_instance1->GetSiteInfo().unique_sandbox_id(),
            child_site_instance3->GetSiteInfo().unique_sandbox_id());
}

// Verify that a sandboxed iframe with an about:blank subframe shares its
// SiteInstance with that subframe. Further, if the about:blank subframe
// navigates cross-site, it gets a new SiteInstance.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerDocumentIsolatedSandboxedIframeTest,
                       SandboxedAboutBlankSubframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to be same-site to the parent.
  GURL same_site_child_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create sandboxed child frame.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        same_site_child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame-tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);

  // Add about:blank subframe to child. Verify that it stays in its parent's
  // SiteInstance.
  {
    std::string js_str =
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.src = 'about:blank'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(child->current_frame_host(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(1U, child->child_count());
  FrameTreeNode* grandchild = child->child_at(0);

  scoped_refptr<SiteInstanceImpl> child_site_instance =
      child->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> grandchild_site_instance1 =
      grandchild->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child_site_instance, grandchild_site_instance1);

  // Navigate the grandchild same-site but cross-document and verify it gets a
  // new sandboxing id (and therefore a new SiteInstance).
  GURL cross_document_child_url(
      embedded_test_server()->GetURL("a.com", "/title2.html"));
  ASSERT_TRUE(NavigateToURLFromRenderer(grandchild, cross_document_child_url));
  scoped_refptr<SiteInstanceImpl> grandchild_site_instance2 =
      grandchild->current_frame_host()->GetSiteInstance();
  EXPECT_NE(child_site_instance, grandchild_site_instance2);
  EXPECT_NE(child_site_instance->GetSiteInfo().unique_sandbox_id(),
            grandchild_site_instance2->GetSiteInfo().unique_sandbox_id());
}

// Test to verify that sibling srcdoc sandboxed iframes are placed in separate
// SiteInstances in the per-document grouping model.
IN_PROC_BROWSER_TEST_P(SitePerProcessPerDocumentIsolatedSandboxedIframeTest,
                       SiblingSrcdocIframesGetDifferentProcesses) {
  // Create any main frame.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create two identical sibling srcdoc sandboxed iframes.
  // Create sandboxed child frame, with srcdoc content.
  {
    std::string js_str =
        "var frame1 = document.createElement('iframe'); "
        "frame1.sandbox = ''; "
        "frame1.srcdoc = 'srcdoc sandboxed subframe1'; "
        "var frame2 = document.createElement('iframe'); "
        "frame2.sandbox = ''; "
        "frame2.srcdoc = 'srcdoc sandboxed subframe2'; "
        "document.body.appendChild(frame1); "
        "document.body.appendChild(frame2);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Check frame tree.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* child1 = root->child_at(0);  // frame1
  FrameTreeNode* child2 = root->child_at(1);  // frame2
  auto* root_site_instance = root->current_frame_host()->GetSiteInstance();
  auto* child1_site_instance = child1->current_frame_host()->GetSiteInstance();
  auto* child2_site_instance = child2->current_frame_host()->GetSiteInstance();

  EXPECT_FALSE(root_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child1->effective_frame_policy().sandbox_flags);
  EXPECT_TRUE(child1_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            child2->effective_frame_policy().sandbox_flags);
  EXPECT_TRUE(child2_site_instance->GetSiteInfo().is_sandboxed());
  EXPECT_NE(root_site_instance, child1_site_instance);
  EXPECT_NE(root_site_instance, child2_site_instance);
  // Verify siblings have different SiteInstances and processes.
  EXPECT_NE(child1_site_instance, child2_site_instance);
  EXPECT_NE(child1_site_instance->GetSiteInfo().unique_sandbox_id(),
            child2_site_instance->GetSiteInfo().unique_sandbox_id());
  EXPECT_NE(child1_site_instance->GetProcess(),
            child2_site_instance->GetProcess());
}

// Test that changes to an iframe's srcdoc attribute propagate through the
// browser and are stored/cleared on the RenderFrameHost as needed.
IN_PROC_BROWSER_TEST_P(SrcdocIsolatedSandboxedIframeTest, SrcdocIframe) {
  StartEmbeddedServer();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create srcdoc iframe.
  {
    std::string js_str =
        "const frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.srcdoc = 'srcdoc test content'; "
        "document.body.append(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Verify content on RenderFrameHost.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(GURL(url::kAboutSrcdocURL), child->current_url());
  EXPECT_EQ("srcdoc test content", child->srcdoc_value());
  if (SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()) {
    EXPECT_EQ(main_url, GetFrameBaseUrl(child->parent()));
  }
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));

  // Reset the srcdoc attribute, and verify the FrameTreeNode is updated
  // accordingly.
  {
    std::string js_str =
        "const frame = document.getElementById('test_frame'); "
        "frame.removeAttribute('srcdoc');";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
    // The next line serves two purposes. First, it confirms via JS that the
    // srcdoc attribute has indeed been removed. Secondly, and more importantly,
    // it synchronizes the mojo pipe where the two DidChangeSrcDoc calls occur;
    // the first call sets the srcdoc value to '' and the second call removes
    // the BaseUrl. Waiting for loadstop is insufficient to catch the second
    // call.
    EXPECT_EQ(
        false,
        EvalJs(shell(),
               "document.getElementById('test_frame').hasAttribute('srcdoc')"));
  }
  EXPECT_EQ(GURL(url::kAboutBlankURL), child->current_url());
  EXPECT_EQ("", child->srcdoc_value());
  // The base url is set on the parent, and not cleared with the child's srcdoc
  // information.
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Repeat the srcdoc attribute tests from above, but this time using
  // src='about:srcdoc' to make the frame srcdoc.

  {
    std::string js_str =
        "const frame = document.createElement('iframe'); "
        "frame.id = 'test_frame2'; "
        "frame.src = 'about:srcdoc'; "
        "document.body.append(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child2 = root->child_at(1);
  EXPECT_EQ(GURL(url::kAboutSrcdocURL), child2->current_url());
  EXPECT_EQ("", child2->srcdoc_value());
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, GetFrameBaseUrl(child2->parent()));
  EXPECT_EQ(main_url, GetFrameBaseUrl(child2->current_frame_host()));
  EXPECT_EQ(main_url, child2->current_frame_host()->GetInheritedBaseUrl());

  // Reset the src attribute, and verify the FrameTreeNode is updated
  // accordingly.
  {
    std::string js_str =
        "const frame = document.getElementById('test_frame2'); "
        "frame.removeAttribute('src');";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
    // The next line serves two purposes. First, it confirms via JS that the
    // srcdoc attribute has indeed been removed. Secondly, and more importantly,
    // it synchronizes the mojo pipe where the two DidChangeSrcDoc calls occur;
    // the first call sets the srcdoc value to '' and the second call removes
    // the BaseUrl. Waiting for loadstop is insufficient to catch the second
    // call.
    EXPECT_EQ(
        false,
        EvalJs(shell(),
               "document.getElementById('test_frame').hasAttribute('src')"));
  }
  EXPECT_EQ(GURL(url::kAboutBlankURL), child2->current_url());
  EXPECT_EQ("", child2->srcdoc_value());
  EXPECT_EQ(GURL("about:blank"), child->current_url());
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());
}

// Test that when a frame changes its base url by manipulating its
// base-element, and then undoes those changes, that the browser is properly
// notified.
IN_PROC_BROWSER_TEST_P(SrcdocIsolatedSandboxedIframeTest, FrameChangesBaseUrl) {
  StartEmbeddedServer();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root_ftn =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Initially we don't expect the baseurl value to have been sent from the
  // renderer.
  EXPECT_EQ(main_url, GetFrameBaseUrl(root_ftn->current_frame_host()));
  EXPECT_EQ(GURL(), root_ftn->current_frame_host()->GetInheritedBaseUrl());

  // The page modifies its base element to set a non-standard value the browser
  // knows nothing about, so the renderer sends it to the browser.
  {
    std::string js_str =
        "const base_element = document.createElement('base'); "
        "base_element.id = 'base_element'; "
        "base_element.href = 'http://foo.com'; "
        "document.head.append(base_element);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    // The following JS is useful, but also forces synchronization on the mojo
    // pipe that sends the srcdoc base url data.
    EXPECT_EQ(
        true,
        EvalJs(shell(),
               "document.getElementById('base_element').hasAttribute('href')"));
  }
  GURL foo_url("http://foo.com");
  EXPECT_EQ(foo_url, GetFrameBaseUrl(root_ftn->current_frame_host()));
  EXPECT_EQ(GURL(), root_ftn->current_frame_host()->GetInheritedBaseUrl());

  // The page removes its base element, restoring the standard baseurl value.
  // The previous value sent to the browser should be reset.
  {
    EXPECT_TRUE(ExecJs(shell(), "document.querySelector('base').remove();"));
    // The following JS is useful, but also forces synchronization on the mojo
    // pipe that sends the srcdoc base url data.
    EXPECT_EQ(true,
              EvalJs(shell(),
                     "document.getElementById('base_element') == undefined"));
  }
  EXPECT_EQ(main_url, GetFrameBaseUrl(root_ftn->current_frame_host()));
  EXPECT_EQ(GURL(), root_ftn->current_frame_host()->GetInheritedBaseUrl());
}

// A test to make sure that a sandboxed srcdoc iframe correctly updates its
// base url with the <base> element, and restores the snapshotted base url from
// the parent if it removes its <base> element.
IN_PROC_BROWSER_TEST_P(SrcdocIsolatedSandboxedIframeTest,
                       SandboxedSrcdocIframeAddsRemovesBaseUrl) {
  StartEmbeddedServer();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create srcdoc iframe with base url from a.com.
  {
    std::string js_str =
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  auto* child = root->child_at(0);
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, GetFrameBaseUrl(root->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Srcdoc frame changes its base url.
  GURL b_url("http://b.com/");
  {
    std::string js_str = base::StringPrintf(
        "var base_element = document.createElement('base'); "
        "base_element.href = '%s'; "
        "document.head.appendChild(base_element);",
        b_url.spec().c_str());
    EXPECT_TRUE(ExecJs(child, js_str));
    EXPECT_EQ(b_url, GetFrameBaseUrl(child->current_frame_host()));
  }
  EXPECT_EQ(main_url, GetFrameBaseUrl(root->current_frame_host()));
  EXPECT_EQ(b_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Root frame adds base element.
  GURL c_url("http://c.com/");
  {
    std::string js_str = base::StringPrintf(
        "var base_element = document.createElement('base'); "
        "base_element.href = '%s'; "
        "document.head.appendChild(base_element);",
        c_url.spec().c_str());
    EXPECT_TRUE(ExecJs(root, js_str));
    EXPECT_EQ(c_url, GetFrameBaseUrl(root->current_frame_host()));
  }
  EXPECT_EQ(b_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(c_url, GetFrameBaseUrl(root->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  // The srcdoc removes its base element.
  {
    EXPECT_TRUE(ExecJs(child, "document.querySelector('base').remove();"));
    if (SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()) {
      EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
    }
    EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
    EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());
  }
  EXPECT_EQ(c_url, GetFrameBaseUrl(root->current_frame_host()));
}

// Test that when a sandboxed srcdoc iframe's parent changes its base url, the
// srcdoc continues to use the original base url until it reloads.
IN_PROC_BROWSER_TEST_P(SrcdocIsolatedSandboxedIframeTest,
                       SrcdocParentChangesBaseUrl) {
  StartEmbeddedServer();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL b_url("http://b.com/");
  {
    std::string js_str = base::StringPrintf(
        "var base_element = document.createElement('base'); "
        "base_element.href = '%s'; "
        "document.head.appendChild(base_element);",
        b_url.spec().c_str());
    EXPECT_TRUE(ExecJs(root, js_str));
    EXPECT_EQ(b_url, GetFrameBaseUrl(root->current_frame_host()));
  }

  // Create srcdoc iframe inheriting a base url of b.com.
  {
    std::string js_str =
        "var frame = document.createElement('iframe'); "
        "frame.id = 'child-srcdoc'; "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  auto* child = root->child_at(0);
  EXPECT_EQ(b_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(b_url, GetFrameBaseUrl(root->current_frame_host()));
  EXPECT_EQ(b_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Remove base element from root.
  EXPECT_TRUE(ExecJs(root, "document.querySelector('base').remove();"));
  EXPECT_EQ(main_url, GetFrameBaseUrl(root->current_frame_host()));
  EXPECT_EQ(b_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(b_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Reload child. Since the child is initiating the reload, it should reload
  // with the same base url it had before the reload.
  {
    EXPECT_TRUE(ExecJs(child, "location.reload();"));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  EXPECT_EQ(b_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(b_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Have the parent initiate the reload. This time the parent's original url
  // should be sent to the child as its base url.
  {
    EXPECT_TRUE(ExecJs(shell(),
                       "var frame = document.getElementById('child-srcdoc'); "
                       "frame.srcdoc = frame.srcdoc;"));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());
}

// A test to verify that the base url stored in RFHI for an about:srcdoc frame
// is cleared when the frame navigates to a non-srcdoc/blank url.
IN_PROC_BROWSER_TEST_P(SrcdocIsolatedSandboxedIframeTest,
                       InheritedBaseUrlClearedOnNavigation) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL child_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    std::string js_str =
        "var frame = document.createElement('iframe'); "
        "frame.id = 'child-srcdoc'; "
        "frame.srcdoc = 'foo'; "
        "document.body.appendChild(frame);";
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  auto* child = root->child_at(0);
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Remove the srcdoc attribute from the child frame. This should trigger a
  // navigation to about:blank.
  {
    EXPECT_TRUE(ExecJs(shell(),
                       "var frame = document.getElementById('child-srcdoc'); "
                       "frame.removeAttribute('srcdoc'); "));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  EXPECT_EQ(GURL("about:blank"),
            child->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(main_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  // Navigate the subframe to `child_url`. This should remove the inherited base
  // URL.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.getElementById('child-srcdoc'); "
        "frame.src = '%s';",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(shell(), js_str));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  EXPECT_EQ(child_url, child->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(child_url, GetFrameBaseUrl(child->current_frame_host()));
  EXPECT_EQ(GURL(), child->current_frame_host()->GetInheritedBaseUrl());
}

// A test to verify the initial stages of the initiator base url plumbing work.
// The test verifies the value propagates as far as NavigationRequest and
// FrameNavigationEntry.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, VerifyBaseUrlPlumbing) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameNavigationEntry* root_frame_entry =
      root->current_frame_host()->last_committed_frame_entry();
  ASSERT_TRUE(root_frame_entry);
  EXPECT_FALSE(root_frame_entry->initiator_base_url().has_value());

  // Create srcdoc iframe. Verify the baseurl is plumbed as far as the
  // FrameNavigationEntry.
  {
    std::string js_str =
        "const frm = document.createElement('iframe'); "
        "frm.srcdoc = 'foo'; "
        "document.body.appendChild(frm); ";
    EXPECT_TRUE(ExecJs(shell(), js_str));
  }
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());

  FrameNavigationEntry* child_frame_entry =
      child->current_frame_host()->last_committed_frame_entry();
  ASSERT_TRUE(child_frame_entry);
  ASSERT_TRUE(child_frame_entry->initiator_base_url().has_value());
  EXPECT_EQ(main_url, child_frame_entry->initiator_base_url().value());

  // Create about:blank iframe. Verify the baseurl is plumbed as far as the
  // FrameNavigationEntry.
  {
    std::string js_str =
        "const frm = document.createElement('iframe'); "
        "frm.src = 'about:blank'; "
        "document.body.appendChild(frm); ";
    EXPECT_TRUE(ExecJs(shell(), js_str));
  }
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ASSERT_EQ(2U, root->child_count());
  child = root->child_at(1);
  EXPECT_EQ(main_url, child->current_frame_host()->GetInheritedBaseUrl());
  child_frame_entry = child->current_frame_host()->last_committed_frame_entry();

  ASSERT_TRUE(child_frame_entry);
  ASSERT_TRUE(child_frame_entry->initiator_base_url().has_value());
  EXPECT_EQ(main_url, child_frame_entry->initiator_base_url().value());

  // Renderer-initiated navigation of the top-level frame to about:blank; there
  // should be an initiator base url.
  EXPECT_TRUE(ExecJs(shell(), "location = 'about:blank';"));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  root_frame_entry = root->current_frame_host()->last_committed_frame_entry();
  ASSERT_TRUE(root_frame_entry);
  ASSERT_TRUE(root_frame_entry->initiator_base_url().has_value());
  EXPECT_EQ(main_url, root_frame_entry->initiator_base_url().value());

  // Browser-initiated navigation of the top-level frame to about:blank; there
  // should be no initiator base url.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  root_frame_entry = root->current_frame_host()->last_committed_frame_entry();
  ASSERT_TRUE(root_frame_entry);
  EXPECT_FALSE(root_frame_entry->initiator_base_url().has_value());
  EXPECT_EQ(GURL(), root->current_frame_host()->GetInheritedBaseUrl());
}

// This test verifies that a renderer process doesn't crash if a srcdoc calls
// document.write on a mainframe parent.
IN_PROC_BROWSER_TEST_F(BaseUrlInheritanceIframeTest, SrcdocWritesMainFrame) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Create srcdoc child.
  EXPECT_TRUE(ExecJs(root,
                     "var frm = document.createElement('iframe'); "
                     "frm.srcdoc = 'foo'; "
                     "document.body.appendChild(frm);"));
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Have the srcdoc child call document.write on the mainframe-parent.
  std::string test_str("test-complete");
  // Since having the child write the parent's document will delete the child,
  // we use setTimeout to ensure ExecJS returns true, and then wait for the
  // child's RenderFrameHost to be deleted so we know that the write has
  // completed. Note: the child's subframe exiting does not mean that its
  // process, which it shares with the parent, has exited.
  RenderFrameDeletedObserver observer(child->current_frame_host());
  EXPECT_TRUE(ExecJs(
      child, JsReplace("setTimeout(() => { parent.document.write($1); }, 100);",
                       test_str)));
  observer.WaitUntilDeleted();

  // But fortunately `root` is still valid.
  EXPECT_EQ(test_str, EvalJs(root, "document.body.innerText").ExtractString());
  // If we get here without a crash, we've passed.
}

// A test to verify that a new about:blank mainframe inherits its base url
// from its initiator.
IN_PROC_BROWSER_TEST_F(BaseUrlInheritanceIframeTest, PopupsInheritBaseUrl) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "var w = window.open()"));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  ASSERT_NE(new_contents, shell()->web_contents());

  // The popup should get the same base URL as its initiator.
  FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
  EXPECT_EQ(EvalJs(root, "document.baseURI").ExtractString(),
            EvalJs(new_root, "document.baseURI").ExtractString());
}

IN_PROC_BROWSER_TEST_F(BaseUrlInheritanceIframeTest,
                       AboutBlankInheritsBaseUrlFromSiblingInitiator) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Create siblings.
  EXPECT_TRUE(ExecJs(root,
                     "var frm = document.createElement('iframe'); "
                     "frm.src = 'about:blank'; "
                     "frm.id = 'frm1'; "
                     "document.body.appendChild(frm);"));
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child1 = root->child_at(0);

  EXPECT_TRUE(ExecJs(root,
                     "var frm = document.createElement('iframe'); "
                     "frm.id = 'frm2'; "
                     "document.body.appendChild(frm);"));
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child2 = root->child_at(1);

  // First child navigates to about:blank on second child.
  EXPECT_TRUE(ExecJs(child1,
                     "var base = document.createElement('base'); "
                     "base.href = 'https://example.com'; "
                     "document.head.appendChild(base); "
                     "window.top.window[1].location.href = 'about:blank';"));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Make sure second child inherited base url from the first child.
  EXPECT_EQ(GURL("https://example.com"),
            GetFrameBaseUrl(child2->current_frame_host()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(
    All,
    OriginKeyedProcessIsolatedSandboxedIframeTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "OriginKeyedProcessesByDefault_enabled"
                        : "OriginKeyedProcessesByDefault_disabled";
    });
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessNotIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessPerOriginIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(
    All,
    SitePerProcessIsolatedSandboxWithoutStrictSiteIsolationBrowserTest,
    testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessPerDocumentIsolatedSandboxedIframeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SrcdocIsolatedSandboxedIframeTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "isolated" : "non_isolated";
                         });

}  // namespace content
