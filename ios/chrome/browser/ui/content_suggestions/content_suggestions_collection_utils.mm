// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#import "base/i18n/rtl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
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

// Top margin for the doodle.
const CGFloat kDoodleTopMarginRegularXRegular = 162;
const CGFloat kDoodleTopMarginOther = 65;
// Size of the doodle top margin which is multiplied by the scaled font factor,
// and added to `kDoodleTopMarginOther` on non Regular x Regular form factors.
const CGFloat kDoodleScaledTopMarginOther = 10;
const CGFloat kLargeFakeboxExtraDoodleTopMargin = 10;

// Top margin for the search field
const CGFloat kSearchFieldTopMargin = 22;
const CGFloat kLargeFakeboxSearchFieldTopMargin = 40;

// Bottom margin for the search field.
const CGFloat kNTPShrunkLogoSearchFieldBottomPadding = 20;

// Height for the logo and doodle frame.
const CGFloat kGoogleSearchDoodleHeight = 120;

// Height for the shrunk doodle frame.
// TODO(crbug.com/40744549): clean up post-launch.
const CGFloat kGoogleSearchDoodleShrunkHeight = 68;

// Height for the shrunk logo frame.
// TODO(crbug.com/40744549): clean up post-launch.
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
const CGFloat kFakeboxHeight = 65;
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

// Returns the color to use for the Lens and Voice icons in the Fakebox.
UIColor* FakeboxIconColor() {
  return [UIColor colorNamed:kGrey700Color];
}

// Sets up fakebox button with a round background and new badge view.
void SetUpButtonWithNewFeatureBadge(UIButton* button) {
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  button.backgroundColor = [UIColor colorNamed:kOmniboxKeyboardButtonColor];
  button.layer.cornerRadius = kSymbolButtonSize / 2;

  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;

  NewFeatureBadgeView* badgeView =
      [[NewFeatureBadgeView alloc] initWithBadgeSize:kNewFeatureBadgeSize
                                            fontSize:kNewFeatureFontSize];
  badgeView.translatesAutoresizingMaskIntoConstraints = NO;
  badgeView.accessibilityElementsHidden = YES;
  [button.imageView addSubview:badgeView];

  [NSLayoutConstraint activateConstraints:@[
    [button.widthAnchor constraintEqualToConstant:kSymbolButtonSize],
    [button.heightAnchor constraintEqualToConstant:kSymbolButtonSize],
    [badgeView.centerXAnchor
        constraintEqualToAnchor:button.imageView.centerXAnchor
                       constant:kNewBadgeOffsetFromButtonCenter],
    [badgeView.centerYAnchor
        constraintEqualToAnchor:button.imageView.centerYAnchor
                       constant:-kNewBadgeOffsetFromButtonCenter],
  ]];
}
}

namespace content_suggestions {

const CGFloat kHintTextScale = 0.15;
const CGFloat kReturnToRecentTabSectionBottomMargin = 25;

CGFloat DoodleHeight(BOOL logo_is_showing,
                     BOOL doodle_is_showing,
                     UITraitCollection* trait_collection) {
  // For users with non-Google default search engine, there is no doodle.
  if (!IsRegularXRegularSizeClass(trait_collection) && !logo_is_showing) {
    return 0;
  }

  if (logo_is_showing) {
    if (doodle_is_showing ||
        (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)) {
      return kGoogleSearchDoodleShrunkHeight;
    } else if (IsIOSLargeFakeboxEnabled()) {
      return kLargeFakeboxGoogleSearchLogoHeight;
    } else {
      return kGoogleSearchLogoHeight;
    }
  }

  return kGoogleSearchDoodleHeight;
}

CGFloat DoodleTopMargin(CGFloat top_inset,
                        UITraitCollection* trait_collection) {
  if (IsRegularXRegularSizeClass(trait_collection))
    return kDoodleTopMarginRegularXRegular;
  CGFloat top_margin =
      top_inset +
      AlignValueToPixel(kDoodleScaledTopMarginOther *
                        ui_util::SystemSuggestedFontSizeMultiplier());
  // If Magic Stack is not enabled, this value is zero (e.g. no-op).
  top_margin -= ReducedNTPTopMarginSpaceForMagicStack();
  if (IsIOSLargeFakeboxEnabled()) {
    top_margin += kLargeFakeboxExtraDoodleTopMargin;
  }
  top_margin += kDoodleTopMarginOther;
  return top_margin;
}

CGFloat HeaderSeparatorHeight() {
  return ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);
}

CGFloat SearchFieldTopMargin() {
  return IsIOSLargeFakeboxEnabled() ? kLargeFakeboxSearchFieldTopMargin
                                    : kSearchFieldTopMargin;
}

CGFloat SearchFieldWidth(CGFloat width, UITraitCollection* trait_collection) {
  if (!IsCompactWidth(trait_collection) && !IsCompactHeight(trait_collection))
    return kSearchFieldLarge;

  // Special case for narrow sizes.
  return std::max(
      kSearchFieldSmallMin,
      std::min(kSearchFieldSmall, width - kSearchFieldMinMargin * 2));
}

CGFloat FakeOmniboxHeight() {
  if (IsIOSLargeFakeboxEnabled()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToPixel((kFakeboxHeight - kFakeboxHeightNonDynamic) *
                                 multiplier +
                             kFakeboxHeightNonDynamic);
  }
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat PinnedFakeOmniboxHeight() {
  if (IsIOSLargeFakeboxEnabled()) {
    CGFloat multiplier = ui_util::SystemSuggestedFontSizeMultiplier();
    return AlignValueToPixel(
        (kPinnedFakeboxHeight - kPinnedFakeboxHeightNonDynamic) * multiplier +
        kPinnedFakeboxHeightNonDynamic);
  }
  return LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat FakeToolbarHeight() {
  if (IsIOSLargeFakeboxEnabled()) {
    return PinnedFakeOmniboxHeight() + FakeToolbarVerticalMargin();
  }
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

CGFloat HeightForLogoHeader(BOOL logo_is_showing,
                            BOOL doodle_is_showing,
                            UITraitCollection* trait_collection) {
  CGFloat header_height =
      DoodleTopMargin(0, trait_collection) +
      DoodleHeight(logo_is_showing, doodle_is_showing, trait_collection) +
      SearchFieldTopMargin() + FakeOmniboxHeight() +
      ntp_header::kScrolledToTopOmniboxBottomMargin +
      ceil(HeaderSeparatorHeight());
  if (!IsRegularXRegularSizeClass(trait_collection)) {
    return header_height;
  }
  if (!logo_is_showing) {
    // Returns sufficient vertical space for the Identity Disc to be
    // displayed.
    return ntp_home::kIdentityAvatarDimension +
           2 * (ntp_home::kHeaderIconMargin + ntp_home::kIdentityAvatarPadding);
  }

  header_height += kTopSpacingMaterial;

  return header_height;
}

CGFloat HeaderBottomPadding() {
  return kNTPShrunkLogoSearchFieldBottomPadding;
}

void ConfigureSearchHintLabel(UILabel* search_hint_label,
                              UIView* search_tab_target) {
  [search_hint_label setTranslatesAutoresizingMaskIntoConstraints:NO];
  [search_tab_target addSubview:search_hint_label];

  [search_hint_label setText:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
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

  voice_search_button.tintColor = FakeboxIconColor();
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
                                   BOOL use_color_icon) {
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
  lens_button.tintColor = FakeboxIconColor();

  if (use_new_badge) {
    // Show the "New" badge and colored symbol.
    SetUpButtonWithNewFeatureBadge(lens_button);
  }
}

void ConfigureLensButtonWithNewBadgeAlpha(UIButton* lens_button,
                                          CGFloat new_badge_alpha) {
  // Fade button background.
  lens_button.backgroundColor =
      [[UIColor colorNamed:kOmniboxKeyboardButtonColor]
          colorWithAlphaComponent:new_badge_alpha];
  lens_button.layer.shadowOpacity = kButtonShadowOpacity * new_badge_alpha;

  // Scale the N badge.
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

int SetUpListTitleStringID() {
  return IsIOSTipsNotificationsEnabled() ? IDS_IOS_SET_UP_LIST_TIPS_TITLE
                                         : IDS_IOS_SET_UP_LIST_TITLE;
}

NSString* SetUpListTitleString() {
  return l10n_util::GetNSString(SetUpListTitleStringID());
}

}  // namespace content_suggestions
