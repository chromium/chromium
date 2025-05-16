// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

namespace content_suggestions {

CGFloat kDoodleHeightNoLogo = 0;

class ContentSuggestionsCollectionUtilsTest : public PlatformTest {
 public:
  UITraitCollection* IPadTraitCollection() {
    if (@available(iOS 17, *)) {
      return [UITraitCollection
          traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
            mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
            mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
          }];
    }
#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
    else {
      UITraitCollection* horizontalRegular =
          [UITraitCollection traitCollectionWithHorizontalSizeClass:
                                 UIUserInterfaceSizeClassRegular];
      UITraitCollection* verticalRegular = [UITraitCollection
          traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassRegular];
      return [UITraitCollection traitCollectionWithTraitsFromCollections:@[
        verticalRegular, horizontalRegular
      ]];
    }
#else
    return UITraitCollection.currentTraitCollection;
#endif
  }

  UITraitCollection* IPhoneLandscapeTraitCollection() {
    if (@available(iOS 17, *)) {
      return [UITraitCollection
          traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
            mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
            mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassCompact;
          }];
    }
#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
    else {
      UITraitCollection* horizontalCompact =
          [UITraitCollection traitCollectionWithHorizontalSizeClass:
                                 UIUserInterfaceSizeClassCompact];
      UITraitCollection* verticalCompact = [UITraitCollection
          traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassCompact];
      return [UITraitCollection traitCollectionWithTraitsFromCollections:@[
        verticalCompact, horizontalCompact
      ]];
    }
#else
    return UITraitCollection.currentTraitCollection;
#endif
  }

  UITraitCollection* IPhonePortraitTraitCollection() {
    if (@available(iOS 17, *)) {
      return [UITraitCollection
          traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
            mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
            mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
          }];
    }
#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
    else {
      UITraitCollection* horizontalCompact =
          [UITraitCollection traitCollectionWithHorizontalSizeClass:
                                 UIUserInterfaceSizeClassCompact];
      UITraitCollection* verticalRegular = [UITraitCollection
          traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassRegular];
      return [UITraitCollection traitCollectionWithTraitsFromCollections:@[
        verticalRegular, horizontalCompact
      ]];
    }
#else
    return UITraitCollection.currentTraitCollection;
#endif
  }

  bool IsIPad() {
    return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  }
};

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPad) {
  // Action.
  CGFloat heightDoodle = DoodleHeight(YES, YES, IPadTraitCollection());
  CGFloat topMarginDoodle = DoodleTopMargin(YES, YES, IPadTraitCollection());
  CGFloat heightLogo = DoodleHeight(YES, NO, IPadTraitCollection());
  CGFloat topMarginLogo = DoodleTopMargin(YES, NO, IPadTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(162, topMarginDoodle);
  EXPECT_EQ(IsIPad() ? 68 : 36, heightLogo);
  EXPECT_EQ(162, topMarginLogo);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhonePortrait) {
  // Action.
  CGFloat heightDoodle =
      DoodleHeight(YES, YES, IPhonePortraitTraitCollection());
  CGFloat topMarginDoodle =
      DoodleTopMargin(YES, YES, IPhonePortraitTraitCollection());
  CGFloat heightLogo = DoodleHeight(YES, NO, IPhonePortraitTraitCollection());
  CGFloat topMarginLogo =
      DoodleTopMargin(YES, NO, IPhonePortraitTraitCollection());
  CGFloat heightNoLogo = DoodleHeight(NO, NO, IPhonePortraitTraitCollection());
  CGFloat topMarginNoLogo =
      DoodleTopMargin(NO, NO, IPhonePortraitTraitCollection());

  // Action when large logo is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, "true"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kDeprecateFeedHeader, large_fakebox_params}},
      /*disabled_features=*/{});
  CGFloat heightLargeLogo =
      DoodleHeight(YES, NO, IPhonePortraitTraitCollection());
  CGFloat topMarginLargeLogo =
      DoodleTopMargin(YES, NO, IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(55, topMarginDoodle);
  EXPECT_EQ(IsIPad() ? 68 : 36, heightLogo);
  EXPECT_EQ(55, topMarginLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(55, topMarginNoLogo);
  EXPECT_EQ(IsIPad() ? 68 : 50, heightLargeLogo);
  EXPECT_EQ(41, topMarginLargeLogo);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhoneLandscape) {
  // Action.
  CGFloat heightDoodle =
      DoodleHeight(YES, YES, IPhoneLandscapeTraitCollection());
  CGFloat topMarginDoodle =
      DoodleTopMargin(YES, YES, IPhonePortraitTraitCollection());
  CGFloat heightLogo = DoodleHeight(YES, NO, IPhoneLandscapeTraitCollection());
  CGFloat topMarginLogo =
      DoodleTopMargin(YES, NO, IPhoneLandscapeTraitCollection());
  CGFloat heightNoLogo = DoodleHeight(NO, NO, IPhoneLandscapeTraitCollection());
  CGFloat topMarginNoLogo =
      DoodleTopMargin(NO, NO, IPhoneLandscapeTraitCollection());

  // Action when large logo is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, "true"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kDeprecateFeedHeader, large_fakebox_params}},
      /*disabled_features=*/{});
  CGFloat heightLargeLogo =
      DoodleHeight(YES, NO, IPhonePortraitTraitCollection());
  CGFloat topMarginLargeLogo =
      DoodleTopMargin(YES, NO, IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(55, topMarginDoodle);
  EXPECT_EQ(IsIPad() ? 68 : 36, heightLogo);
  EXPECT_EQ(55, topMarginLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(55, topMarginNoLogo);
  EXPECT_EQ(IsIPad() ? 68 : 50, heightLargeLogo);
  EXPECT_EQ(41, topMarginLargeLogo);
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
  EXPECT_EQ(IsIPad() ? 331 : 299,
            HeightForLogoHeader(YES, NO, IPadTraitCollection()));
  EXPECT_EQ(64 + kDoodleHeightNoLogo,
            HeightForLogoHeader(NO, NO, IPadTraitCollection()));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  // Action, tests.
  EXPECT_EQ(200,
            HeightForLogoHeader(YES, YES, IPhonePortraitTraitCollection()));
  EXPECT_EQ(IsIPad() ? 200 : 168,
            HeightForLogoHeader(YES, NO, IPhonePortraitTraitCollection()));
  EXPECT_EQ(132, HeightForLogoHeader(NO, NO, IPhonePortraitTraitCollection()));
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

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeOmniboxHeight) {
  EXPECT_EQ(50, FakeOmniboxHeight());
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, "true"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kDeprecateFeedHeader, large_fakebox_params}},
      /*disabled_features=*/{});
  EXPECT_EQ(65, FakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, pinnedFakeOmniboxHeight) {
  EXPECT_EQ(36, PinnedFakeOmniboxHeight());
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, "true"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kDeprecateFeedHeader, large_fakebox_params}},
      /*disabled_features=*/{});
  EXPECT_EQ(48, PinnedFakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeToolbarHeighta) {
  EXPECT_EQ(50, FakeToolbarHeight());
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, "true"}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kDeprecateFeedHeader, large_fakebox_params}},
      /*disabled_features=*/{});
  EXPECT_EQ(62, FakeToolbarHeight());
}

}  // namespace content_suggestions
