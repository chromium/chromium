// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_instructions_view_controller.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
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
NSString* const kDefaultBrowserAnimationDarkmodeIpad =
    @"default_browser_animation_darkmode_ipad";
NSString* const kDefaultBrowserAnimationRtlDarkmodeIpad =
    @"default_browser_animation_rtl_darkmode_ipad";

// Keys in the lottie assets.
NSString* const kBrowserAppKeypath = @"IDS_BROWSER_APP";
NSString* const kDefaultBrowserAppKeypath = @"IDS_DEFAULT_BROWSER_APP";
NSString* const kChromeKeypath = @"IDS_CHROME";
NSString* const kChromeSecondaryKeypath = @"IDS_CHROME_SECONDARY";
NSString* const kSettingsKeypath = @"IDS_IOS_SETTINGS";

}  // namespace

NSString* const kDefaultBrowserInstructionsViewAnimationViewId =
    @"DefaultBrowserInstructionsViewAnimationViewId";

NSString* const kDefaultBrowserInstructionsViewDarkAnimationViewId =
    @"DefaultBrowserInstructionsViewDarkAnimationViewId";

@implementation DefaultBrowserInstructionsViewController {
  BOOL _hasDismissButton;
  BOOL _hasRemindMeLater;
  BOOL _useDefaultAppsDestination;
  BOOL _hasSteps;
  NSString* _titleText;
}

- (instancetype)initWithDismissButton:(BOOL)hasDismissButton
                     hasRemindMeLater:(BOOL)hasRemindMeLater
            useDefaultAppsDestination:(BOOL)useDefaultAppsDestination
                             hasSteps:(BOOL)hasSteps
                            titleText:(NSString*)titleText {
  if ((self = [super init])) {
    _hasDismissButton = hasDismissButton;
    _hasRemindMeLater = hasRemindMeLater;
    _useDefaultAppsDestination = useDefaultAppsDestination;
    _useDefaultAppsDestination |= IsDefaultAppsDestinationAvailable() &&
                                  IsUseDefaultAppsDestinationForPromosEnabled();
    _hasSteps = hasSteps;
    _titleText = titleText;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = [self animationAssetName];
  if ([self areColorsDynamic]) {
    self.useLegacyDarkMode = NO;
  } else {
    self.animationNameDarkMode = [self animationAssetNameDarkmode];
  }
  self.animationTextProvider = @{
    kBrowserAppKeypath :
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_BROWSER_APP),
    kDefaultBrowserAppKeypath : l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_BROWSER_APP),
    kChromeKeypath : l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME),
    kChromeSecondaryKeypath :
        l10n_util::GetNSString(IDS_IOS_SHORT_PRODUCT_NAME),
    kSettingsKeypath : l10n_util::GetNSString(IDS_IOS_SETTINGS_TITLE)
  };

  // Title.
  self.titleString =
      _titleText ? _titleText
                 : l10n_util::GetNSString(
                       IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT);

  // Subtitle or instruction steps.
  if (_hasSteps) {
    NSMutableArray* defaultBrowserSteps = [[NSMutableArray alloc] init];
    if (_useDefaultAppsDestination) {
      [defaultBrowserSteps
          addObject:l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_FIRST_STEP)];
      [defaultBrowserSteps
          addObject:l10n_util::GetNSString(
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

    UIView* instructionView =
        [[InstructionView alloc] initWithList:defaultBrowserSteps];
    instructionView.translatesAutoresizingMaskIntoConstraints = NO;
    self.underTitleView = instructionView;
  } else {
    self.subtitleString =
        _useDefaultAppsDestination
            ? l10n_util::GetNSString(
                  IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_DEFAULT_APPS_SUBTITLE_TEXT)
            : l10n_util::GetNSString(
                  IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_SUBTITLE_TEXT);
  }

  // Buttons.
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_PROMO_PRIMARY_BUTTON_TEXT);
  if (_hasDismissButton) {
    self.secondaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_SECONDARY_BUTTON_TEXT);
  }
  if (_hasRemindMeLater) {
    self.tertiaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_PROMO_TERTIARY_BUTTON_TEXT);
  }

  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];

  self.animationViewWrapper.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewAnimationViewId;
  self.animationViewWrapperDarkMode.animationView.accessibilityIdentifier =
      kDefaultBrowserInstructionsViewDarkAnimationViewId;

  if ([self areColorsDynamic]) {
    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(configureAnimationColors)];
    [self configureAnimationColors];
  }
}

#pragma mark - Private

- (NSString*)animationAssetName {
  if (IsDefaultBrowserPromoIpadInstructions() &&
      [[UIDevice currentDevice] userInterfaceIdiom] ==
          UIUserInterfaceIdiomPad) {
    return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlIpad
                               : kDefaultBrowserAnimationIpad;
  }

  if (_useDefaultAppsDestination) {
    return base::i18n::IsRTL() ? kDefaultBrowserDefaultAppsAnimationRtl
                               : kDefaultBrowserDefaultAppsAnimation;
  }

  return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtl
                             : kDefaultBrowserAnimation;
}

- (NSString*)animationAssetNameDarkmode {
  if (IsDefaultBrowserPromoIpadInstructions() &&
      [[UIDevice currentDevice] userInterfaceIdiom] ==
          UIUserInterfaceIdiomPad) {
    // No separate dark mode asset for iPad when using dynamic colors.
    return nil;
  }

  if (_useDefaultAppsDestination) {
    return base::i18n::IsRTL() ? kDefaultBrowserDefaultAppsAnimationRtlDarkmode
                               : kDefaultBrowserDefaultAppsAnimationDarkmode;
  }

  return base::i18n::IsRTL() ? kDefaultBrowserAnimationRtlDarkmode
                             : kDefaultBrowserAnimationDarkmode;
}

// Configures the animation with semantic and custom colors.
- (void)configureAnimationColors {
  // Custom dynamic colors for kDefaultBrowserAnimationIpad.
  if ([[self animationAssetName]
          isEqualToString:kDefaultBrowserAnimationIpad]) {
    // Configure shape layers.
    ConfigureAnimationSemanticColor(self.animationViewWrapper, kSeparatorColor,
                                    kSeparatorColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper, kBlueColor,
                                    kBlueColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper, kGrey200Color,
                                    kGrey200Color);
    ConfigureAnimationSemanticColor(self.animationViewWrapper, kBackgroundColor,
                                    kBackgroundColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper,
                                    kGroupedPrimaryBackgroundColor,
                                    kGroupedPrimaryBackgroundColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper,
                                    kGroupedSecondaryBackgroundColor,
                                    kGroupedSecondaryBackgroundColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper,
                                    kSecondaryBackgroundColor,
                                    kSecondaryBackgroundColor);
    // Configure text layers.
    ConfigureAnimationSemanticColor(self.animationViewWrapper, kChromeKeypath,
                                    kTextPrimaryColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper,
                                    kChromeSecondaryKeypath,
                                    kTextSecondaryColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper,
                                    kDefaultBrowserAppKeypath,
                                    kTextPrimaryColor);
    ConfigureAnimationSemanticColor(self.animationViewWrapper, kSettingsKeypath,
                                    kTextPrimaryColor);
  }
}

// Returns whether the animation supports dynamic colors.
- (BOOL)areColorsDynamic {
  return IsDefaultBrowserPromoIpadInstructions() &&
         [[UIDevice currentDevice] userInterfaceIdiom] ==
             UIUserInterfaceIdiomPad;
}

@end
