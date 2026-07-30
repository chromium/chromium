// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_scrim_view.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Accessibility identifier for the actor overlay scrim view.
NSString* const kActorOverlayScrimAccessibilityIdentifier =
    @"ActorOverlayScrimAccessibilityIdentifier";

// Alpha value matching production `ActorOverlayScrimView` background alpha.
constexpr CGFloat kActorOverlayScrimViewAlpha = 0.15f;

class ActorOverlayScrimViewTest : public PlatformTest {};

// Test that `ActorOverlayScrimView` initializes with custom scrim color.
TEST_F(ActorOverlayScrimViewTest, CustomScrimColor) {
  UIColor* black_color = [UIColor blackColor];
  ActorOverlayScrimView* scrim_view =
      [[ActorOverlayScrimView alloc] initWithScrimColor:black_color];
  UIColor* expected_color =
      [black_color colorWithAlphaComponent:kActorOverlayScrimViewAlpha];
  EXPECT_NSEQ(scrim_view.backgroundColor, expected_color);
  EXPECT_NSEQ(scrim_view.accessibilityIdentifier,
              kActorOverlayScrimAccessibilityIdentifier);
  EXPECT_FALSE(scrim_view.translatesAutoresizingMaskIntoConstraints);
  EXPECT_TRUE(scrim_view.userInteractionEnabled);
}

}  // namespace
