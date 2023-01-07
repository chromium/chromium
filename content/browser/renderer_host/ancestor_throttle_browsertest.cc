// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

base::FilePath TestFilePath(const char* filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return GetTestFilePath("", filename);
}

class AncestorThrottleTest : public ContentBrowserTest,
                             public ::testing::WithParamInterface<bool> {
 public:
  AncestorThrottleTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that an iframe navigation with frame-ancestors 'none' is blocked.
IN_PROC_BROWSER_TEST_F(AncestorThrottleTest, FailedCSP) {
  GURL parent_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  GURL iframe_url(
      embedded_test_server()->GetURL("foo.com", "/frame-ancestors-none.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(web_contents, parent_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test_iframe", iframe_url));

  // Check that we have an opaque origin, since the frame was blocked.
  // TODO(lfg): We can't check last_navigation_succeded because of
  // https://crbug.com/1000804.
  EXPECT_TRUE(web_contents->GetPrimaryFrameTree()
                  .root()
                  ->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
}

// Tests that X-Frame-Options is ignored if frame-ancestors is specified.
IN_PROC_BROWSER_TEST_F(AncestorThrottleTest, XFOAndCSPFrameAncestors) {
  GURL parent_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "foo.com",
      "/set-header?"
      "Content-Security-Policy: frame-ancestors *&"
      "X-Frame-Options: DENY"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(web_contents, parent_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test_iframe", iframe_url));

  // Check that we do not have an opaque origin, since the frame was allowed.
  // TODO(lfg): We can't check last_navigation_succeded because of
  // https://crbug.com/1000804.
  EXPECT_FALSE(web_contents->GetPrimaryFrameTree()
                   .root()
                   ->child_at(0)
                   ->current_frame_host()
                   ->GetLastCommittedOrigin()
                   .opaque());
}

// Tests that redirecting on a forbidden frame-ancestors will still commit if
// the final response does not have a CSP policy that prevents the navigation.
IN_PROC_BROWSER_TEST_F(AncestorThrottleTest, RedirectCommitsIfNoCSP) {
  GURL parent_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  GURL iframe_url(
      embedded_test_server()->GetURL("foo.com", "/redirect301-csp-to-http"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(web_contents, parent_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test_iframe", iframe_url));

  // Check that we don't have an opaque origin, since the frame should have
  // loaded.
  // TODO(lfg): We can't check last_navigation_succeded because of
  // https://crbug.com/1000804.
  EXPECT_FALSE(web_contents->GetPrimaryFrameTree()
                   .root()
                   ->child_at(0)
                   ->current_frame_host()
                   ->GetLastCommittedOrigin()
                   .opaque());
}

// Tests that redirecting on a forbidden frame-ancestors will not commit if
// the final response does have a CSP policy that prevents the navigation.
IN_PROC_BROWSER_TEST_F(AncestorThrottleTest, RedirectFails) {
  GURL parent_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "foo.com", "/redirect301-csp-to-frame-ancestors-none"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(web_contents, parent_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test_iframe", iframe_url));

  // Check that we have an opaque origin, since the frame was blocked.
  // TODO(lfg): We can't check last_navigation_succeded because of
  // https://crbug.com/1000804.
  EXPECT_TRUE(web_contents->GetPrimaryFrameTree()
                  .root()
                  ->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
}

// Check that we don't process CSP for 204 responses.
IN_PROC_BROWSER_TEST_F(AncestorThrottleTest, Response204CSP) {
  GURL parent_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "foo.com", "/response204-csp-frame-ancestors-none"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(web_contents, parent_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test_iframe", iframe_url));

  // Not crashing means that the test succeeded.
}

// Tests iframes embedded by local files.
IN_PROC_BROWSER_TEST_F(AncestorThrottleTest, FrameAncestorsFileURLs) {
  struct {
    const char* csp;
    bool expect_allowed;
  } testCases[]{
      {"frame-ancestors 'none'", false},
      {"frame-ancestors file:", true},
      {"frame-ancestors 'self'", false},
  };

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL parent_url =
      net::FilePathToFileURL(TestFilePath("page_with_iframe.html"));
  for (const auto& testCase : testCases) {
    GURL iframe_url(embedded_test_server()->GetURL(
        "foo.com",
        base::StringPrintf("/set-header?Content-Security-Policy: %s;",
                           testCase.csp)));

    EXPECT_TRUE(NavigateToURL(web_contents, parent_url));
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test_iframe", iframe_url));

    // Check that we have an opaque origin, since the frame was blocked.
    // TODO(lfg): We can't check last_navigation_succeded because of
    // https://crbug.com/1000804.
    bool is_opaque = web_contents->GetPrimaryFrameTree()
                         .root()
                         ->child_at(0)
                         ->current_frame_host()
                         ->GetLastCommittedOrigin()
                         .opaque();
    EXPECT_EQ(is_opaque, !(testCase.expect_allowed));
  }
}

class AncestorThrottleSXGTest : public AncestorThrottleTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AncestorThrottleTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUp() override {
    sxg_test_helper_.SetUp();
    AncestorThrottleTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    AncestorThrottleTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
    AncestorThrottleTest::TearDownOnMainThread();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    AncestorThrottleTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  ContentMockCertVerifier mock_cert_verifier_;
  SignedExchangeBrowserTestHelper sxg_test_helper_;
};

IN_PROC_BROWSER_TEST_F(AncestorThrottleSXGTest, SXGWithCSP) {
  sxg_test_helper_.InstallMockCert(mock_cert_verifier_.mock_cert_verifier());
  sxg_test_helper_.InstallMockCertChainInterceptor();

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("/page_with_iframe.html")));

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_csp.sxg");
  FrameTreeNode* iframe_node =
      web_contents->GetPrimaryFrameTree().root()->child_at(0);
  NavigateFrameToURL(iframe_node, url);
  EXPECT_TRUE(
      iframe_node->current_frame_host()->GetLastCommittedOrigin().opaque());
}

}  // namespace

}  // namespace content
