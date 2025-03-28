// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation BestFeaturesItem
- (instancetype)initWithType:(BestFeaturesItemType)type {
  self = [super init];
  if (self) {
    self.type = type;
  }
  return self;
}

#pragma mark - Getters

- (NSString*)title {
  if (!_title) {
    _title = l10n_util::GetNSString([self titleID]);
  }
  return _title;
}

- (NSString*)subtitle {
  if (!_subtitle) {
    _subtitle = l10n_util::GetNSString([self subtitleID]);
  }
  return _subtitle;
}

- (NSString*)caption {
  if (!_caption) {
    _caption = l10n_util::GetNSString([self captionID]);
  }
  return _caption;
}

- (UIImage*)iconImage {
  if (!_iconImage) {
    UIImageConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:20.0
                            weight:UIImageSymbolWeightSemibold
                             scale:UIImageSymbolScaleMedium];
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
        _iconImage = MakeSymbolMulticolor(
            CustomSymbolWithConfiguration(kCameraLensSymbol, configuration));
        break;
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        _iconImage = SymbolWithPalette(
            CustomSymbolWithConfiguration(kSafetyCheckSymbol, configuration),
            @[ [UIColor whiteColor] ]);
        break;
      case BestFeaturesItemType::kLockedIncognitoTabs:
        _iconImage = SymbolWithPalette(
            CustomSymbolWithConfiguration(kIncognitoSymbol, configuration),
            @[ [UIColor whiteColor] ]);
        break;
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        _iconImage = MakeSymbolMulticolor(CustomSymbolWithConfiguration(
            kPasswordManagerSymbol, configuration));
        break;
      case BestFeaturesItemType::kTabGroups:
        _iconImage = SymbolWithPalette(
            DefaultSymbolWithConfiguration(kTabGroupsSymbol, configuration),
            @[ [UIColor whiteColor] ]);
        break;
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        _iconImage = SymbolWithPalette(
            DefaultSymbolWithConfiguration(kChartLineDowntrendXYAxisSymbol,
                                           configuration),
            @[ [UIColor whiteColor] ]);
        break;
    }
  }
  return _iconImage;
}

- (UIColor*)iconBackgroundColor {
  if (!_iconBackgroundColor) {
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        _iconBackgroundColor = [UIColor whiteColor];
        break;
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        _iconBackgroundColor = [UIColor colorNamed:kBlue500Color];
        break;
      case BestFeaturesItemType::kLockedIncognitoTabs:
        _iconBackgroundColor = [UIColor colorNamed:kGrey700Color];
        break;
      case BestFeaturesItemType::kTabGroups:
        _iconBackgroundColor = [UIColor colorNamed:kGreen500Color];
        break;
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        _iconBackgroundColor = [UIColor colorNamed:kPink500Color];
        break;
    }
  }
  return _iconBackgroundColor;
}

- (NSDictionary*)textProvider {
  if (!_textProvider) {
    _textProvider = [self retrieveTextLocalization];
  }
  return _textProvider;
}

- (NSString*)animationName {
  if (!_animationName) {
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
        _animationName = @"lens_promo";
        break;
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        _animationName = @"enhanced_safe_browsing_promo";
        break;
      case BestFeaturesItemType::kLockedIncognitoTabs:
        _animationName = @"locked_incognito_tabs";
        break;
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
        _animationName = @"save_passwords";
        break;
      case BestFeaturesItemType::kTabGroups:
        _animationName = @"tab_groups";
        break;
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        _animationName = @"PriceTracking";
        break;
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
        _animationName = @"CPE_promo_animation_edu_autofill";
        break;
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        _animationName = @"PasswordSharing";
        break;
    }
  }
  return _animationName;
}

- (NSArray<NSString*>*)instructionSteps {
  if (!_instructionSteps) {
    NSMutableArray* instructions = [[NSMutableArray alloc] init];
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_STEP_3),
        ]];
        break;
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(
              IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP1),
          l10n_util::GetNSString(
              IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP2),
          l10n_util::GetNSString(
              IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP3),
        ]];
        break;
      case BestFeaturesItemType::kLockedIncognitoTabs:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_FIRST_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_SECOND_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_THIRD_STEP),
        ]];
        break;
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_FIRST_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_SECOND_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_THIRD_STEP),
        ]];
        break;
      case BestFeaturesItemType::kTabGroups:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_3),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_4),
        ]];
        break;
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_3),
        ]];
        break;
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
        // Add the correct strings depending on the device OS.
        if (@available(iOS 18, *)) {
          [instructions addObjectsFromArray:@[
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_1),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_2_GENERAL),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_3_AUTOFILL_SETTINGS),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_4_TOGGLE),
          ]];
        } else {
          [instructions addObjectsFromArray:@[
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_1),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_2_PASSWORDS),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_3_PASSWORD_OPTIONS),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_4_SELECT),
          ]];
        }
        break;
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        [instructions addObjectsFromArray:@[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_3),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_4),
        ]];
        break;
    }
    _instructionSteps = instructions;
  }
  return _instructionSteps;
}

#pragma mark - Private

// Returns the ID for the title string.
- (int)titleID {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      return IDS_IOS_BEST_FEATURES_LENS_TITLE;
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
      return IDS_IOS_BEST_FEATURES_ENHANCED_SAFE_BROWSING_TITLE;
    case BestFeaturesItemType::kLockedIncognitoTabs:
      return IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_TITLE;
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
      return IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_TITLE;
    case BestFeaturesItemType::kTabGroups:
      return IDS_IOS_BEST_FEATURES_TAB_GROUPS_TITLE;
    case BestFeaturesItemType::kPriceTrackingAndInsights:
      return IDS_IOS_BEST_FEATURES_PRICE_TRACKING_TITLE;
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      return IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_TITLE;
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_TITLE;
  }
}

// Returns the ID for the subtitle string.
- (int)subtitleID {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      return IDS_IOS_BEST_FEATURES_LENS_SUBTITLE;
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
      return IDS_IOS_BEST_FEATURES_ENHANCED_SAFE_BROWSING_SUBTITLE;
    case BestFeaturesItemType::kLockedIncognitoTabs:
      return IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_SUBTITLE;
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
      return IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_SUBTITLE;
    case BestFeaturesItemType::kTabGroups:
      return IDS_IOS_BEST_FEATURES_TAB_GROUPS_SUBTITLE;
    case BestFeaturesItemType::kPriceTrackingAndInsights:
      return IDS_IOS_BEST_FEATURES_PRICE_TRACKING_SUBTITLE;
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      return IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_SUBTITLE;
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_SUBTITLE;
  }
}

// Returns the ID for the caption string.
- (int)captionID {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      return IDS_IOS_BEST_FEATURES_LENS_CAPTION;
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
      return IDS_IOS_BEST_FEATURES_ENHANCED_SAFE_BROWSING_CAPTION;
    case BestFeaturesItemType::kLockedIncognitoTabs:
      return IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_CAPTION;
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
      return IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_CAPTION;
    case BestFeaturesItemType::kTabGroups:
      return IDS_IOS_BEST_FEATURES_TAB_GROUPS_CAPTION;
    case BestFeaturesItemType::kPriceTrackingAndInsights:
      return IDS_IOS_BEST_FEATURES_PRICE_TRACKING_CAPTION;
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      return IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_CAPTION;
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_CAPTION;
  }
}

// Returns the text dictionary for the given `BestFeaturesItemType`.
- (NSDictionary*)retrieveTextLocalization {
  NSDictionary* textProvider;
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      // Animation has no strings.
      return nil;
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
      textProvider = @{
        @"Safe Browsing" :
            l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE),
        @"Enhanced Protection" : l10n_util::GetNSString(
            IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE),
        @"Standard Protection" : l10n_util::GetNSString(
            IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE),
        @"No Protection" : l10n_util::GetNSString(
            IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_DETAIL_TITLE),
      };
      break;
    case BestFeaturesItemType::kLockedIncognitoTabs:
      textProvider = @{
        @"Close Incognito tabs" : l10n_util::GetNSString(
            IDS_IOS_INCOGNITO_REAUTH_CLOSE_INCOGNITO_TABS),
        @"face_id" : l10n_util::GetNSStringF(
            IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
            base::SysNSStringToUTF16(BiometricAuthenticationTypeString())),
      };
      break;
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
      textProvider = @{
        @"save_password" : l10n_util::GetNSString(
            IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
        @"save" : l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
        @"done" : l10n_util::GetNSString(IDS_IOS_SUGGESTIONS_DONE),
      };
      break;
    case BestFeaturesItemType::kTabGroups:
      textProvider = @{
        @"Tab Groups" : l10n_util ::GetNSString(
            IDS_IOS_BEST_FEATURES_TAB_GROUPS_ANIMATION_TEXT_1),
      };
      break;
    case BestFeaturesItemType::kPriceTrackingAndInsights:
      textProvider = @{
        @"Smart Watch" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_PRICE_TRACKING_ANIMATION_TEXT_1),
        @"Price $100 - $180" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_PRICE_TRACKING_ANIMATION_TEXT_2),
        @"Track Price" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_PRICE_TRACKING_ANIMATION_TEXT_3),
      };
      break;
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      // Animation has no strings.
      return nil;
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      textProvider = @{
        @"SharePass_Light" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_ANIMATION_TEXT_1),
        @"Share" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_ANIMATION_TEXT_1),
      };
      break;
  }
  return textProvider;
}

@end
