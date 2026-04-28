// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kPageWidth = 400;
constexpr int kPageHeight = 400;
constexpr int kIframeX = 50;
constexpr int kIframeY = 200;

class ActionTargetJavaScriptTest : public web::JavascriptTest {
 public:
  ActionTargetJavaScriptTest() {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
    web_view().frame = CGRectMake(0.0, 0.0, kPageWidth, kPageHeight);
  }

 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();

    test_server_.ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_.Start());

    AddGCrWebScript();
    AddUserScript(@"autofill_form_features");
    AddUserScript(@"action_target");

    // This page has an iframe at (50, 200) and a button at (50, 50).
    ASSERT_TRUE(
        LoadUrl(GURL(test_server_.GetURL("/actor/click_tool_test.html"))));
  }

  NSDictionary* GetTargetFrame(int x, int y, int pixelType) {
    NSString* script = [NSString
        stringWithFormat:
            @"__gCrWeb.getRegisteredApi('action_target').getFunction('"
            @"resolveTargetIframe')(%d, %d, %d)",
            x, y, pixelType];

    id result = web::test::ExecuteJavaScript(web_view(), script);
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

TEST_F(ActionTargetJavaScriptTest, TargetOnIframe_ReturnsChildFrameInfo) {
  NSDictionary* result = GetTargetFrame(kIframeX, kIframeY, /*pixelType=*/1);
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(actor::ActionTargetResultCode::kOk));
  NSDictionary* childFrame = result[@"childFrame"];
  EXPECT_TRUE(childFrame);
  EXPECT_TRUE(childFrame[@"remoteFrameToken"]);
  EXPECT_TRUE(childFrame[@"frameX"]);
  EXPECT_TRUE(childFrame[@"frameY"]);
}

TEST_F(ActionTargetJavaScriptTest,
       TargetOnMainFrame_ReturnsSuccessWithoutChildFrameInfo) {
  // The iframe is at (50, 200).
  NSDictionary* result = GetTargetFrame(/*x=*/10, /*y=*/10, /*pixelType=*/1);
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(actor::ActionTargetResultCode::kOk));
  EXPECT_FALSE(result[@"childFrame"]);
}

TEST_F(ActionTargetJavaScriptTest, InvalidCoordinates_Fails) {
  NSDictionary* result =
      GetTargetFrame(kPageWidth + 1, kPageHeight + 1, /*pixelType=*/1);
  EXPECT_EQ(
      [result[@"resultCode"] intValue],
      static_cast<int>(actor::ActionTargetResultCode::kCoordinatesOutOfBounds));
  EXPECT_TRUE([result[@"message"]
      containsString:@"No element found at the target coordinates."]);
}

}  // namespace
