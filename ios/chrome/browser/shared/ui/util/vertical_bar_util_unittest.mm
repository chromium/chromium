// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/vertical_bar_util.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/platform_test.h"

namespace {

// Returns a trait collection with `size_class` as its horizontal size class.
UITraitCollection* TraitsWithHorizontalSizeClass(
    UIUserInterfaceSizeClass size_class) {
  return [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutable_traits) {
        mutable_traits.horizontalSizeClass = size_class;
      }];
}

}  // namespace

using VerticalBarUtilTest = PlatformTest;

#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1

// Tests that on iPhone Duo the bar is kept for compact and unspecified widths
// and disabled for regular widths.
TEST_F(VerticalBarUtilTest, DuoBehaviorFollowsHorizontalSizeClass) {
  if (!IsDuoDevice()) {
    GTEST_SKIP() << "Requires an iPhone Duo simulator.";
  }
  if (@available(iOS 27.1, *)) {
    EXPECT_EQ(UIVerticalBarBehaviorAutomatic,
              PreferredVerticalBarBehavior(TraitsWithHorizontalSizeClass(
                  UIUserInterfaceSizeClassCompact)));
    EXPECT_EQ(UIVerticalBarBehaviorAutomatic,
              PreferredVerticalBarBehavior(TraitsWithHorizontalSizeClass(
                  UIUserInterfaceSizeClassUnspecified)));
    EXPECT_EQ(UIVerticalBarBehaviorDisabled,
              PreferredVerticalBarBehavior(TraitsWithHorizontalSizeClass(
                  UIUserInterfaceSizeClassRegular)));
  } else {
    GTEST_SKIP() << "Requires iOS 27.1.";
  }
}

// Tests that other devices keep the system default for every width.
TEST_F(VerticalBarUtilTest, NonDuoKeepsSystemDefault) {
  if (IsDuoDevice()) {
    GTEST_SKIP() << "Requires a non-Duo simulator.";
  }
  if (@available(iOS 27.1, *)) {
    EXPECT_EQ(UIVerticalBarBehaviorAutomatic,
              PreferredVerticalBarBehavior(TraitsWithHorizontalSizeClass(
                  UIUserInterfaceSizeClassRegular)));
  } else {
    GTEST_SKIP() << "Requires iOS 27.1.";
  }
}

#endif

// Tests that the bar is inactive when the system reports no bar edge, as for
// standalone trait collections and non-Duo devices. The active case needs a
// real Duo window and is covered by EarlGrey tests.
TEST_F(VerticalBarUtilTest, InactiveWithoutSystemBarEdge) {
  EXPECT_FALSE(IsVerticalBarActive(
      TraitsWithHorizontalSizeClass(UIUserInterfaceSizeClassCompact)));
}
