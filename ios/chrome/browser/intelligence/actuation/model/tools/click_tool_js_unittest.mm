// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <set>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

struct EventInfo {
  std::string type;
  int x;
  int y;
  bool bubbles;
  bool cancelable;
  int button;
  int detail;

  bool operator==(const EventInfo& other) const {
    return type == other.type && x == other.x && y == other.y &&
           bubbles == other.bubbles && cancelable == other.cancelable &&
           button == other.button && detail == other.detail;
  }
};

class ClickToolJavascriptTest : public web::JavascriptTest {
 public:
  static constexpr int kButtonX = 50;
  static constexpr int kButtonY = 50;
  static constexpr int kIframeX = 50;
  static constexpr int kIframeY = 200;
  static constexpr int kEmptyX = 10;
  static constexpr int kEmptyY = 10;
  static constexpr int kDevicePixelRatio = 2;

  ClickToolJavascriptTest() {
    // TODO(crbug.com/483433952): Remove this once it's enabled by default.
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
    web_view().frame = CGRectMake(0.0, 0.0, 400.0, 400.0);
  }

 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();

    test_server_.ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_.Start());

    AddGCrWebScript();
    AddUserScript(@"click_tool");

    ASSERT_TRUE(
        LoadUrl(GURL(test_server_.GetURL("/actuation/click_tool_test.html"))));
  }

  std::vector<EventInfo> ExpectedEventsOnClick(int x,
                                               int y,
                                               int button,
                                               int detail) {
    std::vector<EventInfo> events;
    events.push_back(
        {"touchstart", x, y, /*bubbles=*/true, /*cancelable=*/false, 0, 0});
    events.push_back(
        {"touchend", x, y, /*bubbles=*/true, /*cancelable=*/false, 0, 0});
    events.push_back({"mousemove", x, y, /*bubbles=*/true, /*cancelable=*/false,
                      button, detail});
    events.push_back({"mousedown", x, y, /*bubbles=*/true, /*cancelable=*/false,
                      button, detail});
    events.push_back({"mouseup", x, y, /*bubbles=*/true, /*cancelable=*/false,
                      button, detail});
    events.push_back({"click", x, y, /*bubbles=*/true, /*cancelable=*/false,
                      button, detail});
    return events;
  }

  NSDictionary* PerformClick(int x,
                             int y,
                             int clickType,
                             int clickCount,
                             int pixelType) {
    NSString* script =
        [NSString stringWithFormat:
                      @"__gCrWeb.getRegisteredApi('click_tool').getFunction('"
                      @"clickByCoordinate')(%d, %d, %d, %d, %d)",
                      x, y, clickType, clickCount, pixelType];

    id result = web::test::ExecuteJavaScript(web_view(), script);
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  std::vector<EventInfo> GetCapturedEvents() {
    NSString* eventsJson = web::test::ExecuteJavaScript(
        web_view(), @"JSON.stringify(window.capturedEvents)");
    NSData* data = [eventsJson dataUsingEncoding:NSUTF8StringEncoding];
    NSArray* eventsArray = [NSJSONSerialization JSONObjectWithData:data
                                                           options:0
                                                             error:nil];
    std::vector<EventInfo> captured_events;
    for (NSDictionary* eventDict in eventsArray) {
      EventInfo info;
      info.type = base::SysNSStringToUTF8(eventDict[@"type"]);
      info.x = [eventDict[@"x"] intValue];
      info.y = [eventDict[@"y"] intValue];
      info.bubbles = [eventDict[@"bubbles"] boolValue];
      info.cancelable = [eventDict[@"cancelable"] boolValue];
      info.button = [eventDict[@"button"] intValue];
      info.detail = [eventDict[@"detail"] intValue];
      captured_events.push_back(info);
    }
    return captured_events;
  }

  std::string GetButtonText() {
    id result = web::test::ExecuteJavaScript(
        web_view(), @"document.getElementById('target_button').innerText");
    NSString* resultString = base::apple::ObjCCast<NSString>(result);
    return base::SysNSStringToUTF8(resultString);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

TEST_F(ClickToolJavascriptTest, DensityIndependentPixels_SingleClick_OnButton) {
  NSDictionary* result = PerformClick(kButtonX, kButtonY, /*clickType=*/1,
                                      /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_TRUE([result[@"success"] boolValue]);

  std::vector<EventInfo> expected =
      ExpectedEventsOnClick(kButtonX, kButtonY, /*button=*/0, /*detail=*/1);
  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(GetButtonText(), "Clicked");
}

TEST_F(ClickToolJavascriptTest, DensityIndependentPixels_DoubleClick_OnButton) {
  NSDictionary* result = PerformClick(kButtonX, kButtonY, /*clickType=*/1,
                                      /*clickCount=*/2, /*pixelType=*/1);
  EXPECT_TRUE([result[@"success"] boolValue]);

  std::vector<EventInfo> expected =
      ExpectedEventsOnClick(kButtonX, kButtonY, /*button=*/0, /*detail=*/1);
  std::vector<EventInfo> second_click =
      ExpectedEventsOnClick(kButtonX, kButtonY, /*button=*/0, /*detail=*/2);
  expected.insert(expected.end(), second_click.begin(), second_click.end());
  expected.push_back({"dblclick", kButtonX, kButtonY, /*bubbles=*/true,
                      /*cancelable=*/false, /*button=*/0, /*detail=*/2});

  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(GetButtonText(), "Clicked");
}

TEST_F(ClickToolJavascriptTest, DensityIndependentPixels_RightClick_OnButton) {
  NSDictionary* result = PerformClick(kButtonX, kButtonY, /*clickType=*/2,
                                      /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_TRUE([result[@"success"] boolValue]);

  std::vector<EventInfo> expected =
      ExpectedEventsOnClick(kButtonX, kButtonY, /*button=*/2, /*detail=*/1);
  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_EQ(expected, actual);
}

TEST_F(ClickToolJavascriptTest,
       DensityIndependentPixels_SingleClick_OnIframe_Fails) {
  NSDictionary* result = PerformClick(kIframeX, kIframeY, /*clickType=*/1,
                                      /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_FALSE([result[@"success"] boolValue]);
  EXPECT_TRUE([result[@"message"]
      containsString:@"iframe found at the target coordinates."]);
}

TEST_F(ClickToolJavascriptTest,
       DensityIndependentPixels_SingleClick_OnEmptySpace) {
  NSDictionary* result = PerformClick(kEmptyX, kEmptyY, /*clickType=*/1,
                                      /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_TRUE([result[@"success"] boolValue]);

  std::vector<EventInfo> expected =
      ExpectedEventsOnClick(kEmptyX, kEmptyY, /*button=*/0, /*detail=*/1);
  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_EQ(expected, actual);
}

TEST_F(ClickToolJavascriptTest,
       DensityIndependentPixels_Click_NegativeCoordinates_Fails) {
  NSDictionary* result = PerformClick(-50, -50, /*clickType=*/1,
                                      /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_FALSE([result[@"success"] boolValue]);
  EXPECT_TRUE([result[@"message"]
      containsString:@"No element found at the target coordinates."]);
}

TEST_F(ClickToolJavascriptTest,
       DensityIndependentPixels_UnknownClickType_DefaultsToLeft) {
  NSDictionary* result = PerformClick(kButtonX, kButtonY, /*clickType=*/99,
                                      /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_TRUE([result[@"success"] boolValue]);

  std::vector<EventInfo> expected =
      ExpectedEventsOnClick(kButtonX, kButtonY, /*button=*/0, /*detail=*/1);
  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_EQ(expected, actual);
}

TEST_F(ClickToolJavascriptTest, PhysicalPixels_SingleClick_OnButton) {
  (void)web::test::ExecuteJavaScript(
      web_view(), [NSString stringWithFormat:@"window.devicePixelRatio = %d;",
                                             kDevicePixelRatio]);

  int x = kButtonX * kDevicePixelRatio;
  int y = kButtonY * kDevicePixelRatio;
  NSDictionary* result = PerformClick(x, y, /*clickType=*/1,
                                      /*clickCount=*/1, /*pixelType=*/2);
  EXPECT_TRUE([result[@"success"] boolValue]);

  std::vector<EventInfo> expected =
      ExpectedEventsOnClick(kButtonX, kButtonY, /*button=*/0, /*detail=*/1);
  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(GetButtonText(), "Clicked");
}

}  // namespace
