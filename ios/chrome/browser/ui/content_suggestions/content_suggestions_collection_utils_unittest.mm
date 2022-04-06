// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace content_suggestions {

CGFloat kTopInset = 20;
CGFloat kDoodleHeightNoLogo = 0;

class ContentSuggestionsCollectionUtilsTest : public PlatformTest {
 public:
  UITraitCollection* IPadTraitCollection() {
    UITraitCollection* horizontalRegular = [UITraitCollection
        traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassRegular];
    UITraitCollection* verticalRegular = [UITraitCollection
        traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassRegular];
    return [UITraitCollection traitCollectionWithTraitsFromCollections:@[
      verticalRegular, horizontalRegular
    ]];
  }

  UITraitCollection* IPhoneLandscapeTraitCollection() {
    UITraitCollection* horizontalCompact = [UITraitCollection
        traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassCompact];
    UITraitCollection* verticalCompact = [UITraitCollection
        traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassCompact];
    return [UITraitCollection traitCollectionWithTraitsFromCollections:@[
      verticalCompact, horizontalCompact
    ]];
  }

  UITraitCollection* IPhonePortraitTraitCollection() {
    UITraitCollection* horizontalCompact = [UITraitCollection
        traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassCompact];
    UITraitCollection* verticalRegular = [UITraitCollection
        traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassRegular];
    return [UITraitCollection traitCollectionWithTraitsFromCollections:@[
      verticalRegular, horizontalCompact
    ]];
  }
};

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPad) {
  // Action.
  CGFloat height = doodleHeight(YES, YES, IPadTraitCollection());
  CGFloat topMargin = doodleTopMargin(YES, kTopInset, IPadTraitCollection());

  // Test.
  EXPECT_EQ(68, height);
  EXPECT_EQ(162, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhonePortrait) {
  // Action.
  CGFloat heightLogo = doodleHeight(YES, YES, IPhonePortraitTraitCollection());
  CGFloat heightNoLogo = doodleHeight(NO, NO, IPhonePortraitTraitCollection());
  CGFloat topMargin =
      doodleTopMargin(YES, kTopInset, IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(75 + kTopInset, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhoneLandscape) {
  // Action.
  CGFloat heightLogo = doodleHeight(YES, YES, IPhoneLandscapeTraitCollection());
  CGFloat heightNoLogo = doodleHeight(NO, NO, IPhoneLandscapeTraitCollection());
  CGFloat topMargin =
      doodleTopMargin(YES, kTopInset, IPhoneLandscapeTraitCollection());

  // Test.
  EXPECT_EQ(68, heightLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(78, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPad) {
  // Setup.
  CGFloat width = 500;
  CGFloat largeIPadWidth = 1366;

  // Action.
  CGFloat resultWidth = searchFieldWidth(width, IPadTraitCollection());
  CGFloat resultWidthLargeIPad =
      searchFieldWidth(largeIPadWidth, IPadTraitCollection());
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(22, topMargin);
  EXPECT_EQ(432, resultWidth);
  EXPECT_EQ(432, resultWidthLargeIPad);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhonePortrait) {
  // Setup.
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth =
      searchFieldWidth(width, IPhonePortraitTraitCollection());
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(22, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhoneLandscape) {
  // Setup.
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth =
      searchFieldWidth(width, IPhoneLandscapeTraitCollection());
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(22, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  // Action, tests.
  EXPECT_EQ(322,
            heightForLogoHeader(YES, YES, YES, YES, 0, IPadTraitCollection()));
  EXPECT_EQ(346,
            heightForLogoHeader(YES, YES, NO, YES, 0, IPadTraitCollection()));
  EXPECT_EQ(322,
            heightForLogoHeader(YES, YES, YES, NO, 0, IPadTraitCollection()));
  EXPECT_EQ(346,
            heightForLogoHeader(YES, YES, NO, NO, 0, IPadTraitCollection()));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  // Action, tests.
  EXPECT_EQ(235, heightForLogoHeader(YES, YES, YES, YES, 0,
                                     IPhonePortraitTraitCollection()));
  EXPECT_EQ(235, heightForLogoHeader(YES, YES, NO, YES, 0,
                                     IPhonePortraitTraitCollection()));
  EXPECT_EQ(235, heightForLogoHeader(YES, YES, YES, NO, 0,
                                     IPhonePortraitTraitCollection()));
  EXPECT_EQ(235, heightForLogoHeader(YES, YES, NO, NO, 0,
                                     IPhonePortraitTraitCollection()));
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

TEST_F(ContentSuggestionsCollectionUtilsTest, shrunkDoodleFrameIPhone) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kStartSurfaceShrinkLogoParam] = "true";
  feature_list.InitAndEnableFeatureWithParameters(kStartSurface, parameters);

  // Landscape.
  CGFloat heightLogoLandscape =
      doodleHeight(YES, YES, IPhoneLandscapeTraitCollection());
  CGFloat heightNoLogoLandscape =
      doodleHeight(NO, NO, IPhoneLandscapeTraitCollection());
  CGFloat topMarginLandscape =
      doodleTopMargin(YES, kTopInset, IPhoneLandscapeTraitCollection());
  EXPECT_EQ(68, heightLogoLandscape);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogoLandscape);
  EXPECT_EQ(78, topMarginLandscape);

  // Portrait
  CGFloat heightLogoPortrait =
      doodleHeight(YES, YES, IPhonePortraitTraitCollection());
  CGFloat heightNoLogoPortrait =
      doodleHeight(NO, NO, IPhonePortraitTraitCollection());
  CGFloat topMarginPortrait =
      doodleTopMargin(YES, kTopInset, IPhonePortraitTraitCollection());
  EXPECT_EQ(68, heightLogoPortrait);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogoPortrait);
  EXPECT_EQ(95, topMarginPortrait);
}

}  // namespace content_suggestions
