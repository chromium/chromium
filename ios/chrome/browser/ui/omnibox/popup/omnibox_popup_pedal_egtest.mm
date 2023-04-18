// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kDinoPedalString = @"chrome://dino";
NSString* kDinoSearchString = @"dino game";

// Returns a matcher for a popup row containing `string` as accessibility label.
id<GREYMatcher> popupRowWithString(NSString* string) {
  id<GREYMatcher> textMatcher = grey_descendant(
      chrome_test_util::StaticTextWithAccessibilityLabel(string));
  id<GREYMatcher> popupRow =
      grey_allOf(chrome_test_util::OmniboxPopupRow(), textMatcher,
                 grey_sufficientlyVisible(), nil);
  return popupRow;
}

}  // namespace

@interface OmniboxPedalsTestCase : ChromeTestCase
@end
@implementation OmniboxPedalsTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];

  [OmniboxAppInterface
      setUpFakeSuggestionsService:@"fake_suggestions_pedal.json"];
}

- (void)tearDown {
  [OmniboxAppInterface tearDownFakeSuggestionsService];
}

// Tests that the dino pedal is present and that it opens the dino game.
- (void)testDinoPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndType:@"pedaldino"];

  // Matcher for the dino pedal and search suggestions.
  id<GREYMatcher> dinoPedal = popupRowWithString(kDinoPedalString);
  id<GREYMatcher> dinoSearch = popupRowWithString(kDinoSearchString);

  // Dino pedal and search suggestions should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:dinoPedal];
  [[EarlGrey selectElementWithMatcher:dinoSearch]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on dino pedal.
  [[EarlGrey selectElementWithMatcher:dinoPedal] performAction:grey_tap()];

  // The dino game should be loaded.
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(kDinoPedalString))];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the dino pedal does not appear when the search suggestion is below
// the top 3.
- (void)testNoPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndType:@"nopedal"];

  // Matcher for the dino pedal and search suggestions.
  id<GREYMatcher> dinoPedal = popupRowWithString(kDinoPedalString);
  id<GREYMatcher> dinoSearch = popupRowWithString(kDinoSearchString);

  // The dino search suggestion should be present.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:dinoSearch];

  // The dino pedal should not appear.
  [[EarlGrey selectElementWithMatcher:dinoPedal] assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeCurrentTab];
}
@end
