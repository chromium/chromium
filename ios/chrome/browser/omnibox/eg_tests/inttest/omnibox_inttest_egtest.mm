// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_app_interface.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_earl_grey.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_earl_grey.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_matchers.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_test_util.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"

namespace {

/// Shortcut text. Autocompleted text associated to the shortcut URL.
NSString* const kShortcutText = @"shortcut";
/// Shortcut URL. URL loaded when accepting the `kShortcutText` autocomplete.
const GURL kShortcutURL = GURL("https://www.shortcut.com");

}  // namespace

// Tests the omnibox integration with OmniboxInttestCoordinator.
@interface OmniboxInttestTestCase : ChromeTestCase
@end

@implementation OmniboxInttestTestCase

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startOmniboxCoordinator];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  [ChromeCoordinatorAppInterface reset];
}

#pragma mark - Rich inline test cases

// Tests accepting a shortcut suggestion.
- (void)testRichInlineSelection {
  [OmniboxInttestEarlGrey addURLShortcutMatch:kShortcutText
                               destinationURL:kShortcutURL];
  [OmniboxInttestEarlGrey focusOmniboxAndType:@"short"];

  [OmniboxEarlGrey acceptOmniboxText];
  [OmniboxInttestEarlGrey assertURLLoaded:kShortcutURL];
}

// Tests removing shortcut suggestion by tapping the textfield.
- (void)testRichInlineRemovedByTap {
  [OmniboxInttestEarlGrey addURLShortcutMatch:kShortcutText
                               destinationURL:kShortcutURL];
  [OmniboxInttestEarlGrey focusOmniboxAndType:@"short"];

  // Tap the omnibox to accept autocomplete and remove rich inline.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];

  [OmniboxEarlGrey acceptOmniboxText];
  [OmniboxInttestEarlGrey assertSearchLoaded:kShortcutText];
}

// Tests removing shorcut suggestion by pressing delete.
- (void)testRichInlineRemovedByDelete {
  [OmniboxInttestEarlGrey addURLShortcutMatch:kShortcutText
                               destinationURL:kShortcutURL];
  [OmniboxInttestEarlGrey focusOmniboxAndType:@"short"];

  // Press the backspace HW keyboard key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\b" flags:0];

  [OmniboxEarlGrey acceptOmniboxText];
  [OmniboxInttestEarlGrey assertSearchLoaded:@"short"];
}

// Tests removing shortcut suggestion by pressing an arrow key.
- (void)testRichInlineRemovedWithArrowKey {
  [OmniboxInttestEarlGrey addURLShortcutMatch:kShortcutText
                               destinationURL:kShortcutURL];
  [OmniboxInttestEarlGrey focusOmniboxAndType:@"short"];

  // Simulate press the HW left arrow key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"leftArrow" flags:0];

  [OmniboxEarlGrey acceptOmniboxText];
  [OmniboxInttestEarlGrey assertSearchLoaded:kShortcutText];
}

@end
