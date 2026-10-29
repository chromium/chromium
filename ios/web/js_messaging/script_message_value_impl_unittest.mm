// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_value.h"
#import "testing/platform_test.h"

namespace web {

using ScriptMessageValueTest = PlatformTest;

// Tests that the default constructor initializes to a NONE type.
TEST_F(ScriptMessageValueTest, DefaultConstructor) {
  ScriptMessageValue message_value;
  ASSERT_EQ(base::Value::Type::NONE, message_value.type());
  EXPECT_EQ(base::Value(), message_value.GetValue());
}

// Tests initialization with a boolean value.
TEST_F(ScriptMessageValueTest, BoolConstructor) {
  ScriptMessageValue message_value_true(true);
  EXPECT_TRUE(message_value_true.GetValue().GetBool());

  ScriptMessageValue message_value_false(false);
  EXPECT_FALSE(message_value_false.GetValue().GetBool());
}

// Tests initialization with a string.
TEST_F(ScriptMessageValueTest, StringConstructor) {
  std::string string_val = "hello";
  ScriptMessageValue message_value(std::move(string_val));
  ASSERT_EQ(base::Value::Type::STRING, message_value.type());
  EXPECT_EQ("hello", message_value.GetValue().GetString());
}

// Tests initialization with a double value.
TEST_F(ScriptMessageValueTest, DoubleConstructor) {
  double double_val = 3.14;
  ScriptMessageValue message_value(double_val);
  ASSERT_EQ(base::Value::Type::DOUBLE, message_value.type());
  EXPECT_EQ(3.14, message_value.GetValue().GetDouble());
}

// Tests initialization with a NSDictionary
TEST_F(ScriptMessageValueTest, ValueShouldSupportDictConstruction) {
  NSDictionary* ns_dict = @{@"key" : @"value"};
  ScriptMessageValue message_value(ns_dict);
  ASSERT_EQ(base::Value::Type::DICT, message_value.type());
}

// Tests that `GetIfDict` returns a pointer to the dictionary when the value is
// a dictionary, and `nullptr` otherwise.
TEST_F(ScriptMessageValueTest, GetIfDict) {
  NSDictionary* ns_dict = @{@"key" : @"value"};
  ScriptMessageValue dict_value(ns_dict);
  const ScriptMessageDictValue* dict = dict_value.GetIfDict();
  ASSERT_NE(nullptr, dict);
  EXPECT_EQ("value", dict->FindString("key"));

  ScriptMessageValue none_value;
  EXPECT_EQ(nullptr, none_value.GetIfDict());

  ScriptMessageValue bool_value(true);
  EXPECT_EQ(nullptr, bool_value.GetIfDict());

  NSArray* ns_array = @[ @"key", @"value" ];
  ScriptMessageValue list_value(ns_array);
  EXPECT_EQ(nullptr, list_value.GetIfDict());
}

// Tests initialization with a NSArray
TEST_F(ScriptMessageValueTest, ValueShouldSupportArrayConstruction) {
  NSArray* ns_array = @[ @"key", @"value" ];
  ScriptMessageValue message_value(ns_array);
  ASSERT_EQ(base::Value::Type::LIST, message_value.type());
}

// Tests move construction and move assignment.
TEST_F(ScriptMessageValueTest, MoveOperations) {
  ScriptMessageValue original(true);

  // Move construction
  ScriptMessageValue moved_constructed(std::move(original));
  EXPECT_TRUE(moved_constructed.GetValue().GetBool());

  // Move assignment
  ScriptMessageValue moved_assigned;
  moved_assigned = std::move(moved_constructed);
  EXPECT_TRUE(moved_assigned.GetValue().GetBool());
}

}  // namespace web
