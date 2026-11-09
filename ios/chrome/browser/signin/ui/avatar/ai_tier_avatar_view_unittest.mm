// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_content_configuration.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_ring_image_view.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns a solid black image of `size`.
UIImage* CreateTestImage(CGSize size) {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [[UIColor blackColor] setFill];
    [context fillRect:CGRectMake(0, 0, size.width, size.height)];
  }];
}

}  // namespace

// Test fixture for `AITierAvatarView`.
class AITierAvatarViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    test_image_ = CreateTestImage(CGSizeMake(40, 40));
  }

  // Adds `avatar_view` to `container_` with centered Auto Layout constraints
  // and forces a layout pass.
  void LayoutInContainer(AITierAvatarView* avatar_view) {
    container_ = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    [container_ addSubview:avatar_view];
    [NSLayoutConstraint activateConstraints:@[
      [avatar_view.centerXAnchor
          constraintEqualToAnchor:container_.centerXAnchor],
      [avatar_view.centerYAnchor
          constraintEqualToAnchor:container_.centerYAnchor],
    ]];
    [container_ setNeedsLayout];
    [container_ layoutIfNeeded];
  }

  UIImage* test_image_;
  // Superview of the `AITierAvatarView` under test, kept alive for the whole
  // test.
  UIView* container_;
};

// Tests that configuring full view sizing shrinks the avatar so the total
// view bounds including the ring match the configured diameter.
TEST_F(AITierAvatarViewTest, FullViewRingAndAvatarSizing) {
  constexpr CGFloat kConfiguredDiameter = 30.0;
  constexpr CGFloat kExpectedAvatarDiameter =
      kConfiguredDiameter - 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);
  constexpr CGFloat kExpectedRingDiameter = kConfiguredDiameter;

  AITierAvatarContentConfiguration* config =
      [[AITierAvatarContentConfiguration alloc]
          initWithAvatarImage:test_image_
                     diameter:kConfiguredDiameter
                     sizeType:AITierAvatarSizeTypeFullView
              showsAITierRing:YES];

  AITierAvatarView* avatar_view =
      [[AITierAvatarView alloc] initWithConfiguration:config];
  LayoutInContainer(avatar_view);

  // Verify total view size matches the configured full view diameter.
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.width);
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.height);

  // Verify avatar image view size is decreased to fit inside the ring.
  UIImageView* avatar_image_view = chrome_test_util::FindViewById<UIImageView>(
      avatar_view, kIdentityAvatarImageAccessibilityIdentifier);
  ASSERT_NE(nil, avatar_image_view);
  EXPECT_EQ(kExpectedAvatarDiameter, avatar_image_view.bounds.size.width);
  EXPECT_EQ(kExpectedAvatarDiameter, avatar_image_view.bounds.size.height);

  // Verify ring image view size matches the full view diameter.
  AITierRingImageView* ring_image_view =
      chrome_test_util::FindViewById<AITierRingImageView>(
          avatar_view, kPremiumAvatarRingAccessibilityIdentifier);
  ASSERT_NE(nil, ring_image_view);
  EXPECT_EQ(kExpectedRingDiameter, ring_image_view.bounds.size.width);
  EXPECT_EQ(kExpectedRingDiameter, ring_image_view.bounds.size.height);
}

// Tests that toggling the AI tier ring dynamically resizes the avatar to fill
// the view when hidden and shrinks it back when shown.
TEST_F(AITierAvatarViewTest, AvatarResizedWhenRingDisappearsAndReciprocally) {
  constexpr CGFloat kConfiguredDiameter = 30.0;
  constexpr CGFloat kShrunkAvatarDiameter =
      kConfiguredDiameter - 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);

  AITierAvatarContentConfiguration* config_with_ring =
      [[AITierAvatarContentConfiguration alloc]
          initWithAvatarImage:test_image_
                     diameter:kConfiguredDiameter
                     sizeType:AITierAvatarSizeTypeFullView
              showsAITierRing:YES];

  AITierAvatarView* avatar_view =
      [[AITierAvatarView alloc] initWithConfiguration:config_with_ring];
  LayoutInContainer(avatar_view);

  // Initial state with ring: avatar is shrunk, ring is present, total view is
  // `kConfiguredDiameter`.
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.width);
  chrome_test_util::ExpectUniqueSubview<AITierRingImageView>(avatar_view);
  UIImageView* avatar_image_view = chrome_test_util::FindViewById<UIImageView>(
      avatar_view, kIdentityAvatarImageAccessibilityIdentifier);
  ASSERT_NE(nil, avatar_image_view);
  EXPECT_EQ(kShrunkAvatarDiameter, avatar_image_view.bounds.size.width);

  // Step 1: Hide the AI tier ring.
  AITierAvatarContentConfiguration* config_without_ring =
      [[AITierAvatarContentConfiguration alloc]
          initWithAvatarImage:test_image_
                     diameter:kConfiguredDiameter
                     sizeType:AITierAvatarSizeTypeFullView
              showsAITierRing:NO];
  avatar_view.configuration = config_without_ring;
  [container_ setNeedsLayout];
  [container_ layoutIfNeeded];

  // The ring is removed, and avatar is resized to take the full
  // `kConfiguredDiameter` space.
  chrome_test_util::ExpectNoSubview<AITierRingImageView>(avatar_view);
  avatar_image_view = chrome_test_util::FindViewById<UIImageView>(
      avatar_view, kIdentityAvatarImageAccessibilityIdentifier);
  ASSERT_NE(nil, avatar_image_view);
  EXPECT_EQ(kConfiguredDiameter, avatar_image_view.bounds.size.width);
  EXPECT_EQ(kConfiguredDiameter, avatar_image_view.bounds.size.height);
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.width);
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.height);

  // Step 2 (Reciprocal): Show the AI tier ring again.
  avatar_view.configuration = config_with_ring;
  [container_ setNeedsLayout];
  [container_ layoutIfNeeded];

  // The ring is re-added, and avatar is shrunk back to `kShrunkAvatarDiameter`.
  chrome_test_util::ExpectUniqueSubview<AITierRingImageView>(avatar_view);
  avatar_image_view = chrome_test_util::FindViewById<UIImageView>(
      avatar_view, kIdentityAvatarImageAccessibilityIdentifier);
  ASSERT_NE(nil, avatar_image_view);
  EXPECT_EQ(kShrunkAvatarDiameter, avatar_image_view.bounds.size.width);
  EXPECT_EQ(kShrunkAvatarDiameter, avatar_image_view.bounds.size.height);
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.width);
  EXPECT_EQ(kConfiguredDiameter, avatar_view.bounds.size.height);
}

// Tests that configuring avatar sizing preserves the avatar diameter and
// expands the ring outward.
TEST_F(AITierAvatarViewTest, AvatarSizeTypeSizing) {
  constexpr CGFloat kConfiguredDiameter = 40.0;
  constexpr CGFloat kExpectedRingDiameter =
      kConfiguredDiameter + 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);

  AITierAvatarContentConfiguration* config =
      [[AITierAvatarContentConfiguration alloc]
          initWithAvatarImage:test_image_
                     diameter:kConfiguredDiameter
                     sizeType:AITierAvatarSizeTypeAvatar
              showsAITierRing:YES];

  AITierAvatarView* avatar_view =
      [[AITierAvatarView alloc] initWithConfiguration:config];
  LayoutInContainer(avatar_view);

  // Total view size matches the expanded ring diameter.
  EXPECT_EQ(kExpectedRingDiameter, avatar_view.bounds.size.width);
  EXPECT_EQ(kExpectedRingDiameter, avatar_view.bounds.size.height);

  UIImageView* avatar_image_view = chrome_test_util::FindViewById<UIImageView>(
      avatar_view, kIdentityAvatarImageAccessibilityIdentifier);
  ASSERT_NE(nil, avatar_image_view);
  EXPECT_EQ(kConfiguredDiameter, avatar_image_view.bounds.size.width);
  EXPECT_EQ(kConfiguredDiameter, avatar_image_view.bounds.size.height);

  AITierRingImageView* ring_image_view =
      chrome_test_util::FindViewById<AITierRingImageView>(
          avatar_view, kPremiumAvatarRingAccessibilityIdentifier);
  ASSERT_NE(nil, ring_image_view);
  EXPECT_EQ(kExpectedRingDiameter, ring_image_view.bounds.size.width);
  EXPECT_EQ(kExpectedRingDiameter, ring_image_view.bounds.size.height);
}
