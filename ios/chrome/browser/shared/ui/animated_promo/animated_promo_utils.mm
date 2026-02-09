// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"

#import <map>
#import <string>
#import <tuple>

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

void ConfigureAnimationSemanticColors(id<LottieAnimation> animation) {
  // LINT.IfChange(AnimationSemanticColorNames)
  NSArray<NSString*>* color_names = @[
    kAimInputItemTopBackgroundColor,
    kAimComposeboxButtonBackgroundColor,
    kBackgroundColor,
    kCloseButtonColor,
    kDisabledTintColor,
    kFaviconBackgroundColor,
    kGroupedPrimaryBackgroundColor,
    kGroupedSecondaryBackgroundColor,
    kBackgroundShadowColor,
    kMDCInkColor,
    kMDCSecondaryInkColor,
    kPlaceholderImageTintColor,
    kPrimaryBackgroundColor,
    kInvertedPrimaryBackgroundColor,
    kScrimBackgroundColor,
    kDarkerScrimBackgroundColor,
    kSecondaryBackgroundColor,
    kSeparatorColor,
    kBWGSeparatorColor,
    kSolidButtonTextColor,
    kTableViewRowHighlightColor,
    kTertiaryBackgroundColor,
    kTextPrimaryColor,
    kInvertedTextPrimaryColor,
    kTextSecondaryColor,
    kTextLightTertiaryDarkPrimaryColor,
    kInvertedTextSecondaryColor,
    kTextTertiaryColor,
    kTextQuaternaryColor,
    kTextfieldBackgroundColor,
    kTextfieldFocusedBackgroundColor,
    kTextfieldHighlightBackgroundColor,
    kTextfieldPlaceholderColor,
    kToolbarButtonColor,
    kToolbarShadowColor,
    kMiniFakeOmniboxBackgroundColor,
    kOmniboxKeyboardButtonColor,
    kOmniboxPopoutOverlayColor,
    kOmniboxSuggestionRowSeparatorColor,
    kOmniboxSuggestionAnswerIconColor,
    kOmniboxSuggestionIconColor,
    kOmniboxPopoutSuggestionRowSeparatorColor,
    kTabGroupFaviconBackgroundColor,
    kTabStripV3BackgroundColor,
    kTabStripNewTabButtonColor,
    kTabGroupPinkColor,
    kTabGroupCyanColor,
    kTabGroupPurpleColor,
    kTabGroupGreenColor,
    kTabGroupGreyColor,
    kWhiteBlackAlpha50Color,
    kLensOverlayConsentDialogDescriptionColor,
    kLensOverlayConsentDialogAnimationPlayerButtonColor,
    kSolidBlackColor,
    kSolidWhiteColor,
    kBlueColor,
    kBlueHaloColor,
    kBlue100Color,
    kBlue300Color,
    kBlue400Color,
    kBlue500Color,
    kBlue600Color,
    kBlue700Color,
    kBlue900Color,
    kStaticBlueColor,
    kStaticBlue400Color,
    kGreenColor,
    kGreen100Color,
    kGreen300Color,
    kGreen400Color,
    kGreen500Color,
    kGreen600Color,
    kGreen700Color,
    kGreen800Color,
    kStaticGreen50Color,
    kStaticGreen700Color,
    kRed50Color,
    kRed100Color,
    kRed300Color,
    kRed400Color,
    kRed500Color,
    kRed600Color,
    kRedColor,
    kPink400Color,
    kPink500Color,
    kPink600Color,
    kPink700Color,
    kPurple500Color,
    kPurple600Color,
    kYellow500Color,
    kYellow600Color,
    kOrange500Color,
    kOrange600Color,
    kCyan600Color,
    kCyan700Color,
    kGrey50Color,
    kGrey100Color,
    kGrey200Color,
    kGrey300Color,
    kGrey400Color,
    kGrey500Color,
    kGrey600Color,
    kGrey700Color,
    kGrey800Color,
    kGrey900Color,
    kStaticGrey50Color,
    kStaticGrey300Color,
    kStaticGrey400Color,
    kStaticGrey600Color,
    kStaticGrey700Color,
    kStaticGrey900Color,
    kLightOnlyGrey200Color
  ];
  // LINT.ThenChange(//ios/chrome/common/ui/colors/semantic_color_names.h:SemanticColorNames)

  for (NSString* color_name in color_names) {
    NSString* keypath =
        [NSString stringWithFormat:@"**%@.**.Color", color_name];
    [animation setColorValue:[UIColor colorNamed:color_name]
                  forKeypath:keypath];
  }
}

void ConfigureAnimationSemanticColor(id<LottieAnimation> animation,
                                     NSString* key,
                                     NSString* color_name) {
  NSString* keypath = [NSString stringWithFormat:@"**%@.**.Color", key];
  [animation setColorValue:[UIColor colorNamed:color_name] forKeypath:keypath];
}

void ConfigureAnimationCustomColor(id<LottieAnimation> animation,
                                   NSString* key,
                                   UIColor* light_color,
                                   UIColor* dark_color) {
  UIColor* selected_color = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        return (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
                   ? dark_color
                   : light_color;
      }];
  NSString* keypath = [NSString stringWithFormat:@"**%@.**.Color", key];
  [animation setColorValue:selected_color forKeypath:keypath];
}
