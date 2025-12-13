// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Accessibility identifiers used in the view controller.
NSString* const kSyncedSetUpAvatarAccessibilityID =
    @"kSyncedSetUpAvatarAccessibilityID";
NSString* const kSyncedSetUpTitleAccessibilityID =
    @"kSyncedSetUpTitleAccessibilityID";

// Recursively searches the view hierarchy for a view with the specified
// accessibility identifier.
UIView* FindViewById(UIView* view, NSString* accessibility_id) {
  if ([view.accessibilityIdentifier isEqualToString:accessibility_id]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* foundView = FindViewById(subview, accessibility_id);
    if (foundView) {
      return foundView;
    }
  }
  return nil;
}

// Helper function to create a dummy `UIImage` for testing.
UIImage* CreateDummyImage(CGSize size, UIColor* color) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];
  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [color setFill];
        [context fillRect:CGRectMake(0, 0, size.width, size.height)];
      }];
  return image;
}

}  // namespace

// Test fixture for `SyncedSetUpViewController`.
class SyncedSetUpViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[SyncedSetUpViewController alloc] init];
  }

  SyncedSetUpViewController* view_controller_;
};

// Tests that the view controller can be initialized.
TEST_F(SyncedSetUpViewControllerTest, TestInitialization) {
  EXPECT_TRUE(view_controller_);
}

// Tests that `-viewDidLoad` sets up the key views.
TEST_F(SyncedSetUpViewControllerTest, TestViewsExistAfterViewDidLoad) {
  UIView* view = view_controller_.view;
  ASSERT_TRUE(view);

  EXPECT_TRUE(FindViewById(view, kSyncedSetUpAvatarAccessibilityID));
  EXPECT_TRUE(FindViewById(view, kSyncedSetUpTitleAccessibilityID));
}

// Tests the `-setWelcomeMessage:` consumer method.
TEST_F(SyncedSetUpViewControllerTest, TestSetWelcomeMessage) {
  UIView* view = view_controller_.view;
  ASSERT_TRUE(view);

  NSString* testMessage = @"Welcome, Test User!";
  [view_controller_ setWelcomeMessage:testMessage];

  UILabel* titleLabel =
      (UILabel*)FindViewById(view, kSyncedSetUpTitleAccessibilityID);
  ASSERT_TRUE(titleLabel);

  // Wait for the main queue to process the update.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^BOOL {
        return [titleLabel.text isEqualToString:testMessage];
      }));
  EXPECT_NSEQ(testMessage, titleLabel.text);
}

// Tests the `-setAvatarImage:` consumer method.
TEST_F(SyncedSetUpViewControllerTest, TestSetAvatarImage) {
  UIView* view = view_controller_.view;
  ASSERT_TRUE(view);

  UIImage* testImage = CreateDummyImage(CGSizeMake(10, 10), UIColor.redColor);
  [view_controller_ setAvatarImage:testImage];

  UIImageView* avatarImageView =
      (UIImageView*)FindViewById(view, kSyncedSetUpAvatarAccessibilityID);
  ASSERT_TRUE(avatarImageView);

  // Wait for the main queue to process the update.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^BOOL {
        return avatarImageView.image != nil;
      }));

  EXPECT_TRUE(avatarImageView.image);
  EXPECT_NE(nil, avatarImageView.image);
}
