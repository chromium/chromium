// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
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
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using base::test::ios::kWaitForUIElementTimeout;

namespace {

/// Shortcut text. Autocompleted text associated to the shortcut URL.
NSString* const kShortcutText = @"shortcut";
/// Shortcut URL. URL loaded when accepting the `kShortcutText` autocomplete.
const GURL kShortcutURL = GURL("https://www.shortcut.com");
/// URL of google search by image.
const GURL kSearchByImageURL =
    GURL("https://www.google.com/searchbyimage/upload");

/// Returns Search Copied Image button from UIMenuController.
id<GREYMatcher> SearchCopiedImageMenuButton() {
  NSString* a11yLabelCopiedImage =
      l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE);
  return grey_allOf(grey_accessibilityLabel(a11yLabelCopiedImage),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

}  // namespace

// Tests the omnibox integration with OmniboxInttestCoordinator using fake
// suggestions.
@interface OmniboxFakeSuggestionsInttestTestCase : ChromeTestCase
@end

@implementation OmniboxFakeSuggestionsInttestTestCase

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startOmniboxCoordinator];
  // Stub autocomplete suggestions with fake suggestions.
  [OmniboxInttestAppInterface enableFakeSuggestions];
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

#pragma mark - Omnibox long press menu test cases

// Tests that Search Copied Image menu button is shown with an image in the
// clipboard and is starting an image search.
- (void)testOmniboxMenuSearchCopiedImage {
  [OmniboxInttestEarlGrey focusOmnibox];

  UIImage* image = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(10, 10), [UIColor greenColor]);
  [ChromeEarlGrey copyImageToPasteboard:image];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Wait for UIMenuController to appear or timeout after 2 seconds.
  GREYCondition* SearchImageButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Search Copied Image button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:SearchCopiedImageMenuButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([SearchImageButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Search Copied Image button display failed");
  [[EarlGrey selectElementWithMatcher:SearchCopiedImageMenuButton()]
      performAction:grey_tap()];

  // Check that the omnibox started an image search.
  NSString* lastLoadedURL =
      [NSString cr_fromString:ChromeCoordinatorAppInterface.lastURLLoaded
                                  .absoluteString.UTF8String];
  NSString* expectedURL = [NSString cr_fromString:kSearchByImageURL.spec()];
  GREYAssertEqualObjects(lastLoadedURL, expectedURL, @"Image search expected");
}

@end
