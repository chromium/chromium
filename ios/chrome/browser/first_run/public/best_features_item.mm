// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/public/best_features_item.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the color provider for the Lens Search animation.
NSDictionary<NSString*, UIColor*>* LensColorProvider(
    int omnibox_color,
    int lens_background_color) {
  return @{
    @"Omnibox.*.*.Color" : UIColorFromRGB(omnibox_color),
    @"Lens_Icon_Background.*.*.Color" : UIColorFromRGB(lens_background_color),
  };
}

}  // namespace

@implementation BestFeaturesItem
- (instancetype)initWithType:(BestFeaturesItemType)type {
  self = [super init];
  if (self) {
    _type = type;
  }
  return self;
}

#pragma mark - Getters

- (NSString*)title {
  return l10n_util::GetNSString([self titleID]);
}

- (NSString*)subtitle {
  return l10n_util::GetNSString([self subtitleID]);
}

- (NSString*)caption {
  return l10n_util::GetNSString([self captionID]);
}

- (UIImage*)iconImage {
    UIImageConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:20.0
                            weight:UIImageSymbolWeightSemibold
                             scale:UIImageSymbolScaleMedium];
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
        return MakeSymbolMulticolor(
            CustomSymbolWithConfiguration(kCameraLensSymbol, configuration));
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        return SymbolWithPalette(
            CustomSymbolWithConfiguration(kSafetyCheckSymbol, configuration),
            @[ [UIColor whiteColor] ]);
      case BestFeaturesItemType::kLockedIncognitoTabs:
        return SymbolWithPalette(
            CustomSymbolWithConfiguration(kIncognitoSymbol, configuration),
            @[ [UIColor whiteColor] ]);
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        return MakeSymbolMulticolor(CustomSymbolWithConfiguration(
            kPasswordManagerSymbol, configuration));
      case BestFeaturesItemType::kTabGroups:
        return SymbolWithPalette(
            DefaultSymbolWithConfiguration(kTabGroupsSymbol, configuration),
            @[ [UIColor whiteColor] ]);
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        return SymbolWithPalette(
            DefaultSymbolWithConfiguration(kChartLineDowntrendXYAxisSymbol,
                                           configuration),
            @[ [UIColor whiteColor] ]);
    }
}

- (UIColor*)iconBackgroundColor {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return [UIColor whiteColor];
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
      return [UIColor colorNamed:kBlue500Color];
    case BestFeaturesItemType::kLockedIncognitoTabs:
      return [UIColor colorNamed:kGrey700Color];
    case BestFeaturesItemType::kTabGroups:
      return [UIColor colorNamed:kGreen500Color];
    case BestFeaturesItemType::kPriceTrackingAndInsights:
      return [UIColor colorNamed:kPink500Color];
  }
}

- (NSDictionary*)textProvider {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      // Animation has no strings.
      return nil;
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
      return @{
        @"Safe Browsing" :
            l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE),
        @"Enhanced Protection" : l10n_util::GetNSString(
            IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE),
        @"Standard Protection" : l10n_util::GetNSString(
            IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE),
        @"No Protection" : l10n_util::GetNSString(
            IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_DETAIL_TITLE),
      };
    case BestFeaturesItemType::kLockedIncognitoTabs:
      return @{
        @"Close Incognito tabs" : l10n_util::GetNSString(
            IDS_IOS_INCOGNITO_REAUTH_CLOSE_INCOGNITO_TABS),
        @"face_id" : l10n_util::GetNSStringF(
            IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
            base::SysNSStringToUTF16(BiometricAuthenticationTypeString())),
      };
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
      return @{
        @"save_password" : l10n_util::GetNSString(
            IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT),
        @"save" : l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
        @"done" : l10n_util::GetNSString(IDS_IOS_SUGGESTIONS_DONE),
      };
    case BestFeaturesItemType::kTabGroups:
      return @{
        @"Tab Groups" : l10n_util ::GetNSString(
            IDS_IOS_BEST_FEATURES_TAB_GROUPS_ANIMATION_TEXT_1),
      };
    case BestFeaturesItemType::kPriceTrackingAndInsights:
      return @{
        @"Smart Watch" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_PRICE_TRACKING_ANIMATION_TEXT_1),
        @"Price $100 - $180" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_PRICE_TRACKING_ANIMATION_TEXT_2),
        @"Track Price" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_PRICE_TRACKING_ANIMATION_TEXT_3),
      };
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
      // Animation has no strings.
      return nil;
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return @{
        @"SharePass_Light" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_ANIMATION_TEXT_1),
        @"Share" : l10n_util::GetNSString(
            IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_ANIMATION_TEXT_1),
      };
  }
}

- (NSString*)animationName {
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
        return @"lens_promo";
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        return @"enhanced_safe_browsing_promo";
      case BestFeaturesItemType::kLockedIncognitoTabs:
        return @"locked_incognito_tabs";
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
        return @"save_passwords";
      case BestFeaturesItemType::kTabGroups:
        return @"tab_groups";
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        return @"price_tracking";
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
        return @"CPE_promo_animation_edu_autofill";
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        return @"password_sharing";
    }
}

- (NSArray<NSString*>*)instructionSteps {
    switch (self.type) {
      case BestFeaturesItemType::kLensSearch:
        return @[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_STEP_3),
        ];
      case BestFeaturesItemType::kEnhancedSafeBrowsing:
        return @[
          l10n_util::GetNSString(
              IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP1),
          l10n_util::GetNSString(
              IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP2),
          l10n_util::GetNSString(
              IDS_IOS_ENHANCED_SAFE_BROWSING_PROMO_INSTRUCTIONS_STEP3),
        ];
      case BestFeaturesItemType::kLockedIncognitoTabs:
        return @[
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_FIRST_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_SECOND_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_LOCKED_INCOGNITO_THIRD_STEP),
        ];
      case BestFeaturesItemType::kSaveAndAutofillPasswords:
        return @[
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_FIRST_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_SECOND_STEP),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_NEVER_FORGET_PASSWORDS_THIRD_STEP),
        ];
      case BestFeaturesItemType::kTabGroups:
        return @[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_3),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TAB_GROUPS_STEP_4),
        ];
      case BestFeaturesItemType::kPriceTrackingAndInsights:
        return @[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_PRICE_TRACKING_STEP_3),
        ];
      case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
        // Add the correct strings depending on the device OS.
        if (@available(iOS 18, *)) {
          return @[
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_1),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_2_GENERAL),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_3_AUTOFILL_SETTINGS),
            l10n_util::GetNSString(
                IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_4_TOGGLE),
          ];
        }
        return @[
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_1),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_2_PASSWORDS),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_3_PASSWORD_OPTIONS),
          l10n_util::GetNSString(
              IDS_IOS_BEST_FEATURES_PASSWORDS_IN_OTHER_APPS_STEP_4_SELECT),
        ];
      case BestFeaturesItemType::kSharePasswordsWithFamily:
        return @[
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_1),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_2),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_3),
          l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SHARE_PASSWORDS_STEP_4),
        ];
    }
}

- (NSDictionary<NSString*, UIColor*>*)lightModeColorProvider {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      return LensColorProvider(0xEDF4FE, 0xFFFFFF);
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
    case BestFeaturesItemType::kLockedIncognitoTabs:
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
    case BestFeaturesItemType::kTabGroups:
    case BestFeaturesItemType::kPriceTrackingAndInsights:
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return nil;
  }
}

- (NSDictionary<NSString*, UIColor*>*)darkModeColorProvider {
  switch (self.type) {
    case BestFeaturesItemType::kLensSearch:
      return LensColorProvider(0x232428, 0x464A4E);
    case BestFeaturesItemType::kEnhancedSafeBrowsing:
    case BestFeaturesItemType::kLockedIncognitoTabs:
    case BestFeaturesItemType::kSaveAndAutofillPasswords:
    case BestFeaturesItemType::kTabGroups:
    case BestFeaturesItemType::kPriceTrackingAndInsights:
    case BestFeaturesItemType::kAutofillPasswordsInOtherApps:
    case BestFeaturesItemType::kSharePasswordsWithFamily:
      return nil;
  }
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

@end
