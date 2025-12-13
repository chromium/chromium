// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

namespace content_suggestions {

constexpr CGFloat kDoodleHeightNoLogo = 0;

class ContentSuggestionsCollectionUtilsTest : public PlatformTest {
 public:
  UITraitCollection* IPadTraitCollection() {
    return [UITraitCollection
        traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
          mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
          mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
        }];
  }

  UITraitCollection* IPhoneLandscapeTraitCollection() {
    return [UITraitCollection
        traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
          mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
          mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassCompact;
        }];
  }

  UITraitCollection* IPhonePortraitTraitCollection() {
    return [UITraitCollection
        traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
          mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
          mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
        }];
  }

  bool IsIPad() {
    return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  }

  bool IsIPhone() {
    return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  }
};

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPad) {
  if (IsIPhone()) {
    GTEST_SKIP() << "Test unsupported on iPhone";
  }
  // Action.
  CGFloat heightDoodle =
      DoodleHeight(SearchEngineLogoState::kDoodle, IPadTraitCollection());
  CGFloat topMarginDoodle =
      DoodleTopMargin(SearchEngineLogoState::kDoodle, IPadTraitCollection());
  CGFloat heightLogo =
      DoodleHeight(SearchEngineLogoState::kLogo, IPadTraitCollection());
  CGFloat topMarginLogo =
      DoodleTopMargin(SearchEngineLogoState::kLogo, IPadTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(162, topMarginDoodle);
  EXPECT_EQ(IsIPad() ? 68 : 36, heightLogo);
  EXPECT_EQ(162, topMarginLogo);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhonePortrait) {
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }
  // Action.
  CGFloat heightDoodle = DoodleHeight(SearchEngineLogoState::kDoodle,
                                      IPhonePortraitTraitCollection());
  CGFloat topMarginDoodle = DoodleTopMargin(SearchEngineLogoState::kDoodle,
                                            IPhonePortraitTraitCollection());
  CGFloat heightLogo = DoodleHeight(SearchEngineLogoState::kLogo,
                                    IPhonePortraitTraitCollection());
  CGFloat topMarginLogo = DoodleTopMargin(SearchEngineLogoState::kLogo,
                                          IPhonePortraitTraitCollection());
  CGFloat heightNoLogo = DoodleHeight(SearchEngineLogoState::kNone,
                                      IPhonePortraitTraitCollection());
  CGFloat topMarginNoLogo = DoodleTopMargin(SearchEngineLogoState::kNone,
                                            IPhonePortraitTraitCollection());

  // Action when large logo is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kNTPMIAEntrypointParam,
       kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kNTPMIAEntrypoint, large_fakebox_params}},
      /*disabled_features=*/{});
  CGFloat heightLargeLogo = DoodleHeight(SearchEngineLogoState::kLogo,
                                         IPhonePortraitTraitCollection());
  CGFloat topMarginLargeLogo = DoodleTopMargin(SearchEngineLogoState::kLogo,
                                               IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(55, topMarginDoodle);
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 50 : 36, heightLogo);
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 41 : 55, topMarginLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(55, topMarginNoLogo);
  EXPECT_EQ(50, heightLargeLogo);
  EXPECT_EQ(41, topMarginLargeLogo);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhoneLandscape) {
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }

  // Action.
  CGFloat heightDoodle = DoodleHeight(SearchEngineLogoState::kDoodle,
                                      IPhoneLandscapeTraitCollection());
  CGFloat topMarginDoodle = DoodleTopMargin(SearchEngineLogoState::kDoodle,
                                            IPhonePortraitTraitCollection());
  CGFloat heightLogo = DoodleHeight(SearchEngineLogoState::kLogo,
                                    IPhoneLandscapeTraitCollection());
  CGFloat topMarginLogo = DoodleTopMargin(SearchEngineLogoState::kLogo,
                                          IPhoneLandscapeTraitCollection());
  CGFloat heightNoLogo = DoodleHeight(SearchEngineLogoState::kNone,
                                      IPhoneLandscapeTraitCollection());
  CGFloat topMarginNoLogo = DoodleTopMargin(SearchEngineLogoState::kNone,
                                            IPhoneLandscapeTraitCollection());

  // Action when large logo is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kNTPMIAEntrypointParam,
       kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kNTPMIAEntrypoint, large_fakebox_params}},
      /*disabled_features=*/{});
  CGFloat heightLargeLogo = DoodleHeight(SearchEngineLogoState::kLogo,
                                         IPhonePortraitTraitCollection());
  CGFloat topMarginLargeLogo = DoodleTopMargin(SearchEngineLogoState::kLogo,
                                               IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(55, topMarginDoodle);
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 50 : 36, heightLogo);
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 41 : 55, topMarginLogo);
  EXPECT_EQ(kDoodleHeightNoLogo, heightNoLogo);
  EXPECT_EQ(55, topMarginNoLogo);
  EXPECT_EQ(50, heightLargeLogo);
  EXPECT_EQ(41, topMarginLargeLogo);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPad) {
  if (IsIPhone()) {
    GTEST_SKIP() << "Test unsupported on iPhone";
  }

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
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }
  // Setup.
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth =
      SearchFieldWidth(width, IPhonePortraitTraitCollection());
  CGFloat topMargin = SearchFieldTopMargin();

  // Test.
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 29 : 22, topMargin);
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 452 : 343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhoneLandscape) {
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }
  // Setup.
  CGFloat width = 500;

  // Action.
  CGFloat resultWidth =
      SearchFieldWidth(width, IPhoneLandscapeTraitCollection());
  CGFloat topMargin = SearchFieldTopMargin();

  // Test.
  EXPECT_EQ(ShouldEnlargeNTPFakeboxForMIA() ? 29 : 22, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  if (IsIPhone()) {
    GTEST_SKIP() << "Test unsupported on iPhone";
  }

  // Action, tests.
  EXPECT_EQ(331, HeightForLogoHeader(SearchEngineLogoState::kDoodle,
                                     IPadTraitCollection()));
  EXPECT_EQ(331, HeightForLogoHeader(SearchEngineLogoState::kLogo,
                                     IPadTraitCollection()));
  EXPECT_EQ(
      64 + kDoodleHeightNoLogo,
      HeightForLogoHeader(SearchEngineLogoState::kNone, IPadTraitCollection()));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }

  // Extra spacing when MIA is shown.
  CGFloat gain_for_MIA = ShouldEnlargeNTPFakeboxForMIA() ? 21 : 0;
  // Action, tests.
  EXPECT_EQ(200 + gain_for_MIA,
            HeightForLogoHeader(SearchEngineLogoState::kDoodle,
                                IPhonePortraitTraitCollection()));
  EXPECT_EQ(168 + gain_for_MIA,
            HeightForLogoHeader(SearchEngineLogoState::kLogo,
                                IPhonePortraitTraitCollection()));
  EXPECT_EQ(132 + gain_for_MIA,
            HeightForLogoHeader(SearchEngineLogoState::kNone,
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
  EXPECT_EQ(leafView, NearestAncestor(leafView, [UIScrollView class]));
  EXPECT_EQ(leafView, NearestAncestor(leafView, [UIView class]));
  EXPECT_EQ(rootView, NearestAncestor(leafView, [UILabel class]));
  EXPECT_EQ(nil, NearestAncestor(leafView, [UITextView class]));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeOmniboxHeight) {
  CGFloat expectedHeight = ShouldEnlargeNTPFakeboxForMIA() ? 64 : 50;
  EXPECT_EQ(expectedHeight, FakeOmniboxHeight());
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kNTPMIAEntrypointParam,
       kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kNTPMIAEntrypoint, large_fakebox_params}},
      /*disabled_features=*/{});
  EXPECT_EQ(64, FakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, pinnedFakeOmniboxHeight) {
  CGFloat expectedHeight = ShouldEnlargeNTPFakeboxForMIA() ? 48 : 36;
  EXPECT_EQ(expectedHeight, PinnedFakeOmniboxHeight());
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kNTPMIAEntrypointParam,
       kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kNTPMIAEntrypoint, large_fakebox_params}},
      /*disabled_features=*/{});
  EXPECT_EQ(48, PinnedFakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeToolbarHeighta) {
  CGFloat expectedHeight = ShouldEnlargeNTPFakeboxForMIA() ? 62 : 50;
  EXPECT_EQ(expectedHeight, FakeToolbarHeight());
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams large_fakebox_params = {
      {kNTPMIAEntrypointParam,
       kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}};
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kNTPMIAEntrypoint, large_fakebox_params}},
      /*disabled_features=*/{});
  EXPECT_EQ(62, FakeToolbarHeight());
}

}  // namespace content_suggestions
