// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
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
  CGFloat heightLargeLogo = DoodleHeight(SearchEngineLogoState::kLogo,
                                         IPhonePortraitTraitCollection());
  CGFloat topMarginLargeLogo = DoodleTopMargin(SearchEngineLogoState::kLogo,
                                               IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(55, topMarginDoodle);
  EXPECT_EQ(IsAimEnabledInNtp() ? 50 : 36, heightLogo);
  EXPECT_EQ(IsAimEnabledInNtp() ? 41 : 55, topMarginLogo);
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

  CGFloat heightLargeLogo = DoodleHeight(SearchEngineLogoState::kLogo,
                                         IPhonePortraitTraitCollection());
  CGFloat topMarginLargeLogo = DoodleTopMargin(SearchEngineLogoState::kLogo,
                                               IPhonePortraitTraitCollection());

  // Test.
  EXPECT_EQ(68, heightDoodle);
  EXPECT_EQ(55, topMarginDoodle);
  EXPECT_EQ(IsAimEnabledInNtp() ? 50 : 36, heightLogo);
  EXPECT_EQ(IsAimEnabledInNtp() ? 41 : 55, topMarginLogo);
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
  CGFloat topMargin = SearchFieldTopMargin(SearchEngineLogoState::kLogo);

  // Test.
  EXPECT_EQ(29, topMargin);
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
  CGFloat topMargin = SearchFieldTopMargin(SearchEngineLogoState::kLogo);

  // Test.
  EXPECT_EQ(IsAimEnabledInNtp() ? 29 : 22, topMargin);
  EXPECT_EQ(IsAimEnabledInNtp() ? 432 : 343, resultWidth);
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
  CGFloat topMargin = SearchFieldTopMargin(SearchEngineLogoState::kLogo);

  // Test.
  EXPECT_EQ(IsAimEnabledInNtp() ? 29 : 22, topMargin);
  EXPECT_EQ(343, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  if (IsIPhone()) {
    GTEST_SKIP() << "Test unsupported on iPhone";
  }

  // Action, tests.
  EXPECT_EQ(328, HeightForLogoHeader(SearchEngineLogoState::kDoodle,
                                     IPadTraitCollection()));
  EXPECT_EQ(328, HeightForLogoHeader(SearchEngineLogoState::kLogo,
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
  CGFloat gain_for_MIA = IsAimEnabledInNtp() ? 21 : 0;
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
  ScopedBlockSwizzler preferredContentSizeSwizzler(
      [UIApplication class], @selector(preferredContentSizeCategory), ^{
        return UIContentSizeCategoryLarge;
      });

  // Control (Disabled).
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kNewTabPageUICleanup);
    CGFloat expectedHeight = IsAimEnabledInNtp() ? 64 : 50;
    EXPECT_EQ(expectedHeight, FakeOmniboxHeight());
  }

  // Enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kNewTabPageUICleanup);
    EXPECT_EQ(72.0, FakeOmniboxHeight());
  }
}

TEST_F(ContentSuggestionsCollectionUtilsTest, pinnedFakeOmniboxHeight) {
  CGFloat expectedHeight = IsAimEnabledInNtp() ? 48 : 36;
  EXPECT_EQ(expectedHeight, PinnedFakeOmniboxHeight());
}

TEST_F(ContentSuggestionsCollectionUtilsTest, fakeToolbarHeighta) {
  CGFloat expectedHeight = IsAimEnabledInNtp() ? 62 : 50;
  EXPECT_EQ(expectedHeight, FakeToolbarHeight());
}

// Tests that the header height is the same for Logo and Doodle, when the
// kConsistentLogoDoodleHeight feature is enabled.
TEST_F(ContentSuggestionsCollectionUtilsTest, SameLogoAndDoodleHeight) {
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kConsistentLogoDoodleHeight);

  CGFloat height_with_logo = HeightForLogoHeader(
      SearchEngineLogoState::kLogo, IPhonePortraitTraitCollection());
  CGFloat height_with_doodle = HeightForLogoHeader(
      SearchEngineLogoState::kDoodle, IPhonePortraitTraitCollection());
  EXPECT_EQ(height_with_logo, height_with_doodle);
}

// Tests that the total vertical space for Logo and Doodle is the same across
// all kNewTabPageUICleanup experiment arms when kConsistentLogoDoodleHeight is
// enabled.
TEST_F(ContentSuggestionsCollectionUtilsTest,
       SameLogoAndDoodleHeightWithUICleanup) {
  if (IsIPad()) {
    GTEST_SKIP() << "Test unsupported on iPad";
  }
  for (const char* arm : {"1", "2", "3"}) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kConsistentLogoDoodleHeight, {}},
         {kNewTabPageUICleanup, {{kNewTabPageUICleanupArmParam, arm}}}},
        {});

    CGFloat total_logo = LogoTopPadding(SearchEngineLogoState::kLogo,
                                        IPhonePortraitTraitCollection()) +
                         DoodleHeight(SearchEngineLogoState::kLogo,
                                      IPhonePortraitTraitCollection()) +
                         LogoToFakeboxPadding(SearchEngineLogoState::kLogo);

    CGFloat total_doodle = LogoTopPadding(SearchEngineLogoState::kDoodle,
                                          IPhonePortraitTraitCollection()) +
                           DoodleHeight(SearchEngineLogoState::kDoodle,
                                        IPhonePortraitTraitCollection()) +
                           LogoToFakeboxPadding(SearchEngineLogoState::kDoodle);

    // Extra height for logo when AIM is enabled (50pt vs 36pt standard logo).
    constexpr CGFloat kAimLogoHeightGain = 14.0;
    CGFloat gain_for_aim = IsAimEnabledInNtp() ? kAimLogoHeightGain : 0;
    EXPECT_EQ(total_logo, total_doodle + gain_for_aim);
  }
}

// Test padding helpers for kNewTabPageUICleanup experiment arms.
TEST_F(ContentSuggestionsCollectionUtilsTest, NTPPaddingExperimentHelpers) {
  // Control (Disabled).
  EXPECT_FALSE(IsNewTabPageUICleanupEnabled());
  EXPECT_FALSE(ShouldApplyFakeboxBackgroundAndShadow());
  EXPECT_EQ(DoodleTopMargin(SearchEngineLogoState::kLogo,
                            IPhonePortraitTraitCollection()),
            LogoTopPadding(SearchEngineLogoState::kLogo,
                           IPhonePortraitTraitCollection()));
  EXPECT_EQ(DoodleTopMargin(SearchEngineLogoState::kDoodle,
                            IPhonePortraitTraitCollection()),
            LogoTopPadding(SearchEngineLogoState::kDoodle,
                           IPhonePortraitTraitCollection()));
  EXPECT_EQ(SearchFieldTopMargin(SearchEngineLogoState::kLogo),
            LogoToFakeboxPadding(SearchEngineLogoState::kLogo));
  EXPECT_EQ(SearchFieldTopMargin(SearchEngineLogoState::kDoodle),
            LogoToFakeboxPadding(SearchEngineLogoState::kDoodle));
  EXPECT_EQ(kQuickActionsTopPaddingControl, QuickActionsTopPadding());
  EXPECT_EQ(kMostVisitedTopPaddingControl, MostVisitedTopPadding());
  EXPECT_EQ(kReducedModuleSpacingControl, ReducedModuleSpacing());

  // Tight Padding (Arm 1).
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNewTabPageUICleanup, {{kNewTabPageUICleanupArmParam, "1"}});
    EXPECT_TRUE(IsNewTabPageUICleanupEnabled());
    EXPECT_TRUE(ShouldApplyFakeboxBackgroundAndShadow());
    EXPECT_EQ(FakeToolbarHeight() + kLogoTopPaddingTight,
              LogoTopPadding(SearchEngineLogoState::kLogo,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(FakeToolbarHeight() + kDoodleTopPaddingTight,
              LogoTopPadding(SearchEngineLogoState::kDoodle,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(kLogoToFakeboxPaddingTight,
              LogoToFakeboxPadding(SearchEngineLogoState::kLogo));
    EXPECT_EQ(kDoodleToFakeboxPaddingTight,
              LogoToFakeboxPadding(SearchEngineLogoState::kDoodle));
    EXPECT_EQ(
        kQuickActionsTopPadding - ntp_header::kScrolledToTopOmniboxBottomMargin,
        QuickActionsTopPadding());
    EXPECT_EQ(kMostVisitedTopPaddingTight, MostVisitedTopPadding());
    EXPECT_EQ(kReducedModuleSpacing, ReducedModuleSpacing());
  }

  // Medium Padding (Arm 2).
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNewTabPageUICleanup, {{kNewTabPageUICleanupArmParam, "2"}});
    EXPECT_TRUE(IsNewTabPageUICleanupEnabled());
    EXPECT_TRUE(ShouldApplyFakeboxBackgroundAndShadow());
    EXPECT_EQ(FakeToolbarHeight() + kLogoTopPaddingMedium,
              LogoTopPadding(SearchEngineLogoState::kLogo,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(FakeToolbarHeight() + kDoodleTopPaddingMedium,
              LogoTopPadding(SearchEngineLogoState::kDoodle,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(kLogoToFakeboxPaddingMedium,
              LogoToFakeboxPadding(SearchEngineLogoState::kLogo));
    EXPECT_EQ(kDoodleToFakeboxPaddingMedium,
              LogoToFakeboxPadding(SearchEngineLogoState::kDoodle));
    EXPECT_EQ(
        kQuickActionsTopPadding - ntp_header::kScrolledToTopOmniboxBottomMargin,
        QuickActionsTopPadding());
    EXPECT_EQ(kMostVisitedTopPaddingMedium, MostVisitedTopPadding());
    EXPECT_EQ(kReducedModuleSpacing, ReducedModuleSpacing());
  }

  // Preferred Padding (Arm 3).
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNewTabPageUICleanup, {{kNewTabPageUICleanupArmParam, "3"}});
    EXPECT_TRUE(IsNewTabPageUICleanupEnabled());
    EXPECT_TRUE(ShouldApplyFakeboxBackgroundAndShadow());
    EXPECT_EQ(FakeToolbarHeight() + kLogoTopPaddingPreferred,
              LogoTopPadding(SearchEngineLogoState::kLogo,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(FakeToolbarHeight() + kDoodleTopPaddingPreferred,
              LogoTopPadding(SearchEngineLogoState::kDoodle,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(kLogoToFakeboxPaddingPreferred,
              LogoToFakeboxPadding(SearchEngineLogoState::kLogo));
    EXPECT_EQ(kDoodleToFakeboxPaddingPreferred,
              LogoToFakeboxPadding(SearchEngineLogoState::kDoodle));
    EXPECT_EQ(
        kQuickActionsTopPadding - ntp_header::kScrolledToTopOmniboxBottomMargin,
        QuickActionsTopPadding());
    EXPECT_EQ(kMostVisitedTopPaddingPreferred, MostVisitedTopPadding());
    EXPECT_EQ(kReducedModuleSpacing, ReducedModuleSpacing());
  }

  // Fakebox Background and Shadow Update (Arm 4).
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNewTabPageUICleanup, {{kNewTabPageUICleanupArmParam, "4"}});
    EXPECT_FALSE(IsNewTabPageUICleanupEnabled());
    EXPECT_TRUE(ShouldApplyFakeboxBackgroundAndShadow());
    EXPECT_EQ(DoodleTopMargin(SearchEngineLogoState::kLogo,
                              IPhonePortraitTraitCollection()),
              LogoTopPadding(SearchEngineLogoState::kLogo,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(DoodleTopMargin(SearchEngineLogoState::kDoodle,
                              IPhonePortraitTraitCollection()),
              LogoTopPadding(SearchEngineLogoState::kDoodle,
                             IPhonePortraitTraitCollection()));
    EXPECT_EQ(SearchFieldTopMargin(SearchEngineLogoState::kLogo),
              LogoToFakeboxPadding(SearchEngineLogoState::kLogo));
    EXPECT_EQ(SearchFieldTopMargin(SearchEngineLogoState::kDoodle),
              LogoToFakeboxPadding(SearchEngineLogoState::kDoodle));
    EXPECT_EQ(kQuickActionsTopPaddingControl, QuickActionsTopPadding());
    EXPECT_EQ(kMostVisitedTopPaddingControl, MostVisitedTopPadding());
    EXPECT_EQ(kReducedModuleSpacingControl, ReducedModuleSpacing());
  }

  // iPad (Regular x Regular Size Class) overrides.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNewTabPageUICleanup, {{kNewTabPageUICleanupArmParam, "1"}});
    EXPECT_EQ(162.0, LogoTopPadding(SearchEngineLogoState::kLogo,
                                    IPadTraitCollection()));
    EXPECT_EQ(162.0, LogoTopPadding(SearchEngineLogoState::kDoodle,
                                    IPadTraitCollection()));
    EXPECT_EQ(162.0, DoodleTopMargin(SearchEngineLogoState::kLogo,
                                     IPadTraitCollection()));
    EXPECT_EQ(kReducedModuleSpacingRegularXRegular,
              ReducedModuleSpacing(IPadTraitCollection()));
  }
}

}  // namespace content_suggestions
