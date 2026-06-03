// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"

#import <CoreGraphics/CoreGraphics.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
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
            EXPECT_EQ(url::Origin::Create(GURL("https://chromium.test/")),
                      params.frame_security_origin);
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
  url::Origin expected_main_origin =
      url::Origin::Create(test_server.base_url());

  __block bool callback_called = false;
  ContextMenuJavaScriptFeature::FromBrowserState(GetBrowserState())
      ->GetElementAtPoint(
          web_state(), request_id, CGPointMake(10.0, 10.0),
          base::BindOnce(^(const std::string& callback_request_id,
                           const web::ContextMenuParams& params) {
            EXPECT_EQ(request_id, callback_request_id);
            EXPECT_EQ(true, params.is_main_frame);
            EXPECT_EQ(expected_main_origin, params.frame_security_origin);
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

// Tests that `GetElementAtPoint` successfully extracts parameters from a link
// element located inside a subframe (iframe), and populated params are correct
// (e.g. is_main_frame is false, frame_id and frame_security_origin match).
TEST_F(ContextMenuJavaScriptFeatureTest, FetchLinkElementInsideIframe) {
  web_state()->GetView().frame = CGRectMake(0.0, 0.0, 250.0, 250.0);

  net::EmbeddedTestServer test_server;
  test_server.ServeFilesFromSourceDirectory(
      base::FilePath("ios/testing/data/http_server_files/"));
  ASSERT_TRUE(test_server.Start());

  GURL iframe_url = test_server.GetURL("/link.html");
  NSString* iframe_src = base::SysUTF8ToNSString(iframe_url.spec());

  NSString* html = [NSString
      stringWithFormat:
          @"<html><head>"
           "<style>body { font-size:14em; }</style>"
           "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
           "</head><body>"
           "<iframe id='iframe' src='%@' "
           "style='width:100px;height:100px;border:none;'></iframe>"
           "</body></html>",
          iframe_src];

  LoadHtml(html, test_server.base_url());

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    id iframe_exists =
        ExecuteJavaScript(@"document.getElementById('iframe') !== null");
    if (![iframe_exists boolValue]) {
      return NO;
    }
    id link_exists = ExecuteJavaScript(
        @"var iframe = document.getElementById('iframe');"
        @"iframe && iframe.contentDocument && "
        @"iframe.contentDocument.getElementById('link') !== null");
    return [link_exists boolValue];
  }));

  std::string request_id("123");

  __block web::WebFrame* iframe = nullptr;
  web::WebFramesManager* frames_manager =
      ContextMenuJavaScriptFeature::FromBrowserState(GetBrowserState())
          ->GetWebFramesManager(web_state());

  // Wait for the iframe to be registered in WebFramesManager.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
      if (!frame->IsMainFrame()) {
        iframe = frame;
        return YES;
      }
    }
    return NO;
  }));
  ASSERT_TRUE(iframe);
  std::string expected_iframe_id = iframe->GetFrameId();

  url::Origin expected_origin = url::Origin::Create(iframe_url);
  std::string expected_pony_url = test_server.GetURL("/pony.html").spec();

  // Retrieve the exact main frame coordinate of the link inside the iframe
  // dynamically.
  NSDictionary* tap_coords =
      ExecuteJavaScript(@"(function (){\n"
                        @"  var iframe = document.getElementById('iframe');\n"
                        @"  var iframe_rect = iframe.getBoundingClientRect();\n"
                        @"  var link_rect = "
                        @"iframe.contentDocument.getElementById('link')."
                        @"getBoundingClientRect();\n"
                        @"  return {\n"
                        @"    x: iframe_rect.left + window.pageXOffset + "
                        @"link_rect.left + link_rect.width / 2,\n"
                        @"    y: iframe_rect.top + window.pageYOffset + "
                        @"link_rect.top + link_rect.height / 2\n"
                        @"  };\n"
                        @"})();");
  ASSERT_TRUE(tap_coords);
  CGPoint tap_point =
      CGPointMake([tap_coords[@"x"] floatValue], [tap_coords[@"y"] floatValue]);

  __block bool callback_called = false;
  ContextMenuJavaScriptFeature::FromBrowserState(GetBrowserState())
      ->GetElementAtPoint(
          web_state(), request_id, tap_point,
          base::BindOnce(^(const std::string& callback_request_id,
                           const web::ContextMenuParams& params) {
            EXPECT_EQ(request_id, callback_request_id);
            EXPECT_EQ(false, params.is_main_frame);
            EXPECT_EQ(expected_iframe_id, params.frame_id);
            EXPECT_EQ(expected_origin, params.frame_security_origin);
            EXPECT_NSEQ(@"Link", params.text);
            EXPECT_EQ(expected_pony_url, params.link_url.spec());
            callback_called = true;
          }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return callback_called;
  }));
}

}  // namespace web
