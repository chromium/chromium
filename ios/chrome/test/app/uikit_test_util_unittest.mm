// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/uikit_test_util.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace chrome_test_util {
namespace {

// Test fixture for UIKit test utility functions.
using UIKitTestUtilTest = PlatformTest;

// Tests that `FindViewById` matches the root view directly.
TEST_F(UIKitTestUtilTest, FindViewByIdFindsRootView) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  root.accessibilityIdentifier = @"root_id";

  UIView* found = FindViewById(root, @"root_id");
  EXPECT_EQ(found, root);
}

// Tests that `FindViewById` finds child and deeply nested subviews.
TEST_F(UIKitTestUtilTest, FindViewByIdFindsNestedSubviews) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* child = [[UIView alloc] initWithFrame:CGRectZero];
  child.accessibilityIdentifier = @"child_id";
  [root addSubview:child];

  UIView* grandchild = [[UIView alloc] initWithFrame:CGRectZero];
  grandchild.accessibilityIdentifier = @"grandchild_id";
  [child addSubview:grandchild];

  EXPECT_EQ(FindViewById(root, @"child_id"), child);
  EXPECT_EQ(FindViewById(root, @"grandchild_id"), grandchild);
}

// Tests that `FindViewById` returns nil when the view is not found.
TEST_F(UIKitTestUtilTest, FindViewByIdReturnsNilWhenNotFound) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* child = [[UIView alloc] initWithFrame:CGRectZero];
  child.accessibilityIdentifier = @"child_id";
  [root addSubview:child];

  EXPECT_EQ(FindViewById(root, @"non_existent_id"), nil);
}

// Tests that `FindViewById` returns the first matching view when duplicates
// exist.
TEST_F(UIKitTestUtilTest, FindViewByIdReturnsFirstMatchWhenDuplicatesExist) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* first_child = [[UIView alloc] initWithFrame:CGRectZero];
  first_child.accessibilityIdentifier = @"duplicate_id";
  [root addSubview:first_child];

  UIView* second_child = [[UIView alloc] initWithFrame:CGRectZero];
  second_child.accessibilityIdentifier = @"duplicate_id";
  [root addSubview:second_child];

  EXPECT_EQ(FindViewById(root, @"duplicate_id"), first_child);
}

// Tests that `FindViewByClass` returns the first matching view of the given
// class (or nil if none exists), and `FindViewsByClass` returns all matching
// views in depth-first order.
TEST_F(UIKitTestUtilTest, FindViewByClass) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  [root addSubview:container];

  UIButton* button1 = [[UIButton alloc] initWithFrame:CGRectZero];
  UIButton* button2 = [[UIButton alloc] initWithFrame:CGRectZero];
  [container addSubview:button1];
  [root addSubview:button2];

  EXPECT_EQ(FindViewByClass<UIButton>(root), button1);
  EXPECT_EQ(FindViewByClass<UILabel>(root), nil);

  NSArray<UIButton*>* buttons = FindViewsByClass<UIButton>(root);
  ASSERT_EQ(buttons.count, 2u);
  EXPECT_EQ(buttons[0], button1);
  EXPECT_EQ(buttons[1], button2);

  ExpectSubviewCount<UIButton>(root, 2);
  ExpectUniqueSubview<UIButton>(container);
  ExpectNoSubview<UILabel>(root);
}

// Tests that `FindLabelWithText` returns the UILabel matching the given text,
// or nil if no label has that text.
TEST_F(UIKitTestUtilTest, FindLabelWithText) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.text = @"Hello World";
  [root addSubview:label];

  EXPECT_EQ(FindLabelWithText(root, @"Hello World"), label);
  EXPECT_EQ(FindLabelWithText(root, @"Other"), nil);
}

}  // namespace
}  // namespace chrome_test_util
