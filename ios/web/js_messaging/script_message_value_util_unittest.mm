// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/script_message_value_util.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_value.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

using ScriptMessageValueUtilTest = PlatformTest;

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a NSString.
TEST_F(ScriptMessageValueUtilTest, ConvertNSStringToScriptMessageValue) {
  NSString* string_element = @"test_string";

  std::unique_ptr<ScriptMessageValue> value =
      CreateScriptMessageValue(string_element);

  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::STRING, value->type());
  EXPECT_EQ("test_string", value->GetValue().GetString());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from an integer.
TEST_F(ScriptMessageValueUtilTest, ConvertIntegerToScriptMessageValue) {
  NSNumber* integer_element = @(3);

  std::unique_ptr<ScriptMessageValue> value =
      CreateScriptMessageValue(integer_element);

  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  EXPECT_EQ(3, value->GetValue().GetInt());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a double.
TEST_F(ScriptMessageValueUtilTest, ConvertDoubleToScriptMessageValue) {
  NSNumber* double_element = @3.14;

  std::unique_ptr<ScriptMessageValue> value =
      CreateScriptMessageValue(double_element);

  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::DOUBLE, value->type());
  EXPECT_EQ(3.14, value->GetValue().GetDouble());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a Boolean.
TEST_F(ScriptMessageValueUtilTest, ConvertBooleanToScriptMessageValue) {
  NSNumber* bool_element = @YES;

  std::unique_ptr<ScriptMessageValue> value =
      CreateScriptMessageValue(bool_element);

  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::BOOLEAN, value->type());
  EXPECT_TRUE(value->GetValue().GetBool());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a NSDictionary.
TEST_F(ScriptMessageValueUtilTest, ConvertNSDictionaryToScriptMessageValue) {
  NSDictionary* dict_element = @{@"key" : @"value"};

  std::unique_ptr<ScriptMessageValue> value =
      CreateScriptMessageValue(dict_element);

  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::DICT, value->type());
}

}  // namespace web
