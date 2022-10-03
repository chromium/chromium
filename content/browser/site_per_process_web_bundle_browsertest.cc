// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/render_document_feature.h"

namespace content {

class SitePerProcessWebBundleBrowserTest
    : public SitePerProcessIgnoreCertErrorsBrowserTest {
 public:
  SitePerProcessWebBundleBrowserTest() = default;
  void SetUpOnMainThread() override {
    SitePerProcessIgnoreCertErrorsBrowserTest::SetUpOnMainThread();
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
  }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{
      net::EmbeddedTestServer::Type::TYPE_HTTPS};
};

// Check that a uuid-in-package: subframe instantiated from a same-origin
// WebBundle reuses its parent's process.
IN_PROC_BROWSER_TEST_P(SitePerProcessWebBundleBrowserTest, SameSiteBundle) {
  GURL bundle_url(
      https_server()->GetURL("foo.test", "/web_bundle/uuid-in-package.wbn"));
  GURL frame_url("uuid-in-package:429fcc4e-0696-4bad-b099-ee9175f023ae");
  GURL main_url(https_server()->GetURL(
      "foo.test", "/web_bundle/frame_parent.html?wbn=" + bundle_url.spec() +
                      "&frame=" + frame_url.spec()));
  std::u16string expected_title(u"OK");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_EQ(child_node->current_url(), frame_url);
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());
}

// Check that a uuid-in-package: subframe instantiated from a WebBundle gets a
// process for the Bundle's origin.
IN_PROC_BROWSER_TEST_P(SitePerProcessWebBundleBrowserTest, CrossSiteBundle) {
  GURL bundle_url(
      https_server()->GetURL("bar.test", "/web_bundle/uuid-in-package.wbn"));
  GURL frame_url("uuid-in-package:429fcc4e-0696-4bad-b099-ee9175f023ae");
  GURL main_url(https_server()->GetURL(
      "foo.test", "/web_bundle/frame_parent.html?wbn=" + bundle_url.spec() +
                      "&frame=" + frame_url.spec()));
  std::u16string expected_title(u"OK");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_EQ(child_node->current_url(), frame_url);
  url::Origin last_committed_origin =
      child_node->current_frame_host()->GetLastCommittedOrigin();
  EXPECT_TRUE(last_committed_origin.opaque());
  const url::SchemeHostPort& tuple =
      last_committed_origin.GetTupleOrPrecursorTupleIfOpaque();
  EXPECT_EQ("bar.test", tuple.host());

  // An iframe nested in the uuid-in-package: iframe gets a non-opaque origin.
  GURL c_url = https_server()->GetURL("c.test", "/title1.html");
  TestNavigationObserver observer(c_url);
  observer.WatchExistingWebContents();

  // Create the subframe now.
  std::string create_frame_script = base::StringPrintf(
      "var new_iframe = document.createElement('iframe');"
      "new_iframe.src = '%s';"
      "document.body.appendChild(new_iframe);",
      c_url.spec().c_str());
  EXPECT_TRUE(ExecJs(child_node, create_frame_script));

  observer.WaitForNavigationFinished();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  ASSERT_EQ(1U, child_node->child_count());
  FrameTreeNode* grandchild_node = child_node->child_at(0);
  url::Origin grandchild_committed_origin =
      grandchild_node->current_frame_host()->GetLastCommittedOrigin();
  EXPECT_FALSE(grandchild_committed_origin.opaque());
  const url::SchemeHostPort& c_tuple =
      grandchild_committed_origin.GetTupleOrPrecursorTupleIfOpaque();
  EXPECT_EQ("c.test", c_tuple.host());
  EXPECT_FALSE(
      last_committed_origin.IsSameOriginWith(grandchild_committed_origin));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = https://foo.test/\n"
      "      B = https://bar.test/\n"
      "      C = https://c.test/",
      DepictFrameTree(root));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessWebBundleBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

}  // namespace content
