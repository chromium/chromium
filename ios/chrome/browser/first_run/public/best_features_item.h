// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_BEST_FEATURES_ITEM_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_BEST_FEATURES_ITEM_H_

#import <UIKit/UIKit.h>

// An enum representing the different features promoted by Bling's Best
// Features. These values should not be reordered or reused.
// LINT.IfChange(BestFeaturesItemType)
enum class BestFeaturesItemType {
  kLensSearch = 0,
  kEnhancedSafeBrowsing = 1,
  kLockedIncognitoTabs = 2,
  kSaveAndAutofillPasswords = 3,
  kTabGroups = 4,
  kPriceTrackingAndInsights = 5,
  kAutofillPasswordsInOtherApps = 6,
  kSharePasswordsWithFamily = 7,
  kMaxValue = kSharePasswordsWithFamily,
};
// LINT.ThenChange(
// /ios/chrome/browser/welcome_back/model/welcome_back_prefs.mm:IntToBestFeaturesItemType,
// /tools/metrics/histograms/metadata/ios/enums.xml:IOSBestFeaturesItemType)

// Holds properties and values needed to configure the items in the Best
// Features Screen.
@interface BestFeaturesItem : NSObject

// Best Features type.
@property(nonatomic, assign, readonly) BestFeaturesItemType type;
// Best Features item title.
@property(nonatomic, copy, readonly) NSString* title;
// Best Features item subtitle for feature promo.
@property(nonatomic, copy, readonly) NSString* subtitle;
// Best Features item caption for feature row.
@property(nonatomic, copy, readonly) NSString* caption;
// Best Features item icon image.
@property(nonatomic, copy, readonly) UIImage* iconImage;
// Best Features item icon background color.
@property(nonatomic, copy, readonly) UIColor* iconBackgroundColor;
// Best Features item animation name.
@property(nonatomic, copy, readonly) NSString* animationName;
// Best Features item animation text provider for localization.
@property(nonatomic, copy, readonly) NSDictionary* textProvider;
// Best Features item instruction steps.
@property(nonatomic, copy, readonly) NSArray<NSString*>* instructionSteps;
// A dictionary that associate a keypath with a color, for the light/dark mode.
// If it is not nil, then only a single animation will be used (the _darkmode
// one will not be used).
@property(nonatomic, copy, readonly)
    NSDictionary<NSString*, UIColor*>* lightModeColorProvider;
@property(nonatomic, copy, readonly)
    NSDictionary<NSString*, UIColor*>* darkModeColorProvider;

// Initializes the item for BestFeaturesItemType `type`.
- (instancetype)initWithType:(BestFeaturesItemType)type
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_BEST_FEATURES_ITEM_H_
