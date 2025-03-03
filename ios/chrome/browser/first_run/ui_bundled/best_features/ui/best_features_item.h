// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_ITEM_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_ITEM_H_

#import <UIKit/UIKit.h>

// An enum representing the different features promoted by Bling's Best
// Features.
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

// Holds properties and values needed to configure the items in the Best
// Features Screen.
@interface BestFeaturesItem : NSObject

// Best Features type.
@property(nonatomic, assign) BestFeaturesItemType type;
// Best Features item title.
@property(nonatomic, copy) NSString* title;
// Best Features item subtitle.
@property(nonatomic, copy) NSString* subtitle;
// Best Features item icon image.
@property(nonatomic, copy) UIImage* iconImage;
// Best Features item icon background color.
@property(nonatomic, copy) UIColor* iconBackgroundColor;
// Best Features item animation name.
@property(nonatomic, copy) NSString* animationName;
// Best Features item instruction steps.
@property(nonatomic, copy) NSArray<NSString*>* instructionSteps;

// Returns a configured item for the given `itemType`.
+ (BestFeaturesItem*)itemForType:(BestFeaturesItemType)itemType;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_ITEM_H_
