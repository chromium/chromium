// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_ITEM_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_ITEM_H_

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
  kSharePasswordsWithFamily = 7
};
// LINT.ThenChange(/ios/chrome/browser/first_run/ui_bundled/welcome_back/model/welcome_back_prefs.mm:IntToBestFeaturesItemType)

// Holds properties and values needed to configure the items in the Best
// Features Screen.
@interface BestFeaturesItem : NSObject

// Best Features type.
@property(nonatomic, assign) BestFeaturesItemType type;
// Best Features item title.
@property(nonatomic, copy) NSString* title;
// Best Features item subtitle for feature promo.
@property(nonatomic, copy) NSString* subtitle;
// Best Features item caption for feature row.
@property(nonatomic, copy) NSString* caption;
// Best Features item icon image.
@property(nonatomic, copy) UIImage* iconImage;
// Best Features item icon background color.
@property(nonatomic, copy) UIColor* iconBackgroundColor;
// Best Features item animation name.
@property(nonatomic, copy) NSString* animationName;
// Best Features item animation text provider for localization.
@property(nonatomic, copy) NSDictionary* textProvider;
// Best Features item instruction steps.
@property(nonatomic, copy) NSArray<NSString*>* instructionSteps;

// Initializes the item for BestFeaturesItemType `type`.
- (instancetype)initWithType:(BestFeaturesItemType)type
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_ITEM_H_
