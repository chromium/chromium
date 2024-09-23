// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/prepopulated_engines.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Returns the search engine button element interaction for the settings table
// view controller.
GREYElementInteraction* GetInteractionForSearchEngineSettingButton() {
  return
      [[EarlGrey selectElementWithMatcher:
                     grey_allOf(chrome_test_util::SettingsSearchEngineButton(),
                                grey_sufficientlyVisible(), nil)]
             usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
          onElementWithMatcher:chrome_test_util::SettingsCollectionView()];
}

}  // namespace

@implementation SearchEngineChoiceEarlGreyUI

+ (void)selectSearchEngineCellWithName:(NSString*)searchEngineName
                       scrollDirection:(GREYDirection)scrollDirection
                                amount:(CGFloat)scrollAmount {
  NSString* searchEngineAccessibiltyIdentifier =
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 searchEngineName];
  id<GREYMatcher> searchEngineRowMatcher =
      grey_allOf(grey_userInteractionEnabled(),
                 grey_accessibilityID(searchEngineAccessibiltyIdentifier),
                 grey_sufficientlyVisible(), nil);
  // Scroll down to find the search engine cell.
  id<GREYMatcher> scrollView =
      grey_accessibilityID(kSearchEngineChoiceScrollViewIdentifier);
  [[[EarlGrey selectElementWithMatcher:searchEngineRowMatcher]
         usingSearchAction:grey_scrollInDirection(scrollDirection, scrollAmount)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
  // Tap on the search engine cell.
  [[[EarlGrey selectElementWithMatcher:searchEngineRowMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
}

+ (void)confirmSearchEngineChoiceScreen {
  id<GREYMatcher> moreButtonMatcher =
      grey_allOf(grey_accessibilityID(kSearchEngineMoreButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    // Tap on the "More" button if it exists.
    [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  }
  // Tap on the "Set as Default" button.
  id<GREYMatcher> primaryButtonMatcher =
      grey_allOf(grey_accessibilityID(kSetAsDefaultSearchEngineIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:primaryButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
}

+ (void)verifySearchEngineChoiceScreenIsDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSearchEngineChoiceTitleAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  return;
}

+ (void)openSearchEngineSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [GetInteractionForSearchEngineSettingButton() performAction:grey_tap()];
}

+ (void)verifyDefaultSearchEngineSetting:(NSString*)searchEngineName {
  // Opens the default search engine settings menu.
  [ChromeEarlGreyUI openSettingsMenu];
  // Verifies that the correct search engine is selected. The default engine's
  // name appears in the name of the selected row.
  [GetInteractionForSearchEngineSettingButton()
      assertWithMatcher:grey_allOf(grey_accessibilityValue(searchEngineName),
                                   grey_sufficientlyVisible(), nil)];
}

+ (id<GREYMatcher>)settingsSearchEngineMatcherWithName:(NSString*)name {
  NSString* label = [NSString
      stringWithFormat:@"%@%@", kSettingsSearchEngineCellIdentifierPrefix,
                       name];
  return grey_accessibilityID(label);
}

+ (GREYElementInteraction*)interactionForSettingsSearchEngineWithName:
    (NSString*)name {
  id<GREYMatcher> searchEngineCellMatcher =
      [self settingsSearchEngineMatcherWithName:name];
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(searchEngineCellMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               kSearchEngineTableViewControllerId)];
}

+ (GREYElementInteraction*)interactionForSettingsWithPrepopulatedSearchEngine:
    (const TemplateURLPrepopulateData::PrepopulatedEngine&)prepopulatedEngine {
  NSString* name =
      [self searchEngineNameWithPrepopulatedEngine:prepopulatedEngine];
  return [self interactionForSettingsSearchEngineWithName:name];
}

+ (NSString*)searchEngineNameWithPrepopulatedEngine:
    (const TemplateURLPrepopulateData::PrepopulatedEngine&)prepopulatedEngine {
  return base::SysUTF16ToNSString(prepopulatedEngine.name);
}

@end
