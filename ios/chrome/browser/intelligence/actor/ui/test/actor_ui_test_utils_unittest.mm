// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/test/actor_ui_test_utils.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace intelligence::actor {
namespace {

using ActorUiTestUtilsTest = PlatformTest;

// Tests handling nil inputs safely.
TEST_F(ActorUiTestUtilsTest, HandlesNilInputs) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  EXPECT_EQ(FindViewByAccessibilityIdentifier(nil, @"some_id"), nil);
  EXPECT_EQ(FindViewByAccessibilityIdentifier(root, nil), nil);
  EXPECT_EQ(FindViewByAccessibilityIdentifier(nil, nil), nil);
}

// Tests matching the root view directly.
TEST_F(ActorUiTestUtilsTest, FindsRootView) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  root.accessibilityIdentifier = @"root_id";

  UIView* found = FindViewByAccessibilityIdentifier(root, @"root_id");
  EXPECT_EQ(found, root);
}

// Tests finding child and deeply nested views.
TEST_F(ActorUiTestUtilsTest, FindsNestedSubviews) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* child = [[UIView alloc] initWithFrame:CGRectZero];
  child.accessibilityIdentifier = @"child_id";
  [root addSubview:child];

  UIView* grandchild = [[UIView alloc] initWithFrame:CGRectZero];
  grandchild.accessibilityIdentifier = @"grandchild_id";
  [child addSubview:grandchild];

  EXPECT_EQ(FindViewByAccessibilityIdentifier(root, @"child_id"), child);
  EXPECT_EQ(FindViewByAccessibilityIdentifier(root, @"grandchild_id"),
            grandchild);
}

// Tests returning nil when the view is not found.
TEST_F(ActorUiTestUtilsTest, ReturnsNilWhenNotFound) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* child = [[UIView alloc] initWithFrame:CGRectZero];
  child.accessibilityIdentifier = @"child_id";
  [root addSubview:child];

  EXPECT_EQ(FindViewByAccessibilityIdentifier(root, @"non_existent_id"), nil);
}

// Tests returning the first matching view when duplicates exist.
TEST_F(ActorUiTestUtilsTest, ReturnsFirstMatchWhenDuplicatesExist) {
  UIView* root = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* first_child = [[UIView alloc] initWithFrame:CGRectZero];
  first_child.accessibilityIdentifier = @"duplicate_id";
  [root addSubview:first_child];

  UIView* second_child = [[UIView alloc] initWithFrame:CGRectZero];
  second_child.accessibilityIdentifier = @"duplicate_id";
  [root addSubview:second_child];

  EXPECT_EQ(FindViewByAccessibilityIdentifier(root, @"duplicate_id"),
            first_child);
}

}  // namespace
}  // namespace intelligence::actor
