// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"

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
