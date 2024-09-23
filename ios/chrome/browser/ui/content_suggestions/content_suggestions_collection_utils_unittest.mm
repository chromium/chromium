// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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
  CGFloat height = DoodleHeight(YES, YES, IPadTraitCollection());
  CGFloat topMargin = DoodleTopMargin(kTopInset, IPadTraitCollection());

  // Test.
  EXPECT_EQ(68, height);
  EXPECT_EQ(162, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhonePortrait) {
  // Action.
  CGFloat heightLogo = DoodleHeight(YES, YES, IPhonePortraitTraitCollection());
  CGFloat heightNoLogo = DoodleHeight(NO, NO, IPhonePortraitTraitCollection());
  CGFloat topMargin =
      DoodleTopMargin(kTopInset, IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(75, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhoneLandscape) {
  // Action.
  CGFloat heightLogo = DoodleHeight(YES, YES, IPhoneLandscapeTraitCollection());
  CGFloat heightNoLogo = DoodleHeight(NO, NO, IPhoneLandscapeTraitCollection());
  CGFloat topMargin =
      DoodleTopMargin(kTopInset, IPhoneLandscapeTraitCollection());

  // Test.
  EXPECT_EQ(68, heightLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(75, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPad) {
  // Setup.
  CGFloat width = 500;
  CGFloat largeIPadWidth = 1366;

  // Action.
  CGFloat resultWidth = SearchFieldWidth(width, IPadTraitCollection());
  CGFloat resultWidthLargeIPad =
      SearchFieldWidth(largeIPadWidth, IPadTraitCollection());
  CGFloat topMargin = SearchFieldTopMargin();

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
      SearchFieldWidth(width, IPhonePortraitTraitCollection());
  CGFloat topMargin = SearchFieldTopMargin();

  // Test.
  EXPECT_EQ(22, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhoneLandscape) {
  // Setup.
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth =
      SearchFieldWidth(width, IPhoneLandscapeTraitCollection());
  CGFloat topMargin = SearchFieldTopMargin();

  // Test.
  EXPECT_EQ(22, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  // Action, tests.
  EXPECT_EQ(331, HeightForLogoHeader(YES, YES, IPadTraitCollection()));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  // Action, tests.
  EXPECT_EQ(200,
            HeightForLogoHeader(YES, YES, IPhonePortraitTraitCollection()));
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
  EXPECT_EQ(leafView, NearestAncestor(leafView, [UIScrollView class]));
  EXPECT_EQ(leafView, NearestAncestor(leafView, [UIView class]));
  EXPECT_EQ(rootView, NearestAncestor(leafView, [UILabel class]));
  EXPECT_EQ(nil, NearestAncestor(leafView, [UITextView class]));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, shrunkDoodleFrameIPhone) {
  // Landscape.
  CGFloat heightLogoLandscape =
      DoodleHeight(YES, YES, IPhoneLandscapeTraitCollection());
  CGFloat heightNoLogoLandscape =
      DoodleHeight(NO, NO, IPhoneLandscapeTraitCollection());
  CGFloat topMarginLandscape =
      DoodleTopMargin(kTopInset, IPhoneLandscapeTraitCollection());
  EXPECT_EQ(68, heightLogoLandscape);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogoLandscape);
  EXPECT_EQ(75, topMarginLandscape);

  // Portrait
  CGFloat heightLogoPortrait =
      DoodleHeight(YES, YES, IPhonePortraitTraitCollection());
  CGFloat heightNoLogoPortrait =
      DoodleHeight(NO, NO, IPhonePortraitTraitCollection());
  CGFloat topMarginPortrait =
      DoodleTopMargin(kTopInset, IPhonePortraitTraitCollection());
  EXPECT_EQ(68, heightLogoPortrait);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogoPortrait);
  EXPECT_EQ(75, topMarginPortrait);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeOmniboxHeight) {
  EXPECT_EQ(50, FakeOmniboxHeight());
  base::test::ScopedFeatureList scoped_feature_list(kIOSLargeFakebox);
  EXPECT_EQ(65, FakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, pinnedFakeOmniboxHeight) {
  EXPECT_EQ(36, PinnedFakeOmniboxHeight());
  base::test::ScopedFeatureList scoped_feature_list(kIOSLargeFakebox);
  EXPECT_EQ(48, PinnedFakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeToolbarHeight) {
  EXPECT_EQ(50, FakeToolbarHeight());
  base::test::ScopedFeatureList scoped_feature_list(kIOSLargeFakebox);
  EXPECT_EQ(62, FakeToolbarHeight());
}

}  // namespace content_suggestions
