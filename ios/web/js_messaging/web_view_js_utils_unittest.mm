// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_js_utils.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/test/fakes/crw_fake_script_message_handler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace web {

namespace {
// Mock implementation of `__gCrWeb.message.getExistingFrames` which counts the
// number of times the function is called.
NSString* const kMockGetExistingFramesScript =
    @"var getExistingFramesCallCount = 0;"
    @"__gCrWeb = {};"
    @"__gCrWeb['message'] = {};"
    @"__gCrWeb.message['getExistingFrames'] = function() {"
    @"  getExistingFramesCallCount++;"
    @"};"
    @"true;";

// Returns the WKFrameInfo instance for the main frame of `web_view`.
WKFrameInfo* GetMainFrameWKFrameInfo(WKWebView* web_view) {
  // Setup a message handler and receive a message to obtain a WKFrameInfo
  // instance.
  CRWFakeScriptMessageHandler* script_message_handler =
      [[CRWFakeScriptMessageHandler alloc] init];
  [web_view.configuration.userContentController
      addScriptMessageHandler:script_message_handler
                         name:@"TestHandler"];
  web::ExecuteJavaScript(
      web_view,
      @"window.webkit.messageHandlers['TestHandler'].postMessage({});",
      /*completion_handler=*/nil);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!script_message_handler.lastReceivedScriptMessage.frameInfo;
  }));

  return script_message_handler.lastReceivedScriptMessage.frameInfo;
}

// Sets up the mock script `kMockGetExistingFramesScript` in the given web view,
// frame, and content world.
void SetupMockGetExistingFramesScript(WKWebView* web_view,
                                      WKFrameInfo* frame_info,
                                      WKContentWorld* content_world) {
  __block bool js_execution_complete = false;
  web::ExecuteJavaScript(web_view, content_world, frame_info,
                         kMockGetExistingFramesScript,
                         ^(id block_result, NSError* block_error) {
                           ASSERT_FALSE(block_error);
                           js_execution_complete = true;
                         });
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return js_execution_complete;
  }));
}

// Returns the number of times that the mock function setup by
// `SetupMockGetExistingFramesScript` has been called.
int GetExistingFramesScriptCallCount(WKWebView* web_view,
                                     WKFrameInfo* frame_info,
                                     WKContentWorld* content_world) {
  __block int function_call_count = -1;
  __block bool js_execution_complete = false;
  web::ExecuteJavaScript(web_view, content_world, frame_info,
                         @"getExistingFramesCallCount",
                         ^(id block_result, NSError* block_error) {
                           ASSERT_FALSE(block_error);
                           function_call_count = [block_result intValue];
                           js_execution_complete = true;
                         });
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return js_execution_complete;
  }));
  return function_call_count;
}

}  // namespace

using WebViewJsUtilsTest = PlatformTest;

// Tests that ValueResultFromWKResult converts nil value to nullptr.
TEST_F(WebViewJsUtilsTest, ValueResultFromUndefinedWKResult) {
  EXPECT_FALSE(ValueResultFromWKResult(nil));
}

// Tests that ValueResultFromWKResult converts string to Value::Type::STRING.
TEST_F(WebViewJsUtilsTest, ValueResultFromStringWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@"test"));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::Type::STRING, value->type());
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("test", value->GetString());
}

// Tests that ValueResultFromWKResult converts inetger to Value::Type::DOUBLE.
// NOTE: WKWebView API returns all numbers as kCFNumberFloat64Type, so there is
// no way to tell if the result is integer or double.
TEST_F(WebViewJsUtilsTest, ValueResultFromIntegerWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@1));
  EXPECT_TRUE(value);
  ASSERT_EQ(base::Value::Type::DOUBLE, value->type());
  EXPECT_EQ(1, value->GetDouble());
}

// Tests that ValueResultFromWKResult converts double to Value::Type::DOUBLE.
TEST_F(WebViewJsUtilsTest, ValueResultFromDoubleWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@3.14));
  EXPECT_TRUE(value);
  ASSERT_EQ(base::Value::Type::DOUBLE, value->type());
  EXPECT_EQ(3.14, value->GetDouble());
}

// Tests that ValueResultFromWKResult converts bool to Value::Type::BOOLEAN.
TEST_F(WebViewJsUtilsTest, ValueResultFromBoolWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@YES));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_TRUE(value->GetBool());
}

// Tests that ValueResultFromWKResult converts null to Value::Type::NONE.
TEST_F(WebViewJsUtilsTest, ValueResultFromNullWKResult) {
  std::unique_ptr<base::Value> value(
      web::ValueResultFromWKResult([NSNull null]));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::Type::NONE, value->type());
}

// Tests that ValueResultFromWKResult converts NSDictionaries to properly
// initialized base::DictionaryValue.
TEST_F(WebViewJsUtilsTest, ValueResultFromDictionaryWKResult) {
  NSDictionary* test_dictionary =
      @{@"Key1" : @"Value1",
        @"Key2" : @{@"Key3" : @42}};

  std::unique_ptr<base::Value> value(
      web::ValueResultFromWKResult(test_dictionary));
  base::Value::Dict* dictionary = value->GetIfDict();
  EXPECT_NE(nullptr, dictionary);

  std::string* value1 = dictionary->FindString("Key1");
  EXPECT_EQ("Value1", *value1);

  base::Value::Dict const* inner_dictionary = dictionary->FindDict("Key2");
  EXPECT_NE(nullptr, inner_dictionary);

  EXPECT_EQ(42, *inner_dictionary->FindDouble("Key3"));
}

// Tests that ValueResultFromWKResult converts NSArray to properly
// initialized base::ListValue.
TEST_F(WebViewJsUtilsTest, ValueResultFromArrayWKResult) {
  NSArray* test_array = @[ @"Value1", @[ @YES ], @42 ];

  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(test_array));
  ASSERT_TRUE(value->is_list());
  const base::Value::List& list = value->GetList();

  size_t list_size = 3;
  ASSERT_EQ(list_size, list.size());

  ASSERT_TRUE(list[0].is_string());
  std::string value1 = list[0].GetString();
  EXPECT_EQ("Value1", value1);

  EXPECT_TRUE(list[1].is_list());

  ASSERT_TRUE(list[2].is_double());
  double value3 = list[2].GetDouble();
  EXPECT_EQ(42, value3);
}

// Tests that an NSDictionary with a cycle does not cause infinite recursion.
TEST_F(WebViewJsUtilsTest, ValueResultFromDictionaryWithDepthCheckWKResult) {
  // Create a dictionary with a cycle.
  NSMutableDictionary* test_dictionary =
      [NSMutableDictionary dictionaryWithCapacity:1];
  NSMutableDictionary* test_dictionary_2 =
      [NSMutableDictionary dictionaryWithCapacity:1];
  const char* key = "key";
  NSString* obj_c_key = [NSString stringWithCString:key
                                           encoding:NSASCIIStringEncoding];
  test_dictionary[obj_c_key] = test_dictionary_2;
  test_dictionary_2[obj_c_key] = test_dictionary;

  // Break the retain cycle so that the dictionaries are freed.
  absl::Cleanup cycle_breaker = ^{
    [test_dictionary_2 removeAllObjects];
  };

  // Check that parsing the dictionary stopped at a depth of
  // `kMaximumParsingRecursionDepth`.
  std::unique_ptr<base::Value> value =
      web::ValueResultFromWKResult(test_dictionary);
  base::Value::Dict* current_dictionary = value->GetIfDict();
  base::Value::Dict* inner_dictionary = nullptr;

  EXPECT_NE(nullptr, current_dictionary);

  for (int current_depth = 0; current_depth <= kMaximumParsingRecursionDepth;
       current_depth++) {
    EXPECT_NE(nullptr, current_dictionary);
    inner_dictionary = current_dictionary->FindDict(key);
    current_dictionary = inner_dictionary;
  }
  EXPECT_EQ(nullptr, current_dictionary);
}

// Tests that an NSArray with a cycle does not cause infinite recursion.
TEST_F(WebViewJsUtilsTest, ValueResultFromArrayWithDepthCheckWKResult) {
  // Create an array with a cycle.
  NSMutableArray* test_array = [NSMutableArray arrayWithCapacity:1];
  NSMutableArray* test_array_2 = [NSMutableArray arrayWithCapacity:1];
  test_array[0] = test_array_2;
  test_array_2[0] = test_array;

  // Break the retain cycle so that the arrays are freed.
  absl::Cleanup cycle_breaker = ^{
    [test_array removeAllObjects];
  };

  // Check that parsing the array stopped at a depth of
  // `kMaximumParsingRecursionDepth`.
  std::unique_ptr<base::Value> value = web::ValueResultFromWKResult(test_array);
  base::Value::List* current_list = nullptr;
  base::Value::List* inner_list = nullptr;

  ASSERT_TRUE(value->is_list());
  current_list = &value->GetList();

  for (int current_depth = 0; current_depth <= kMaximumParsingRecursionDepth;
       current_depth++) {
    ASSERT_TRUE(current_list);

    inner_list = nullptr;
    if (!current_list->empty())
      inner_list = (*current_list)[0].GetIfList();
    current_list = inner_list;
  }
  EXPECT_FALSE(current_list);
}

// Tests that NSObjectFromValueResult converts nullptr to nil.
TEST_F(WebViewJsUtilsTest, NSObjectFromNullptr) {
  id wk_result = web::NSObjectFromValueResult(nullptr);
  EXPECT_FALSE(wk_result);
}

// Tests that NSObjectFromValueResult converts Value::Type::STRING to NSString.
TEST_F(WebViewJsUtilsTest, NSObjectFromStringValueResult) {
  auto value = std::make_unique<base::Value>("test");
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSString class]]);
  EXPECT_NSEQ(@"test", wk_result);
}

// Tests that NSObjectFromValueResult converts Value::Type::INT to NSNumber.
TEST_F(WebViewJsUtilsTest, NSObjectFromIntValueResult) {
  auto value = std::make_unique<base::Value>(1);
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ(1, [wk_result intValue]);
}

// Tests that NSObjectFromValueResult converts Value::Type::DOUBLE to NSNumber.
TEST_F(WebViewJsUtilsTest, NSObjectFromDoubleValueResult) {
  auto value = std::make_unique<base::Value>(3.14);
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ(3.14, [wk_result doubleValue]);
}

// Tests that NSObjectFromValueResult converts Value::Type::BOOLEAN to NSNumber.
TEST_F(WebViewJsUtilsTest, NSObjectFromBoolValueResult) {
  auto value = std::make_unique<base::Value>(true);
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ(YES, [wk_result boolValue]);

  value.reset(new base::Value(false));
  wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ(NO, [wk_result boolValue]);
}

// Tests that NSObjectFromValueResult converts Value::Type::NONE to NSNull.
TEST_F(WebViewJsUtilsTest, NSObjectFromNoneValueResult) {
  auto value = std::make_unique<base::Value>();
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSNull class]]);
}

// Tests that NSObjectFromValueResult converts Value::Type::DICT to
// NSDictionary.
TEST_F(WebViewJsUtilsTest, NSObjectFromDictValueResult) {
  base::Value::Dict test_dict;
  test_dict.Set("Key1", "Value1");

  base::Value::Dict inner_test_dict;
  inner_test_dict.Set("Key3", 42);
  test_dict.Set("Key2", std::move(inner_test_dict));

  auto value = std::make_unique<base::Value>(std::move(test_dict));
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSDictionary class]]);

  NSDictionary* wk_result_dictionary =
      base::apple::ObjCCastStrict<NSDictionary>(wk_result);
  EXPECT_NSEQ(@"Value1", wk_result_dictionary[@"Key1"]);

  NSDictionary* inner_dictionary = wk_result_dictionary[@"Key2"];
  EXPECT_TRUE(inner_dictionary);
  EXPECT_NSEQ(@(42), inner_dictionary[@"Key3"]);
}

// Tests that NSObjectFromValueResult converts Value::Type::LIST to NSArray.
TEST_F(WebViewJsUtilsTest, NSObjectFromListValueResult) {
  base::Value::List test_list;
  test_list.Append("Value1");

  base::Value::List inner_test_list;
  inner_test_list.Append(true);
  test_list.Append(std::move(inner_test_list));

  test_list.Append(42);

  auto value = std::make_unique<base::Value>(std::move(test_list));
  id wk_result = web::NSObjectFromValueResult(value.get());
  EXPECT_TRUE(wk_result);
  EXPECT_TRUE([wk_result isKindOfClass:[NSArray class]]);

  NSArray* wk_result_array = base::apple::ObjCCastStrict<NSArray>(wk_result);

  EXPECT_EQ(3UL, wk_result_array.count);
  EXPECT_NSEQ(@"Value1", wk_result_array[0]);

  NSArray* inner_array = wk_result_array[1];
  EXPECT_TRUE(inner_array);
  EXPECT_TRUE([inner_array isKindOfClass:[NSArray class]]);

  EXPECT_NSEQ(@(42), wk_result_array[2]);
}

// Tests that ExecuteJavaScript returns an error if there is no web view.
TEST_F(WebViewJsUtilsTest, ExecuteJavaScriptNoWebView) {
  __block bool complete = false;
  __block id block_result = nil;
  __block NSError* block_error = nil;
  web::ExecuteJavaScript(nil, @"return true;", ^(id result, NSError* error) {
    block_result = [result copy];
    block_error = [error copy];
    complete = true;
  });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return complete;
  }));

  EXPECT_TRUE(block_error);
  EXPECT_FALSE(block_result);
}

// Tests that javascript can be executed.
TEST_F(WebViewJsUtilsTest, ExecuteJavaScript) {
  WKWebView* web_view = [[WKWebView alloc] init];

  __block bool complete = false;
  __block id block_result = nil;
  __block NSError* block_error = nil;
  web::ExecuteJavaScript(web_view, @"true", ^(id result, NSError* error) {
    block_result = [result copy];
    block_error = [error copy];
    complete = true;
  });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return complete;
  }));

  EXPECT_FALSE(block_error);
  EXPECT_TRUE(block_result);
}

// Tests that javascript can be executed in the page content world when the page
// content world and web frame are both specified.
TEST_F(WebViewJsUtilsTest, ExecuteJavaScriptPageContentWorld) {
  WKWebView* web_view = [[WKWebView alloc] init];
  WKFrameInfo* frame_info = GetMainFrameWKFrameInfo(web_view);
  ASSERT_TRUE(frame_info);

  __block bool complete = false;
  __block id result = nil;
  __block NSError* error = nil;

  __block bool set_value_complete = false;
  __block NSError* set_value_error = nil;

  // Set `value` in the page content world.
  web::ExecuteJavaScript(web_view, WKContentWorld.pageWorld, frame_info,
                         @"var value = 3;",
                         ^(id innerResult, NSError* innerError) {
                           set_value_error = [innerError copy];
                           set_value_complete = true;
                         });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return set_value_complete;
  }));
  ASSERT_FALSE(set_value_error);

  // Ensure the value can be accessed when specifying `frame_info`.
  web::ExecuteJavaScript(web_view, WKContentWorld.pageWorld, frame_info,
                         @"value", ^(id block_result, NSError* block_error) {
                           result = [block_result copy];
                           error = [block_error copy];
                           complete = true;
                         });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return complete;
  }));

  EXPECT_FALSE(error);
  EXPECT_TRUE(result);
  EXPECT_NSEQ(@(3), result);
}

// Tests that javascript can be executed in an isolated content world and that
// it can not be accessed from the page content world.
TEST_F(WebViewJsUtilsTest, ExecuteJavaScriptIsolatedWorld) {
  WKWebView* web_view = [[WKWebView alloc] init];
  WKFrameInfo* frame_info = GetMainFrameWKFrameInfo(web_view);
  ASSERT_TRUE(frame_info);

  __block bool set_value_complete = false;
  __block NSError* set_value_error = nil;
  // Set `value` in the page content world.
  web::ExecuteJavaScript(web_view, WKContentWorld.defaultClientWorld,
                         frame_info, @"var value = 3;",
                         ^(id result, NSError* error) {
                           set_value_error = [error copy];
                           set_value_complete = true;
                         });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return set_value_complete;
  }));
  ASSERT_FALSE(set_value_error);

  __block bool isolated_world_complete = false;
  __block id isolated_world_result = nil;
  __block NSError* isolated_world_error = nil;
  // Ensure the value can be accessed when specifying an isolated world and
  // `frame_info`.
  web::ExecuteJavaScript(web_view, WKContentWorld.defaultClientWorld,
                         frame_info, @"value",
                         ^(id block_result, NSError* block_error) {
                           isolated_world_result = [block_result copy];
                           isolated_world_error = [block_error copy];
                           isolated_world_complete = true;
                         });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return isolated_world_complete;
  }));

  EXPECT_FALSE(isolated_world_error);
  EXPECT_TRUE(isolated_world_result);
  EXPECT_NSEQ(@(3), isolated_world_result);

  __block bool page_world_complete = false;
  __block id page_world_result = nil;
  __block NSError* page_world_error = nil;
  // The value should not be accessible from the page content world.
  web::ExecuteJavaScript(web_view, WKContentWorld.pageWorld, frame_info,
                         @"try { value } catch (error) { false }",
                         ^(id block_result, NSError* block_error) {
                           page_world_result = [block_result copy];
                           page_world_error = [block_error copy];
                           page_world_complete = true;
                         });

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return page_world_complete;
  }));

  EXPECT_FALSE(page_world_error);
  EXPECT_TRUE(page_world_result);
  EXPECT_FALSE([page_world_result boolValue]);
}

// Tests that __gCrWeb.message.getExistingFrames() is called in the specified
// world.
TEST_F(WebViewJsUtilsTest, RegisterExistingFrames) {
  WKWebView* web_view = [[WKWebView alloc] init];
  WKFrameInfo* frame_info = GetMainFrameWKFrameInfo(web_view);
  ASSERT_TRUE(frame_info);

  // Create mock __gCrWeb.message.getExistingFrames() in both content worlds.
  SetupMockGetExistingFramesScript(web_view, frame_info,
                                   WKContentWorld.pageWorld);
  SetupMockGetExistingFramesScript(web_view, frame_info,
                                   WKContentWorld.defaultClientWorld);

  // Verify that getExistingFrames is correctly called in the page world. Only
  // WKContentWorld.pageWorld should receive the call.
  web::RegisterExistingFrames(web_view, WKContentWorld.pageWorld);
  EXPECT_EQ(1, GetExistingFramesScriptCallCount(web_view, frame_info,
                                                WKContentWorld.pageWorld));
  EXPECT_EQ(0, GetExistingFramesScriptCallCount(
                   web_view, frame_info, WKContentWorld.defaultClientWorld));

  // Verify that getExistingFrames is correctly called in an isolated world.
  // WKContentWorld.pageWorld should not receive another call, but
  // WKContentWorld.defaultClientWorld should now receive it.
  web::RegisterExistingFrames(web_view, WKContentWorld.defaultClientWorld);
  EXPECT_EQ(1, GetExistingFramesScriptCallCount(web_view, frame_info,
                                                WKContentWorld.pageWorld));
  EXPECT_EQ(1, GetExistingFramesScriptCallCount(
                   web_view, frame_info, WKContentWorld.defaultClientWorld));
}

}  // namespace web
