// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"

using optimization_guide::proto::Coordinate;

namespace {

enum class CallType {
  kByCoordinate,
  kByNodeId,
};

struct EventInfo {
  std::string type;
  std::string targetId;
  std::string value;
  bool bubbles;
  bool cancelable;

  bool operator==(const EventInfo& other) const {
    return type == other.type && targetId == other.targetId &&
           value == other.value && bubbles == other.bubbles &&
           cancelable == other.cancelable;
  }
};

}  // namespace

namespace actor {

class SelectToolJavaScriptTest
    : public web::JavascriptTest,
      public ::testing::WithParamInterface<CallType> {
 public:
  SelectToolJavaScriptTest() {
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
    AddUserScript(@"autofill_form_features");
    AddUserScript(@"dom_node_ids_test");
    AddUserScript(@"select_tool");

    ASSERT_TRUE(
        LoadUrl(GURL(test_server_.GetURL("/actor/select_tool_test.html"))));
  }

  NSDictionary* Select(const std::string& html_element_id,
                       const std::string& value) {
    if (GetParam() == CallType::kByCoordinate) {
      NSDictionary* rect = GetElementClientRect(html_element_id);
      int x = [rect[@"left"] intValue] + [rect[@"width"] intValue] / 2;
      int y = [rect[@"top"] intValue] + [rect[@"height"] intValue] / 2;
      return SelectByCoordinate(
          x, y, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS), value);
    } else {
      int node_id = GetNodeId(html_element_id);
      return SelectByNodeId(node_id, value);
    }
  }

  std::vector<EventInfo> GetCapturedEvents() {
    id result =
        web::test::ExecuteJavaScript(web_view(), @"window.capturedEvents");
    NSArray* eventsArray = base::apple::ObjCCast<NSArray>(result);
    if (!eventsArray) {
      return {};
    }
    std::vector<EventInfo> captured_events;
    for (NSDictionary* eventDict in eventsArray) {
      EventInfo info;
      info.type = base::SysNSStringToUTF8(eventDict[@"type"]);
      info.targetId = base::SysNSStringToUTF8(eventDict[@"targetId"]);
      info.value = base::SysNSStringToUTF8(eventDict[@"value"]);
      info.bubbles = [eventDict[@"bubbles"] boolValue];
      info.cancelable = [eventDict[@"cancelable"] boolValue];
      captured_events.push_back(info);
    }
    return captured_events;
  }

 private:
  NSDictionary* SelectByCoordinate(int x,
                                   int y,
                                   int pixelType,
                                   const std::string& value) {
    const std::string script = base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('select_tool').getFunction()"
        R"('selectByCoordinate')(%d, %d, %d, '%s'))",
        x, y, pixelType, value.c_str());
    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  NSDictionary* SelectByNodeId(int nodeId, const std::string& value) {
    const std::string script = base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('select_tool').getFunction()"
        R"('selectByNodeId')(%d, '%s'))",
        nodeId, value.c_str());
    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  NSDictionary* GetElementClientRect(const std::string& element_id) {
    const std::string script = base::StringPrintf(
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

TEST_P(SelectToolJavaScriptTest, Select_NotASelect) {
  NSDictionary* result =
      Select(/*html_element_id=*/"not_a_select", /*value=*/"v1");
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(SelectToolResultCode::kSelectInvalidElement));
  EXPECT_TRUE(
      [result[@"message"] containsString:@"Target element is not a <select>."]);
}

TEST_P(SelectToolJavaScriptTest, Select_DisabledSelect) {
  NSDictionary* result =
      Select(/*html_element_id=*/"disabled_select", /*value=*/"v1");
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(SelectToolResultCode::kElementDisabled));
  EXPECT_TRUE(
      [result[@"message"] containsString:@"<select> element is disabled."]);
}

TEST_P(SelectToolJavaScriptTest, Select_DisabledOption) {
  NSDictionary* result =
      Select(/*html_element_id=*/"target_select", /*value=*/"v3");
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(SelectToolResultCode::kSelectOptionDisabled));
  EXPECT_TRUE([result[@"message"]
      containsString:@"Specified value to select does exist but is disabled."]);
}

TEST_P(SelectToolJavaScriptTest, Select_OptionNotFound) {
  NSDictionary* result =
      Select(/*html_element_id=*/"target_select", /*value=*/"nonexistent");
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(SelectToolResultCode::kSelectNoSuchOption));
  EXPECT_TRUE([result[@"message"] containsString:@"not found"]);
}

TEST_P(SelectToolJavaScriptTest, Select_Success) {
  // Focus on another element to verify that the Select tool grabs focus
  // and hands it back to the original active element.
  id __unused focusAndClearEvents = web::test::ExecuteJavaScript(
      web_view(), @"document.getElementById('initial_focus').focus(); "
                  @"window.capturedEvents = [];");

  NSDictionary* result =
      Select(/*html_element_id=*/"target_select", /*value=*/"v2");
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(SelectToolResultCode::kOk));

  std::vector<EventInfo> actual = GetCapturedEvents();
  std::vector<EventInfo> expected = {
      {/*type=*/"blur", /*targetId=*/"initial_focus", /*value=*/"",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"focus", /*targetId=*/"target_select", /*value=*/"v1",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"keydown", /*targetId=*/"target_select", /*value=*/"v2",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"keypress", /*targetId=*/"target_select", /*value=*/"v2",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"input", /*targetId=*/"target_select", /*value=*/"v2",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"keyup", /*targetId=*/"target_select", /*value=*/"v2",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"change", /*targetId=*/"target_select", /*value=*/"v2",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"blur", /*targetId=*/"target_select", /*value=*/"v2",
       /*bubbles=*/true, /*cancelable=*/false},
      {/*type=*/"focus", /*targetId=*/"initial_focus", /*value=*/"",
       /*bubbles=*/true, /*cancelable=*/false},
  };
  EXPECT_EQ(expected, actual);
}

TEST_P(SelectToolJavaScriptTest, Select_WithWhitespace) {
  // The actual HTML option has value="&#9;v4&#10;" but we trim the whitespace
  // to "v4" when looking for the matching option.
  NSDictionary* result =
      Select(/*html_element_id=*/"target_select", /*value=*/"v4");
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(SelectToolResultCode::kOk));

  // The original value is used, despite trimming the whitespace for comparison.
  std::vector<EventInfo> actual = GetCapturedEvents();
  EXPECT_THAT(actual,
              testing::Contains(testing::Truly([](const EventInfo& event) {
                return event.type == "input" && event.value == "\tv4\n";
              })));
}

INSTANTIATE_TEST_SUITE_P(,
                         SelectToolJavaScriptTest,
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

}  // namespace actor
