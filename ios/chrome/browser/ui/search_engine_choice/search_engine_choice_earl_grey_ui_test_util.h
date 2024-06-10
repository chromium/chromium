// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_EARL_GREY_UI_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_EARL_GREY_UI_TEST_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/third_party/earl_grey2/src/CommonLib/GREYConstants.h"

@class GREYElementInteraction;
@protocol GREYMatcher;

namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
}  // namespace TemplateURLPrepopulateData

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

// Opens the search engine settings.
+ (void)openSearchEngineSettings;

// Checks that the default search engine was correctly set.
+ (void)verifyDefaultSearchEngineSetting:(NSString*)searchEngineName;

// Returns GreyMatcher for the search engine for the settings table view.
+ (id<GREYMatcher>)settingsSearchEngineMatcherWithName:(NSString*)name;

// Returns the GREYElementInteraction for the item in the search engine list
// with the given `name`. It scrolls the settings table view in down direction
// if necessary to ensure that the matched item is sufficiently visible, thus
// interactable.
+ (GREYElementInteraction*)interactionForSettingsSearchEngineWithName:
    (NSString*)name;

// Returns the GREYElementInteraction* for the item in the search engine list
// with the given `prepopulatedEngine`. It scrolls the settings table view in
// down direction if necessary to ensure that the matched item is  sufficiently
// visible, thus interactable.
+ (GREYElementInteraction*)interactionForSettingsWithPrepopulatedSearchEngine:
    (const TemplateURLPrepopulateData::PrepopulatedEngine&)prepopulatedEngine;

// Returns the search engine name for `prepopulatedEngine`.
+ (NSString*)searchEngineNameWithPrepopulatedEngine:
    (const TemplateURLPrepopulateData::PrepopulatedEngine&)prepopulatedEngine;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_EARL_GREY_UI_TEST_UTIL_H_
