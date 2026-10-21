// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_instructions_view_controller.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Video animation asset names.
NSString* const kDefaultBrowserAnimation = @"default_browser_animation";
NSString* const kDefaultBrowserAnimationRtl = @"default_browser_animation_rtl";
NSString* const kDefaultBrowserAnimationDarkmode =
    @"default_browser_animation_darkmode";
NSString* const kDefaultBrowserAnimationRtlDarkmode =
    @"default_browser_animation_rtl_darkmode";
NSString* const kDefaultBrowserDefaultAppsAnimation =
    @"default_browser_default_apps_animation";
NSString* const kDefaultBrowserDefaultAppsAnimationRtl =
    @"default_browser_default_apps_animation_rtl";
NSString* const kDefaultBrowserDefaultAppsAnimationDarkmode =
    @"default_browser_default_apps_animation_darkmode";
NSString* const kDefaultBrowserDefaultAppsAnimationRtlDarkmode =
    @"default_browser_default_apps_animation_rtl_darkmode";
NSString* const kDefaultBrowserAnimationIpad =
    @"default_browser_animation_ipad";
NSString* const kDefaultBrowserAnimationRtlIpad =
    @"default_browser_animation_rtl_ipad";

// Keys in the lottie assets.
NSString* const kBrowserAppKeypath = @"IDS_BROWSER_APP";
NSString* const kDefaultBrowserAppKeypath = @"IDS_DEFAULT_BROWSER_APP";
NSString* const kChromeKeypath = @"IDS_CHROME";
NSString* const kChromeSecondaryKeypath = @"IDS_CHROME_SECONDARY";
NSString* const kSettingsKeypath = @"IDS_IOS_SETTINGS";

// Returns the animation name.
NSString* AnimationAssetName(BOOL useDefaultAppsDestination) {
  if (IsDefaultBrowserPromoIpadInstructions() &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlIpad
                               : kDefaultBrowserAnimationIpad;
  }

  if (useDefaultAppsDestination) {
    return base::i18n::IsRTL() ? kDefaultBrowserDefaultAppsAnimationRtl
                               : kDefaultBrowserDefaultAppsAnimation;
  }

  return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtl
                             : kDefaultBrowserAnimation;
}

// Returns the animation name in dark mode.
NSString* AnimationAssetNameDarkmode(BOOL useDefaultAppsDestination) {
  if (IsDefaultBrowserPromoIpadInstructions() &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // No separate dark mode asset for iPad when using dynamic colors.
    return nil;
  }

  if (useDefaultAppsDestination) {
    return base::i18n::IsRTL() ? kDefaultBrowserDefaultAppsAnimationRtlDarkmode
                               : kDefaultBrowserDefaultAppsAnimationDarkmode;
  }

  return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlDarkmode
                             : kDefaultBrowserAnimationDarkmode;
}

// Returns whether the animation supports dynamic colors.
BOOL AreColorsDynamic() {
  return IsDefaultBrowserPromoIpadInstructions() &&
         ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}

// Returns the color provider for the animation.
NSDictionary<NSString*, UIColor*>* ColorProvider() {
  return @{
    kSeparatorColor : [UIColor colorNamed:kSeparatorColor],
    kBlueColor : [UIColor colorNamed:kBlueColor],
    kGrey200Color : [UIColor colorNamed:kGrey200Color],
    kBackgroundColor : [UIColor colorNamed:kBackgroundColor],
    kGroupedPrimaryBackgroundColor :
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor],
    kGroupedSecondaryBackgroundColor :
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    kChromeKeypath : [UIColor colorNamed:kTextPrimaryColor],
    kChromeSecondaryKeypath : [UIColor colorNamed:kTextSecondaryColor],
    kDefaultBrowserAppKeypath : [UIColor colorNamed:kTextPrimaryColor],
    kSettingsKeypath : [UIColor colorNamed:kTextPrimaryColor],
  };
}

}  // namespace

NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";

NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";

@implementation DefaultBrowserInstructionsViewController

- (instancetype)initWithDismissButton:(BOOL)hasDismissButton
                     hasRemindMeLater:(BOOL)hasRemindMeLater
            useDefaultAppsDestination:(BOOL)useDefaultAppsDestination
                             hasSteps:(BOOL)hasSteps
                            titleText:(NSString*)titleText {
  useDefaultAppsDestination |= IsDefaultAppsDestinationAvailable() &&
                               IsUseDefaultAppsDestinationForPromosEnabled();

  ButtonStackConfiguration* buttonStackConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonStackConfiguration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_PROMO_PRIMARY_BUTTON_TEXT);
  if (hasDismissButton) {
    buttonStackConfiguration.secondaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_SECONDARY_BUTTON_TEXT);
  }
  if (hasRemindMeLater) {
    buttonStackConfiguration.tertiaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_TERTIARY_BUTTON_TEXT);
  }

  AnimationBrickConfiguration* animationConfiguration =
      [[AnimationBrickConfiguration alloc] init];
  animationConfiguration.animationName =
      AnimationAssetName(useDefaultAppsDestination);
  animationConfiguration.accessibilityID =
      kDefaultBrowserInstructionsViewAnimationViewId;
  animationConfiguration.darkAccessibilityID =
      kDefaultBrowserInstructionsViewDarkAnimationViewId;
  if (AreColorsDynamic()) {
    animationConfiguration.useLegacyDarkMode = NO;
    animationConfiguration.lightModeColorProvider = ColorProvider();
    animationConfiguration.darkModeColorProvider = ColorProvider();
  } else {
    animationConfiguration.darkAnimationName =
        AnimationAssetNameDarkmode(useDefaultAppsDestination);
  }
  animationConfiguration.animationBackgroundColor =
      [UIColor colorNamed:kGrey100Color];
  animationConfiguration.animationTextProvider = @{
    kBrowserAppKeypath :
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_BROWSER_APP),
    kDefaultBrowserAppKeypath : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    kChromeKeypath : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME),
    kChromeSecondaryKeypath :
        l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME),
    kSettingsKeypath : l10n_util::GetNSString(IDS_IOS_SETTINGS_TITLE)
  };

  TitleBrickConfiguration* titleConfiguration =
      [[TitleBrickConfiguration alloc] init];
  titleConfiguration.title =
      titleText ? titleText
                : l10n_util::GetNSString(
                      IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT);
  titleConfiguration.titleStyle = UIFontTextStyleTitle2;

  UIView* instructionView;

  if (hasSteps) {
    NSMutableArray* defaultBrowserSteps = [[NSMutableArray alloc] init];
    if (useDefaultAppsDestination) {
      [defaultBrowserSteps
          addObject:
              l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_FIRST_STEP)];
      [defaultBrowserSteps
          addObject:
              l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_SECOND_STEP)];
    } else {
      [defaultBrowserSteps
          addObject:l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP)];
      [defaultBrowserSteps
          addObject:l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP)];
    }
    [defaultBrowserSteps
        addObject:l10n_util::GetNSString(
                      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)];

    instructionView =
        [[InstructionView alloc] initWithList:defaultBrowserSteps];
    instructionView.translatesAutoresizingMaskIntoConstraints = NO;
  } else {
    titleConfiguration.subtitle =
        useDefaultAppsDestination
            ? l10n_util::GetNSString(
                  IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_APPS_SUBTITLE_TEXT)
            : l10n_util::GetNSString(
                  IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_SUBTITLE_TEXT);
  }

  AnimatedPromoConfiguration* config = [[AnimatedPromoConfiguration alloc]
      initWithAnimationBrickConfiguration:animationConfiguration
                 buttonStackConfiguration:buttonStackConfiguration
                  titleBrickConfiguration:titleConfiguration
                           underTitleView:instructionView];

  return [super initWithConfiguration:config];
}

@end
