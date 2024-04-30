// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"
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
    config.features_enabled.push_back(
        omnibox::kOmniboxPopulateShortcutsDatabase);
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
  [ChromeEarlGreyUI focusOmniboxAndType:@"something"];

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

- (void)testTapBehaviors {
  [OmniboxEarlGrey openPage:omnibox::Page(1) testServer:self.testServer];

  GURL fullPage1GURL = self.testServer->GetURL(
      omnibox::PageURL(omnibox::Page(1)));  //< http://127.0.0.1/foobar

  NSString* schemePrefix = [NSString
      stringWithFormat:@"%@://", base::SysUTF8ToNSString(
                                     fullPage1GURL.scheme())];  //< http://
  NSString* page1URL = base::SysUTF8ToNSString(fullPage1GURL.spec());
  page1URL = [page1URL
      substringFromIndex:schemePrefix.length];  //< 127.0.0.1:123/page1.html

  // Expect "127" to autocomplete to 127[0.0.1:123/page1.html]
  NSString* typedText = [page1URL substringToIndex:3];
  NSString* inlineAutocomplete = [page1URL substringFromIndex:typedText.length];

  [ChromeEarlGreyUI focusOmniboxAndType:typedText];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingAutocompleteText(
                            inlineAutocomplete)];

  // Tapping the inline autocomplete should accept it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingAutocompleteText(
                            @"")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(page1URL))];
}

#pragma mark - Test rich inline

// Tests navigating to a shortcut as a default match.
- (void)testRichInlineDefaultSuggestion {
  // Add 2 shortcuts Page(1) and Page(2).
  [OmniboxEarlGrey addShorcuts:2 toTestServer:self.testServer];

  omnibox::Page shortcutPage = omnibox::Page(1);

  // Type the shortcut input in the omnibox.
  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(
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
