// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"

#import <CoreGraphics/CoreGraphics.h>

#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

typedef WebTestWithWebState ContextMenuJavaScriptFeatureTest;

TEST_F(ContextMenuJavaScriptFeatureTest, FetchLinkElement) {
  NSString* html =
      @"<html><head>"
       "<style>body { font-size:14em; }</style>"
       "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
       "</head><body><p><a href=\"http://destination/\"> "
       "link</a></p></body></html>";
  LoadHtml(html);

  std::string request_id("123");

  __block bool callback_called = false;
  ContextMenuJavaScriptFeature::FromBrowserState(GetBrowserState())
      ->GetElementAtPoint(
          web_state(), request_id, CGPointMake(10.0, 10.0),
          base::BindOnce(^(const std::string& callback_request_id,
                           const web::ContextMenuParams& params) {
            EXPECT_EQ(request_id, callback_request_id);
            EXPECT_EQ(true, params.is_main_frame);
            EXPECT_NSEQ(@"link", params.text);
            EXPECT_NSEQ(nil, params.title_attribute);
            EXPECT_NSEQ(nil, params.alt_text);
            EXPECT_EQ("http://destination/", params.link_url.spec());
            EXPECT_EQ("", params.src_url.spec());
            callback_called = true;
          }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return callback_called;
  }));
}

TEST_F(ContextMenuJavaScriptFeatureTest, FetchImageElement) {
  net::EmbeddedTestServer test_server;
  test_server.ServeFilesFromSourceDirectory(
      base::FilePath("ios/testing/data/http_server_files/"));
  ASSERT_TRUE(test_server.Start());

  NSString* html =
      @"<html><head>"
       "<style>body { font-size:14em; }</style>"
       "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
       "</head><body><p><a href=\"http://destination/\"><img title=\"MyTitle\" "
       "alt=\"alt text\" height=100 width=100 "
       "src=\"chromium_logo.png\"/></a></p></body></html>";
  LoadHtml(html, test_server.base_url());

  std::string request_id("123");
  std::string expected_src_url =
      test_server.GetURL("/chromium_logo.png").spec();

  __block bool callback_called = false;
  ContextMenuJavaScriptFeature::FromBrowserState(GetBrowserState())
      ->GetElementAtPoint(
          web_state(), request_id, CGPointMake(10.0, 10.0),
          base::BindOnce(^(const std::string& callback_request_id,
                           const web::ContextMenuParams& params) {
            EXPECT_EQ(request_id, callback_request_id);
            EXPECT_EQ(true, params.is_main_frame);
            EXPECT_NSEQ(nil, params.text);
            EXPECT_NSEQ(@"MyTitle", params.title_attribute);
            EXPECT_NSEQ(@"alt text", params.alt_text);
            EXPECT_EQ("http://destination/", params.link_url.spec());
            EXPECT_EQ(expected_src_url, params.src_url.spec());
            callback_called = true;
          }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return callback_called;
  }));
}

}  // namespace web
