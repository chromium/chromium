// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_dict_value.h"
#import "ios/web/public/js_messaging/script_message_list_value.h"
#import "ios/web/public/js_messaging/script_message_value.h"
#import "testing/platform_test.h"

namespace web {

using ScriptMessageListValueTest = PlatformTest;

// Tests that Empty() returns true for an empty list.
TEST_F(ScriptMessageListValueTest, ListValueIsEmptyGivenEmptyArray) {
  NSArray* empty_ns_array = @[];
  ScriptMessageListValue empty_list(empty_ns_array);

  EXPECT_TRUE(empty_list.Empty());
}

// Tests that Size() returns 0 for an empty list.
TEST_F(ScriptMessageListValueTest, SizeIsZeroGivenEmptyArray) {
  NSArray* empty_ns_array = @[];
  ScriptMessageListValue empty_list(empty_ns_array);

  EXPECT_EQ(0u, empty_list.Size());
}

// Tests that Empty() returns false for non-empty list.
TEST_F(ScriptMessageListValueTest, ListValueIsNotEmptyGivenNonEmptyArray) {
  NSArray* non_empty_ns_array = @[ @1, @2 ];
  ScriptMessageListValue list(non_empty_ns_array);

  EXPECT_FALSE(list.Empty());
}

// Tests that Size() is equivalent to the number of elements in a non-empty
// list.
TEST_F(ScriptMessageListValueTest, SizeIsTwoGivenNonEmptyArray) {
  NSArray* array_with_two_elements = @[ @1, @2 ];
  ScriptMessageListValue list(array_with_two_elements);

  EXPECT_EQ(list.Size(), 2u);
}

// Tests whether the Front() function correctly returns the first element in the
// list.
TEST_F(ScriptMessageListValueTest, FrontElementIsEquivalentToTheFirstElement) {
  NSArray* array_with_dict_elements =
      @[ @{@"name" : @"item1"}, @{@"name" : @"item2"} ];
  ScriptMessageListValue list(array_with_dict_elements);

  std::unique_ptr<ScriptMessageValue> front = list.Front();

  ASSERT_TRUE(front);
  EXPECT_EQ(base::Value::Type::DICT, front->type());
  EXPECT_EQ("item1", front->GetDict().FindString("name").value_or(""));
}

// Tests whether the Front() function correctly returns the first element in the
// list containing NSNumbers.
TEST_F(ScriptMessageListValueTest,
       FrontElementIsEquivalentToTheFirstElementInNumberArray) {
  NSArray* array_with_nsnumber_elements =
      @[ @1, @2, @3, @4, @5, @6, @7, @8, @9, @10 ];
  ScriptMessageListValue list(array_with_nsnumber_elements);

  std::unique_ptr<ScriptMessageValue> front = list.Front();

  ASSERT_TRUE(front);
  EXPECT_EQ(base::Value::Type::INTEGER, front->type());
  EXPECT_EQ(1, front->GetValue().GetInt());
}

// Tests whether the Front() function correctly returns the first element in the
// list containing strings.
TEST_F(ScriptMessageListValueTest,
       FrontElementIsEquivalentToTheFirstElementInStringArray) {
  NSArray* array_with_string_elements = @[ @"a", @"b", @"c", @"d", @"e" ];
  ScriptMessageListValue list(array_with_string_elements);

  std::unique_ptr<ScriptMessageValue> front = list.Front();

  ASSERT_TRUE(front);
  EXPECT_EQ(base::Value::Type::STRING, front->type());
  EXPECT_EQ("a", front->GetValue().GetString());
}

// Tests whether the Back() function correctly returns the last element in the
// list.
TEST_F(ScriptMessageListValueTest, BackElementIsEquivalentToTheLastElement) {
  NSArray* array_with_dict_elements =
      @[ @{@"name" : @"item1"}, @{@"name" : @"item2"} ];
  ScriptMessageListValue list(array_with_dict_elements);

  std::unique_ptr<ScriptMessageValue> back = list.Back();

  ASSERT_TRUE(back);
  EXPECT_EQ(base::Value::Type::DICT, back->type());
  EXPECT_EQ("item2", back->GetDict().FindString("name").value_or(""));
}

// Tests whether the Back() function correctly returns the last element in the
// list containing NSNumbers.
TEST_F(ScriptMessageListValueTest,
       BackElementIsEquivalentToTheLastElementInNumberArray) {
  NSArray* array_with_nsnumber_elements =
      @[ @1, @2, @3, @4, @5, @6, @7, @8, @9, @10 ];
  ScriptMessageListValue list(array_with_nsnumber_elements);

  std::unique_ptr<ScriptMessageValue> back = list.Back();

  ASSERT_TRUE(back);
  EXPECT_EQ(base::Value::Type::INTEGER, back->type());
  EXPECT_EQ(10, back->GetValue().GetInt());
}

// Tests whether the Back() function correctly returns the last element in the
// list containing Strings.
TEST_F(ScriptMessageListValueTest,
       BackElementIsEquivalentToTheLastElementInStringArray) {
  NSArray* array_with_string_elements = @[ @"a", @"b", @"c", @"d", @"e" ];
  ScriptMessageListValue list(array_with_string_elements);

  std::unique_ptr<ScriptMessageValue> back = list.Back();

  ASSERT_TRUE(back);
  EXPECT_EQ(base::Value::Type::STRING, back->type());
  EXPECT_EQ("e", back->GetValue().GetString());
}

// Tests move construction and assignment.
TEST_F(ScriptMessageListValueTest, ListValueSupportsMoveConstruction) {
  NSArray* array_with_two_elements = @[ @1, @2 ];
  ScriptMessageListValue original(array_with_two_elements);

  ScriptMessageListValue moved_constructed(std::move(original));

  EXPECT_EQ(2u, moved_constructed.Size());
}

// Tests move assignment.
TEST_F(ScriptMessageListValueTest, ListValueSupportsMoveAssignment) {
  NSArray* array_with_two_elements = @[ @1, @2 ];
  ScriptMessageListValue original(array_with_two_elements);
  ScriptMessageListValue moved_assigned(@[]);
  ScriptMessageListValue moved_constructed(std::move(original));

  moved_assigned = std::move(moved_constructed);

  EXPECT_EQ(2u, moved_assigned.Size());
}

}  // namespace web
