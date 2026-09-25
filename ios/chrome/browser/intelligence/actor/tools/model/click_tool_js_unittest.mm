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
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

constexpr std::string_view kSingleButtonMouseEvents =
    "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
    "mouseup[BUTTON#clickable],click[BUTTON#clickable]";

constexpr std::string_view kDoubleButtonMouseEvents =
    "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
    "mouseup[BUTTON#clickable],click[BUTTON#clickable],"
    "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
    "mouseup[BUTTON#clickable],click[BUTTON#clickable]";

constexpr std::string_view kEmptySpaceMouseEvents =
    "mousemove[BODY#],mousedown[BODY#],mouseup[BODY#],click[BODY#]";

class ClickToolJavascriptTest : public web::JavascriptTest {
 public:
  static constexpr int kEmptyX = 350;
  static constexpr int kEmptyY = 350;
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
        base::FilePath("components/test/data/actor/"));
    ASSERT_TRUE(test_server_.Start());

    AddGCrWebScript();
    AddUserScript(@"dom_node_ids_test");
    AddUserScript(@"click_tool");

    ASSERT_TRUE(LoadUrl(
        GURL(test_server_.GetURL("/page_with_clickable_element.html"))));
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^{
          id ready = web::test::ExecuteJavaScript(
              web_view(),
              @"document.readyState === 'complete' && window.innerHeight > 0;");
          return [ready boolValue];
        }));
  }

  int GetElementCenterX(NSString* element_id) {
    NSString* script = [NSString
        stringWithFormat:@"Math.round((() => { const r = "
                         @"document.getElementById('%@').getBoundingClientRect("
                         @"); return r.left + r.width / 2; })());",
                         element_id];
    id result = web::test::ExecuteJavaScript(web_view(), script);
    return [result intValue];
  }

  int GetElementCenterY(NSString* element_id) {
    NSString* script = [NSString
        stringWithFormat:@"Math.round((() => { const r = "
                         @"document.getElementById('%@').getBoundingClientRect("
                         @"); return r.top + r.height / 2; })());",
                         element_id];
    id result = web::test::ExecuteJavaScript(web_view(), script);
    return [result intValue];
  }

  int GetButtonX() { return GetElementCenterX(@"clickable"); }

  int GetButtonY() { return GetElementCenterY(@"clickable"); }

  bool IsButtonClicked() {
    id result = web::test::ExecuteJavaScript(web_view(), @"button_clicked;");
    return [result boolValue];
  }

  bool ExpectSingleLeftClick() {
    id result = web::test::ExecuteJavaScript(
        web_view(), @"button_clicked && button_click_count === 1 && "
                    @"button_mouse_down && button_mouse_up;");
    return [result boolValue];
  }

  bool ExpectDoubleLeftClick() {
    id result = web::test::ExecuteJavaScript(
        web_view(), @"button_clicked && button_click_count === 2 && "
                    @"button_mouse_down && button_mouse_up;");
    return [result boolValue];
  }

  std::string GetMouseEventLog() {
    id result =
        web::test::ExecuteJavaScript(web_view(), @"mouse_event_log.join(',');");
    NSString* str = base::apple::ObjCCast<NSString>(result);
    return str ? base::SysNSStringToUTF8(str) : "";
  }

  NSDictionary* ExecuteClickAndVerifyMouseEventLog(
      int click_count,
      std::string_view expected_mouse_log) {
    NSDictionary* result =
        ClickByCoordinate(GetButtonX(), GetButtonY(), /*clickType=*/1,
                          click_count, /*pixelType=*/1);
    EXPECT_TRUE(result);
    EXPECT_NE(result[@"resultCode"], nil);
    EXPECT_EQ(static_cast<actor::ClickToolResultCode>(
                  [result[@"resultCode"] intValue]),
              actor::ClickToolResultCode::kOk);
    EXPECT_EQ(GetMouseEventLog(), expected_mouse_log);
    return result;
  }

  NSDictionary* ClickByCoordinate(int x,
                                  int y,
                                  int clickType,
                                  int clickCount,
                                  int pixelType) {
    NSString* script = base::SysUTF8ToNSString(base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('click_tool').getFunction()"
        R"('click')({coordinate: {x: %d, y: %d, pixelType: %d}}, %d, %d))",
        x, y, pixelType, clickType, clickCount));

    id result = web::test::ExecuteJavaScript(web_view(), script);
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  NSDictionary* ClickByNodeId(int nodeId, int clickType, int clickCount) {
    NSString* script = base::SysUTF8ToNSString(base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('click_tool').getFunction()"
        R"('click')({contentNodeId: %d}, %d, %d))",
        nodeId, clickType, clickCount));

    id result = web::test::ExecuteJavaScript(web_view(), script);
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  bool ExpectSingleRightClick() {
    id result = web::test::ExecuteJavaScript(
        web_view(), @"button_clicked && button_mouse_down && button_mouse_up;");
    return [result boolValue];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_DensityIndependentPixels_SingleClick_OnButton) {
  const int button_x = GetButtonX();
  const int button_y = GetButtonY();
  NSDictionary* result = ClickByCoordinate(button_x, button_y, /*clickType=*/1,
                                           /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kSingleButtonMouseEvents);
  EXPECT_TRUE(ExpectSingleLeftClick());
  EXPECT_TRUE(IsButtonClicked());
}

TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_DensityIndependentPixels_DoubleClick_OnButton) {
  const int button_x = GetButtonX();
  const int button_y = GetButtonY();
  NSDictionary* result = ClickByCoordinate(button_x, button_y, /*clickType=*/1,
                                           /*clickCount=*/2, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kDoubleButtonMouseEvents);
  EXPECT_TRUE(ExpectDoubleLeftClick());
  EXPECT_TRUE(IsButtonClicked());
}

TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_DensityIndependentPixels_RightClick_OnButton) {
  const int button_x = GetButtonX();
  const int button_y = GetButtonY();
  NSDictionary* result = ClickByCoordinate(button_x, button_y, /*clickType=*/2,
                                           /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kSingleButtonMouseEvents);
  EXPECT_TRUE(ExpectSingleRightClick());
}

TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_DensityIndependentPixels_SingleClick_OnEmptySpace) {
  NSDictionary* result = ClickByCoordinate(kEmptyX, kEmptyY, /*clickType=*/1,
                                           /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kEmptySpaceMouseEvents);
  EXPECT_FALSE(IsButtonClicked());
}

TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_DensityIndependentPixels_NegativeCoordinates_Fails) {
  NSDictionary* result = ClickByCoordinate(-50, -50, /*clickType=*/1,
                                           /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kCoordinatesOutOfBounds);
  EXPECT_TRUE(
      [result[@"message"] containsString:@"Point is outside of the viewport."]);
}

TEST_F(
    ClickToolJavascriptTest,
    ClickByCoordinate_DensityIndependentPixels_UnknownClickType_DefaultsToLeft) {
  const int button_x = GetButtonX();
  const int button_y = GetButtonY();
  NSDictionary* result = ClickByCoordinate(button_x, button_y, /*clickType=*/99,
                                           /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kSingleButtonMouseEvents);
  EXPECT_TRUE(ExpectSingleLeftClick());
}

TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_PhysicalPixels_SingleClick_OnButton) {
  (void)web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(
                      R"(window.devicePixelRatio = %d;)", kDevicePixelRatio)));

  const int button_x = GetButtonX();
  const int button_y = GetButtonY();
  int x = button_x * kDevicePixelRatio;
  int y = button_y * kDevicePixelRatio;
  NSDictionary* result = ClickByCoordinate(x, y, /*clickType=*/1,
                                           /*clickCount=*/1, /*pixelType=*/2);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kSingleButtonMouseEvents);
  EXPECT_TRUE(ExpectSingleLeftClick());
  EXPECT_TRUE(IsButtonClicked());
}

TEST_F(ClickToolJavascriptTest, ClickByCoordinate_DisabledElement_Fails) {
  const int disabled_x = GetElementCenterX(@"disabled");
  const int disabled_y = GetElementCenterY(@"disabled");

  NSDictionary* result =
      ClickByCoordinate(disabled_x, disabled_y, /*clickType=*/1,
                        /*clickCount=*/1, /*pixelType=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kElementDisabled);
  EXPECT_TRUE([result[@"message"] containsString:@"disabled"]);
}

TEST_F(ClickToolJavascriptTest, ClickByNodeId_DisabledElement_Fails) {
  id nodeIdResult =
      web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
        var el = document.getElementById('disabled');
        __gCrWeb.getRegisteredApi('dom_node_ids_test')
                .getFunction('getOrCreateNodeId')(el);
      )"));
  int nodeId = [nodeIdResult intValue];
  ASSERT_GT(nodeId, 0);

  NSDictionary* result =
      ClickByNodeId(nodeId, /*clickType=*/1, /*clickCount=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kElementDisabled);
  EXPECT_TRUE([result[@"message"] containsString:@"disabled"]);
}

TEST_F(ClickToolJavascriptTest, ClickByNodeId_NotFound) {
  NSDictionary* result = ClickByNodeId(999, /*clickType=*/1, /*clickCount=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kInvalidDomNodeId);
  EXPECT_TRUE(
      [result[@"message"] containsString:@"No element found with id 999."]);
}

TEST_F(ClickToolJavascriptTest, ClickByNodeId_Success) {
  id nodeIdResult =
      web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
        var el = document.getElementById('clickable');
        __gCrWeb.getRegisteredApi('dom_node_ids_test')
                .getFunction('getOrCreateNodeId')(el);
      )"));
  int nodeId = [nodeIdResult intValue];
  ASSERT_GT(nodeId, 0);

  NSDictionary* result =
      ClickByNodeId(nodeId, /*clickType=*/1, /*clickCount=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kSingleButtonMouseEvents);
  EXPECT_TRUE(ExpectSingleLeftClick());
  EXPECT_TRUE(IsButtonClicked());
}

TEST_F(ClickToolJavascriptTest, ClickByNodeId_TextNode_Success) {
  id nodeIdResult =
      web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
        var el = document.getElementById('clickable').firstChild;
        __gCrWeb.getRegisteredApi('dom_node_ids_test')
                .getFunction('getOrCreateNodeId')(el);
      )"));
  int nodeId = [nodeIdResult intValue];
  ASSERT_GT(nodeId, 0);

  NSDictionary* result =
      ClickByNodeId(nodeId, /*clickType=*/1, /*clickCount=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kOk);

  EXPECT_EQ(GetMouseEventLog(), kSingleButtonMouseEvents);
  EXPECT_TRUE(ExpectSingleLeftClick());
  EXPECT_TRUE(IsButtonClicked());
}

TEST_F(ClickToolJavascriptTest, ClickByNodeId_UnclickableNode_Fails) {
  id nodeIdResult =
      web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('dom_node_ids_test')
                .getFunction('getOrCreateNodeId')(document);
      )"));
  int nodeId = [nodeIdResult intValue];
  ASSERT_GT(nodeId, 0);

  NSDictionary* result =
      ClickByNodeId(nodeId, /*clickType=*/1, /*clickCount=*/1);
  EXPECT_EQ(
      static_cast<actor::ClickToolResultCode>([result[@"resultCode"] intValue]),
      actor::ClickToolResultCode::kInvalidDomNodeId);
  NSString* expectedMessage =
      [NSString stringWithFormat:@"Node with id %d is not clickable.", nodeId];
  EXPECT_TRUE([result[@"message"] containsString:expectedMessage]);
}

// Tests that when a site calls preventDefault() on 'touchstart', touch events
// finish but mouse and click events are suppressed per
// https://w3c.github.io/touch-events/#mouse-events.
TEST_F(
    ClickToolJavascriptTest,
    ClickByCoordinate_SingleClick_TouchStartPreventDefault_SuppressesMouseEvents_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"document.addEventListener('touchstart', (e) => "
                  @"e.preventDefault(), {passive: false});");
  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/1, "");
}

// Tests that when a site calls preventDefault() on 'touchend', touch events
// finish but mouse and click events are suppressed per
// https://w3c.github.io/touch-events/#mouse-events.
TEST_F(
    ClickToolJavascriptTest,
    ClickByCoordinate_SingleClick_TouchEndPreventDefault_SuppressesMouseEvents_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(),
      @"document.addEventListener('touchend', (e) => e.preventDefault());");
  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/1, "");
}

// Tests that when an event listener calls preventDefault() on a 'click' event,
// the tool returns kOk.
TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_SingleClick_ClickPreventDefault_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(),
      @"document.addEventListener('click', (e) => e.preventDefault());");
  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/1,
                                     kSingleButtonMouseEvents);
}

// Tests that when an event listener calls preventDefault() on a 'dblclick'
// event, the tool returns kOk.
TEST_F(ClickToolJavascriptTest,
       ClickByCoordinate_DoubleClick_DblclickPreventDefault_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(),
      @"document.addEventListener('dblclick', (e) => e.preventDefault());");

  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/2,
                                     kDoubleButtonMouseEvents);
}

// Tests that when an event listener calls preventDefault() on 'touchstart'
// during the first click of a double click, the second touch sequence still
// dispatches touch and mouse events (with detail=1 since no prior click event
// was dispatched), but 'dblclick' is suppressed.
TEST_F(
    ClickToolJavascriptTest,
    ClickByCoordinate_DoubleClick_FirstClickTouchPreventDefault_DispatchesSecondTouchAndMouseEvents_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"(() => {"
                  @"  let touchCount = 0;"
                  @"  document.addEventListener('touchstart', (e) => {"
                  @"    touchCount++;"
                  @"    if (touchCount === 1) {"
                  @"      e.preventDefault();"
                  @"    }"
                  @"  }, {passive: false});"
                  @"})();");

  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/2,
                                     kSingleButtonMouseEvents);
}

// Tests that when an event listener calls preventDefault() on 'touchstart'
// during the second click of a double click, the second click's mouse events
// and 'dblclick' are suppressed.
TEST_F(
    ClickToolJavascriptTest,
    ClickByCoordinate_DoubleClick_SecondClickTouchPreventDefault_SuppressesSecondMouseEvents_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"(() => {"
                  @"  let touchCount = 0;"
                  @"  document.addEventListener('touchstart', (e) => {"
                  @"    touchCount++;"
                  @"    if (touchCount === 2) {"
                  @"      e.preventDefault();"
                  @"    }"
                  @"  }, {passive: false});"
                  @"})();");

  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/2,
                                     kSingleButtonMouseEvents);
}

// Tests that when event listeners call preventDefault() on 'touchstart' during
// both clicks of a double click, all mouse events and 'dblclick' are
// suppressed.
TEST_F(
    ClickToolJavascriptTest,
    ClickByCoordinate_DoubleClick_BothClicksTouchPreventDefault_SuppressesAllMouseEvents_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(),
      @"document.addEventListener('touchstart', (e) => e.preventDefault(), "
      @"{passive: false});");

  ExecuteClickAndVerifyMouseEventLog(/*click_count=*/2, "");
}

}  // namespace
