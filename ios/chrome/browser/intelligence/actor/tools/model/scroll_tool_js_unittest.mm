// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <optional>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_constants.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using optimization_guide::proto::Coordinate;
using optimization_guide::proto::ScrollAction;

namespace actor {

namespace {

enum class CallType {
  kByCoordinate,
  kByNodeId,
};

void PrintTo(CallType call_type, std::ostream* os) {
  switch (call_type) {
    case CallType::kByCoordinate:
      *os << "ByCoordinate";
      break;
    case CallType::kByNodeId:
      *os << "ByNodeId";
      break;
  }
}

// Proto fields that are set in the ScrollAction but not ScrollToAction.
struct ScrollActionFields {
  int direction;
  float distance;
};

}  // namespace

class ScrollToolJavascriptTest
    : public web::JavascriptTest,
      public ::testing::WithParamInterface<CallType> {
 public:
  ScrollToolJavascriptTest() {
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
    AddUserScript(@"dom_node_ids_test");
    AddUserScript(@"scroll_tool");

    ASSERT_TRUE(
        LoadUrl(GURL(test_server_.GetURL("/actor/scroll_tool_test.html"))));
  }

  NSDictionary* ScrollByCoordinate(
      int x,
      int y,
      int pixel_type,
      std::optional<ScrollActionFields> additional_fields) {
    std::string script;
    if (additional_fields) {
      script = base::StringPrintf(
          R"(
            (function() {
              return __gCrWeb.getRegisteredApi('scroll_tool')
                             .getFunction('scrollByCoordinate')
                                         (%d, %d, %d, %d, %f);
            })();
          )",
          x, y, pixel_type, additional_fields->direction,
          additional_fields->distance);
    } else {
      script = base::StringPrintf(
          R"(
            (function() {
              return __gCrWeb.getRegisteredApi('scroll_tool')
                             .getFunction('scrollByCoordinate')
                                         (%d, %d, %d);
            })();
          )",
          x, y, pixel_type);
    }

    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    NSDictionary* result_dict = base::apple::ObjCCast<NSDictionary>(result);
    return result_dict;
  }

  NSDictionary* ScrollByNodeId(
      int node_id,
      std::optional<ScrollActionFields> additional_fields) {
    std::string script;
    if (additional_fields) {
      script = base::StringPrintf(
          R"(
            (function() {
              return __gCrWeb.getRegisteredApi('scroll_tool')
                             .getFunction('scrollByNodeId')(%d, %d, %f);
            })();
          )",
          node_id, additional_fields->direction, additional_fields->distance);
    } else {
      script = base::StringPrintf(
          R"(
            (function() {
              return __gCrWeb.getRegisteredApi('scroll_tool')
                             .getFunction('scrollByNodeId')(%d);
            })();
          )",
          node_id);
    }

    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    NSDictionary* result_dict = base::apple::ObjCCast<NSDictionary>(result);
    return result_dict;
  }

  NSDictionary* Scroll(const std::string& html_element_id,
                       std::optional<ScrollActionFields> additional_fields) {
    if (GetParam() == CallType::kByCoordinate) {
      NSDictionary* rect = GetElementClientRect(html_element_id);
      int x = [rect[@"left"] intValue] + [rect[@"width"] intValue] / 2;
      int y = [rect[@"top"] intValue] + [rect[@"height"] intValue] / 2;
      return ScrollByCoordinate(x, y,
                                static_cast<int>(Coordinate::PIXEL_TYPE_DIPS),
                                additional_fields);
    } else {
      int node_id = GetNodeId(html_element_id);
      return ScrollByNodeId(node_id, additional_fields);
    }
  }

  NSDictionary* GetElementClientRect(const std::string& element_id) {
    std::string script = base::StringPrintf(
        R"(
        (function() {
          var rect = document.getElementById('%s').getBoundingClientRect();
          return {
            left: rect.left,
            top: rect.top,
            width: rect.width,
            height: rect.height
          };
        })();
      )",
        element_id.c_str());
    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    NSDictionary* result_dict = base::apple::ObjCCast<NSDictionary>(result);
    return result_dict;
  }

  float GetDivScrollTop() {
    id result =
        web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
          document.getElementById('scrollable_div').scrollTop
        )"));
    return [result floatValue];
  }

  float GetDivScrollLeft() {
    id result =
        web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
          document.getElementById('scrollable_div').scrollLeft
        )"));
    return [result floatValue];
  }

  bool IsScrollable(NSString* element_expression, int direction) {
    NSString* script =
        [NSString stringWithFormat:@"var el = %@;"
                                   @"__gCrWeb.getRegisteredApi('scroll_tool')."
                                   @"getFunction('isScrollable')(el, %d);",
                                   element_expression, direction];
    id result = web::test::ExecuteJavaScript(web_view(), script);
    return [result boolValue];
  }

 protected:
  int GetNodeId(const std::string& element_id) {
    std::string script = base::StringPrintf(
        R"(
        __gCrWeb.getRegisteredApi('dom_node_ids_test')
                .getFunction('getOrCreateNodeId')
                  (document.getElementById('%s'));
      )",
        element_id.c_str());
    id node_id_result = web::test::ExecuteJavaScript(
        web_view(), base::SysUTF8ToNSString(script));
    return [node_id_result intValue];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

TEST_P(ScrollToolJavascriptTest, Scroll_WithDistanceAndDirection_Down_Success) {
  float initial_scroll_top = GetDivScrollTop();
  float distance = 100.0;

  NSDictionary* result = Scroll(
      "scrollable_div",
      ScrollActionFields{static_cast<int>(ScrollAction::DOWN), distance});
  EXPECT_TRUE([result[@"success"] boolValue])
      << base::SysNSStringToUTF8(result[@"message"]);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollTop() == initial_scroll_top + distance;
      }));
}

TEST_P(ScrollToolJavascriptTest,
       Scroll_WithDistanceAndDirection_Right_Success) {
  float initial_scroll_left = GetDivScrollLeft();
  float distance = 100.0;

  NSDictionary* result = Scroll(
      "scrollable_div",
      ScrollActionFields{static_cast<int>(ScrollAction::RIGHT), distance});
  EXPECT_TRUE([result[@"success"] boolValue])
      << base::SysNSStringToUTF8(result[@"message"]);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollLeft() == initial_scroll_left + distance;
      }));
}

TEST_P(ScrollToolJavascriptTest, Scroll_WithDistanceAndDirection_Up_Success) {
  std::string nodeId = "scrollable_div";
  // Scroll down first
  Scroll(nodeId, ScrollActionFields{static_cast<int>(ScrollAction::DOWN), 100});
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollTop() == 100.0;
      }));
  float initial_scroll_top = GetDivScrollTop();
  float distance = 40.0;

  // Scroll up
  NSDictionary* result = Scroll(
      nodeId, ScrollActionFields{static_cast<int>(ScrollAction::UP), distance});
  EXPECT_TRUE([result[@"success"] boolValue]);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollTop() == initial_scroll_top - distance;
      }));
}

TEST_P(ScrollToolJavascriptTest, Scroll_WithDistanceAndDirection_Left_Success) {
  std::string nodeId = "scrollable_div";
  // Scroll right first
  Scroll(nodeId,
         ScrollActionFields{static_cast<int>(ScrollAction::RIGHT), 100});
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollLeft() == 100.0;
      }));
  float initial_scroll_left = GetDivScrollLeft();
  float distance = 40.0;

  // Scroll left
  NSDictionary* result = Scroll(
      nodeId,
      ScrollActionFields{static_cast<int>(ScrollAction::LEFT), distance});
  EXPECT_TRUE([result[@"success"] boolValue]);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollLeft() == initial_scroll_left - distance;
      }));
}

TEST_P(ScrollToolJavascriptTest, Scroll_WithoutDistanceAndDirection_Success) {
  std::string node_id = "scrollable_div";
  NSDictionary* rect = GetElementClientRect(node_id);
  int element_x = [rect[@"left"] intValue];
  int element_y = [rect[@"top"] intValue];
  EXPECT_GT(element_x, 0);
  EXPECT_GT(element_y, 0);

  // Scroll the window so that the element is not fully in the viewport.
  id __unused js_result =
      web::test::ExecuteJavaScript(web_view(), @"window.scrollTop=150;"
                                               @"window.scrollLeft=100;"
                                               @"true;");
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        id result = web::test::ExecuteJavaScript(web_view(),
                                                 @"window.scrollTop===150 &&"
                                                 @"window.scrollLeft===100;");
        return [result boolValue];
      }));

  NSDictionary* result = Scroll(node_id, std::nullopt);
  EXPECT_TRUE([result[@"success"] boolValue])
      << "Scroll failed: " << base::SysNSStringToUTF8(result[@"message"]);

  EXPECT_NSEQ(result[@"message"], @"Initiated scrollIntoView.");

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSDictionary* position = GetElementClientRect(node_id);
        return [position[@"left"] intValue] >= 0 &&
               [position[@"top"] intValue] >= 0;
      }));
}

TEST_P(ScrollToolJavascriptTest, Scroll_WithZoom_Success) {
  web_view().scrollView.zoomScale = 2.0;

  float initial_scroll_top = GetDivScrollTop();
  float distance = 100.0;

  NSDictionary* result = Scroll(
      "scrollable_div",
      ScrollActionFields{static_cast<int>(ScrollAction::DOWN), distance});
  EXPECT_TRUE([result[@"success"] boolValue])
      << base::SysNSStringToUTF8(result[@"message"]);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return GetDivScrollTop() == initial_scroll_top + distance;
      }));
}

INSTANTIATE_TEST_SUITE_P(,
                         ScrollToolJavascriptTest,
                         ::testing::Values(CallType::kByCoordinate,
                                           CallType::kByNodeId),
                         [](const ::testing::TestParamInfo<CallType>& info) {
                           switch (info.param) {
                             case CallType::kByCoordinate:
                               return "ByCoordinate";
                             case CallType::kByNodeId:
                               return "ByNodeId";
                           }
                         });

TEST_F(ScrollToolJavascriptTest, Scroll_InvalidDirection_Fails) {
  std::string nodeId = "scrollable_div";
  NSDictionary* result =
      Scroll(nodeId,
             ScrollActionFields{
                 static_cast<int>(ScrollAction::UNKNOWN_SCROLL_DIRECTION), 50});
  EXPECT_FALSE([result[@"success"] boolValue]);
  EXPECT_NSEQ(result[@"message"], @"Element is not scrollable.");
}

TEST_P(ScrollToolJavascriptTest, ScrollByNodeId_NotScrollable_Fails) {
  int node_id = GetNodeId("non_scrollable_div");
  NSDictionary* result = ScrollByNodeId(
      node_id, ScrollActionFields{static_cast<int>(ScrollAction::DOWN), 100});

  EXPECT_FALSE([result[@"success"] boolValue]);
  EXPECT_NSEQ(result[@"message"], @"Element is not scrollable.");
}

TEST_F(ScrollToolJavascriptTest, Scroll_ByCoordinate_InvalidCoordinates) {
  NSDictionary* result = ScrollByCoordinate(
      -10, -10, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS),
      ScrollActionFields{static_cast<int>(ScrollAction::DOWN), 100});
  EXPECT_FALSE([result[@"success"] boolValue]);
  EXPECT_NSEQ(result[@"message"],
              @"No element found at the target coordinates.");
}

TEST_F(ScrollToolJavascriptTest, Scroll_ByNodeId_InvalidNode) {
  NSDictionary* result = ScrollByNodeId(
      -1, ScrollActionFields{static_cast<int>(ScrollAction::DOWN), 100});
  EXPECT_FALSE([result[@"success"] boolValue]);
  EXPECT_NSEQ(result[@"message"], @"No element found with id -1.");
}

TEST_F(ScrollToolJavascriptTest, IsScrollable_DocumentScrollableElement) {
  // Ensure that the document is scrollable.
  int client_height =
      [web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
    document.documentElement.clientHeight;
  )")) intValue];
  int body_height =
      [web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
    document.body.clientHeight;
  )")) intValue];
  ASSERT_TRUE(body_height > client_height);

  EXPECT_TRUE(IsScrollable(@"document.scrollingElement",
                           static_cast<int>(ScrollAction::DOWN)));
}

TEST_F(ScrollToolJavascriptTest, IsScrollable_ScrollableDiv) {
  // scrollable_div has 'overflow:scroll' by default.
  EXPECT_TRUE(IsScrollable(@"document.getElementById('scrollable_div')",
                           static_cast<int>(ScrollAction::DOWN)));
  EXPECT_TRUE(IsScrollable(@"document.getElementById('scrollable_div')",
                           static_cast<int>(ScrollAction::RIGHT)));

  // Scroll the element down and right to make it scrollable UP and LEFT.
  id __unused js_result =
      web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
    var div = document.getElementById('scrollable_div');
    div.scrollTop = 50;
    div.scrollLeft = 50;
    true;
  )"));

  EXPECT_TRUE(IsScrollable(@"document.getElementById('scrollable_div')",
                           static_cast<int>(ScrollAction::UP)));
  EXPECT_TRUE(IsScrollable(@"document.getElementById('scrollable_div')",
                           static_cast<int>(ScrollAction::LEFT)));

  // Explicitly set overflow to 'auto' and verify it's still scrollable.
  js_result = web::test::ExecuteJavaScript(
      web_view(),
      @"document.getElementById('scrollable_div').style.overflow = 'auto';"
      @"true;");
  EXPECT_TRUE(IsScrollable(@"document.getElementById('scrollable_div')",
                           static_cast<int>(ScrollAction::DOWN)));
}

TEST_F(ScrollToolJavascriptTest, IsScrollable_NonScrollableDiv) {
  for (NSString* overflow : @[ @"hidden", @"clip", @"visible" ]) {
    id __unused js_result = web::test::ExecuteJavaScript(
        web_view(),
        [NSString
            stringWithFormat:@"document.getElementById('non_scrollable_div')"
                             @".style.overflow = '%@';true;",
                             overflow]);
    EXPECT_FALSE(IsScrollable(@"document.getElementById('non_scrollable_div')",
                              static_cast<int>(ScrollAction::DOWN)))
        << "Should not be scrollable with overflow: "
        << base::SysNSStringToUTF8(overflow);
    EXPECT_FALSE(IsScrollable(@"document.getElementById('non_scrollable_div')",
                              static_cast<int>(ScrollAction::RIGHT)))
        << "Should not be scrollable with overflow: "
        << base::SysNSStringToUTF8(overflow);
  }
}

TEST_F(ScrollToolJavascriptTest, IsScrollable_NotOverflowingDiv) {
  EXPECT_FALSE(IsScrollable(@"document.getElementById('not_overflowing_div')",
                            static_cast<int>(ScrollAction::DOWN)));
  EXPECT_FALSE(IsScrollable(@"document.getElementById('not_overflowing_div')",
                            static_cast<int>(ScrollAction::RIGHT)));
}

TEST_F(ScrollToolJavascriptTest,
       ScrollByNodeId_kRootElementDomNodeId_ScrollsViewport) {
  // Ensure that the document is scrollable.
  int client_height =
      [web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
    document.documentElement.clientHeight;
  )")) intValue];
  int body_height =
      [web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
    document.body.clientHeight;
  )")) intValue];
  ASSERT_TRUE(body_height > client_height);
  float initial_scroll_top = [web::test::ExecuteJavaScript(
      web_view(), @"document.scrollingElement.scrollTop;") floatValue];
  float distance = 100.0;

  NSDictionary* result = ScrollByNodeId(
      kRootElementDomNodeId,
      ScrollActionFields{static_cast<int>(ScrollAction::DOWN), distance});

  EXPECT_TRUE([result[@"success"] boolValue])
      << base::SysNSStringToUTF8(result[@"message"]);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        float new_scroll_top = [web::test::ExecuteJavaScript(
            web_view(), @"document.scrollingElement.scrollTop;") floatValue];
        return new_scroll_top == initial_scroll_top + distance;
      }));
}

}  // namespace actor
