// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_egtest_util.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/find_in_page/features.h"
#import "ios/chrome/browser/find_in_page/find_in_page_app_interface.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constants to identify the Find navigator UI components in the view hierarchy.
constexpr char kFindInPageDoneButtonID[] = "find.doneButton";
constexpr char kFindInPageSearchFieldID[] = "find.searchField";
constexpr char kFindInPageResultLabelID[] = "find.resultLabel";
constexpr char kFindInPageNextButtonID[] = "find.nextButton";
constexpr char kFindInPagePreviousButtonID[] = "find.previousButton";
constexpr char kFindInPageClearButtonKindOfClassName[] =
    "_UITextFieldClearButton";
}

// Tests for Native Find in Page. This tests the variant of Native Find in Page
// with a Find interaction i.e. with the system UI or Find navigator. Many tests
// use the `secondTestServer` to ensure what is being tested also works with
// cross-origin iframes.
@interface FindInPageTestCase
    : ChromeTestCase <FindInPageTestCaseHelperDelegate>

@end

@implementation FindInPageTestCase {
  FindInPageTestCaseHelper* _helper;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // Enable `kNativeFindInPage`. This test case only tests the "System Find
  // Panel" variant of Native Find in Page i.e. the one relying on a Find
  // interaction and the new system UI.
  config.additional_args.push_back(
      "--enable-features=" + std::string(kNativeFindInPage.name) + "<" +
      std::string(kNativeFindInPage.name));
  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kNativeFindInPage.name) + "/Test");

  return config;
}

- (void)setUp {
  [super setUp];

  // Clear saved search term.
  [FindInPageAppInterface clearSearchTerm];

  // Creating helper.
  _helper = [[FindInPageTestCaseHelper alloc] init];
  _helper.testServer = self.testServer;
  _helper.delegate = self;
}

#pragma mark - FindInPageTestCaseHelperDelegate

- (void)openFindInPageWithOverflowMenu {
  [ChromeEarlGreyUI openToolsMenu];

  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuFindInPageId),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:tableViewMatcher] performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[self findInPageInputField]];
  [ChromeEarlGreyUI waitForAppToIdle];
}

- (void)closeFindInPageWithDoneButton {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@(kFindInPageDoneButtonID))]
      performAction:grey_tap()];
}

- (void)typeFindInPageText:(NSString*)text {
  // TODO(crbug.com/1454851): This should use grey_replaceText, but the clear
  // button doesn't show up, so use simulatePhysicalKeyboardEvent instead.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:text flags:0];
}

- (void)pasteTextToFindInPage:(NSString*)text {
  [ChromeEarlGrey copyTextToPasteboard:text];
  [[EarlGrey selectElementWithMatcher:[self findInPageInputField]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasteButton()] performAction:grey_tap()];
}

- (void)clearFindInPageText {
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(
                                   @(kFindInPageClearButtonKindOfClassName))]
      performAction:grey_tap()];
}

- (id<GREYMatcher>)findInPageInputField {
  return grey_accessibilityID(@(kFindInPageSearchFieldID));
}

- (void)assertResultStringIsResult:(int)resultIndex
                        outOfTotal:(int)resultCount {
  // Returns "<current> of <total>" search results label (e.g "1 of 5").
  NSString* expectedResultsString =
      [NSString stringWithFormat:@"%d of %d", resultIndex, resultCount];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @(kFindInPageResultLabelID))]
        assertWithMatcher:chrome_test_util::StaticTextWithAccessibilityLabel(
                              expectedResultsString)
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)assertResultStringIsEmptyOrZero {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @(kFindInPageResultLabelID))]
        assertWithMatcher:
            grey_anyOf(chrome_test_util::StaticTextWithAccessibilityLabel(@"0"),
                       grey_notVisible(), nil)
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)assertResultStringIsNonZero {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @(kFindInPageResultLabelID))]
        assertWithMatcher:
            grey_not(grey_anyOf(
                chrome_test_util::StaticTextWithAccessibilityLabel(@"0"),
                grey_notVisible(), nil))
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for correct Find in Page results string to appear");
}

- (void)advanceToNextResult {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@(kFindInPageNextButtonID))]
      performAction:grey_tap()];
}

- (void)advanceToPreviousResult {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @(kFindInPagePreviousButtonID))]
      performAction:grey_tap()];
}

#pragma mark - Tests

// Tests that FIP can be opened with Overflow menu.
- (void)testFindInPageFromOverflowMenu {
  [_helper helperTestFindInPageFromOverflowMenu];
}

// Tests that characters appear in the search box and that results UI updates as
// each characters is entered/deleted.
- (void)testFindInPageTextInput {
  [_helper helperTestFindInPageTextInput];
}

// Tests that the number of results for a query accounts for all the matches
// across frames, here with a main frame and a cross-origin iframe.
- (void)testFindInPageSupportsCrossOriginFrame {
  [_helper helperTestFindInPageSupportsCrossOriginFrame];
}

// Tests that FIP can find different types of characters: special characters,
// number, strings with both letters and numbers as well as non-ASCII
// characters.
- (void)testFindInPageSpecialCharacters {
  [_helper helperTestFindInPageSpecialCharacters];
}

// Tests that text can be copied from the web page and pasted into the FIP input
// field and that the results UI updates accordingly.
- (void)testFindInPageCopyPaste {
  [_helper helperTestFindInPageCopyPaste];
}

// Tests that FIP yields no results for an empty search query.
- (void)testFindInPageEmptySearchQuery {
  [_helper helperTestFindInPageEmptySearchQuery];
}

// Tests that FIP yields no results for a non-empty query with no matches in the
// page.
- (void)testFindInPageQueryWithNoMatches {
  [_helper helperTestFindInPageQueryWithNoMatches];
}

// Tests that FIP yields no matches for a text with spanish accents e.g. 'á' if
// the web page contains the same text without spanish accents e.g. 'a'. This
// test assumes removing accents from `kFindInPageTestWithSpanishAccentText`
// yields `kFindInPageTestWithoutSpanishAccentText`.
- (void)testFindInPageDifferentAccent {
  [_helper helperTestFindInPageDifferentAccent];
}

// Test that there is no query persistence with this variant of Native Find in
// Page i.e. with Find interaction.
- (void)testFindInPageHistory {
  [_helper helperTestFindInPageHistoryWithQueryPersistence:NO];
}

// Tests that there is no query persistence from an non-Incognito to an
// Incognito tab.
- (void)testFindInPageNormalToIncognito {
  [_helper helperTestFindInPageNormalToIncognito];
}

// Tests that Next/Previous buttons work and wrap.
- (void)testFindInPageNextPreviousArrows {
  [_helper helperTestFindInPageNextPreviousArrows];
}

// Tests the various ways to dismiss the keyboard during a Find session.
- (void)testFindInPageDismissKeyboard {
  [_helper helperTestFindInPageDismissKeyboard];
}

// Tests that FIP can find long strings of characters.
- (void)testFindInPageLongString {
  [_helper helperTestFindInPageLongString];
}

// Tests that FIP is not case sensitive.
- (void)testFindInPageNotCaseSensitive {
  [_helper helperTestFindInPageNotCaseSensitive];
}

// Tests that there is no leak of the FIP search query from Incognito tabs to
// normal tabs.
- (void)testFindInPageIncognitoHistory {
  [_helper helperTestFindInPageIncognitoHistory];
}

// Tests that there is no query persistence when coming back to a normal tab
// after switching temporarily to another tab.
- (void)testFindInPageSwitchingTabs {
  [_helper helperTestFindInPageSwitchingTabsWithQueryPersistence:NO];
}

// Tests that FIP can find RTL text in a web page.
- (void)testFindInPageRTL {
  [_helper helperTestFindInPageRTL];
}

// Tests that Find in Page can find matches in an Incognito tab.
- (void)testFindInPageIncognito {
  [_helper helperTestFindInPageIncognito];
}

// Tests accessibility of the Find in Page screen.
- (void)testFindInPageAccessibility {
  [_helper helperTestFindInPageAccessibility];
}

// Tests that Native Find in Page works as expected for PDF documents.
- (void)testFindInPagePDF {
  [_helper helperTestFindInPagePDF];
}

@end
