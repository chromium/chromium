// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include "base/i18n/rtl.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/components/ui_util/dynamic_type_util.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Width of search field.
const CGFloat kSearchFieldLarge = 432;
const CGFloat kSearchFieldSmall = 343;
const CGFloat kSearchFieldSmallMin = 304;
const CGFloat kSearchFieldMinMargin = 8;

// Top margin for the doodle.
const CGFloat kDoodleTopMarginRegularXRegular = 162;
const CGFloat kDoodleTopMarginOther = 48;
const CGFloat kShrunkDoodleTopMarginOther = 65;
// Size of the doodle top margin which is multiplied by the scaled font factor,
// and added to `kDoodleTopMarginOther` on non Regular x Regular form factors.
const CGFloat kDoodleScaledTopMarginOther = 10;

// Top margin for the search field
const CGFloat kSearchFieldTopMargin = 32;
const CGFloat kShrunkLogoSearchFieldTopMargin = 22;

// Bottom margin for the search field.
const CGFloat kNTPSearchFieldBottomPadding = 18;
const CGFloat kNTPShrunkLogoSearchFieldBottomPadding = 20;

const CGFloat kTopSpacingMaterial = 24;

// Height for the logo and doodle frame.
const CGFloat kGoogleSearchDoodleHeight = 120;

// Height for the shrunk doodle frame.
// TODO(crbug.com/1170491): clean up post-launch.
const CGFloat kGoogleSearchDoodleShrunkHeight = 68;

// Height for the shrunk logo frame.
// TODO(crbug.com/1170491): clean up post-launch.
const CGFloat kGoogleSearchLogoShrunkHeight = 36;

// The size of the symbol image.
const CGFloat kSymbolContentSuggestionsPointSize = 18;
}

namespace content_suggestions {

const int kSearchFieldBackgroundColor = 0xF1F3F4;
const CGFloat kHintTextScale = 0.15;
const CGFloat kReturnToRecentTabSectionBottomMargin = 25;

CGFloat doodleHeight(BOOL logoIsShowing,
                     BOOL doodleIsShowing,
                     UITraitCollection* traitCollection) {
  // For users with non-Google default search engine, there is no doodle.
  if (!IsRegularXRegularSizeClass(traitCollection) && !logoIsShowing) {
    return 0;
  }

  if (ShouldShrinkLogoForStartSurface() && logoIsShowing) {
    if (doodleIsShowing ||
        (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)) {
      return kGoogleSearchDoodleShrunkHeight;
    } else {
      return kGoogleSearchLogoShrunkHeight;
    }
  }

  return kGoogleSearchDoodleHeight;
}

CGFloat doodleTopMargin(BOOL toolbarPresent,
                        CGFloat topInset,
                        UITraitCollection* traitCollection) {
  if (IsRegularXRegularSizeClass(traitCollection))
    return kDoodleTopMarginRegularXRegular;
  if (IsCompactHeight(traitCollection) && !ShouldShrinkLogoForStartSurface())
    return topInset;
  CGFloat topMargin =
      topInset +
      AlignValueToPixel(kDoodleScaledTopMarginOther *
                        ui_util::SystemSuggestedFontSizeMultiplier());
  if (ShouldShrinkLogoForStartSurface() && !IsCompactHeight(traitCollection)) {
    topMargin += kShrunkDoodleTopMarginOther;
  } else {
    topMargin += kDoodleTopMarginOther;
  }
  return topMargin;
}

CGFloat searchFieldTopMargin() {
  return ShouldShrinkLogoForStartSurface() ? kShrunkLogoSearchFieldTopMargin
                                           : kSearchFieldTopMargin;
}

CGFloat searchFieldWidth(CGFloat superviewWidth,
                         UITraitCollection* traitCollection) {
  if (!IsCompactWidth(traitCollection) && !IsCompactHeight(traitCollection))
    return kSearchFieldLarge;

  // Special case for narrow sizes.
  return MAX(
      kSearchFieldSmallMin,
      MIN(kSearchFieldSmall, superviewWidth - kSearchFieldMinMargin * 2));
}

CGFloat heightForLogoHeader(BOOL logoIsShowing,
                            BOOL doodleIsShowing,
                            BOOL promoCanShow,
                            BOOL toolbarPresent,
                            CGFloat topInset,
                            UITraitCollection* traitCollection) {
  CGFloat headerHeight =
      doodleTopMargin(toolbarPresent, topInset, traitCollection) +
      doodleHeight(logoIsShowing, doodleIsShowing, traitCollection) +
      searchFieldTopMargin() +
      ToolbarExpandedHeight(
          [UIApplication sharedApplication].preferredContentSizeCategory) +
      headerBottomPadding();
  if (!IsRegularXRegularSizeClass(traitCollection)) {
    return headerHeight;
  }
  if (!logoIsShowing) {
    // Returns sufficient vertical space for the Identity Disc to be
    // displayed.
    return ntp_home::kIdentityAvatarDimension +
           2 * ntp_home::kIdentityAvatarMargin;
  }
  if (!promoCanShow) {
    headerHeight += kTopSpacingMaterial;
  }

  return headerHeight;
}

CGFloat headerBottomPadding() {
  return ShouldShowReturnToMostRecentTabForStartSurface()
             ? kNTPShrunkLogoSearchFieldBottomPadding
             : kNTPSearchFieldBottomPadding;
}

void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIView* searchTapTarget) {
  [searchHintLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:searchHintLabel];

  [searchHintLabel setText:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  if (base::i18n::IsRTL()) {
    [searchHintLabel setTextAlignment:NSTextAlignmentRight];
  }
  searchHintLabel.textColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  searchHintLabel.adjustsFontForContentSizeCategory = YES;
  searchHintLabel.textAlignment = NSTextAlignmentCenter;
}

void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIView* searchTapTarget) {
  [voiceSearchButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:voiceSearchButton];

  [voiceSearchButton setAdjustsImageWhenHighlighted:NO];

  UIImage* micImage = UseSymbols() ? DefaultSymbolWithPointSize(
                                         kMicrophoneFillSymbol,
                                         kSymbolContentSuggestionsPointSize)
                                   : [UIImage imageNamed:@"location_bar_voice"];
  micImage =
      [micImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  [voiceSearchButton setImage:micImage forState:UIControlStateNormal];
  voiceSearchButton.tintColor = [UIColor colorNamed:kGrey500Color];
  [voiceSearchButton setAccessibilityLabel:l10n_util::GetNSString(
                                               IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voiceSearchButton setAccessibilityIdentifier:@"Voice Search"];

  voiceSearchButton.pointerInteractionEnabled = YES;
  // Make the pointer shape fit the location bar's semi-circle end shape.
  voiceSearchButton.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();
}

UIView* nearestAncestor(UIView* view, Class aClass) {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return nearestAncestor([view superview], aClass);
}

}  // namespace content_suggestions
