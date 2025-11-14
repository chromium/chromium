// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"

#import <algorithm>

#import "base/i18n/rtl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/ntp_home_constant.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/ui_util/dynamic_type_util.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

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
const CGFloat kGoogleSearchDoodleShrunkHeight = 68;

// Height for the shrunk logo frame.
const CGFloat kGoogleSearchLogoHeight = 36;
const CGFloat kLargeFakeboxGoogleSearchLogoHeight = 50;

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
const CGFloat kReturnToRecentTabSectionBottomMargin = 25;

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
  if (ShouldEnlargeNTPFakeboxForMIA()) {
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
  if ((logo_state == SearchEngineLogoState::kLogo) &&
      ShouldEnlargeNTPFakeboxForMIA()) {
    // Shrink the top inset so that the enlarged logo has the same bottom
    // positioning as the regular logo.
    top_inset = kGoogleSearchLogoHeight - kLargeFakeboxGoogleSearchLogoHeight;
  }
  CGFloat top_margin =
      top_inset +
      AlignValueToPixel(kDoodleScaledTopMarginOther *
                        ui_util::SystemSuggestedFontSizeMultiplier());
  top_margin += kDoodleTopMarginOther;
  return top_margin;
}

CGFloat HeaderSeparatorHeight() {
  return ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);
}

CGFloat SearchFieldTopMargin() {
  return ShouldEnlargeNTPFakeboxForMIA() ? kMIASearchFieldTopMargin
                                         : kSearchFieldTopMargin;
}

CGFloat SearchFieldWidth(CGFloat width, UITraitCollection* trait_collection) {
  if (IsRegularXRegularSizeClass(trait_collection)) {
    return kSearchFieldLarge;
  }

  if (ShouldEnlargeNTPFakeboxForMIA() && !IsCompactHeight(trait_collection)) {
    return std::max(width - kMIASearchFieldMinMargin * 2, kSearchFieldSmallMin);
  }

  // Special case for narrow sizes.
  return std::clamp(width - kSearchFieldMinMargin * 2, kSearchFieldSmallMin,
                    kSearchFieldSmall);
}

CGFloat FakeOmniboxHeight() {
  if (ShouldEnlargeNTPFakeboxForMIA()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToPixel((kFakeboxHeight - kFakeboxHeightNonDynamic) *
                                 multiplier +
                             kFakeboxHeightNonDynamic);
  }
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat PinnedFakeOmniboxHeight() {
  if (ShouldEnlargeNTPFakeboxForMIA()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToPixel(
        (kPinnedFakeboxHeight - kPinnedFakeboxHeightNonDynamic) * multiplier +
        kPinnedFakeboxHeightNonDynamic);
  }
  return LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat FakeToolbarHeight() {
  if (ShouldEnlargeNTPFakeboxForMIA()) {
    return PinnedFakeOmniboxHeight() + FakeToolbarVerticalMargin();
  }
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat HeightForLogoHeader(SearchEngineLogoState logo_state,
                            UITraitCollection* trait_collection) {
  CGFloat header_height = DoodleTopMargin(logo_state, trait_collection) +
                          DoodleHeight(logo_state, trait_collection) +
                          SearchFieldTopMargin() + FakeOmniboxHeight() +
                          ntp_header::kScrolledToTopOmniboxBottomMargin +
                          ceil(HeaderSeparatorHeight());
  if (!IsRegularXRegularSizeClass(trait_collection)) {
    return header_height;
  }
  if (logo_state == SearchEngineLogoState::kNone) {
    // Returns sufficient vertical space for the Identity Disc to be
    // displayed.
    return ntp_home::kIdentityAvatarDimension +
           2 * (ntp_home::kHeaderIconMargin + ntp_home::kIdentityAvatarPadding);
  }

  header_height += kTopSpacingMaterial;

  return header_height;
}

CGFloat HeaderBottomPadding(UITraitCollection* trait_collection) {
  return IsSplitToolbarMode(trait_collection)
             ? 0
             : kNTPShrunkLogoSearchFieldBottomPadding;
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
  voice_search_button.configuration = buttonConfig;
  UIImage* mic_image = CustomSymbolWithPointSize(
      kVoiceSymbol, kSymbolContentSuggestionsPointSize);
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
  lens_button.configuration = buttonConfig;
  lens_button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_LENS);
  lens_button.accessibilityIdentifier = @"Lens";

  lens_button.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  lens_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();

  // Use a monochrome or colored symbol with no background.
  UIImage* camera_image = CustomSymbolWithPointSize(
      kCameraLensSymbol, kSymbolContentSuggestionsPointSize);
  camera_image = use_color_icon ? MakeSymbolMulticolor(camera_image)
                                : MakeSymbolMonochrome(camera_image);
  [lens_button setImage:camera_image forState:UIControlStateNormal];

  if (use_new_badge) {
    // Show the "New" badge and colored symbol.
    SetUpButtonWithNewFeatureBadge(lens_button, new_badge_color);
  }
}

void ConfigureMIAButton(UIButton* mia_button, BOOL use_color_icon) {
  [mia_button setTranslatesAutoresizingMaskIntoConstraints:NO];

  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfig.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
  mia_button.configuration = buttonConfig;

  UIImage* magnifier_icon = CustomSymbolWithPointSize(
      kMagnifyingglassSparkSymbol, kSymbolContentSuggestionsPointSize);

  magnifier_icon = use_color_icon ? MakeSymbolMulticolor(magnifier_icon)
                                  : MakeSymbolMonochrome(magnifier_icon);
  [mia_button setImage:magnifier_icon forState:UIControlStateNormal];
  // TODO(crbug.com/425339867): Handle button accessibility

  mia_button.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  mia_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();
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

UIView* NearestAncestor(UIView* view, Class of_class) {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:of_class]) {
    return view;
  }
  return NearestAncestor([view superview], of_class);
}

UIColor* SearchHintLabelColor() {
  return [UIColor colorNamed:kGrey800Color];
}

UIColor* DefaultIconTintColorWithAIMAllowed(bool aim_allowed) {
  if (aim_allowed && ShouldEnlargeNTPFakeboxForMIA()) {
    return [UIColor colorNamed:kSolidBlackColor];
  }
  return [UIColor colorNamed:kGrey700Color];
}

}  // namespace content_suggestions
