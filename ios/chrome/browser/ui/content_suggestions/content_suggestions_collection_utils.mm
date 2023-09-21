// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#import "base/i18n/rtl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
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
const CGFloat kDoodleTopMarginOther = 48;
const CGFloat kShrunkDoodleTopMarginOther = 65;
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
// TODO(crbug.com/1170491): clean up post-launch.
const CGFloat kGoogleSearchDoodleShrunkHeight = 68;

// Height for the shrunk logo frame.
// TODO(crbug.com/1170491): clean up post-launch.
const CGFloat kGoogleSearchLogoHeight = 36;
const CGFloat kLargeFakeboxGoogleSearchLogoHeight = 50;

// The size of the symbol image.
const CGFloat kSymbolContentSuggestionsPointSize = 18;

// The height of the Fakebox.
const CGFloat kFakeboxHeight = 65;
const CGFloat kFakeboxHeightNonDynamic = 45;

// Returns the color of the search hint label in the fakebox.
UIColor* SearchHintLabelColor() {
  if (IsIOSLargeFakeboxEnabled()) {
    return [UIColor colorNamed:kGrey700Color];
  } else if (IsMagicStackEnabled()) {
    return [UIColor colorNamed:@"fake_omnibox_placeholder_color"];
  }
  return [UIColor colorNamed:kTextfieldPlaceholderColor];
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
  if (!IsCompactHeight(trait_collection)) {
    top_margin += kShrunkDoodleTopMarginOther;
  } else {
    top_margin += kDoodleTopMarginOther;
  }
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
           2 * ntp_home::kIdentityAvatarMargin;
  }

  header_height += kTopSpacingMaterial;

  return header_height;
}

CGFloat HeaderBottomPadding() {
  return kNTPShrunkLogoSearchFieldBottomPadding;
}

UIImageView* CreateMagnifyingGlassView() {
  UIImageView* image_view = [[UIImageView alloc] init];
  image_view.translatesAutoresizingMaskIntoConstraints = NO;
  image_view.contentMode = UIViewContentModeScaleAspectFit;
  image_view.userInteractionEnabled = NO;

  UIImage* magnifying_glass_image = DefaultSymbolWithPointSize(
      kMagnifyingglassSymbol, kSymbolContentSuggestionsPointSize);
  image_view.tintColor = [UIColor colorNamed:kGrey500Color];

  [image_view setImage:magnifying_glass_image];
  return image_view;
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
                                UIView* search_tab_target) {
  [voice_search_button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [search_tab_target addSubview:voice_search_button];

  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
    voice_search_button.configuration = buttonConfig;
  } else {
    SetAdjustsImageWhenHighlighted(voice_search_button, NO);
  }

  UIImage* mic_image = DefaultSymbolWithPointSize(
      kMicrophoneSymbol, kSymbolContentSuggestionsPointSize);
  voice_search_button.tintColor = [UIColor colorNamed:kGrey600Color];

  [voice_search_button setImage:mic_image forState:UIControlStateNormal];
  [voice_search_button setAccessibilityLabel:l10n_util::GetNSString(
                                                 IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voice_search_button setAccessibilityIdentifier:@"Voice Search"];

  voice_search_button.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  voice_search_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();
}

void ConfigureLensButton(UIButton* lens_button, UIView* search_tap_target) {
  lens_button.translatesAutoresizingMaskIntoConstraints = NO;
  [search_tap_target addSubview:lens_button];

  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
    lens_button.configuration = buttonConfig;
  } else {
    SetAdjustsImageWhenHighlighted(lens_button, NO);
  }

  UIImage* camera_image = CustomSymbolWithPointSize(
      kCameraLensSymbol, kSymbolContentSuggestionsPointSize);

  [lens_button setImage:camera_image forState:UIControlStateNormal];
  lens_button.tintColor = [UIColor colorNamed:kGrey600Color];
  lens_button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_LENS);
  lens_button.accessibilityIdentifier = @"Lens";

  lens_button.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  lens_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();
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

BOOL ShouldShowWiderMagicStackLayer(UITraitCollection* traitCollection,
                                    UIWindow* window) {
  // Some iphone devices in landscape mode are still small enough to have a
  // Compact  UIUserInterfaceSizeClass.
  return traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassRegular ||
         IsLandscape(window);
}

}  // namespace content_suggestions
