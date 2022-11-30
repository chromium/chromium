// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/render_document_feature.h"

namespace content {

// A subclass of SitePerProcessIgnoreCertErrorsBrowsertest that disables mixed
// content autoupgrades.
// TODO(carlosil): Since the flag will be cleaned up eventually, this should be
// changed to proper plumbing that adds the relevant urls to the allowlist.
class SitePerProcessIgnoreCertErrorsAllowMixedContentBrowserTest
    : public SitePerProcessIgnoreCertErrorsBrowserTest {
 public:
  SitePerProcessIgnoreCertErrorsAllowMixedContentBrowserTest() {
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Tests that, when a parent frame is set to strictly block mixed
// content via Content Security Policy, child OOPIFs cannot display
// mixed content.
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       PassiveMixedContentInIframeWithStrictBlocking) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url_with_strict_blocking(https_server.GetURL(
      "/mixed-content/basic-passive-in-iframe-with-strict-blocking.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url_with_strict_blocking));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));

  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the subframe navigates, it should still be marked as enforcing
  // strict mixed content.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), navigate_url));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the main frame navigates, it should no longer be marked as
  // enforcing strict mixed content.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
}

// Tests that, when a parent frame is set to upgrade insecure requests
// via Content Security Policy, child OOPIFs will upgrade as well.
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       PassiveMixedContentInIframeWithUpgrade) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url_with_upgrade(https_server.GetURL(
      "/mixed-content/basic-passive-in-iframe-with-upgrade.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url_with_upgrade));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));

  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the subframe navigates, it should still be marked as upgrading
  // insecure requests.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), navigate_url));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the main frame navigates, it should no longer be marked as
  // upgrading insecure requests.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
}

// Tests that active mixed content is blocked in an OOPIF. The test
// ignores cert errors so that an HTTPS iframe can be loaded from a site
// other than localhost (the EmbeddedTestServer serves a certificate
// that is valid for localhost).
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       ActiveMixedContentInIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  GURL iframe_url(
      https_server.GetURL("/mixed-content/basic-active-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* mixed_child = root->child_at(0)->child_at(0);
  ASSERT_TRUE(mixed_child);
  // The child iframe attempted to create a mixed iframe; this should
  // have been blocked, so the mixed iframe should still be on the initial empty
  // document.
  EXPECT_TRUE(mixed_child->is_on_initial_empty_document());
}

// Tests that the WebContents is notified when passive mixed content is
// displayed in an OOPIF. The test ignores cert errors so that an HTTPS
// iframe can be loaded from a site other than localhost (the
// EmbeddedTestServer serves a certificate that is valid for localhost).
// This test crashes on Windows under Dr. Memory, see https://crbug.com/600942.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PassiveMixedContentInIframe DISABLED_PassiveMixedContentInIframe
#else
#define MAYBE_PassiveMixedContentInIframe PassiveMixedContentInIframe
#endif
IN_PROC_BROWSER_TEST_P(
    SitePerProcessIgnoreCertErrorsAllowMixedContentBrowserTest,
    MAYBE_PassiveMixedContentInIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url(
      https_server.GetURL("/mixed-content/basic-passive-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_INSECURE_CONTENT));

  // When the subframe navigates, the WebContents should still be marked
  // as having displayed insecure content.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), navigate_url));
  entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_INSECURE_CONTENT));

  // When the main frame navigates, it should no longer be marked as
  // displaying insecure content.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SitePerProcessIgnoreCertErrorsAllowMixedContentBrowserTest,
    testing::ValuesIn(RenderDocumentFeatureLevelValues()));

}  // namespace content
