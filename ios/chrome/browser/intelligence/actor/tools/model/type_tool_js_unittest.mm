// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <iterator>
#import <string>
#import <string_view>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
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

class TypeToolJavaScriptTestBase : public web::JavascriptTest {
 public:
  TypeToolJavaScriptTestBase() {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
    web_view().frame = CGRectMake(0.0, 0.0, 400.0, 400.0);
  }
  ~TypeToolJavaScriptTestBase() override = default;

 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddUserScript(@"dom_node_ids_test");
    AddUserScript(@"type_tool");

    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    base::FilePath html_path = test_data_dir.Append(FILE_PATH_LITERAL(
        "ios/testing/data/http_server_files/actor/type_tool_test.html"));
    std::string html;
    ASSERT_TRUE(base::ReadFileToString(html_path, &html));
    ASSERT_TRUE(LoadHtml(base::SysUTF8ToNSString(html)));
  }

  NSDictionary* TypeByCoordinate(const char* selector,
                                 int pixelType,
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
                                                   selector));
    id coordinate_result =
        web::test::ExecuteJavaScript(web_view(), coordinate_script);
    NSDictionary* coordinate_result_dict =
        base::apple::ObjCCast<NSDictionary>(coordinate_result);
    int x = [coordinate_result_dict[@"x"] intValue];
    int y = [coordinate_result_dict[@"y"] intValue];

    NSString* call_api_script = base::SysUTF8ToNSString(base::StringPrintf(
        R"(__gCrWeb.getRegisteredApi('type_tool').getFunction()"
        R"('type')({coordinate: {x: %d, y: %d, pixelType: %d}}, '%s', %d, %s))",
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
        R"('type')({contentNodeId: %d}, '%s', %d, %s))",
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

  std::string GetInputText(const char* selector) {
    id result = web::test::ExecuteJavaScript(
        web_view(), base::SysUTF8ToNSString(base::StringPrintf(
                        R"(
          (function() {
            const el = document.querySelector('%s');
            return (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')
                    ? el.value
                    : el.innerText;
          })();
        )",
                        selector)));
    NSString* resultString = base::apple::ObjCCast<NSString>(result);
    return base::SysNSStringToUTF8(resultString);
  }

  void SetInputText(const char* selector, const std::string& text) {
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
                        selector, text.c_str(), text.c_str())));
  }

  std::vector<EventInfo> ExpectedEvents(bool followByEnter = false) {
    std::vector<EventInfo> events;
    events.reserve(followByEnter ? 12 : 6);
    events.push_back({/*type=*/"keydown", /*bubbles=*/true,
                      /*cancelable=*/true, /*key=*/"", /*keyCode=*/0});
    events.push_back({/*type=*/"keypress", /*bubbles=*/true,
                      /*cancelable=*/true, /*key=*/"", /*keyCode=*/0});
    events.push_back({/*type=*/"beforeinput", /*bubbles=*/true,
                      /*cancelable=*/true, /*key=*/"", /*keyCode=*/0});
    events.push_back({/*type=*/"input", /*bubbles=*/true, /*cancelable=*/false,
                      /*key=*/"", /*keyCode=*/0});
    events.push_back({/*type=*/"keyup", /*bubbles=*/true,
                      /*cancelable=*/true, /*key=*/"", /*keyCode=*/0});
    events.push_back({/*type=*/"change", /*bubbles=*/true, /*cancelable=*/false,
                      /*key=*/"", /*keyCode=*/0});
    if (followByEnter) {
      events.push_back({/*type=*/"keydown", /*bubbles=*/true,
                        /*cancelable=*/true, /*key=*/"Enter", /*keyCode=*/13});
      events.push_back({/*type=*/"keypress", /*bubbles=*/true,
                        /*cancelable=*/true, /*key=*/"Enter", /*keyCode=*/13});
      events.push_back({/*type=*/"beforeinput", /*bubbles=*/true,
                        /*cancelable=*/true, /*key=*/"", /*keyCode=*/0});
      events.push_back({/*type=*/"input", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"", /*keyCode=*/0});
      events.push_back({/*type=*/"keyup", /*bubbles=*/true,
                        /*cancelable=*/true, /*key=*/"Enter", /*keyCode=*/13});
      events.push_back({/*type=*/"change", /*bubbles=*/true,
                        /*cancelable=*/false, /*key=*/"", /*keyCode=*/0});
    }
    return events;
  }

  // Returns the expected events for a single typing or Enter sequence where
  // `canceled_event` ("keydown", "keypress", or "beforeinput") has
  // `preventDefault()` called on it.
  std::vector<EventInfo> ExpectedEventsForCanceledSequence(
      std::string_view canceled_event,
      bool is_enter) {
    const char* key = is_enter ? "Enter" : "";
    const int key_code = is_enter ? 13 : 0;
    std::vector<EventInfo> events;
    events.reserve(4);
    events.push_back({/*type=*/"keydown", /*bubbles=*/true, /*cancelable=*/true,
                      key, key_code});
    if (canceled_event != "keydown") {
      events.push_back({/*type=*/"keypress", /*bubbles=*/true,
                        /*cancelable=*/true, key, key_code});
      if (canceled_event != "keypress") {
        events.push_back({/*type=*/"beforeinput", /*bubbles=*/true,
                          /*cancelable=*/true, /*key=*/"", /*keyCode=*/0});
      }
    }
    events.push_back({/*type=*/"keyup", /*bubbles=*/true, /*cancelable=*/true,
                      key, key_code});
    return events;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class TypeToolJavaScriptTest
    : public TypeToolJavaScriptTestBase,
      public ::testing::WithParamInterface<const char*> {
 protected:
  NSDictionary* TypeByCoordinate(int pixelType,
                                 const std::string& text,
                                 int typeMode,
                                 bool followByEnter) {
    return TypeToolJavaScriptTestBase::TypeByCoordinate(
        /*selector=*/GetParam(), pixelType, text, typeMode, followByEnter);
  }

  std::string GetInputText() {
    return TypeToolJavaScriptTestBase::GetInputText(/*selector=*/GetParam());
  }

  void SetInputText(const std::string& text) {
    TypeToolJavaScriptTestBase::SetInputText(/*selector=*/GetParam(), text);
  }
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

  NSDictionary* result =
      TypeByCoordinate(/*pixelType=*/1, /*text=*/"new text", /*typeMode=*/0,
                       /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  EXPECT_EQ([result[@"resultCode"] intValue],
            static_cast<int>(TypeToolResultCode::kInvalidArguments));
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

// Test that typing focuses the target element.
TEST_P(TypeToolJavaScriptTest, TypeFocusesTargetElement) {
  NSDictionary* result =
      TypeByCoordinate(/*pixelType=*/1, /*text=*/"hello", /*typeMode=*/3,
                       /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);

  NSNumber* is_active =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(),
          base::SysUTF8ToNSString(base::StringPrintf(
              R"(document.activeElement === document.querySelector('%s'))",
              GetParam()))));
  ASSERT_NE(is_active, nil);
  EXPECT_TRUE([is_active boolValue]);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TypeToolJavaScriptTest,
    // These are used by document.querySelector to get the target element.
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

// Test that text discarded by the element's value sanitizer still reports
// `kOk`. `<input type="number">` sanitizes a non-numeric assignment to the
// empty string, discarding the previous value with it.
TEST_F(TypeToolJavaScriptTestBase, TypeNonNumericIntoNumberInput_ReturnsOk) {
  SetInputText(/*selector=*/"input", /*text=*/"42");
  (void)web::test::ExecuteJavaScript(
      web_view(), @"document.querySelector('input').type = 'number';");

  NSDictionary* result = TypeByCoordinate(
      /*selector=*/"input", /*pixelType=*/1, /*text=*/"abc",
      /*typeMode=*/3, /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  EXPECT_EQ(GetInputText("input"), "");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

// Test that targeting a `contentEditable` container wrapping a single-child
// chain (`<div contenteditable="true"><p><span id="leaf">Foo</span></p></div>`)
// unwraps to the leaf `<span>` (ignoring formatting whitespace between tags),
// updates its text without clobbering `<p>` or `<span>`, and focuses the
// editing host.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableSingleChildChain_UnwrapsWithoutClobbering) {
  (void)web::test::ExecuteJavaScript(web_view(), base::SysUTF8ToNSString(R"(
        document.querySelector('div[contenteditable]').innerHTML = `
          <p>
            <span id="leaf">hello-</span>
          </p>
        `;
      )"));
  NSNumber* node_id_result =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"__gCrWeb.getRegisteredApi('dom_node_ids_test')"
                      @".getFunction('getOrCreateNodeId')("
                      @"document.querySelector('div[contenteditable]'));"));
  ASSERT_NE(node_id_result, nil);

  NSDictionary<NSString*, id>* result = TypeByNodeId(
      /*nodeId=*/[node_id_result intValue], /*text=*/"world", /*typeMode=*/3,
      /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  EXPECT_EQ(GetInputText("div[contenteditable] > p > span#leaf"),
            "hello-world");
  NSNumber* is_active = base::apple::ObjCCast<NSNumber>(
      web::test::ExecuteJavaScript(web_view(), @"document.activeElement === "
                                               @"document.querySelector("
                                               @"'div[contenteditable]');"));
  ASSERT_NE(is_active, nil);
  EXPECT_TRUE([is_active boolValue]);
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

// Test that a `contentEditable` element containing a `<br>` placeholder is
// treated as a leaf editable element and succeeds.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableWithBrPlaceholder_ReturnsOk) {
  (void)web::test::ExecuteJavaScript(
      web_view(),
      @"document.querySelector('div[contenteditable]').innerHTML = '<br>';");

  NSDictionary<NSString*, id>* result = TypeByCoordinate(
      /*selector=*/"div[contenteditable]", /*pixelType=*/1, /*text=*/"typed",
      /*typeMode=*/1, /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  EXPECT_EQ(GetInputText("div[contenteditable]"), "typed");
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

// Test that targeting a `contentEditable` container wrapping a single `<input>`
// unwraps to the child `<input>`, updates its `.value` without destroying the
// `<input>` element, and returns `kElementDisabled` if the wrapped `<input>` is
// disabled.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableWrappingInput_TypesIntoInput) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"document.querySelector('div[contenteditable]').innerHTML = "
                  @"'<input id=\"nested-input\" value=\"start-\">';");
  NSNumber* node_id_result =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"__gCrWeb.getRegisteredApi('dom_node_ids_test')"
                      @".getFunction('getOrCreateNodeId')("
                      @"document.querySelector('div[contenteditable]'));"));
  ASSERT_NE(node_id_result, nil);

  NSDictionary<NSString*, id>* result = TypeByNodeId(
      /*nodeId=*/[node_id_result intValue], /*text=*/"end", /*typeMode=*/3,
      /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  EXPECT_EQ(GetInputText("#nested-input"), "start-end");
  NSNumber* is_active =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"document.activeElement === "
                      @"document.querySelector('#nested-input');"));
  ASSERT_NE(is_active, nil);
  EXPECT_TRUE([is_active boolValue]);
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());

  // When the wrapped `<input>` is disabled, typing into the container returns
  // `kElementDisabled`.
  (void)web::test::ExecuteJavaScript(
      web_view(), @"window.capturedEvents = [];"
                  @"document.querySelector('#nested-input').disabled = true;");
  NSDictionary<NSString*, id>* disabled_result = TypeByNodeId(
      /*nodeId=*/[node_id_result intValue], /*text=*/"blocked", /*typeMode=*/3,
      /*followByEnter=*/false);
  ASSERT_NE(disabled_result, nil);
  ASSERT_NE(disabled_result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>(
                [disabled_result[@"resultCode"] intValue]),
            TypeToolResultCode::kElementDisabled);
  EXPECT_EQ(GetInputText("#nested-input"), "start-end");
  EXPECT_TRUE(GetCapturedEvents().empty());
}

// Test that targeting a `contentEditable` container with multiple child
// elements returns `kTypeTargetNotFocusable` without clobbering the children.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableContainerWithMultipleChildren_ReturnsNotFocusable) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"document.querySelector('div[contenteditable]').innerHTML = "
                  @"'<p id=\"child1\">first</p><p id=\"child2\">second</p>';");
  NSNumber* container_node_id =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"__gCrWeb.getRegisteredApi('dom_node_ids_test')"
                      @".getFunction('getOrCreateNodeId')("
                      @"document.querySelector('div[contenteditable]'));"));
  ASSERT_NE(container_node_id, nil);

  NSDictionary<NSString*, id>* result = TypeByNodeId(
      /*nodeId=*/[container_node_id intValue], /*text=*/"clobber",
      /*typeMode=*/1, /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kTypeTargetNotFocusable);
  EXPECT_EQ(GetInputText("#child1"), "first");
  EXPECT_EQ(GetInputText("#child2"), "second");
  EXPECT_TRUE(GetCapturedEvents().empty());
}

// Test that targeting a specific editable child inside a multi-child
// `contentEditable` container updates only that child and focuses the root
// editing host.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableSpecificChild_UpdatesChildAndFocusesHost) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"document.querySelector('div[contenteditable]').innerHTML = "
                  @"'<p id=\"child1\">first</p><p id=\"child2\">second</p>';");
  NSNumber* child1_node_id =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"__gCrWeb.getRegisteredApi('dom_node_ids_test')"
                      @".getFunction('getOrCreateNodeId')("
                      @"document.querySelector('#child1'));"));
  ASSERT_NE(child1_node_id, nil);

  NSDictionary<NSString*, id>* result = TypeByNodeId(
      /*nodeId=*/[child1_node_id intValue], /*text=*/"updated",
      /*typeMode=*/1, /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  EXPECT_EQ(GetInputText("#child1"), "updated");
  EXPECT_EQ(GetInputText("#child2"), "second");
  NSNumber* is_active = base::apple::ObjCCast<NSNumber>(
      web::test::ExecuteJavaScript(web_view(), @"document.activeElement === "
                                               @"document.querySelector("
                                               @"'div[contenteditable]');"));
  ASSERT_NE(is_active, nil);
  EXPECT_TRUE([is_active boolValue]);
  EXPECT_EQ(GetCapturedEvents(), ExpectedEvents());
}

// Test that targeting a `contentEditable` container that mixes direct
// non-whitespace text with a child element returns `kTypeTargetNotFocusable`
// without clobbering the child element.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableMixedTextAndChildElement_ReturnsNotFocusable) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @"document.querySelector('div[contenteditable]').innerHTML = "
                  @"'prefix <span id=\"mixed-child\">suffix</span>';");
  NSNumber* container_node_id =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"__gCrWeb.getRegisteredApi('dom_node_ids_test')"
                      @".getFunction('getOrCreateNodeId')("
                      @"document.querySelector('div[contenteditable]'));"));
  ASSERT_NE(container_node_id, nil);

  NSDictionary<NSString*, id>* result = TypeByNodeId(
      /*nodeId=*/[container_node_id intValue], /*text=*/"clobber",
      /*typeMode=*/1, /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kTypeTargetNotFocusable);
  EXPECT_EQ(GetInputText("#mixed-child"), "suffix");
  EXPECT_TRUE(GetCapturedEvents().empty());
}

// Test that targeting a `contentEditable` container wrapping a child with
// `contenteditable="false"` returns `kTypeTargetNotFocusable` without modifying
// the non-editable child.
TEST_F(TypeToolJavaScriptTestBase,
       TypeContentEditableNonEditableChild_ReturnsNotFocusable) {
  (void)web::test::ExecuteJavaScript(
      web_view(),
      @"document.querySelector('div[contenteditable]').innerHTML = "
      @"'<span id=\"non-editable\" contenteditable=\"false\">locked</span>';");
  NSNumber* container_node_id =
      base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
          web_view(), @"__gCrWeb.getRegisteredApi('dom_node_ids_test')"
                      @".getFunction('getOrCreateNodeId')("
                      @"document.querySelector('div[contenteditable]'));"));
  ASSERT_NE(container_node_id, nil);

  NSDictionary<NSString*, id>* result = TypeByNodeId(
      /*nodeId=*/[container_node_id intValue], /*text=*/"clobber",
      /*typeMode=*/1, /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kTypeTargetNotFocusable);
  EXPECT_EQ(GetInputText("#non-editable"), "locked");
  EXPECT_TRUE(GetCapturedEvents().empty());
}

// Parameterized by the canceled event name ("keydown", "keypress", or
// "beforeinput") to verify that the tool still succeeds on `<input>`.
class TypeToolPreventDefaultTest
    : public TypeToolJavaScriptTestBase,
      public ::testing::WithParamInterface<const char*> {
 protected:
  static constexpr char kInputSelector[] = "input";

  const char* canceled_event() const { return GetParam(); }

  // Installs a one-time listener that calls `preventDefault()` on
  // `canceled_event()` and records that it ran. When `after_change` is set,
  // this listener is preceded by a one-time `change` listener, to intercept
  // the Enter sequence that follows the initial typing.
  void CancelNextEvent(bool after_change) {
    std::string listener = base::StringPrintf(
        R"(document.addEventListener('%s', (e) => {
             window.testPreventDefaultInvoked = true;
             e.preventDefault();
           }, {once: true});)",
        canceled_event());
    if (after_change) {
      listener = base::StringPrintf(
          R"(document.addEventListener('change', () => { %s }, {once: true});)",
          listener.c_str());
    }
    (void)web::test::ExecuteJavaScript(
        web_view(),
        base::SysUTF8ToNSString("window.testPreventDefaultInvoked = false;" +
                                listener));
  }

  bool PreventDefaultInvoked() {
    NSNumber* invoked =
        base::apple::ObjCCast<NSNumber>(web::test::ExecuteJavaScript(
            web_view(), @"window.testPreventDefaultInvoked;"));
    return invoked != nil && [invoked boolValue];
  }
};

// Test that when an event listener calls `preventDefault()` on a cancelable
// typing event, the tool returns `kOk`, leaves the value untouched, and
// suppresses subsequent events.
TEST_P(TypeToolPreventDefaultTest, Type_PreventDefaultCalled_ReturnsOk) {
  CancelNextEvent(/*after_change=*/false);

  NSDictionary* result =
      TypeByCoordinate(/*selector=*/kInputSelector, /*pixelType=*/1,
                       /*text=*/"hello", /*typeMode=*/3,
                       /*followByEnter=*/false);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  // The canceled event suppresses the insertion, so the field stays empty.
  EXPECT_EQ(GetInputText(kInputSelector), "");
  EXPECT_EQ(GetCapturedEvents(),
            ExpectedEventsForCanceledSequence(canceled_event(),
                                              /*is_enter=*/false));
  EXPECT_TRUE(PreventDefaultInvoked());
}

// Test that when an event listener calls `preventDefault()` on a cancelable
// Enter event, the tool returns `kOk`, keeps the typed text, and suppresses
// subsequent Enter events per W3C UI Events.
TEST_P(TypeToolPreventDefaultTest,
       FollowByEnter_PreventDefaultCalled_ReturnsOk) {
  CancelNextEvent(/*after_change=*/true);

  NSDictionary* result =
      TypeByCoordinate(/*selector=*/kInputSelector, /*pixelType=*/1,
                       /*text=*/"submit", /*typeMode=*/3,
                       /*followByEnter=*/true);

  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_EQ(static_cast<TypeToolResultCode>([result[@"resultCode"] intValue]),
            TypeToolResultCode::kOk);
  EXPECT_EQ(GetInputText(kInputSelector), "submit");

  std::vector<EventInfo> expected = ExpectedEvents(/*followByEnter=*/false);
  std::vector<EventInfo> enter_events =
      ExpectedEventsForCanceledSequence(canceled_event(), /*is_enter=*/true);
  expected.reserve(expected.size() + enter_events.size());
  expected.insert(expected.end(), std::make_move_iterator(enter_events.begin()),
                  std::make_move_iterator(enter_events.end()));
  EXPECT_EQ(GetCapturedEvents(), expected);

  EXPECT_TRUE(PreventDefaultInvoked());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TypeToolPreventDefaultTest,
    // These are the events that can be canceled with `preventDefault()`.
    ::testing::Values("keydown", "keypress", "beforeinput"),
    [](const ::testing::TestParamInfo<TypeToolPreventDefaultTest::ParamType>&
           info) { return std::string(info.param); });

}  // namespace actor
