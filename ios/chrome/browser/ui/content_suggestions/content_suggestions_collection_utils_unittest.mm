// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include <memory>

#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace content_suggestions {

CGFloat kTopInset = 20;

class ContentSuggestionsCollectionUtilsTest : public PlatformTest {
 public:
  void SetAsIPad() {
    UITraitCollection* horizontalRegular = [UITraitCollection
        traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassRegular];
    UITraitCollection* verticalRegular = [UITraitCollection
        traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassRegular];
    customTraitCollection_ =
        [UITraitCollection traitCollectionWithTraitsFromCollections:@[
          verticalRegular, horizontalRegular
        ]];

    trait_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIWindow class], @selector(traitCollection),
        ^UITraitCollection*(id self) {
          return customTraitCollection_;
        });

    device_type_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIDevice class], @selector(userInterfaceIdiom),
        ^UIUserInterfaceIdiom(id self) {
          return UIUserInterfaceIdiomPad;
        });
  }
  void SetAsIPhone() {
    UITraitCollection* horizontalCompact = [UITraitCollection
        traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassCompact];
    UITraitCollection* verticalCompact = [UITraitCollection
        traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassCompact];
    customTraitCollection_ =
        [UITraitCollection traitCollectionWithTraitsFromCollections:@[
          verticalCompact, horizontalCompact
        ]];

    trait_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIWindow class], @selector(traitCollection),
        ^UITraitCollection*(id self) {
          return customTraitCollection_;
        });

    device_type_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIDevice class], @selector(userInterfaceIdiom),
        ^UIUserInterfaceIdiom(id self) {
          return UIUserInterfaceIdiomPhone;
        });
  }
  void SetAsPortrait() {
    orientation_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIApplication class], @selector(statusBarOrientation),
        ^UIInterfaceOrientation(id self) {
          return UIInterfaceOrientationPortrait;
        });
  }
  void SetAsLandscape() {
    orientation_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIApplication class], @selector(statusBarOrientation),
        ^UIInterfaceOrientation(id self) {
          return UIInterfaceOrientationLandscapeLeft;
        });
  }

 private:
  UITraitCollection* customTraitCollection_;
  std::unique_ptr<ScopedBlockSwizzler> trait_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> device_type_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> orientation_swizzler_;
};

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPad) {
  // Setup.
  SetAsIPad();

  // Action.
  CGFloat height = doodleHeight(YES);
  CGFloat topMargin = doodleTopMargin(YES, kTopInset);

  // Test.
  EXPECT_EQ(120, height);
  EXPECT_EQ(162, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhonePortrait) {
  // Setup.
  SetAsIPhone();
  SetAsPortrait();

  // Action.
  CGFloat heightLogo = doodleHeight(YES);
  CGFloat heightNoLogo = doodleHeight(NO);
  CGFloat topMargin = doodleTopMargin(YES, kTopInset);

  // Test.
  EXPECT_EQ(120, heightLogo);
  EXPECT_EQ(60, heightNoLogo);
  EXPECT_EQ(58 + kTopInset, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhoneLandscape) {
  // Setup.
  SetAsIPhone();
  SetAsLandscape();

  // Action.
  CGFloat heightLogo = doodleHeight(YES);
  CGFloat heightNoLogo = doodleHeight(NO);
  CGFloat topMargin = doodleTopMargin(YES, kTopInset);

  // Test.
  EXPECT_EQ(120, heightLogo);
  EXPECT_EQ(60, heightNoLogo);
  EXPECT_EQ(58 + kTopInset, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPad) {
  // Setup.
  SetAsIPad();
  CGFloat width = 500;
  CGFloat largeIPadWidth = 1366;

  // Action.
  CGFloat resultWidth = searchFieldWidth(width);
  CGFloat resultWidthLargeIPad = searchFieldWidth(largeIPadWidth);
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(32, topMargin);
  EXPECT_EQ(432, resultWidth);
  EXPECT_EQ(432, resultWidthLargeIPad);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhonePortrait) {
  // Setup.
  SetAsIPhone();
  SetAsPortrait();
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth = searchFieldWidth(width);
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(32, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhoneLandscape) {
  // Setup.
  SetAsIPhone();
  SetAsLandscape();
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth = searchFieldWidth(width);
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(32, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  // Setup.
  SetAsIPad();

  // Action, tests.
  EXPECT_EQ(380, heightForLogoHeader(YES, YES, YES, 0));
  EXPECT_EQ(404, heightForLogoHeader(YES, NO, YES, 0));
  EXPECT_EQ(380, heightForLogoHeader(YES, YES, NO, 0));
  EXPECT_EQ(404, heightForLogoHeader(YES, NO, NO, 0));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  // Setup.
  SetAsIPhone();

  // Action, tests.
  EXPECT_EQ(276, heightForLogoHeader(YES, YES, YES, 0));
  EXPECT_EQ(276, heightForLogoHeader(YES, NO, YES, 0));
  EXPECT_EQ(276, heightForLogoHeader(YES, YES, NO, 0));
  EXPECT_EQ(276, heightForLogoHeader(YES, NO, NO, 0));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, NearestAncestor) {
  // Setup.
  // The types of the view has no meaning.
  UILabel* rootView = [[UILabel alloc] init];
  UIView* intermediaryView = [[UIView alloc] init];
  UIScrollView* leafView = [[UIScrollView alloc] init];
  [rootView addSubview:intermediaryView];
  [intermediaryView addSubview:leafView];

  // Tests.
  EXPECT_EQ(leafView, nearestAncestor(leafView, [UIScrollView class]));
  EXPECT_EQ(leafView, nearestAncestor(leafView, [UIView class]));
  EXPECT_EQ(rootView, nearestAncestor(leafView, [UILabel class]));
  EXPECT_EQ(nil, nearestAncestor(leafView, [UITextView class]));
}

}  // namespace content_suggestions
