// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_

#import <UIKit/UIKit.h>

@class ContentEntryPointUnavailabilityItem;
@class PageActionMenuFeature;
@class PageActionMenuContentEntryPoint;

// Page Menu Action Feature types.
typedef NS_ENUM(NSInteger, PageActionMenuFeatureType);

// The mutator for the page action menu.
@protocol PageActionMenuMutator

// Returns whether Reader mode is currently active.
- (BOOL)isReaderModeActive;

// Returns whether Reader mode is available.
- (BOOL)isReaderModeAvailable;

// Returns Gemini floaty entry point configuration item.
- (PageActionMenuContentEntryPoint*)geminiEntryPoint;

// Returns Lens overlay entry point configuration item.
- (PageActionMenuContentEntryPoint*)lensEntryPointForTraitCollection:
    (UITraitCollection*)traitCollection;

// Returns Reader mode entry point configuration item.
- (PageActionMenuContentEntryPoint*)readerModeEntryPoint;

// Returns the ordered list of unavalability items to display in the footer.
- (NSArray<ContentEntryPointUnavailabilityItem*>*)
    unavailabilityItemsForTraitCollection:(UITraitCollection*)traitCollection;

// Returns whether a page action menu feature is currently available.
- (BOOL)isFeatureAvailable:(PageActionMenuFeatureType)featureType;

// Returns the current translate language pair.
- (NSString*)translateLanguagePair;

// Returns the number of blocked popups.
- (NSInteger)blockedPopupCount;

// Returns the current site domain for permission context.
- (NSString*)currentSiteDomain;

// Updates the specified permission for the current site.
- (void)updatePermission:(BOOL)granted
              forFeature:(PageActionMenuFeatureType)featureType;

// Returns array of currently active features to display.
- (NSArray<PageActionMenuFeature*>*)activeFeatures;

// Allows all blocked popups for the current site.
- (void)allowBlockedPopups;

// Reverts the page translation to show the original language.
- (void)revertTranslation;

// Opens the price insights contextual panel.
- (void)openPriceInsightsPanel;

// Opens the translation settings modal.
- (void)openTranslateOptions;

// Returns whether the AI entry points should be shown.
- (BOOL)shouldShowFeatureEntryPoints;

// Returns YES if the user is signed in.
- (BOOL)isUserSignedIn;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_
