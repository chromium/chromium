// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class AncestorThrottleTest : public ContentBrowserTest,
                             public ::testing::WithParamInterface<bool> {
 public:
  AncestorThrottleTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          network::features::kOutOfBlinkFrameAncestors);
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that an iframe navigation with frame-anecestors 'none' is blocked.
IN_PROC_BROWSER_TEST_P(AncestorThrottleTest, FailedCSP) {
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
  EXPECT_TRUE(web_contents->GetFrameTree()
                  ->root()
                  ->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
}

// Tests that redirecting on a forbidden frame-ancestors will still commit if
// the final response does not have a CSP policy that prevents the navigation.
IN_PROC_BROWSER_TEST_P(AncestorThrottleTest, RedirectCommitsIfNoCSP) {
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
  EXPECT_FALSE(web_contents->GetFrameTree()
                   ->root()
                   ->child_at(0)
                   ->current_frame_host()
                   ->GetLastCommittedOrigin()
                   .opaque());
}

// Tests that redirecting on a forbidden frame-ancestors will not commit if
// the final response does have a CSP policy that prevents the navigation.
IN_PROC_BROWSER_TEST_P(AncestorThrottleTest, RedirectFails) {
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
  EXPECT_TRUE(web_contents->GetFrameTree()
                  ->root()
                  ->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
}

INSTANTIATE_TEST_SUITE_P(, AncestorThrottleTest, ::testing::Bool());

}  // namespace

}  // namespace content
