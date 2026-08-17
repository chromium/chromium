// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

class AttemptFormFillingToolJavaScriptTest : public web::JavascriptTest {
 public:
  AttemptFormFillingToolJavaScriptTest() {
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
    AddUserScript(@"renderer_id_test");
    AddUserScript(@"dom_node_ids_test");
    AddUserScript(@"attempt_form_filling_tool");

    ASSERT_TRUE(LoadUrl(GURL(
        test_server_.GetURL("/actor/attempt_form_filling_tool_test.html"))));

    // Set IDs for form and input in the content world for Autofill features.
    web::test::ExecuteJavaScriptInWebView(
        web_view(),
        @"__gCrWeb.getRegisteredApi('renderer_id_test').getFunction("
        @"'setUniqueIDIfNeeded')(document.getElementById('input1'));");
  }

  // Helper to call getAutofillRendererIds via JS.
  NSDictionary* GetAutofillRendererIds(id nodeId, id x, id y, id pixelType) {
    NSString* targetJson;
    if (nodeId) {
      targetJson = [NSString stringWithFormat:@"{ nodeId: %@ }", nodeId];
    } else {
      targetJson = [NSString
          stringWithFormat:@"{ x: %@, y: %@, pixelType: %@ }", x, y, pixelType];
    }
    NSString* script = [NSString
        stringWithFormat:
            @"__gCrWeb.getRegisteredApi('attempt_form_filling').getFunction('"
            @"getAutofillRendererIds')([%@])",
            targetJson];
    id result = web::test::ExecuteJavaScript(web_view(), script);
    return base::apple::ObjCCast<NSDictionary>(result);
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

// Test lookup by node ID returns the unique ID of the element.
TEST_F(AttemptFormFillingToolJavaScriptTest, GetById_Success) {
  int node_id = GetNodeId("input1");
  NSDictionary* result = GetAutofillRendererIds(@(node_id), nil, nil, nil);
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kOk);
  NSArray* unique_ids = base::apple::ObjCCast<NSArray>(result[@"uniqueIds"]);
  ASSERT_EQ(unique_ids.count, 1u);
  EXPECT_EQ(base::SysNSStringToUTF8(unique_ids[0]), "1");
}

// Test lookup by coordinates returns the unique ID of the element.
TEST_F(AttemptFormFillingToolJavaScriptTest, GetByCoordinate_Success) {
  NSDictionary* rect = GetElementClientRect("input1");
  int x = [rect[@"left"] intValue] + [rect[@"width"] intValue] / 2;
  int y = [rect[@"top"] intValue] + [rect[@"height"] intValue] / 2;

  NSDictionary* result = GetAutofillRendererIds(
      nil, @(x), @(y),
      @(optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS));
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kOk);
  NSArray* unique_ids = base::apple::ObjCCast<NSArray>(result[@"uniqueIds"]);
  ASSERT_EQ(unique_ids.count, 1u);
  EXPECT_EQ(base::SysNSStringToUTF8(unique_ids[0]), "1");
}

// Test lookup on non-existent element returns failure result codes.
TEST_F(AttemptFormFillingToolJavaScriptTest, GetNonExistent_ReturnsErrorCodes) {
  // Invalid target (neither node ID nor coordinates)
  NSDictionary* result_invalid = GetAutofillRendererIds(nil, nil, nil, nil);
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result_invalid[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kInvalidTarget);

  // Invalid node ID
  NSDictionary* result_node = GetAutofillRendererIds(@(99999), nil, nil, nil);
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result_node[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kInvalidDomNodeId);

  // Invalid coordinates (out of bounds)
  NSDictionary* result_coord = GetAutofillRendererIds(
      nil, @(-10), @(-10),
      @(optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS));
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result_coord[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kCoordinatesOutOfBounds);
}

// Test lookup on a non-form element returns kTargetNotAutofillElement.
TEST_F(AttemptFormFillingToolJavaScriptTest,
       GetNonFormElement_ReturnsErrorCodes) {
  // Node ID target
  int node_id = GetNodeId("not_input");
  NSDictionary* result_node = GetAutofillRendererIds(@(node_id), nil, nil, nil);
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result_node[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kTargetNotAutofillElement);

  // Coordinate target
  NSDictionary* rect = GetElementClientRect("not_input");
  int x = [rect[@"left"] intValue] + [rect[@"width"] intValue] / 2;
  int y = [rect[@"top"] intValue] + [rect[@"height"] intValue] / 2;
  NSDictionary* result_coord = GetAutofillRendererIds(
      nil, @(x), @(y),
      @(optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS));
  EXPECT_EQ(static_cast<AttemptFormFillingToolResultCode>(
                [result_coord[@"resultCode"] intValue]),
            AttemptFormFillingToolResultCode::kTargetNotAutofillElement);
}

}  // namespace actor
