// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_matchers.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

// Tests the omnibox text field when editing.
@interface OmniboxEditTestCase : ChromeTestCase
@end

@implementation OmniboxEditTestCase

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&omnibox::OmniboxHTTPResponses));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // Enable flags for rich inline autocomplete tests.
  if ([self isRunningTest:@selector(testRichInlineDefaultSuggestion)]) {
    config.features_enabled.push_back(omnibox::kRichAutocompletion);
  }

  // Disable AutocompleteProvider types: TYPE_SEARCH and TYPE_ON_DEVICE_HEAD.
  omnibox::DisableAutocompleteProviders(config, 1056);

  return config;
}

#pragma mark - Test clear button

// Tests clearing text in the pre-edit state of the omnibox.
- (void)testClearButtonPreEditState {
  [OmniboxEarlGrey openPage:omnibox::Page(1) testServer:self.testServer];
  [ChromeEarlGreyUI focusOmnibox];

  // Tap the clear button.
  [[EarlGrey selectElementWithMatcher:omnibox::ClearButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the omnibox is empty.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];

  // Verify that the clear button is not visible.
  [[EarlGrey selectElementWithMatcher:omnibox::ClearButtonMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests clearing text in the edit state of the omnibox.
- (void)testClearButtonEditState {
  [OmniboxEarlGrey openPage:omnibox::Page(1) testServer:self.testServer];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"something"];

  // Tap the clear button.
  [[EarlGrey selectElementWithMatcher:omnibox::ClearButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the omnibox is empty.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];

  // Verify that the clear button is not visible.
  [[EarlGrey selectElementWithMatcher:omnibox::ClearButtonMatcher()]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - Test rich inline

// Tests navigating to a shortcut as a default match.
- (void)testRichInlineDefaultSuggestion {
  // Add 2 shortcuts Page(1) and Page(2).
  [OmniboxEarlGrey addShorcuts:2 toTestServer:self.testServer];

  omnibox::Page shortcutPage = omnibox::Page(1);

  // Type the shortcut input in the omnibox.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(
                                     omnibox::PageTitle(shortcutPage))];

  // The shortcut suggestion should be default match. Press enter to navigate to
  // it.
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [ChromeEarlGrey
      waitForWebStateContainingText:omnibox::PageContent(shortcutPage)];
}

@end

// Tests the omnibox text field when editing, with search provider.
@interface OmniboxEditAllowSearchTestCase : ChromeTestCase
@end

@implementation OmniboxEditAllowSearchTestCase

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&omnibox::OmniboxHTTPResponses));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [OmniboxAppInterface
      setUpFakeSuggestionsService:@"fake_suggestions_sample.json"];

  [ChromeEarlGrey clearBrowsingHistory];
}

- (void)tearDown {
  [super tearDown];
  [OmniboxAppInterface tearDownFakeSuggestionsService];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // Enable flags for rich inline autocomplete tests.
  if ([self isRunningTest:@selector(testRichInlineRemovedByTap)] ||
      [self isRunningTest:@selector(testRichInlineRemovedByDelete)] ||
      [self isRunningTest:@selector(testRichInlineRemovedWithArrowKey)]) {
    config.features_enabled.push_back(omnibox::kRichAutocompletion);
  }

  // Disable AutocompleteProvider type TYPE_ON_DEVICE_HEAD.
  omnibox::DisableAutocompleteProviders(config, 1024);

  return config;
}

/// Verifies that the first suggestion is of type search.
- (void)assertFirstSuggestionIsSearch {
  NSIndexPath* firstRow = [NSIndexPath indexPathForRow:0 inSection:0];
  // Verifies that the first suggestion doesn't contain a secondary text.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_ancestor(
                                       omnibox::PopupRowAtIndex(firstRow)),
                                   omnibox::PopupRowSecondaryTextMatcher(),
                                   nil)] assertWithMatcher:grey_nil()];
}

/// Verifies that the first suggestion is not of type search.
- (void)assertFirstSuggestionIsURL {
  NSIndexPath* firstRow = [NSIndexPath indexPathForRow:0 inSection:0];
  // Verifies that the first suggestion contains a secondary text.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_ancestor(
                                       omnibox::PopupRowAtIndex(firstRow)),
                                   omnibox::PopupRowSecondaryTextMatcher(),
                                   nil)]
      assertWithMatcher:[OmniboxEarlGrey isURLMatcher]];
}

#pragma mark - Test rich inline

// Tests removing rich inline autocomplete by tapping the omnibox.
- (void)testRichInlineRemovedByTap {
  // Add 2 shortcuts Page(1) and Page(2).
  [OmniboxEarlGrey addShorcuts:2 toTestServer:self.testServer];

  omnibox::Page shortcutPage = omnibox::Page(1);

  // Type the shortcut input in the omnibox.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(
                                     omnibox::PageTitle(shortcutPage))];

  [self assertFirstSuggestionIsURL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];

  [self assertFirstSuggestionIsSearch];
}

// Tests removing rich inline autocomplete by pressing delete.
- (void)testRichInlineRemovedByDelete {
  // Add 2 shortcuts Page(1) and Page(2).
  [OmniboxEarlGrey addShorcuts:2 toTestServer:self.testServer];

  omnibox::Page shortcutPage = omnibox::Page(1);

  // Type the shortcut input in the omnibox.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(
                                     omnibox::PageTitle(shortcutPage))];

  [self assertFirstSuggestionIsURL];

  // Press the backspace HW keyboard key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\b" flags:0];

  [self assertFirstSuggestionIsSearch];
}

// Tests removing rich inline autocomplete by pressing an arrow key.
- (void)testRichInlineRemovedWithArrowKey {
  // Add 2 shortcuts Page(1) and Page(2).
  [OmniboxEarlGrey addShorcuts:2 toTestServer:self.testServer];

  omnibox::Page shortcutPage = omnibox::Page(1);

  // Type the shortcut input in the omnibox.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(
                                     omnibox::PageTitle(shortcutPage))];

  [self assertFirstSuggestionIsURL];

  // Simulate press the HW left arrow key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"leftArrow" flags:0];

  [self assertFirstSuggestionIsSearch];
}

@end
