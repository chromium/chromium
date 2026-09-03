// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"

#import <algorithm>

#import "base/i18n/rtl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/ui_util/dynamic_type_util.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Width of search field.
const CGFloat kSearchFieldLarge = 432;
const CGFloat kSearchFieldSmall = 343;
const CGFloat kSearchFieldSmallMin = 304;
const CGFloat kSearchFieldMinMargin = 8;

const CGFloat kTopSpacingMaterial = 24;

// The special margins used by MIA.
const CGFloat kMIASearchFieldMinMargin = 24;

// Top margin for the doodle.
const CGFloat kDoodleTopMarginRegularXRegular = 162;
const CGFloat kDoodleTopMarginOther = 45;
// Size of the doodle top margin which is multiplied by the scaled font factor,
// and added to `kDoodleTopMarginOther` on non Regular x Regular form factors.
const CGFloat kDoodleScaledTopMarginOther = 10;

// Top margin for the search field
const CGFloat kSearchFieldTopMargin = 22;

// Top margin for the search field for single button MIA variations.
const CGFloat kMIASearchFieldTopMargin = 29;

// Bottom margin for the search field.
const CGFloat kNTPShrunkLogoSearchFieldBottomPadding = 20;

// Height for the logo and doodle frame.
const CGFloat kGoogleSearchDoodleHeight = 120;

// Height for the shrunk doodle frame.
constexpr CGFloat kGoogleSearchDoodleShrunkHeight = 68;

// Height for the shrunk logo frame.
constexpr CGFloat kGoogleSearchLogoHeight = 36;
const CGFloat kLargeFakeboxGoogleSearchLogoHeight = 50;

// Logo and Doodle margin adjustments to make the Logo and the Doodle occupy
// the same amount of vertical space on the NTP. The Doodle's margins are
// decreased and the Logo's top margin is increased.
constexpr CGFloat kDoodleLogoDelta =
    kGoogleSearchDoodleShrunkHeight - kGoogleSearchLogoHeight;
constexpr CGFloat kDoodleTopMarginAdjustment = 10;
constexpr CGFloat kDoodleBottomMarginAdjustment = 10;
constexpr CGFloat kLogoTopMarginAdjustment = kDoodleLogoDelta -
                                             kDoodleTopMarginAdjustment -
                                             kDoodleBottomMarginAdjustment;
constexpr CGFloat kCleanupDoodleTopMarginAdjustment = 15;
constexpr CGFloat kCleanupDoodleBottomMarginAdjustment = 4;

// The size of the symbol image.
const CGFloat kSymbolContentSuggestionsPointSize = 18;

// Constants for a symbol button with an new badge.
const CGFloat kSymbolButtonSize = 37.0;
const CGFloat kButtonShadowOpacity = 0.35;
const CGFloat kButtonShadowRadius = 1.0;
const CGFloat kButtonShadowVerticalOffset = 1.0;
const CGFloat kNewBadgeOffsetFromButtonCenter = 14.0;

// The height of the Fakebox.
const CGFloat kFakeboxHeight = 64;
const CGFloat kFakeboxHeightNonDynamic = 45;
const CGFloat kFakeboxHeightUICleanup = 72;

// The height of the Fakebox when it is pinned to the top.
const CGFloat kPinnedFakeboxHeight = 48;
const CGFloat kPinnedFakeboxHeightNonDynamic = 18;

// Height and width of the new feature badge.
const CGFloat kNewFeatureBadgeSize = 20;
// Font size of the new feature badge label.
const CGFloat kNewFeatureFontSize = 10;

// Returns the amount of vertical margin to include in the Fake Toolbar.
CGFloat FakeToolbarVerticalMargin() {
  UIContentSizeCategory category =
      [UIApplication sharedApplication].preferredContentSizeCategory;
  CGFloat vertical_margin =
      2 * kAdaptiveLocationBarVerticalMargin - kTopToolbarUnsplitMargin;
  CGFloat dynamic_type_vertical_adjustment =
      (ToolbarClampedFontSizeMultiplier(category) - 1) *
      (kLocationBarVerticalMarginDynamicType +
       kAdaptiveLocationBarVerticalMargin);
  return vertical_margin + dynamic_type_vertical_adjustment;
}

// Sets up fakebox button with a round background and new badge view.
void SetUpButtonWithNewFeatureBadge(UIButton* button,
                                    UIColor* new_badge_color) {
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  button.backgroundColor = [UIColor colorNamed:kOmniboxKeyboardButtonColor];
  button.layer.cornerRadius = kSymbolButtonSize / 2;

  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;

  // Remove any possible badge view created as part a previous configuration.
  for (UIView* subview in button.subviews) {
    if ([subview isKindOfClass:[NewFeatureBadgeView class]]) {
      [subview removeFromSuperview];
    }
  }

  NewFeatureBadgeView* badgeView =
      [[NewFeatureBadgeView alloc] initWithBadgeSize:kNewFeatureBadgeSize
                                            fontSize:kNewFeatureFontSize];
  badgeView.translatesAutoresizingMaskIntoConstraints = NO;
  badgeView.accessibilityElementsHidden = YES;
  if (new_badge_color) {
    [badgeView setBadgeColor:new_badge_color];
  }
  [button addSubview:badgeView];

  [NSLayoutConstraint activateConstraints:@[
    [button.widthAnchor constraintEqualToConstant:kSymbolButtonSize],
    [button.heightAnchor constraintEqualToConstant:kSymbolButtonSize],
    [badgeView.centerXAnchor
        constraintEqualToAnchor:button.centerXAnchor
                       constant:kNewBadgeOffsetFromButtonCenter],
    [badgeView.centerYAnchor
        constraintEqualToAnchor:button.centerYAnchor
                       constant:-kNewBadgeOffsetFromButtonCenter],
  ]];
}
}  // namespace

namespace content_suggestions {

const CGFloat kHintTextScale = 0.15;
const CGFloat kHintTextScaleUICleanup = 0.0;
const CGFloat kReturnToRecentTabSectionBottomMargin = 25;

// Tight Padding Arm.
const CGFloat kLogoTopPaddingTight = 9.0;
const CGFloat kLogoToFakeboxPaddingTight = 36.0;
const CGFloat kDoodleTopPaddingTight = 16.0;
const CGFloat kDoodleToFakeboxPaddingTight = 16.0;
const CGFloat kMostVisitedTopPaddingTight = 32.0;

// Medium Padding Arm.
const CGFloat kLogoTopPaddingMedium = 21.0;
const CGFloat kLogoToFakeboxPaddingMedium = 40.0;
const CGFloat kDoodleTopPaddingMedium = 24.0;
const CGFloat kDoodleToFakeboxPaddingMedium = 24.0;
const CGFloat kMostVisitedTopPaddingMedium = 36.0;

// Preferred Padding Arm.
const CGFloat kLogoTopPaddingPreferred = 33.0;
const CGFloat kLogoToFakeboxPaddingPreferred = 40.0;
const CGFloat kDoodleTopPaddingPreferred = 36.0;
const CGFloat kDoodleToFakeboxPaddingPreferred = 24.0;
const CGFloat kMostVisitedTopPaddingPreferred = 36.0;

// Control Padding.
const CGFloat kQuickActionsTopPaddingControl = 3.0;
const CGFloat kMostVisitedTopPaddingControl = 19.0;
const CGFloat kReducedModuleSpacingControl = 14.0;

// Shared spacing constants.
const CGFloat kQuickActionsTopPadding = 12.0;
const CGFloat kReducedModuleSpacing = 12.0;
const CGFloat kReducedModuleSpacingRegularXRegular = 14.0;

CGFloat DoodleHeight(SearchEngineLogoState logo_state,
                     UITraitCollection* trait_collection) {
  // For users with non-Google default search engine, there is no doodle.
  if (logo_state == SearchEngineLogoState::kNone) {
    return IsRegularXRegularSizeClass(trait_collection)
               ? kGoogleSearchDoodleHeight
               : 0;
  }
  if ((logo_state == SearchEngineLogoState::kDoodle) ||
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)) {
    return kGoogleSearchDoodleShrunkHeight;
  }
  if (IsAimEnabledInNtp()) {
    return kLargeFakeboxGoogleSearchLogoHeight;
  }
  return kGoogleSearchLogoHeight;
}

CGFloat DoodleTopMargin(SearchEngineLogoState logo_state,
                        UITraitCollection* trait_collection) {
  if (IsRegularXRegularSizeClass(trait_collection)) {
    return kDoodleTopMarginRegularXRegular;
  }
  CGFloat top_inset = 0;
  if ((logo_state == SearchEngineLogoState::kLogo) && IsAimEnabledInNtp()) {
    // Shrink the top inset so that the enlarged logo has the same bottom
    // positioning as the regular logo.
    top_inset = kGoogleSearchLogoHeight - kLargeFakeboxGoogleSearchLogoHeight;
  }

  if (IsConsistentLogoDoodleHeightEnabled() &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    if (logo_state == SearchEngineLogoState::kDoodle) {
      top_inset -= kDoodleTopMarginAdjustment;
    } else if (logo_state == SearchEngineLogoState::kLogo) {
      top_inset += kLogoTopMarginAdjustment;
    }
  }
  CGFloat top_margin =
      top_inset +
      AlignValueToLowerPixel(kDoodleScaledTopMarginOther *
                             ui_util::SystemSuggestedFontSizeMultiplier());
  top_margin += kDoodleTopMarginOther;
  return top_margin;
}

CGFloat HeaderSeparatorHeight() {
  return AlignValueToUpperPixel(kToolbarSeparatorHeight);
}

CGFloat SearchFieldTopMargin(SearchEngineLogoState logo_state) {
  CGFloat margin =
      IsAimEnabledInNtp() ? kMIASearchFieldTopMargin : kSearchFieldTopMargin;
  if (IsConsistentLogoDoodleHeightEnabled() &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    if (logo_state == SearchEngineLogoState::kDoodle) {
      margin -= kDoodleBottomMarginAdjustment;
    }
  }
  return margin;
}

CGFloat SearchFieldWidth(CGFloat width, UITraitCollection* trait_collection) {
  if (IsRegularXRegularSizeClass(trait_collection)) {
    return IsNewTabPageUICleanupEnabled()
               ? kDiscoverFeedContentMaxWidthUICleanup
               : kSearchFieldLarge;
  }

  if (IsNewTabPageUICleanupEnabled()) {
    return std::clamp(width - (2 * kNewTabPageHorizontalMargin),
                      kSearchFieldSmallMin, kSearchFieldLarge);
  }

  if (IsAimEnabledInNtp() && !IsCompactHeight(trait_collection)) {
    return std::clamp(width - kMIASearchFieldMinMargin * 2,
                      kSearchFieldSmallMin, kSearchFieldLarge);
  }

  // Special case for narrow sizes.
  return std::clamp(width - kSearchFieldMinMargin * 2, kSearchFieldSmallMin,
                    kSearchFieldSmall);
}

CGFloat FakeOmniboxHeight() {
  if (IsNewTabPageUICleanupEnabled()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToLowerPixel(
        (kFakeboxHeightUICleanup - kFakeboxHeightNonDynamic) * multiplier +
        kFakeboxHeightNonDynamic);
  }
  if (IsAimEnabledInNtp()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToLowerPixel((kFakeboxHeight - kFakeboxHeightNonDynamic) *
                                      multiplier +
                                  kFakeboxHeightNonDynamic);
  }
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat PinnedFakeOmniboxHeight() {
  if (IsAimEnabledInNtp()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToLowerPixel(
        (kPinnedFakeboxHeight - kPinnedFakeboxHeightNonDynamic) * multiplier +
        kPinnedFakeboxHeightNonDynamic);
  }
  return LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat FakeToolbarHeight() {
  if (IsAimEnabledInNtp()) {
    return PinnedFakeOmniboxHeight() + FakeToolbarVerticalMargin();
  }
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat HeightForLogoHeader(SearchEngineLogoState logo_state,
                            UITraitCollection* trait_collection) {
  CGFloat header_height = LogoTopPadding(logo_state, trait_collection) +
                          DoodleHeight(logo_state, trait_collection) +
                          LogoToFakeboxPadding(logo_state) +
                          FakeOmniboxHeight() +
                          ntp_header::kScrolledToTopOmniboxBottomMargin +
                          ceil(HeaderSeparatorHeight());
  if (!IsRegularXRegularSizeClass(trait_collection)) {
    return header_height;
  }
  if (logo_state == SearchEngineLogoState::kNone) {
    // Returns sufficient vertical space for the Identity Disc to be
    // displayed.
    return ntp_home::kIdentityAvatarDiameter +
           2 * (ntp_home::kHeaderIconMargin + ntp_home::kIdentityAvatarPadding);
  }

  // Minimize spacing between AI-mode entrypoint on large size class.
  if (!base::FeatureList::IsEnabled(kAIMNTPEntrypointTablet)) {
    header_height += kTopSpacingMaterial;
  }

  return header_height;
}

CGFloat HeaderBottomPadding(UITraitCollection* trait_collection) {
  return IsSplitToolbarMode(trait_collection) || IsNewTabPageUICleanupEnabled()
             ? 0
             : kNTPShrunkLogoSearchFieldBottomPadding;
}

CGFloat LogoTopPadding(SearchEngineLogoState logo_state,
                       UITraitCollection* trait_collection) {
  if (IsRegularXRegularSizeClass(trait_collection)) {
    return kDoodleTopMarginRegularXRegular;
  }
  const bool is_doodle = (logo_state == SearchEngineLogoState::kDoodle);
  CGFloat padding = 0;
  switch (GetNewTabPageUICleanupVariation()) {
    case NTPUICleanupVariation::kTightPadding:
      padding = is_doodle ? kDoodleTopPaddingTight : kLogoTopPaddingTight;
      break;
    case NTPUICleanupVariation::kMediumPadding:
      padding = is_doodle ? kDoodleTopPaddingMedium : kLogoTopPaddingMedium;
      break;
    case NTPUICleanupVariation::kPreferredPadding:
      padding =
          is_doodle ? kDoodleTopPaddingPreferred : kLogoTopPaddingPreferred;
      break;
    case NTPUICleanupVariation::kFakeboxBackgroundAndShadow:
    case NTPUICleanupVariation::kDisabled:
      return DoodleTopMargin(logo_state, trait_collection);
  }
  padding += FakeToolbarHeight();
  if (IsConsistentLogoDoodleHeightEnabled() &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET && is_doodle) {
    padding -= kCleanupDoodleTopMarginAdjustment;
  }
  return padding;
}

CGFloat LogoToFakeboxPadding(SearchEngineLogoState logo_state) {
  const bool is_doodle = (logo_state == SearchEngineLogoState::kDoodle);
  CGFloat padding = 0;
  switch (GetNewTabPageUICleanupVariation()) {
    case NTPUICleanupVariation::kTightPadding:
      padding =
          is_doodle ? kDoodleToFakeboxPaddingTight : kLogoToFakeboxPaddingTight;
      break;
    case NTPUICleanupVariation::kMediumPadding:
      padding = is_doodle ? kDoodleToFakeboxPaddingMedium
                          : kLogoToFakeboxPaddingMedium;
      break;
    case NTPUICleanupVariation::kPreferredPadding:
      padding = is_doodle ? kDoodleToFakeboxPaddingPreferred
                          : kLogoToFakeboxPaddingPreferred;
      break;
    case NTPUICleanupVariation::kFakeboxBackgroundAndShadow:
    case NTPUICleanupVariation::kDisabled:
      return SearchFieldTopMargin(logo_state);
  }
  if (IsConsistentLogoDoodleHeightEnabled() &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET && is_doodle) {
    padding -= kCleanupDoodleBottomMarginAdjustment;
  }
  return padding;
}

CGFloat QuickActionsTopPadding() {
  switch (GetNewTabPageUICleanupVariation()) {
    case NTPUICleanupVariation::kTightPadding:
    case NTPUICleanupVariation::kMediumPadding:
    case NTPUICleanupVariation::kPreferredPadding:
      // When NTP Redesign is enabled, Quick Actions is constrained directly to
      // the fakebox, so the intended 12pt padding is used. Otherwise, subtract
      // `ntp_header::kScrolledToTopOmniboxBottomMargin` from the intended
      // padding to offset the header view's bottom margin.
      return IsNTPRedesignEnabled()
                 ? kQuickActionsTopPadding
                 : (kQuickActionsTopPadding -
                    ntp_header::kScrolledToTopOmniboxBottomMargin);
    case NTPUICleanupVariation::kFakeboxBackgroundAndShadow:
    case NTPUICleanupVariation::kDisabled:
      return kQuickActionsTopPaddingControl;
  }
}

CGFloat MostVisitedTopPadding() {
  switch (GetNewTabPageUICleanupVariation()) {
    case NTPUICleanupVariation::kTightPadding:
      return kMostVisitedTopPaddingTight;
    case NTPUICleanupVariation::kMediumPadding:
      return kMostVisitedTopPaddingMedium;
    case NTPUICleanupVariation::kPreferredPadding:
      return kMostVisitedTopPaddingPreferred;
    case NTPUICleanupVariation::kFakeboxBackgroundAndShadow:
    case NTPUICleanupVariation::kDisabled:
      return kMostVisitedTopPaddingControl;
  }
}

CGFloat ReducedModuleSpacing(UITraitCollection* trait_collection) {
  if (IsRegularXRegularSizeClass(trait_collection)) {
    return kReducedModuleSpacingRegularXRegular;
  }
  switch (GetNewTabPageUICleanupVariation()) {
    case NTPUICleanupVariation::kTightPadding:
    case NTPUICleanupVariation::kMediumPadding:
    case NTPUICleanupVariation::kPreferredPadding:
      return kReducedModuleSpacing;
    case NTPUICleanupVariation::kFakeboxBackgroundAndShadow:
    case NTPUICleanupVariation::kDisabled:
      return kReducedModuleSpacingControl;
  }
}

void ConfigureSearchHintLabel(UILabel* search_hint_label,
                              UIView* search_tab_target,
                              NSString* placeholder_text) {
  [search_hint_label setTranslatesAutoresizingMaskIntoConstraints:NO];
  [search_tab_target addSubview:search_hint_label];

  [search_hint_label setText:placeholder_text];
  if (base::i18n::IsRTL()) {
    [search_hint_label setTextAlignment:NSTextAlignmentRight];
  }
  search_hint_label.textColor = SearchHintLabelColor();
  search_hint_label.adjustsFontForContentSizeCategory = YES;
  search_hint_label.textAlignment = NSTextAlignmentCenter;
}

void ConfigureVoiceSearchButton(UIButton* voice_search_button,
                                BOOL use_color_icon) {
  [voice_search_button setTranslatesAutoresizingMaskIntoConstraints:NO];

  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfig.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
  buttonConfig.background.backgroundColor = [UIColor clearColor];
  voice_search_button.configuration = buttonConfig;
  UIImage* mic_image =
      SymbolWithPointSize(SymbolVoice, kSymbolContentSuggestionsPointSize);
  mic_image = use_color_icon ? MakeSymbolMulticolor(mic_image)
                             : MakeSymbolMonochrome(mic_image);
  [voice_search_button setImage:mic_image forState:UIControlStateNormal];
  [voice_search_button setAccessibilityLabel:l10n_util::GetNSString(
                                                 IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voice_search_button setAccessibilityIdentifier:@"Voice Search"];

  voice_search_button.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  voice_search_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();
}

void ConfigureLensButtonAppearance(UIButton* lens_button,
                                   BOOL use_new_badge,
                                   BOOL use_color_icon,
                                   UIColor* new_badge_color) {
  lens_button.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfig.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
  buttonConfig.background.backgroundColor = [UIColor clearColor];
  lens_button.configuration = buttonConfig;
  lens_button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_LENS);
  lens_button.accessibilityIdentifier = @"Lens";

  lens_button.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  lens_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();

  // Use a monochrome or colored symbol with no background.
  UIImage* camera_image =
      SymbolWithPointSize(SymbolCameraLens, kSymbolContentSuggestionsPointSize);
  camera_image = use_color_icon ? MakeSymbolMulticolor(camera_image)
                                : MakeSymbolMonochrome(camera_image);
  [lens_button setImage:camera_image forState:UIControlStateNormal];

  if (use_new_badge) {
    // Show the "New" badge and colored symbol.
    SetUpButtonWithNewFeatureBadge(lens_button, new_badge_color);
  }
}

void ConfigureLensButtonWithNewBadgeAlpha(UIButton* lens_button,
                                          CGFloat new_badge_alpha) {
  // Fade button background.
  lens_button.backgroundColor =
      [[UIColor colorNamed:kOmniboxKeyboardButtonColor]
          colorWithAlphaComponent:new_badge_alpha];
  lens_button.layer.shadowOpacity = kButtonShadowOpacity * new_badge_alpha;

  UIView* attachedBadgeView = nil;
  for (UIView* subview in lens_button.subviews) {
    if ([subview isKindOfClass:[NewFeatureBadgeView class]]) {
      attachedBadgeView = subview;
      break;
    }
  }

  // Scale the N badge.
  attachedBadgeView.alpha = new_badge_alpha;
  attachedBadgeView.transform = CGAffineTransformScale(
      CGAffineTransformIdentity, new_badge_alpha, new_badge_alpha);

  for (UIView* subview in lens_button.imageView.subviews) {
    subview.alpha = new_badge_alpha;
    subview.transform = CGAffineTransformScale(
        CGAffineTransformIdentity, new_badge_alpha, new_badge_alpha);
  }
}

UIColor* SearchHintLabelColor() {
  if (IsNewTabPageUICleanupEnabled()) {
    return [UIColor colorWithDynamicProvider:^UIColor*(
                        UITraitCollection* trait_collection) {
      if (trait_collection.userInterfaceStyle == UIUserInterfaceStyleDark) {
        return [UIColor colorNamed:kTextSecondaryColor];
      }
      return [UIColor colorNamed:kTextTertiaryColor];
    }];
  }
  return [UIColor colorNamed:kGrey800Color];
}

UIColor* DefaultIconTintColorWithAIMAllowed(bool aim_allowed) {
  if (aim_allowed && IsAimEnabledInNtp()) {
    return [UIColor colorNamed:kSolidBlackColor];
  }
  return [UIColor colorNamed:kGrey700Color];
}

}  // namespace content_suggestions
