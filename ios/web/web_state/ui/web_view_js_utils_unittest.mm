// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_js_utils.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

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
  std::string converted_result;
  value->GetAsString(&converted_result);
  EXPECT_EQ("test", converted_result);
}

// Tests that ValueResultFromWKResult converts inetger to Value::Type::DOUBLE.
// NOTE: WKWebView API returns all numbers as kCFNumberFloat64Type, so there is
// no way to tell if the result is integer or double.
TEST_F(WebViewJsUtilsTest, ValueResultFromIntegerWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@1));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::Type::DOUBLE, value->type());
  double converted_result = 0;
  value->GetAsDouble(&converted_result);
  EXPECT_EQ(1, converted_result);
}

// Tests that ValueResultFromWKResult converts double to Value::Type::DOUBLE.
TEST_F(WebViewJsUtilsTest, ValueResultFromDoubleWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@3.14));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::Type::DOUBLE, value->type());
  double converted_result = 0;
  value->GetAsDouble(&converted_result);
  EXPECT_EQ(3.14, converted_result);
}

// Tests that ValueResultFromWKResult converts bool to Value::Type::BOOLEAN.
TEST_F(WebViewJsUtilsTest, ValueResultFromBoolWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@YES));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::Type::BOOLEAN, value->type());
  bool converted_result = false;
  value->GetAsBoolean(&converted_result);
  EXPECT_TRUE(converted_result);
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
      @{ @"Key1" : @"Value1",
         @"Key2" : @{@"Key3" : @42} };

  std::unique_ptr<base::Value> value(
      web::ValueResultFromWKResult(test_dictionary));
  base::DictionaryValue* dictionary = nullptr;
  value->GetAsDictionary(&dictionary);
  EXPECT_NE(nullptr, dictionary);

  std::string value1;
  dictionary->GetString("Key1", &value1);
  EXPECT_EQ("Value1", value1);

  base::DictionaryValue const* inner_dictionary = nullptr;
  dictionary->GetDictionary("Key2", &inner_dictionary);
  EXPECT_NE(nullptr, inner_dictionary);

  double value3;
  inner_dictionary->GetDouble("Key3", &value3);
  EXPECT_EQ(42, value3);
}

// Tests that ValueResultFromWKResult converts NSArray to properly
// initialized base::ListValue.
TEST_F(WebViewJsUtilsTest, ValueResultFromArrayWKResult) {
  NSArray* test_array = @[ @"Value1", @[ @YES ], @42 ];

  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(test_array));
  base::ListValue* list = nullptr;
  value->GetAsList(&list);
  EXPECT_NE(nullptr, list);

  size_t list_size = 3;
  EXPECT_EQ(list_size, list->GetSize());

  std::string value1;
  list->GetString(0, &value1);
  EXPECT_EQ("Value1", value1);

  base::ListValue const* inner_list = nullptr;
  list->GetList(1, &inner_list);
  EXPECT_NE(nullptr, inner_list);

  double value3;
  list->GetDouble(2, &value3);
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
  NSString* obj_c_key =
      [NSString stringWithCString:key encoding:NSASCIIStringEncoding];
  test_dictionary[obj_c_key] = test_dictionary_2;
  test_dictionary_2[obj_c_key] = test_dictionary;

  // Break the retain cycle so that the dictionaries are freed.
  base::ScopedClosureRunner runner(base::BindOnce(^{
    [test_dictionary_2 removeAllObjects];
  }));

  // Check that parsing the dictionary stopped at a depth of
  // |kMaximumParsingRecursionDepth|.
  std::unique_ptr<base::Value> value =
      web::ValueResultFromWKResult(test_dictionary);
  base::DictionaryValue* current_dictionary = nullptr;
  base::DictionaryValue* inner_dictionary = nullptr;

  value->GetAsDictionary(&current_dictionary);
  EXPECT_NE(nullptr, current_dictionary);

  for (int current_depth = 0; current_depth <= kMaximumParsingRecursionDepth;
       current_depth++) {
    EXPECT_NE(nullptr, current_dictionary);
    inner_dictionary = nullptr;
    current_dictionary->GetDictionary(key, &inner_dictionary);
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
  base::ScopedClosureRunner runner(base::BindOnce(^{
    [test_array removeAllObjects];
  }));

  // Check that parsing the array stopped at a depth of
  // |kMaximumParsingRecursionDepth|.
  std::unique_ptr<base::Value> value = web::ValueResultFromWKResult(test_array);
  base::ListValue* current_list = nullptr;
  base::ListValue* inner_list = nullptr;

  value->GetAsList(&current_list);
  EXPECT_NE(nullptr, current_list);

  for (int current_depth = 0; current_depth <= kMaximumParsingRecursionDepth;
       current_depth++) {
    EXPECT_NE(nullptr, current_list);
    inner_list = nullptr;
    current_list->GetList(0, &inner_list);
    current_list = inner_list;
  }
  EXPECT_EQ(nullptr, current_list);
}

}  // namespace web
