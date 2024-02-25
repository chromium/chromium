// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_EARL_GREY_UI_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_EARL_GREY_UI_TEST_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/third_party/earl_grey2/src/CommonLib/GREYConstants.h"

@class GREYElementInteraction;
@protocol GREYMatcher;

// State of the fake omnibox illustration
typedef NS_ENUM(NSUInteger, FakeOmniboxState) {
  kHidden,
  kEmpty,
  kFull,
};

// Test methods that perform actions on the search engine choice UI
@interface SearchEngineChoiceEarlGreyUI : NSObject

// Selects search engine cell with `search_engine_name`.
+ (void)selectSearchEngineCellWithName:(NSString*)searchEngineName
                       scrollDirection:(GREYDirection)scrollDirection
                                amount:(CGFloat)scrollAmount;

// Confirms the search engine choice screen. A search engine must have been
// selected before.
+ (void)confirmSearchEngineChoiceScreen;

// Checks that the search engine choice screen is being displayed.
+ (void)verifySearchEngineChoiceScreenIsDisplayed;

// Checks that the default search engine was correctly set.
+ (void)verifyDefaultSearchEngineSetting:(NSString*)searchEngineName;

// Checks the state of the fake omnibox illustration.
+ (void)verifyFakeOmniboxIllustrationState:(FakeOmniboxState)state;

// Returns GreyMatcher for the custom search engine for the settings table view.
// The custom search engine URL needs to be 127.0.0.1.
+ (id<GREYMatcher>)settingsCustomSearchEngineAccessibilityLabelWithName:
    (const char*)name;

// Returns the GREYElementInteraction* for the item in the search engine list
// with the given `matcher`. It scrolls in `direction` if necessary to ensure
// that the matched item is sufficiently visible, thus interactable.
// The custom search engine URL needs to be 127.0.0.1.
+ (GREYElementInteraction*)interactionForSettingsCustomSearchEngineWithName:
    (const char*)name;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_EARL_GREY_UI_TEST_UTIL_H_
