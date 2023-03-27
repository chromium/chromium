// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_PER_PROCESS_BROWSERTEST_H_
#define CONTENT_BROWSER_SITE_PER_PROCESS_BROWSERTEST_H_

#include <string>

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;

class SitePerProcessBrowserTestBase : public ContentBrowserTest {
 public:
  SitePerProcessBrowserTestBase();

  SitePerProcessBrowserTestBase(const SitePerProcessBrowserTestBase&) = delete;
  SitePerProcessBrowserTestBase& operator=(
      const SitePerProcessBrowserTestBase&) = delete;

 protected:
  std::string DepictFrameTree(FrameTreeNode* node);

  std::string WaitForMessageScript(const std::string& result_expression);

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  static void ForceUpdateViewportIntersection(
      FrameTreeNode* frame_tree_node,
      const blink::mojom::ViewportIntersectionState& intersection_state);

  void RunPostedTasks();

 private:
  FrameTreeVisualizer visualizer_;
  base::test::ScopedFeatureList feature_list_;
};

class SitePerProcessBrowserTest
    : public SitePerProcessBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  SitePerProcessBrowserTest();

  SitePerProcessBrowserTest(const SitePerProcessBrowserTest&) = delete;
  SitePerProcessBrowserTest& operator=(const SitePerProcessBrowserTest&) =
      delete;

  std::string GetExpectedOrigin(const std::string& host);

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SitePerProcessIgnoreCertErrorsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessIgnoreCertErrorsBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_PER_PROCESS_BROWSERTEST_H_
