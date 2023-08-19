// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

namespace {

// A simple test page with generic content.
const char kDestinationPage[] = "You've arrived!";

// Template for a test page with META refresh tag. Required template arguments
// are: refresh time in seconds (integer) and destination URL for redirect
// (string).
const char kRefreshMetaPageTemplate[] =
    "<!DOCTYPE html>"
    "<html>"
    "  <head><meta HTTP-EQUIV='REFRESH' content='%lld;url=%s'></head>"
    "  <body></body>"
    "</html>";

const char kOriginRelativeUrl[] = "/origin";
const char kDestinationRelativeUrl[] = "/destination";

}  // namespace

using base::test::ios::kWaitForUIElementTimeout;

namespace web {

// Test fixture for integration tests involving page navigation triggered by
// meta-tag.
class MetaTagTest : public WebTestWithWebState,
                    public ::testing::WithParamInterface<base::TimeDelta> {
 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();
    std::string refresh_meta_page =
        base::StringPrintf(kRefreshMetaPageTemplate, GetParam().InSeconds(),
                           kDestinationRelativeUrl);
    server_.RegisterRequestHandler(base::BindRepeating(
        net::test_server::HandlePrefixedRequest, kOriginRelativeUrl,
        base::BindRepeating(::testing::HandlePageWithHtml, refresh_meta_page)));
    server_.RegisterDefaultHandler(
        base::BindRepeating(::testing::HandlePageWithHtml, kDestinationPage));
    ASSERT_TRUE(server_.Start());
  }

  net::test_server::EmbeddedTestServer server_;
};

// Tests that if a page contains <meta HTTP-EQUIV='REFRESH' content='time;url'>,
// the page will redirect to `url` after `time` seconds.
TEST_P(MetaTagTest, HttpEquivRefresh) {
  const GURL origin_url = server_.GetURL(kOriginRelativeUrl);
  const GURL destination_url = server_.GetURL(kDestinationRelativeUrl);

  test::LoadUrl(web_state(), origin_url);
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kDestinationPage, GetParam() + kWaitForUIElementTimeout));
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         MetaTagTest,
                         ::testing::Values(base::Seconds(1), base::Seconds(3)));

}  // namespace web
