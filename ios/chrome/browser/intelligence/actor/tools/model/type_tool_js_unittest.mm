// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <set>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

struct EventInfo {
  std::string type;
  bool bubbles;
  bool cancelable;
  std::string key;
  int keyCode;

  bool operator==(const EventInfo& other) const {
    return type == other.type && bubbles == other.bubbles &&
           cancelable == other.cancelable && key == other.key &&
           keyCode == other.keyCode;
  }
};

}  // namespace

class TypeToolJavaScriptTest
    : public web::JavascriptTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  TypeToolJavaScriptTest() {
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
    AddUserScript(@"type_tool");

    ASSERT_TRUE(
        LoadUrl(GURL(test_server_.GetURL("/actor/type_tool_test.html"))));
  }

  NSDictionary* TypeByCoordinate(int pixelType,
                                 const std::string& text,
                                 int typeMode,
                                 bool followByEnter) {
    // Get the input fields coordinates from JS.
    NSString* coordinate_script =
        base::SysUTF8ToNSString(base::StringPrintf(R"(
      (function() {
        const input = document.querySelector('%s');
        const rect = input.getBoundingClientRect();
        const x = rect.left + rect.width / 2;
        const y = rect.top + rect.height / 2;
        return {x, y};
      })();
      )",
                                                   GetParam()));
    id coordinate_result =
        web::test::ExecuteJavaScript(web_view(), coordinate_script);
    NSDictionary* coordinate_result_dict =
        base::apple::ObjCCast<NSDictionary>(coordinate_result);
    int x = [coordinate_result_dict[@"x"] intValue];
    int y = [coordinate_result_dict[@"y"] intValue];

    NSString* call_api_script = base::SysUTF8ToNSString(base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('type_tool').getFunction()"
        R"('typeByCoordinate')(%d, %d, %d, '%s', %d, %s))",
        x, y, pixelType, text.c_str(), typeMode,
        followByEnter ? "true" : "false"));

    id result = web::test::ExecuteJavaScript(web_view(), call_api_script);
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  NSDictionary* TypeByNodeId(int nodeId,
                             const std::string& text,
                             int typeMode,
                             bool followByEnter) {
    NSString* script = base::SysUTF8ToNSString(base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('type_tool').getFunction()"
        R"('typeByNodeId')(%d, '%s', %d, %s))",
        nodeId, text.c_str(), typeMode, followByEnter ? "true" : "false"));

    id result = web::test::ExecuteJavaScript(web_view(), script);
    NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
    return resultDict;
  }

  std::vector<EventInfo> GetCapturedEvents() {
    NSString* eventsJson =
        web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
          JSON.stringify(window.capturedEvents)
        )"));
    NSData* data = [eventsJson dataUsingEncoding:NSUTF8StringEncoding];
    NSArray* eventsArray = [NSJSONSerialization JSONObjectWithData:data
                                                           options:0
                                                             error:nil];
    std::vector<EventInfo> captured_events;
    for (NSDictionary* eventDict in eventsArray) {
      EventInfo info;
      info.type = base::SysNSStringToUTF8(eventDict[@"type"]);
      info.bubbles = [eventDict[@"bubbles"] boolValue];
      info.cancelable = [eventDict[@"cancelable"] boolValue];
      info.key = base::SysNSStringToUTF8(eventDict[@"key"]);
      info.keyCode = [eventDict[@"keyCode"] intValue];
      captured_events.push_back(info);
    }
    return captured_events;
  }

  std::string GetInputText() {
    id result = web::test::ExecuteJavaScript(
        web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
          (function() {
            const el = document.querySelector('%s');
            return (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')
                    ? el.value
                    : el.innerText;
          })();
        )",
                                                               GetParam())));
    NSString* resultString = base::apple::ObjCCast<NSString>(result);
    return base::SysNSStringToUTF8(resultString);
  }

  void SetInputText(const std::string& text) {
    (void)web::test::ExecuteJavaScript(
        web_view(), base::SysUTF8ToNSString(base::StringPrintf(
                        R"(
          (function() {
            const el = document.querySelector('%s');
            if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') {
              el.value = '%s';
            } else {
              el.innerText = '%s';
            }
          })();
        )",
                        GetParam(), text.c_str(), text.c_str())));
  }

  std::vector<EventInfo> ExpectedEvents(bool followByEnter = false) {
    std::vector<EventInfo> events;
    std::string param = GetParam();
    events = {
        {/*type=*/"keydown", /*bubbles=*/true, /*cancelable=*/false,
         /*key=*/"", /*keyCode=*/0},
        {/*type=*/"keypress", /*bubbles=*/true, /*cancelable=*/false,
         /*key=*/"", /*keyCode=*/0},
        {/*type=*/"input", /*bubbles=*/true, /*cancelable=*/false, /*key=*/"",
         /*keyCode=*/0},
        {/*type=*/"keyup", /*bubbles=*/true, /*cancelable=*/false, /*key=*/"",
         /*keyCode=*/0},
        {/*type=*/"change", /*bubbles=*/true, /*cancelable=*/false,
         /*key=*/"", /*keyCode=*/0},
    };
    if (followByEnter) {
      events.push_back({/*type=*/"keydown", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"Enter", /*keyCode=*/13});
      events.push_back({/*type=*/"keypress", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"Enter", /*keyCode=*/13});
      events.push_back({/*type=*/"input", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"", /*keyCode=*/0});
      events.push_back({/*type=*/"keyup", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"Enter", /*keyCode=*/13});
      events.push_back({/*type=*/"change", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"", /*keyCode=*/0});
    }
    return events;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

TEST_P(TypeToolJavaScriptTest, TypeByCoordinate_Success) {
  NSDictionary* result =
      TypeByCoordinate(/*pixelType=*/1, /*text=*/"hello", /*typeMode=*/3,
                       /*followByEnter=*/false);

  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(TypeToolResultCode::kOk));
  EXPECT_EQ(GetInputText(), "hello");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

TEST_P(TypeToolJavaScriptTest, TypeByNodeId_Success) {
  id nodeIdResult = web::test::ExecuteJavaScript(
      web_view(), base::SysUTF8ToNSString(base::StringPrintf(R"(
        var el = document.querySelector('%s');
        __gCrWeb.getRegisteredApi('dom_node_ids_test')
                .getFunction('getOrCreateNodeId')(el);
      )",
                                                             GetParam())));
  int nodeId = [nodeIdResult intValue];

  NSDictionary* result = TypeByNodeId(nodeId, /*text=*/"world", /*typeMode=*/3,
                                      /*followByEnter=*/false);

  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(TypeToolResultCode::kOk));
  EXPECT_EQ(GetInputText(), "world");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

TEST_P(TypeToolJavaScriptTest, TypeMode_Append) {
  SetInputText(/*text=*/"hello-");

  TypeByCoordinate(/*pixelType=*/1, /*text=*/"world", /*typeMode=*/3,
                   /*followByEnter=*/false);

  EXPECT_EQ(GetInputText(), "hello-world");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

TEST_P(TypeToolJavaScriptTest, TypeMode_Prepend) {
  SetInputText(/*text=*/"-world");

  TypeByCoordinate(/*pixelType=*/1, /*text=*/"hello", /*typeMode=*/2,
                   /*followByEnter=*/false);

  EXPECT_EQ(GetInputText(), "hello-world");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

TEST_P(TypeToolJavaScriptTest, TypeMode_DeleteExisting) {
  SetInputText(/*text=*/"old text");

  TypeByCoordinate(/*pixelType=*/1, /*text=*/"new text", /*typeMode=*/1,
                   /*followByEnter=*/false);

  EXPECT_EQ(GetInputText(), "new text");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

TEST_P(TypeToolJavaScriptTest, TypeMode_UnknownModeUnsupported) {
  SetInputText(/*text=*/"old text");

  TypeByCoordinate(/*pixelType=*/1, /*text=*/"new text", /*typeMode=*/0,
                   /*followByEnter=*/false);

  EXPECT_EQ(GetInputText(), "old text");
  EXPECT_TRUE(GetCapturedEvents().empty());
}

TEST_P(TypeToolJavaScriptTest, FollowByEnter) {
  NSDictionary* result =
      TypeByCoordinate(/*pixelType=*/1, /*text=*/"submit", /*typeMode=*/3,
                       /*followByEnter=*/true);

  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(TypeToolResultCode::kOk));
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents(/*followByEnter=*/true));
}

TEST_P(TypeToolJavaScriptTest, TargetedElementDisabled_Fails) {
  SetInputText(/*text=*/"old text");
  // Disable the <input>
  (void)web::test::ExecuteJavaScript(
      web_view(),
      base::SysUTF8ToNSString(base::StringPrintf(
          "document.querySelector('%s').disabled = true;", GetParam())));

  NSDictionary* result =
      TypeByCoordinate(/*pixelType=*/1, /*text=*/"new text", /*typeMode=*/3,
                       /*followByEnter=*/false);

  EXPECT_EQ(GetInputText(), "old text");
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kElementDisabled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TypeToolJavaScriptTest,
    //  These are used by document.querySelector to get the target element.
    ::testing::Values("input", "textarea", "div[contenteditable]"),
    // Output the selector for context when debugging.
    [](const ::testing::TestParamInfo<TypeToolJavaScriptTest::ParamType>&
           info) {
      std::string name = info.param;
      if (name == "div[contenteditable]") {
        return std::string("div_contenteditable");
      }
      return name;
    });

}  // namespace actor
